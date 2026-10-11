// src/slipperworks_logic.h —— 《库巴的拖鞋工坊》玩法(纯 C,不碰 LVGL / NVS,主机可测)。
// 物理合成桶:▲▼ 挪吊臂,● 丢鞋材,同级相碰合成上一级;长按▼ 收鞋入背包,长按● 开鞋材铺。
// 经济:合成的大鞋收进背包,背包里卖出换原料,原料在铺子里买高级鞋材。没有游戏结束,桶满是软锁。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "kit_heat.h"

// 存档类型定义在 slipperworks_save.h(那边反过来 include 本头文件拿 SW_TIERS)。
typedef struct slipperworks_save slipperworks_save_t;

#define SW_TIERS 10             // 鞋垫..回旋拖鞋
#define SW_MAX_PIECES 26        // 桶里最多几只(26 = 物理和界面的甜点;再往上快照结算开始拖帧)        // 桶里最多几只(26 = 物理和界面的甜点;再往上快照结算开始拖帧)
#define SW_COLLECT_TIER 2       // 小布拖(下标 2)起才算成品,能收进背包
#define SW_HAMMER_MAX 9
#define SW_QUEUE_MAX 32         // 买来的鞋在后台排队,先买先丢;前台只显示队首那只
#define SW_MAT_MAX 99999999u    // 原料封顶:一个亿

// 逻辑坐标 = 屏幕像素(240x320 竖屏)
#define SW_RIM_Y 94             // 警戒线
#define SW_FLOOR_Y 296          // 桶底
#define SW_SAVE_Y_MIN 40        // 存档 y 的基准:鞋只活动在 40..295,减 40 后正好塞进 u8(255)
#define SW_CEIL_Y 44            // 天花板(HUD 下面)
#define SW_DROP_Y 66            // 吊挂圆心
#define SW_HANGER_STEP 12       // ▲▼ 一次挪几像素
#define SW_DROP_CD 350          // 丢下冷却 ms
#define SW_COMBO_MS 400         // 连锁窗口
#define SW_WARN_MS 300          // 压线多久开始急促闪灯(也触发大锤救援教学)
#define SW_FAIL_MS 1000         // 警戒线倒计时:压线 1 秒 = 翻桶(1 秒内根本摞不上去,离开就暂停不数)
#define SW_RAIN_MIN 20          // 拖鞋雨:最少下几只
#define SW_RAIN_MAX_N 10        // 拖鞋雨:在最少之上随机加几只(20~30)
#define SW_BORN_MS 220          // 合成弹跳时长(UI 按它算缩放)
#define SW_UNDO_MS 700          // 丢下后这么久内开铺子,把鞋收回来(长按●先发 PRESS,窗口要盖住整个长按)

// 卖出价(原料):×2.5 递增;下标 = 档位
extern const uint16_t slipperworks_sell_price[SW_TIERS];
#define SW_BASKET_MAX 5                                   // 拖鞋篮最高 5 级
extern const uint8_t slipperworks_basket_cap[SW_BASKET_MAX];   // 各级可合成的最高档(下标 = 级-1)
extern const uint8_t slipperworks_basket_w[SW_BASKET_MAX];     // 各级桶宽百分比(满级 = 100)
extern const uint16_t slipperworks_basket_price[SW_BASKET_MAX - 1];  // 升级价:1→2 / 2→3 / 3→4 / 4→5

#define SW_SHOP_N 5                                       // 0..2 鞋材,3 大锤,4 篮子升级
extern const uint8_t slipperworks_shop_tier[SW_SHOP_N];   // 2 3 4,3 号位是大锤,4 号位是篮子
extern const uint16_t slipperworks_shop_price[SW_SHOP_N]; // 6 14 35 10,4 号位的价按级查表
#define SW_SHOP_BAG 5                                     // 铺子行:去背包
#define SW_SHOP_CLOSE 6                                   // 铺子行:关铺子(长按●在铺子里不做事,免得先误买)

typedef struct {
    float x, y;      // 圆心(逻辑像素)
    float px, py;    // 上一子步位置(verlet)
    float r;         // 物理半径
    uint8_t tier;    // 0..9
    uint16_t born;   // 合成弹跳计时(倒减 ms,0 = 不弹)
    uint8_t awake;   // 睡着的鞋不积分不查碰撞(动静来了会被吵醒)
    uint8_t contact; // 这一步有没有碰到东西(地面/别的鞋/桶壁):睡着的必要条件
    uint8_t still;   // 连续多少步"有接触且几乎不动
} sw_piece_t;

#define SW_FLY_MS 340   // 收鞋飞进背包的动画时长
#define SW_FLY_N 6

typedef struct {
    float x, y;        // 起飞点
    uint16_t t;        // 0..SW_FLY_MS
    uint8_t tier;
} sw_fly_t;

