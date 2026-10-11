// src/slipperworks_logic.c —— 见 slipperworks_logic.h。
#include "slipperworks_logic.h"

#include <math.h>
#include <string.h>

#include "slipperworks_save.h"  // sv->* 解引用需要完整类型;save.h 反过来 include 本头文件拿 SW_TIERS

// 物理参数(逻辑像素 / 毫秒)。调手感只动这一段。
#define SW_STEP 4.0f        // 每个物理子步的时长
#define SW_MAX_SUBSTEPS 8   // 一帧最多几个子步(卡帧丢弃,不让物理追帧)
#define SW_GRAV 0.0022f     // 重力
#define SW_AIR 0.999f       // 每子步速度保留(空气阻力)
#define SW_REST 0.12f       // 鞋与鞋 / 鞋与地的弹性
#define SW_WALL_REST 0.3f   // 桶壁弹性
#define SW_PUSH 0.85f       // 每子步解掉的重叠比例
#define SW_SLEEP 0.02f      // 慢到这个速度以下且贴地就算停了
#define SW_MERGE_SLOP 1.5f  // 距离小于 r1+r2+这么多就合成(静止堆里是刚接触、零重叠,容差必须是正的)

// 各档物理半径(按字符画 w+h)/4 取整,和 slipperworks_art_tier 的尺寸对齐。
static const uint8_t SW_R[SW_TIERS] = {4, 5, 7, 8, 10, 11, 13, 15, 17, 18};

const uint16_t slipperworks_sell_price[SW_TIERS] = {1, 2, 6, 15, 40, 100, 250, 600, 1500, 4000};
const uint8_t slipperworks_shop_tier[SW_SHOP_N] = {2, 3, 4, 0};  // 3 号位是大锤,档位无用
const uint16_t slipperworks_shop_price[SW_SHOP_N] = {6, 14, 35, 10};

// 篮子等级表:可合最高档 / 桶宽百分比。Lv1 只能合到库巴拖,篮子也最小;Lv5 全解锁。
const uint8_t slipperworks_basket_cap[SW_BASKET_MAX] = {4, 6, 7, 8, 9};
const uint8_t slipperworks_basket_w[SW_BASKET_MAX] = {70, 80, 90, 95, 100};
const uint16_t slipperworks_basket_price[SW_BASKET_MAX - 1] = {50, 150, 500, 1200};

uint16_t slipperworks_bin_inner(const slipperworks_game_t *g) {
    uint8_t lv = g->basket_lv < 1 ? 1 : g->basket_lv > SW_BASKET_MAX ? SW_BASKET_MAX : g->basket_lv;
    uint32_t w = 160u * slipperworks_basket_w[lv - 1] * kit_heat_pct(g->heat, 110, 90) / 10000u;
    return (uint16_t)w;
}

uint16_t slipperworks_bin_x0(const slipperworks_game_t *g) {
    return (uint16_t)((240u - slipperworks_bin_inner(g)) / 2u);
}

static uint32_t rnd(slipperworks_game_t *g) {
    uint32_t x = g->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g->rng = x ? x : 0x9E3779B9u;
    return x;
}

// 免费鞋材:六成鞋垫四成鞋坯。
static uint8_t free_roll(slipperworks_game_t *g) {
    return rnd(g) % 10u < 6u ? 0 : 1;
}

static void clamp_hanger(slipperworks_game_t *g) {
    uint16_t x0 = slipperworks_bin_x0(g);
    uint16_t x1 = (uint16_t)(x0 + slipperworks_bin_inner(g));
    int16_t lo = (int16_t)(x0 + SW_R[g->cur % SW_TIERS] + 2);
    int16_t hi = (int16_t)(x1 - SW_R[g->cur % SW_TIERS] - 2);
    if (hi < lo) hi = lo;
    if (g->hanger_x < (uint16_t)lo) g->hanger_x = (uint16_t)lo;
    if (g->hanger_x > (uint16_t)hi) g->hanger_x = (uint16_t)hi;
}

