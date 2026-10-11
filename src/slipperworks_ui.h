// src/slipperworks_ui.h —— 工坊的画面(LVGL,只在持有 LVGL 锁时调用)。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "slipperworks_logic.h"
#include "slipperworks_save.h"

// 标题页:sel 0..3 = 进工坊 / 背包 / 辣度 / 回菜单。anim_ms 传一个自由累计的时钟(英雄拖鞋上下飘)。
void slipperworks_ui_title(uint8_t sel, const slipperworks_save_t *sv, bool rpg_won, uint32_t anim_ms);
void slipperworks_ui_title_frame(uint32_t anim_ms);

// 大锤震屏:短促左右抖动 200ms(在工坊画面的下一帧生效)。
void slipperworks_ui_shake(void);

// 进工坊时建一次画面(桶宽按这场辣度定)。之后每帧 update。
void slipperworks_ui_work(const slipperworks_game_t *g);
void slipperworks_ui_work_update(const slipperworks_game_t *g, const slipperworks_save_t *sv);
void slipperworks_ui_toast(const char *text, uint32_t bg, uint32_t ms);       // 全页面提示条
void slipperworks_ui_toast_frame(uint32_t dt);  // 提示条计时(帧里调)

// 功能菜单浮层(盖在工坊画面上;物理已经停了,顺带当暂停用)。
// sel 0..6:继续做工 / 鞋材铺 / 背包 / 收鞋入背包 / 大锤敲一下 / 回标题 / 回菜单。
#define SW_MENU_N 7
void slipperworks_ui_menu(uint8_t sel);
void slipperworks_ui_menu_update(uint8_t sel);
void slipperworks_ui_menu_hide(void);

// 鞋材铺浮层(盖在工坊画面上;物理已经停了)。
void slipperworks_ui_shop(uint8_t sel, uint8_t queue_n, uint8_t basket_lv);
void slipperworks_ui_shop_update(uint8_t sel, uint8_t queue_n, uint8_t basket_lv);
void slipperworks_ui_shop_hide(void);

// 失败页:翻桶了,桶里和背包里的拖鞋全洒掉。参数是洒掉的数量(展示用)。
void slipperworks_ui_fail(uint16_t lost_bin, uint16_t lost_bag);

// 「新的工坊」确认页:stage 1/2,两遍确认。
void slipperworks_ui_confirm(uint8_t stage);

// 剧情播放器:page = 全页(开场);modal = 盖在工坊画面上(物理已暂停)。
// step 换台词并播配音;tick 推打字机(true = 整句显示完);finish 一下子显示完;hide 收掉。
struct sw_story_step;
void slipperworks_ui_story_page(const struct sw_story_step *st);
void slipperworks_ui_story_modal(const struct sw_story_step *st);
void slipperworks_ui_story_step(const struct sw_story_step *st);
bool slipperworks_ui_story_tick(uint32_t dt);
bool slipperworks_ui_story_done(void);  // 整句是否已经显示完
void slipperworks_ui_story_finish(void);
void slipperworks_ui_story_hide(void);
// 细节教学提示:播一句奶奶语音(配合提示条用)。
void slipperworks_ui_tip_voice(const struct sw_story_step *st);

// 背包页:sel 0..7 = 小布拖..回旋拖鞋的行,最后一格 = 回标题。
#define SW_BAG_ROWS (SW_TIERS - SW_COLLECT_TIER)
#define SW_BAG_SEL_BACK SW_BAG_ROWS
void slipperworks_ui_bag(uint8_t sel, const slipperworks_save_t *sv);
void slipperworks_ui_bag_update(uint8_t sel, const slipperworks_save_t *sv);