typedef struct {
    uint32_t rng;      // xorshift32,不能为 0
    sw_piece_t pcs[SW_MAX_PIECES];
    uint8_t n;
    uint8_t cur, nxt;  // 吊挂中的档位 / 下一只的档位(晾绳空着时才用)
    uint8_t queue[SW_QUEUE_MAX];  // 买来的鞋按顺序排队,先买先丢
    uint8_t queue_n;   // 排了几只
    uint8_t last_drop;       // 刚丢下那只的档位(撤销用)
    uint8_t undo_from_queue; // 这一丢手里接班的是不是从队列出来的
    uint8_t undo_prev_nxt;   // 丢之前的免费鞋材(撤销时还回去)
    uint16_t hanger_x; // 吊挂圆心 x
    uint16_t drop_cd;  // >0:刚丢下,不能丢(倒减)
    uint16_t undo_ms;  // >0:刚丢的这只还能被长按●收回来(倒减,盖住整个长按)
    uint8_t hammers;   // 大锤存量(本局)
    uint16_t jam_ms;   // 警戒线倒计时:有鞋压线的累计时长,到 SW_FAIL_MS = 翻桶;离开就慢慢消退
    uint16_t calm_ms;  // 大锤后的震感宽限:弹跳落定前不判翻桶
    uint8_t basket_lv; // 拖鞋篮等级 1..5:管桶宽和能合到的最高档
    uint8_t rain_left; // 拖鞋雨还剩几只没落下来(0 = 没在下雨)
    uint16_t rain_ms;  // 距下一只雨鞋落下的倒计时
    uint8_t combo;     // 当前连锁数(0 = 没在连锁)
    uint16_t combo_ms; // 连锁窗口倒计时
    uint16_t anim_ms;  // 动画时钟(装饰用,自由累计)
    uint32_t acc;      // 物理子步累加器
    kit_heat_t heat;   // 进工坊时从存档拷来(一场内不变)
    sw_fly_t fly[SW_FLY_N];
} slipperworks_game_t;

typedef struct {
    uint8_t merges;    // 这一帧合成几次
    uint8_t combo;     // 最后一次合成后的连锁数
    bool spin;         // 双回旋拖鞋大合体(+500 原料,已入账)
    bool just_warn;    // 这一帧刚到警戒警告线(快收鞋!)
    bool fail;         // 这一帧刚翻桶(警戒线超时或倒计时归零,app 换失败页)
    uint16_t spin_cash; // 双回旋合体的奖金(已入账,给提示条用)
} sw_tick_out_t;

typedef enum { SW_BUY_OK = 0, SW_BUY_NO_MAT, SW_BUY_FULL, SW_BUY_QFULL } sw_buy_t;

// 桶内宽按辣度 × 篮子等级:满级中辣 160 / 微辣 176 / 特辣 144,篮子越小越窄。
uint16_t slipperworks_bin_inner(const slipperworks_game_t *g);
// 桶内壁左沿 x(逻辑像素)。
uint16_t slipperworks_bin_x0(const slipperworks_game_t *g);

void slipperworks_start(slipperworks_game_t *g, uint32_t seed, kit_heat_t heat, uint8_t basket_lv);
void slipperworks_hanger(slipperworks_game_t *g, int dir);
// 丢下吊挂中的鞋:SW_EV_DROP;桶满/冷却/桶满员返回 SW_EV_BLOCKED(● 会转成大锤,调用方处理)。
bool slipperworks_drop(slipperworks_game_t *g);
// 撤销刚丢下的那只(长按●开铺子前用):收回来当吊挂。返回 false = 已经不能撤。
bool slipperworks_undrop(slipperworks_game_t *g);
// 过 dt 毫秒:物理、合成、连锁、桶满判定。经济入账写进 sv(merged/best_tier/materials)。
void slipperworks_tick(slipperworks_game_t *g, slipperworks_save_t *sv, uint32_t dt, sw_tick_out_t *out);
// 收鞋:小布拖及以上的全部进背包,返回收了几只。
uint8_t slipperworks_collect(slipperworks_game_t *g, slipperworks_save_t *sv);
// 大锤敲一下(消耗一把):全桶轻轻弹起。没锤了返回 false。
bool slipperworks_hammer(slipperworks_game_t *g);
// 铺子买东西。买鞋材直接换掉吊挂中的那只。
sw_buy_t slipperworks_buy(slipperworks_game_t *g, slipperworks_save_t *sv, uint8_t item);
// 卖背包:count 只(0 = 全卖),按辣度折价,钱和收入入账。返回实际卖出的只数。
uint16_t slipperworks_sell(slipperworks_save_t *sv, uint8_t tier, uint16_t count);
// 工坊现场 <-> 存档:离开工坊时 export,再进来时 restore(桶里的鞋、排队、锤子都在)。
void slipperworks_export(slipperworks_game_t *g, slipperworks_save_t *sv);
void slipperworks_snapshot(slipperworks_game_t *g, slipperworks_save_t *sv);  // 原样快照不结算
bool slipperworks_jam_column(const slipperworks_game_t *g, float x);  // x 这列有没有压线的鞋(锤子瞄准用)
// 没有存着的现场(bin_n == 0)返回 false,调用方走 start() 开新场。
bool slipperworks_restore(slipperworks_game_t *g, const slipperworks_save_t *sv);
// 原料的实际读写(存档里拆成低 16 位 + 高 16 位,上限一亿)
uint32_t slipperworks_mat_of(const slipperworks_save_t *sv);
void slipperworks_mat_set(slipperworks_save_t *sv, uint32_t v);
// 鞋种升级(背包里花钱升级,同档卖价更高):每级卖价 +30%,最高 3 级(×2)
#define SW_UP_MAX 3
// 某档当前升级等级(0..3;鞋垫/鞋坯恒 0)
uint8_t slipperworks_up_of(const slipperworks_save_t *sv, uint8_t tier);
// 升级费用:该档原价 ×(当前级+1)
uint16_t slipperworks_upgrade_cost(const slipperworks_save_t *sv, uint8_t tier);
// 升一级:扣钱提级。钱不够/已满级返回 false。
bool slipperworks_upgrade(slipperworks_save_t *sv, uint8_t tier);
// 一只该档拖鞋的实卖价(升级加成 + 辣度折算后)。
uint32_t slipperworks_price_of(const slipperworks_save_t *sv, uint8_t tier);