void slipperworks_start(slipperworks_game_t *g, uint32_t seed, kit_heat_t heat, uint8_t basket_lv) {
    memset(g, 0, sizeof(*g));
    g->rng = seed ? seed : 0x9E3779B9u;
    g->heat = heat;
    g->basket_lv = basket_lv < 1 ? 1 : basket_lv > SW_BASKET_MAX ? SW_BASKET_MAX : basket_lv;
    g->cur = free_roll(g);
    g->nxt = free_roll(g);
    g->hammers = 1;  // 开局送一把大锤,教玩家有这个东西
    g->hanger_x = 120;
    clamp_hanger(g);
}

void slipperworks_hanger(slipperworks_game_t *g, int dir) {
    if (dir) g->hanger_x = (uint16_t)((int16_t)g->hanger_x + (dir < 0 ? -SW_HANGER_STEP : SW_HANGER_STEP));
    clamp_hanger(g);
}

// x 这一列有没有压着警戒线的鞋(大锤的瞄准判定:站塔下按●就是锤)
bool slipperworks_jam_column(const slipperworks_game_t *g, float x) {
    for (uint8_t i = 0; i < g->n; i++) {
        const sw_piece_t *p = &g->pcs[i];
        if (p->y - p->r >= SW_RIM_Y) continue;  // 没压线
        if (fabsf(x - p->x) < p->r + 6) return true;
    }
    return false;
}

bool slipperworks_drop(slipperworks_game_t *g) {
    if (g->drop_cd || g->n >= SW_MAX_PIECES) return false;
    // 冒线倒计时期间,压线那几列锁住:不许再往塔上摞(那是给塌塔添柴),
    // 要么站塔下按●敲,要么长按▼收;没冒线的地方随便丢随便摞
    if (g->jam_ms && slipperworks_jam_column(g, g->hanger_x)) return false;
    sw_piece_t *p = &g->pcs[g->n++];
    memset(p, 0, sizeof(*p));
    p->x = p->px = g->hanger_x;
    p->y = p->py = SW_DROP_Y;
    p->r = SW_R[g->cur];
    p->tier = g->cur;
    p->born = 120;  // 出手小弹一下
    p->awake = 1;
    g->last_drop = g->cur;
    g->drop_cd = SW_DROP_CD;
    g->undo_ms = SW_UNDO_MS;
    // 先丢后台排队的(按购买顺序),队空了才出免费鞋材;记快照给撤销用
    g->undo_from_queue = (g->queue_n > 0);
    g->undo_prev_nxt = g->nxt;
    if (g->queue_n) {
        g->cur = g->queue[0];
        for (uint8_t i = 1; i < g->queue_n; i++) g->queue[i - 1] = g->queue[i];
        g->queue_n--;
    } else {
        g->cur = g->nxt;
        g->nxt = free_roll(g);
    }
    clamp_hanger(g);
    return true;
}

bool slipperworks_undrop(slipperworks_game_t *g) {
    // 只有"刚丢下不久、最顶上确实是它"才撤得回来(被合成掉就认了)。
    if (!g->undo_ms || g->n == 0) return false;
    sw_piece_t *p = &g->pcs[g->n - 1];
    if (p->tier != g->last_drop) return false;  // 已经参与合成就不撤了
    uint8_t held = g->cur;  // 丢下后手里接班的:从队列出来的要还回队首
    g->cur = p->tier;
    if (g->undo_from_queue) {
        for (uint8_t i = g->queue_n; i > 0; i--) g->queue[i] = g->queue[i - 1];
        g->queue[0] = held;
        g->queue_n++;
    } else {
        g->nxt = g->undo_prev_nxt;  // 免费鞋材没被真正消费,还回去
    }
    g->n--;
    g->drop_cd = 0;
    g->undo_ms = 0;
    clamp_hanger(g);
    return true;
}

// 从天上落一只鞋(拖鞋雨用):带出场弹跳,醒着落下去。
static void spawn_piece(slipperworks_game_t *g, uint8_t tier, float x, float y) {
    if (g->n >= SW_MAX_PIECES) return;
    sw_piece_t *p = &g->pcs[g->n++];
    memset(p, 0, sizeof(*p));
    p->tier = tier;
    p->r = SW_R[tier];
    p->x = p->px = x;
    p->y = p->py = y;
    p->awake = 1;
    p->born = 200;
}

