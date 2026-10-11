// src/slipperworks_save.c —— 见 slipperworks_save.h。
#include "slipperworks_save.h"

#include <string.h>

void slipperworks_save_reset(slipperworks_save_t *s) {
    memset(s, 0, sizeof(*s));
    s->magic = SLIPPERWORKS_MAGIC;
    s->ver = SLIPPERWORKS_SAVE_VER;
    s->heat = KIT_HEAT_MID;
    s->basket_lv = 1;
}

bool slipperworks_save_valid(const void *blob) {
    const slipperworks_save_t *s = blob;
    if (s->magic != SLIPPERWORKS_MAGIC || s->ver != SLIPPERWORKS_SAVE_VER) return false;
    if (s->heat > KIT_HEAT_HOT) return false;  // 原料无上限校验(高低位拼出来的,任何值都合法)
    if (s->best_tier >= SW_TIERS) return false;
    if (s->bin_n > SW_MAX_PIECES || s->queue_n > SW_QUEUE_MAX) return false;
    if (s->cur >= SW_TIERS || s->nxt >= SW_TIERS) return false;
    if (s->basket_lv < 1 || s->basket_lv > SW_BASKET_MAX) return false;
    for (uint8_t i = 0; i < SW_TIERS - SW_COLLECT_TIER; i++) {
        if (s->up[i] > SW_UP_MAX) return false;
    }
    // mile_sticky 任意值都合法
    return true;
}

// 20 只桶时代的布局:bin 数组在中段,排队/随机数/账本都排在它们后面。
// 桶扩到 26 只时数组挪去了末尾,老档必须按这份布局解包再逐字段搬,
// 整块塞进新结构体会把排队/原料高位全部读成垃圾(余额凭空多出几百万就是它)。
typedef struct {
    uint32_t magic;
    uint8_t ver, heat, basket_lv;
    uint16_t materials;
    uint32_t sales;
    uint16_t merged;
    uint16_t bag[SW_TIERS];
    uint8_t best_tier, flags;
    uint8_t up[SW_TIERS - SW_COLLECT_TIER];
    uint8_t bin_n, cur, nxt, hammers, queue_n;
    uint8_t bin_x[20], bin_y[20], bin_tier[20];
    uint8_t queue[SW_QUEUE_MAX];
    uint32_t rng;
    uint8_t mile_sticky;
    uint16_t mat_hi;
    uint8_t tips_done, mile_hi;
} sw_save_bin20_t;  // 必须 = SLIPPERWORKS_SAVE_BIN20_SIZE(160),init 前用 szchk 脚本校验过

// 老布局(bin 数组在中段)的校验:校验的字段全在 bin 数组之前,偏移和新布局一致
bool slipperworks_save_valid_bin20(const void *blob) {
    const slipperworks_save_t *s = blob;
    if (s->magic != SLIPPERWORKS_MAGIC || s->ver < 3 || s->ver > 4) return false;
    if (s->heat > KIT_HEAT_HOT) return false;
    if (s->best_tier >= SW_TIERS) return false;
    if (s->bin_n > 20 || s->queue_n > SW_QUEUE_MAX) return false;
    if (s->cur >= SW_TIERS || s->nxt >= SW_TIERS) return false;
    if (s->basket_lv < 1 || s->basket_lv > SW_BASKET_MAX) return false;
    for (uint8_t i = 0; i < SW_TIERS - SW_COLLECT_TIER; i++) {
        if (s->up[i] > SW_UP_MAX) return false;
    }
    return true;
}

void slipperworks_save_from_bin20(slipperworks_save_t *dst, const void *blob) {
    const sw_save_bin20_t *o = blob;
    memset(dst, 0, sizeof(*dst));
    dst->magic = o->magic;
    dst->ver = SLIPPERWORKS_SAVE_VER;  // 搬进新布局,就按新版本号落盘,不然下次开机又被当坏档
    dst->heat = o->heat;
    dst->basket_lv = o->basket_lv;
    dst->materials = o->materials;
    dst->sales = o->sales;
    dst->merged = o->merged;
    memcpy(dst->bag, o->bag, sizeof(o->bag));
    dst->best_tier = o->best_tier;
    dst->flags = o->flags;
    memcpy(dst->up, o->up, sizeof(o->up));
    dst->cur = o->cur;
    dst->nxt = o->nxt;
    dst->hammers = o->hammers;
    dst->queue_n = o->queue_n;
    memcpy(dst->queue, o->queue, sizeof(o->queue));
    dst->rng = o->rng;
    dst->mile_sticky = o->mile_sticky;
    dst->mat_hi = o->mat_hi;
    dst->tips_done = o->tips_done;
    dst->mile_hi = o->mile_hi;
    uint8_t n = o->bin_n > 20 ? 20 : o->bin_n;
    dst->bin_n = n;
    memcpy(dst->bin_x, o->bin_x, (size_t)n);
    memcpy(dst->bin_y, o->bin_y, (size_t)n);
    memcpy(dst->bin_tier, o->bin_tier, (size_t)n);
}

uint8_t slipperworks_milestones_of(const slipperworks_save_t *s) {
    uint8_t m = 0;
    if (s->sales >= 1) m |= 1u << 0;                        // 开张大吉:卖出第一只
    if (s->best_tier >= 5) m |= 1u << 1;                    // 合出爸爸拖
    if ((s->flags & 0x01) || s->bag[7] >= 1) m |= 1u << 2;  // 珍藏:奶奶拖收到过就算(翻桶清不掉)
    if (s->best_tier >= 8) m |= 1u << 3;                    // 合出金拖鞋
    if (s->best_tier >= 9) m |= 1u << 4;                    // 合出回旋拖鞋
    if (s->basket_lv >= SW_BASKET_MAX) m |= 1u << 5;        // 篮子升满
    for (uint8_t i = 0; i < SW_TIERS - SW_COLLECT_TIER; i++) {
        if (s->up[i] >= SW_UP_MAX) { m |= 1u << 6; break; }  // 鞋种升满
    }
    if (s->flags & 0x10) m |= 1u << 7;                      // 拖鞋雨(flags 位4 事件记账)
    return m;
}
