// src/slipperworks_save.h —— 工坊的存档(saves 分区的 "slipperworks"/"save")和成就账本。
// 纯数据,不碰 NVS;读写由 slipperworks_app.c 用 kit_store 做。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "slipperworks_logic.h"  // SW_TIERS

#define SLIPPERWORKS_NS        "slipperworks"       // = coopa.toml 的 id
#define SLIPPERWORKS_KEY       "save"
#define SLIPPERWORKS_MAGIC     0x504C4953u          // "SLIP"(小端)
#define SLIPPERWORKS_SAVE_VER  5
// 布局改动史:ver4 = 20 只桶(bin 数组在中段);ver5 = 26 只桶(bin 数组在末尾)。
// 扩容头两天 ver4 的档曾被错位读坏(排队/原料高位成垃圾),这种档读不进来就按新档重开。
// 存档布局只追加不挪位,ver 不用动。桶容量从 20 扩到 26 时把 bin 数组挪到了最末尾:
// 老档前段(到 mile_hi 为止)偏移不变,bin 段读不到就清空现场(账目/星星都在)。
// init 按 新(176) → 160(20 只桶时代) → 155(再老,无账本/原料高位/提示位) 依次试读。
#define SLIPPERWORKS_SAVE_BIN20_SIZE 160u
#define SLIPPERWORKS_SAVE_V3_SIZE    155u

struct slipperworks_save {
    uint32_t magic;
    uint8_t ver;
    uint8_t heat;        // kit_heat_t(0 = 中辣)
    uint8_t basket_lv;   // 拖鞋篮等级 1..5
    uint16_t materials;  // 现有原料低 16 位(≤ SW_MAT_MAX)
    uint32_t sales;      // 累计卖鞋收入
    uint16_t merged;     // 累计合成次数
    uint16_t bag[SW_TIERS];
    uint8_t best_tier;   // 合成过的最高档(0 = 还没合成过东西)
    uint8_t flags;       // 位0 = 奶奶拖曾收进过背包 位4 = 见过拖鞋雨 位5 = 感恩剧情已演
    uint8_t up[SW_TIERS - SW_COLLECT_TIER];  // 各档鞋种升级等级(0..3)
    // ---- 工坊现场(离开时存,回来接着干) ----
    // 注意:bin 数组必须留到最后 —— 扩桶容量只往后追加,老档前段偏移一个不动
    uint8_t bin_n;       // 桶里的鞋数(0 = 没有存着的现场)
    uint8_t cur, nxt;    // 手里 / 下一只
    uint8_t hammers;     // 大锤存量
    uint8_t queue_n;     // 排队的购买鞋数
    uint8_t queue[SW_QUEUE_MAX];
    uint32_t rng;        // 出材随机序列(接着上次继续)
    uint8_t mile_sticky; // 里程碑账本:达成过的永久锁定(新的工坊/翻桶都清不掉)
    uint16_t mat_hi;     // 原料高位:实际原料 = materials | (mat_hi << 16),上限一亿
    uint8_t tips_done;   // 位0 = 铺子提示已看 位1 = 卖鞋提示已看 位2 = 大锤救援已演(追加位,旧档为 0)
    uint8_t mile_hi;     // 兼容位,恒 0(曾短暂当过账本高位;留着免得动存档布局)
    uint8_t bin_x[SW_MAX_PIECES];   // 桶里每只鞋的位置和档位
    uint8_t bin_y[SW_MAX_PIECES];
    uint8_t bin_tier[SW_MAX_PIECES];
};

void slipperworks_save_reset(slipperworks_save_t *s);
bool slipperworks_save_valid(const void *blob);        // 新布局(ver5)专用,严格校验
bool slipperworks_save_valid_bin20(const void *blob);  // 老布局(ver3/4,bin 在中段)校验
// 20 只桶时代的老布局(160B,bin 数组在中段)解包:逐字段搬到现布局,现场照留
void slipperworks_save_from_bin20(slipperworks_save_t *dst, const void *blob);
// 里程碑位(顺序对应 coopa.toml):
// 0 卖出第一只 | 1 合出爸爸拖 | 2 背包存过奶奶拖 | 3 合出金拖鞋
// 4 合出回旋拖鞋 | 5 篮子升满 | 6 鞋种升满 | 7 拖鞋雨(flags 位4 事件记账)
uint8_t slipperworks_milestones_of(const slipperworks_save_t *s);