// 前置声明:导出时的落定结算要用
static void substep(slipperworks_game_t *g);
static uint8_t merge_round(slipperworks_game_t *g, slipperworks_save_t *sv, sw_tick_out_t *out);

static void wake_all(slipperworks_game_t *g) {
    for (uint8_t i = 0; i < g->n; i++) {
        g->pcs[i].awake = 1;
        g->pcs[i].still = 0;
    }
}

static void add_fly(slipperworks_game_t *g, const sw_piece_t *p) {
    for (uint8_t i = 0; i < SW_FLY_N; i++) {
        if (g->fly[i].t >= SW_FLY_MS) {
            g->fly[i].x = p->x;
            g->fly[i].y = p->y;
            g->fly[i].tier = p->tier;
            g->fly[i].t = 0;
            return;
        }
    }
    // 槽满了就顶掉最老的一条
    uint8_t oldest = 0;
    for (uint8_t i = 1; i < SW_FLY_N; i++) {
        if (g->fly[i].t > g->fly[oldest].t) oldest = i;
    }
    g->fly[oldest].x = p->x;
    g->fly[oldest].y = p->y;
    g->fly[oldest].tier = p->tier;
    g->fly[oldest].t = 0;
}

uint8_t slipperworks_collect(slipperworks_game_t *g, slipperworks_save_t *sv) {
    uint8_t got = 0;
    for (uint8_t i = g->n; i-- > 0;) {
        if (g->pcs[i].tier < SW_COLLECT_TIER) continue;
        sv->bag[g->pcs[i].tier]++;
        if (g->pcs[i].tier == 7) sv->flags |= 0x01;  // 珍藏里程碑:收到过就锁定
        add_fly(g, &g->pcs[i]);
        g->pcs[i] = g->pcs[--g->n];
        got++;
    }
    if (got) {
        g->jam_ms = 0;
        wake_all(g);
    }
    return got;
}

bool slipperworks_hammer(slipperworks_game_t *g) {
    if (g->hammers == 0) return false;
    g->hammers--;
    wake_all(g);
    g->calm_ms = 900;  // 弹跳落定前不判翻桶
    for (uint8_t i = 0; i < g->n; i++) {
        sw_piece_t *p = &g->pcs[i];
        // 全桶向上抛起约 50px(带随机差异,不会像一整块木板)
        float up = 1.7f + (float)(rnd(g) % 50u) / 100.0f;
        p->py = p->y + up;
        p->px = p->x - (float)((int16_t)(rnd(g) % 21u) - 10) * 0.02f;
    }
    return true;
}

uint16_t slipperworks_basket_upgrade_price(const slipperworks_game_t *g) {
    if (g->basket_lv >= SW_BASKET_MAX) return 0;
    return slipperworks_basket_price[g->basket_lv - 1];
}

sw_buy_t slipperworks_buy(slipperworks_game_t *g, slipperworks_save_t *sv, uint8_t item) {
    if (item >= SW_SHOP_N) return SW_BUY_NO_MAT;
    if (item == 3) {
        if (g->hammers >= SW_HAMMER_MAX) return SW_BUY_FULL;
    } else if (item == 4) {
        if (g->basket_lv >= SW_BASKET_MAX) return SW_BUY_FULL;  // 已满级
    } else if (g->queue_n >= SW_QUEUE_MAX) {
        return SW_BUY_QFULL;  // 排队的实在太多(32 只,正常玩不到)
    }
    uint16_t price = item == 4 ? slipperworks_basket_upgrade_price(g) : slipperworks_shop_price[item];
    if (slipperworks_mat_of(sv) < price) return SW_BUY_NO_MAT;
    slipperworks_mat_set(sv, slipperworks_mat_of(sv) - price);
    if (item == 3) {
        g->hammers++;
    } else if (item == 4) {
        g->basket_lv++;  // 篮子升级:更宽,能合的档也更高
        sv->basket_lv = g->basket_lv;
    } else {
        g->queue[g->queue_n++] = slipperworks_shop_tier[item];  // 排队等着丢
    }
    return SW_BUY_OK;
}

