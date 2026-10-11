// src/slipperworks_art.h —— 原创像素画:十级拖鞋链 + 原料线卷 + 大锤 + 合成闪光。
// 全部字符画,39 色调色板(k 勾线,'.' 透明);拖鞋一律鞋头朝左的侧视图。
#pragma once

#include <stdint.h>

#include "kit_art.h"

// 拖鞋链:下标 = 档位 0..9(鞋垫→鞋坯→小布拖→大黄拖→库巴拖→爸爸拖→爷爷拖→奶奶拖→金拖鞋→回旋拖鞋)。
// 越界回鞋垫(和 SPR_QUESTION 一个思路)。
const spr_art_t *slipperworks_art_tier(uint8_t tier);

extern const spr_art_t slipperworks_art_spool;   // 原料(线卷) 12x10
extern const spr_art_t slipperworks_art_hammer;  // 大锤 14x11
extern const spr_art_t slipperworks_art_basket;  // 拖鞋篮 12x8
extern const spr_art_t slipperworks_art_fx;      // 合成闪光 7x7
