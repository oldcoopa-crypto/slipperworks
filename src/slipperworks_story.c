// src/slipperworks_story.c —— 剧本台词表(文本在 slipperworks_texts.h)。
#include "slipperworks_story.h"

#include "slipperworks_texts.h"
#include "slipperworks_voices.h"

static const sw_story_step_t OPEN_STEPS[] = {
    {SW_W_NONE,   DT_ST_O1, 0},
    {SW_W_GRANNY, DT_ST_O2, &slipperworks_voice_G_O2},
    {SW_W_KUBA,   DT_ST_O3, &slipperworks_voice_K_O3},
    {SW_W_GRANNY, DT_ST_O4, &slipperworks_voice_G_PRAISE},
    {SW_W_KUBA,   DT_ST_O5, &slipperworks_voice_K_OPEN},
    {SW_W_GRANNY, DT_ST_O6, &slipperworks_voice_G_ENOUGH},
    {SW_W_KUBA,   DT_ST_O7, &slipperworks_voice_K_DEAL},
};

static const sw_story_step_t T1_STEPS[] = {{SW_W_GRANNY, DT_ST_T1, &slipperworks_voice_G_T_DROP}};
static const sw_story_step_t T2_STEPS[] = {{SW_W_GRANNY, DT_ST_T2, 0}};
static const sw_story_step_t T3_STEPS[] = {{SW_W_GRANNY, DT_ST_T3, 0}};
static const sw_story_step_t T4_STEPS[] = {{SW_W_GRANNY, DT_ST_T4, 0}};
static const sw_story_step_t PRAISE_STEPS[] = {{SW_W_GRANNY, DT_ST_PRAISE, &slipperworks_voice_G_PRAISE}};
static const sw_story_step_t SHOP_STEPS[] = {{SW_W_GRANNY, DT_ST_SHOP, 0}};
static const sw_story_step_t SELL_STEPS[] = {{SW_W_GRANNY, DT_ST_SELL, 0}};
static const sw_story_step_t JAM_STEPS[] = {{SW_W_GRANNY, DT_ST_JAM, 0}};
static const sw_story_step_t GRAT_STEPS[] = {
    {SW_W_GRANNY, DT_ST_GRAT1, &slipperworks_voice_G_GRAT1},
    {SW_W_GRANNY, DT_ST_GRAT2, &slipperworks_voice_G_GRAT2},
    {SW_W_KUBA,   DT_ST_GRAT3, &slipperworks_voice_K_GRAT},
};

static const sw_story_t STORIES[SW_ST_N] = {
    {OPEN_STEPS, 7}, {T1_STEPS, 1}, {T2_STEPS, 1}, {T3_STEPS, 1},
    {T4_STEPS, 1}, {PRAISE_STEPS, 1}, {SHOP_STEPS, 1}, {SELL_STEPS, 1}, {JAM_STEPS, 1},
    {GRAT_STEPS, 3},
};

const sw_story_t *slipperworks_story(uint8_t id) {
    return id < SW_ST_N ? &STORIES[id] : &STORIES[SW_ST_OPEN];
}