// 把现场写进存档(桶里的鞋 + 排队 + 锤子 + 手里 + 随机序)
static void bin_save(const slipperworks_game_t *g, slipperworks_save_t *sv) {
    sv->bin_n = g->n;
    for (uint8_t i = 0; i < g->n; i++) {
        float x = g->pcs[i].x;
        float y = g->pcs[i].y;
        if (x < 0) x = 0;
        if (x > 255) x = 255;
        if (y < SW_SAVE_Y_MIN) y = SW_SAVE_Y_MIN;
        if (y > SW_SAVE_Y_MIN + 255) y = SW_SAVE_Y_MIN + 255;  // 桶底 296 存成 255,不然贴底的鞋全被钳半空
        sv->bin_x[i] = (uint8_t)(x + 0.5f);
        sv->bin_y[i] = (uint8_t)(y - SW_SAVE_Y_MIN + 0.5f);
        sv->bin_tier[i] = g->pcs[i].tier;
    }
    sv->queue_n = g->queue_n;
    for (uint8_t i = 0; i < g->queue_n; i++) sv->queue[i] = g->queue[i];
    sv->hammers = g->hammers;
    sv->cur = g->cur;
    sv->nxt = g->nxt;
    sv->rng = g->rng;
}

// 物理结算到稳态:该落的落地、贴着的同级合完
static void settle(slipperworks_game_t *g, slipperworks_save_t *sv) {
    sw_tick_out_t ev;
    memset(&ev, 0, sizeof(ev));
    for (int k = 0; k < 600; k++) {  // 最多快进 2.4 秒物理,足够任何高度落地
        substep(g);
        if (k % 8 == 7) merge_round(g, sv, &ev);
    }
    for (int m = 0; m < 8; m++) {
        if (!merge_round(g, sv, &ev)) break;  // 收尾:把贴着没合完的清干净
    }
    g->jam_ms = 0;
    g->calm_ms = 0;
}

void slipperworks_export(slipperworks_game_t *g, slipperworks_save_t *sv) {
    // 落定结算(就在正玩的现场上,离开页面时反正是暂停的):
    // 恢复时原位冻结就是一堆积稳态,不重落、不悬空、一动也不动。
    settle(g, sv);
    bin_save(g, sv);
}

// 原样续存:在副本里结算好再拍,正在玩的现场一根手指都不动。
// 这样存档里永远没有半空的鞋 —— 恢复时全冻结也不会悬空。
void slipperworks_snapshot(slipperworks_game_t *g, slipperworks_save_t *sv) {
    static slipperworks_game_t sc;
    static slipperworks_save_t sink;  // 副本结算的合成账给它接住:正玩的现场自己会记,别重复入账
    sc = *g;
    memset(&sink, 0, sizeof(sink));
    settle(&sc, &sink);
    bin_save(&sc, sv);
}

