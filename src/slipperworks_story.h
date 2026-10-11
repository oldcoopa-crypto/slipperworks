// src/slipperworks_story.h —— 剧情剧本(台词表)。播放器在 slipperworks_ui.c / app.c。
#pragma once

#include <stdint.h>
#include "kit_lines.h"
#include "kit_sfx.h"

#define SW_W_GRANNY 0  // 奶奶
#define SW_W_KUBA   1  // 库巴
#define SW_W_NONE   2  // 旁白

typedef struct sw_story_step {
    uint8_t who;              // 说话人
    const char *text;         // 台词(打字机显示)
    const kit_line_t *voice;  // 配音(可空 = 纯文本)
} sw_story_step_t;

typedef struct {
    const sw_story_step_t *steps;
    uint8_t n;
} sw_story_t;

typedef enum {
    SW_ST_OPEN,    // 开场:练成了躲拖鞋,要开拖鞋工坊
    SW_ST_T1,      // 教程 1:丢鞋
    SW_ST_T2,      // 教程 2:合成
    SW_ST_T3,      // 教程 3:收鞋
    SW_ST_T4,      // 教程 4:菜单
    SW_ST_PRAISE,  // 里程碑夸奖
    SW_ST_SHOP,    // 细节:鞋材铺
    SW_ST_SELL,    // 细节:背包卖鞋
    SW_ST_JAM,     // 细节:警戒线大锤救援
    SW_ST_GRAT,    // 全里程碑达成:奶奶送奶奶拖
    SW_ST_N
} sw_story_id_t;

const sw_story_t *slipperworks_story(uint8_t id);