bool slipperworks_restore(slipperworks_game_t *g, const slipperworks_save_t *sv) {
    // 有桶里的鞋、有排队的鞋、或有锤子,都算有现场
    if (sv->bin_n == 0 && sv->queue_n == 0 && sv->hammers == 0) return false;
    memset(g, 0, sizeof(*g));
    g->rng = sv->rng ? sv->rng : 0x9E3779B9u;
    g->heat = kit_heat_from(sv->heat);
    g->basket_lv = sv->basket_lv < 1 ? 1 : sv->basket_lv > SW_BASKET_MAX ? SW_BASKET_MAX : sv->basket_lv;
    g->cur = sv->cur;
    g->nxt = sv->nxt;
    g->hammers = sv->hammers > SW_HAMMER_MAX ? SW_HAMMER_MAX : sv->hammers;
    g->queue_n = sv->queue_n > SW_QUEUE_MAX ? SW_QUEUE_MAX : sv->queue_n;
    for (uint8_t i = 0; i < g->queue_n; i++) g->queue[i] = sv->queue[i] % SW_TIERS;
    g->n = sv->bin_n;
    for (uint8_t i = 0; i < g->n; i++) {
        sw_piece_t *p = &g->pcs[i];
        p->tier = sv->bin_tier[i] % SW_TIERS;
        p->r = SW_R[p->tier];
        p->x = p->px = (float)sv->bin_x[i];
        p->y = p->py = (float)sv->bin_y[i] + SW_SAVE_Y_MIN;
        // 原位冻结:睡着,不重新落下、不引发新连锁(碰到动静才醒)
    }
    // 保险丝:老档或异常档可能带悬空的鞋 —— 先把整堆唤醒结算一遍(悬空的落回堆里、
    // 贴着的合完),再全部冻住。新档本来就是稳态,这一遍等于没动。绝不带病上岗,
    // 也绝不在游戏里叫醒:以前"叫醒没支撑的"会醒一只惊一片,整堆重新落一遍。
    for (uint8_t i = 0; i < g->n; i++) {
        g->pcs[i].awake = 1;
        g->pcs[i].still = 0;
    }
    settle(g, (slipperworks_save_t *)sv);  // 结算出的合成账本来就该记进档,去 const 是安全的
    for (uint8_t i = 0; i < g->n; i++) {
        sw_piece_t *p = &g->pcs[i];
        p->awake = 0;
        p->px = p->x;
        p->py = p->y;
        p->still = 0;
        p->born = 0;
    }
    g->hanger_x = 120;
    g->calm_ms = 0;
    clamp_hanger(g);
    return true;
}

// 各升级等级的卖价百分比:Lv0 ×1.0 / Lv1 ×1.3 / Lv2 ×1.6 / Lv3 ×2.0
static const uint16_t SW_UP_PCT[SW_UP_MAX + 1] = {100, 130, 160, 200};

uint8_t slipperworks_up_of(const slipperworks_save_t *sv, uint8_t tier) {
    if (tier < SW_COLLECT_TIER) return 0;
    return sv->up[tier - SW_COLLECT_TIER];
}

uint16_t slipperworks_upgrade_cost(const slipperworks_save_t *sv, uint8_t tier) {
    if (tier < SW_COLLECT_TIER || tier >= SW_TIERS) return 0;
    uint32_t c = (uint32_t)slipperworks_sell_price[tier] * (slipperworks_up_of(sv, tier) + 1u);
    return (uint16_t)(c > 0xFFFFu ? 0xFFFFu : c);
}

bool slipperworks_upgrade(slipperworks_save_t *sv, uint8_t tier) {
    if (slipperworks_up_of(sv, tier) >= SW_UP_MAX) return false;
    uint16_t cost = slipperworks_upgrade_cost(sv, tier);
    if (slipperworks_mat_of(sv) < cost) return false;
    slipperworks_mat_set(sv, slipperworks_mat_of(sv) - cost);
    sv->up[tier - SW_COLLECT_TIER]++;
    return true;
}

uint32_t slipperworks_price_of(const slipperworks_save_t *sv, uint8_t tier) {
    if (tier >= SW_TIERS) return 0;
    uint8_t up = slipperworks_up_of(sv, tier);
    return (uint32_t)slipperworks_sell_price[tier] * SW_UP_PCT[up] / 100u * kit_heat_pct(kit_heat_from(sv->heat), 130, 80) / 100u;
}

uint32_t slipperworks_mat_of(const slipperworks_save_t *sv) {
    return (uint32_t)sv->materials | ((uint32_t)sv->mat_hi << 16);
}

void slipperworks_mat_set(slipperworks_save_t *sv, uint32_t v) {
    sv->materials = (uint16_t)(v & 0xFFFFu);
    sv->mat_hi = (uint16_t)(v >> 16);
}

static void add_materials(slipperworks_save_t *sv, uint32_t amt) {
    uint32_t m = slipperworks_mat_of(sv) + amt;
    slipperworks_mat_set(sv, m > SW_MAT_MAX ? SW_MAT_MAX : m);
}

uint16_t slipperworks_sell(slipperworks_save_t *sv, uint8_t tier, uint16_t count) {
    if (tier >= SW_TIERS || sv->bag[tier] == 0) return 0;
    uint16_t n = sv->bag[tier];
    if (count && count < n) n = count;
    sv->bag[tier] = (uint16_t)(sv->bag[tier] - n);
    uint32_t cash = slipperworks_price_of(sv, tier) * n;
    add_materials(sv, cash);
    sv->sales += cash;
    return n;
}

// 一个物理子步:重力、鞋鞋碰撞、墙和地板。
static void substep(slipperworks_game_t *g) {
    const float dt = SW_STEP;
    for (uint8_t i = 0; i < g->n; i++) {
        sw_piece_t *p = &g->pcs[i];
        if (!p->awake) {
            p->px = p->x;  // 睡着:定格(速度为零)
            p->py = p->y;
            continue;
        }
        float vx = p->x - p->px;
        float vy = p->y - p->py;
        p->px = p->x;
        p->py = p->y;
        p->x += vx * SW_AIR;
        p->y += vy * SW_AIR + SW_GRAV * dt * dt;
    }
    for (uint8_t i = 0; i < g->n; i++) {
        for (uint8_t j = (uint8_t)(i + 1); j < g->n; j++) {
            sw_piece_t *a = &g->pcs[i];
            sw_piece_t *b = &g->pcs[j];
            if (!a->awake && !b->awake) continue;  // 两个都睡死:形状不变,不用管
            float dx = b->x - a->x;
            float dy = b->y - a->y;
            float rr = a->r + b->r;
            float d2 = dx * dx + dy * dy;
            if (d2 >= rr * rr || d2 < 0.0001f) continue;
            a->awake = 1;
            a->contact = 1;
            b->awake = 1;
            b->contact = 1;
            float d = sqrtf(d2);
            float nx = dx / d;
            float ny = dy / d;
            float ov = rr - d;
            float m1 = a->r * a->r;
            float m2 = b->r * b->r;
            float tot = m1 + m2;
            float push = ov * SW_PUSH;
            a->x -= nx * push * (m2 / tot);
            a->y -= ny * push * (m2 / tot);
            b->x += nx * push * (m1 / tot);
            b->y += ny * push * (m1 / tot);
            // 法向相对速度 < 0(在靠近)才给冲量;verlet 里改 prev 就是改速度
            float rvx = (b->x - b->px) - (a->x - a->px);
            float rvy = (b->y - b->py) - (a->y - a->py);
            float vn = rvx * nx + rvy * ny;
            if (vn < 0) {
                float jimp = -(1.0f + SW_REST) * vn / (1.0f / m1 + 1.0f / m2);
                a->px += nx * jimp / m1;
                a->py += ny * jimp / m1;
                b->px -= nx * jimp / m2;
                b->py -= ny * jimp / m2;
            }
        }
    }
    const float x0 = slipperworks_bin_x0(g);
    const float x1 = x0 + slipperworks_bin_inner(g);
    for (uint8_t i = 0; i < g->n; i++) {
        sw_piece_t *p = &g->pcs[i];
        if (!p->awake) continue;
        if (p->x - p->r < x0) {
            p->x = x0 + p->r;
            p->px = p->x + (p->x - p->px) * SW_WALL_REST;
            p->contact = 1;
        } else if (p->x + p->r > x1) {
            p->x = x1 - p->r;
            p->px = p->x + (p->x - p->px) * SW_WALL_REST;
            p->contact = 1;
        }
        if (p->y - p->r < SW_CEIL_Y) {
            p->y = SW_CEIL_Y + p->r;
            p->py = p->y + (p->y - p->py) * SW_WALL_REST;
            p->contact = 1;
        }
        if (p->y + p->r > SW_FLOOR_Y) {
            p->y = SW_FLOOR_Y - p->r;
            float vy = p->y - p->py;
            p->py = p->y + vy * SW_REST;
            float vx = p->x - p->px;
            p->px = p->x - vx * 0.98f;  // 地板摩擦
            p->contact = 1;
        }
    }

    // 睡眠判定:有接触 + 连续 8 步(32ms)几乎不动才睡;半空中没有接触,永远不睡
    for (uint8_t i = 0; i < g->n; i++) {
        sw_piece_t *p = &g->pcs[i];
        if (!p->awake) continue;
        float vx = p->x - p->px;
        float vy = p->y - p->py;
        if (p->contact && vx * vx + vy * vy < 0.0004f) {
            if (++p->still >= 8) {
                p->awake = 0;
                p->px = p->x;
                p->py = p->y;
            }
        } else {
            p->still = 0;
        }
    }
}

// 同级相碰合成。returns 合成次数(这一轮扫描)。
static uint8_t merge_round(slipperworks_game_t *g, slipperworks_save_t *sv, sw_tick_out_t *out) {
    uint8_t did = 0;
    for (uint8_t i = 0; i < g->n; i++) {
        for (uint8_t j = (uint8_t)(i + 1); j < g->n; j++) {
            sw_piece_t *a = &g->pcs[i];
            sw_piece_t *b = &g->pcs[j];
            if (a->tier != b->tier) continue;
            if (a->tier < SW_TIERS - 1 && a->tier + 1 > slipperworks_basket_cap[g->basket_lv - 1])
                continue;  // 篮子等级不够,合不上去(双回旋大合体不受限)
            float dx = b->x - a->x;
            float dy = b->y - a->y;
            float rr = a->r + b->r + SW_MERGE_SLOP;
            if (dx * dx + dy * dy > rr * rr) continue;

            if (a->tier == SW_TIERS - 1) {
                // 双回旋拖鞋大合体:奖金 1000,雨里的大鞋还能继续合,才是大头
                uint32_t cash = 1000u;
                add_materials(sv, cash);
                sv->sales += cash;
                out->spin_cash = (uint16_t)(cash > 0xFFFFu ? 0xFFFFu : cash);
                add_fly(g, a);
                add_fly(g, b);
                g->pcs[j] = g->pcs[g->n - 1];  // j > i:先删 j,再删 i
                g->n--;
                g->pcs[i] = g->pcs[g->n - 1];
                g->n--;
                wake_all(g);
                g->combo = 0;
                g->combo_ms = 0;
                g->rain_left = (uint8_t)(SW_RAIN_MIN + rnd(g) % (SW_RAIN_MAX_N + 1u));
                g->rain_ms = 600;  // 先让合体特效亮一下
                out->spin = true;
                out->merges++;
                return 1;  // 这轮就干这一件大事
            }

            // 中点合成上一级,速度对半
            sw_piece_t m = *a;
            m.x = (a->x + b->x) * 0.5f;
            m.y = (a->y + b->y) * 0.5f;
            m.px = m.x - ((a->x - a->px) + (b->x - b->px)) * 0.25f;
            m.py = m.y - ((a->y - a->py) + (b->y - b->py)) * 0.25f;
            m.tier = (uint8_t)(a->tier + 1);
            m.r = SW_R[m.tier];
            m.born = SW_BORN_MS;
            m.awake = 1;
            g->pcs[i] = m;
            g->pcs[j] = g->pcs[--g->n];

            sv->merged++;
            wake_all(g);  // 被合成的两只没了,压在上面的要重新落
            if (m.tier > sv->best_tier) sv->best_tier = m.tier;
            g->combo = g->combo_ms ? (uint8_t)(g->combo + 1) : 1;
            g->combo_ms = SW_COMBO_MS;
            out->merges++;
            out->combo = g->combo;
            did = 1;
        }
    }
    return did;
}

void slipperworks_tick(slipperworks_game_t *g, slipperworks_save_t *sv, uint32_t dt, sw_tick_out_t *out) {
    memset(out, 0, sizeof(*out));
    if (g->drop_cd) g->drop_cd = (uint16_t)(g->drop_cd > dt ? g->drop_cd - dt : 0);
    if (g->undo_ms) g->undo_ms = (uint16_t)(g->undo_ms > dt ? g->undo_ms - dt : 0);
    if (g->combo_ms) {
        g->combo_ms = (uint16_t)(g->combo_ms > dt ? g->combo_ms - dt : 0);
        if (!g->combo_ms) g->combo = 0;
    }
    g->anim_ms = (uint16_t)(g->anim_ms + dt);
    for (uint8_t i = 0; i < g->n; i++) {
        if (g->pcs[i].born) g->pcs[i].born = (uint16_t)(g->pcs[i].born > dt ? g->pcs[i].born - dt : 0);
    }
    for (uint8_t i = 0; i < SW_FLY_N; i++) {
        if (g->fly[i].t < SW_FLY_MS) g->fly[i].t = (uint16_t)(g->fly[i].t + dt);
    }

    // 本帧接触标记清零:后面的物理碰撞会重新标记,
    // 压线判定和睡眠判定读到的都是"这一帧有没有碰东西"
    for (uint8_t i = 0; i < g->n; i++) g->pcs[i].contact = 0;
    g->acc += dt;
    uint32_t steps = 0;
    while (g->acc >= (uint32_t)SW_STEP && steps < SW_MAX_SUBSTEPS) {
        substep(g);
        g->acc -= (uint32_t)SW_STEP;
        steps++;
    }
    if (steps == SW_MAX_SUBSTEPS) g->acc = 0;  // 卡太久:丢帧别堆雪球

    for (uint8_t round = 0; round < 8; round++) {
        if (!merge_round(g, sv, out)) break;
    }

    // 拖鞋雨:每隔一会儿从天上落一只(小布拖~库巴拖),桶满就先攒着
    if (g->rain_left) {
        if (g->rain_ms > dt) {
            g->rain_ms = (uint16_t)(g->rain_ms - dt);
        } else if (g->n < SW_MAX_PIECES) {
            uint8_t tier = (uint8_t)(2 + rnd(g) % 4u);  // 雨里落的是小布拖~爸爸拖:质量小,合不回回旋,防无限循环
            const float rx0 = slipperworks_bin_x0(g);
            const float rinner = slipperworks_bin_inner(g);
            float rr = SW_R[tier];
            float x = rx0 + rr + (float)(rnd(g) % (uint32_t)(rinner - 2 * rr + 1));
            spawn_piece(g, tier, x, 50);
            g->rain_left--;
            g->rain_ms = (uint16_t)(120 + rnd(g) % 150u);
        }
    }

    // 下雨和大锤震感期间不判翻桶:一个是奖励,一个是宽限
    if (!g->rain_left && !g->calm_ms) {
    bool above = false;
    for (uint8_t i = 0; i < g->n; i++) {
        const sw_piece_t *p = &g->pcs[i];
        // 在线上方且碰着东西(别的鞋/地面/桶壁)就算压线:弹跳中的也算,
        // 空中路过的(还没碰任何东西)不算。睡着的鞋也算 —— 它的接触标记
        // 入睡时被清了零,不认账的话整堆安稳的塔就永远不算压线
        if (p->y - p->r < SW_RIM_Y && (p->contact || !p->awake))
            above = true;
    }
    if (above) {
        uint16_t prev = g->jam_ms;
        g->jam_ms = (uint16_t)(g->jam_ms + dt > 0xFFFFu ? 0xFFFFu : g->jam_ms + dt);
        if (prev < SW_WARN_MS && g->jam_ms >= SW_WARN_MS) out->just_warn = true;
        if (prev < SW_FAIL_MS && g->jam_ms >= SW_FAIL_MS) out->fail = true;
    } else if (g->jam_ms) {
        // 泄漏制:堆在线上颤来颤去不清零,离开多久就消多久(压线才计数,离开就停)
        g->jam_ms = (uint16_t)(g->jam_ms > dt ? g->jam_ms - dt : 0);
    }
    }

    if (g->calm_ms) g->calm_ms = (uint16_t)(g->calm_ms > dt ? g->calm_ms - dt : 0);
}
