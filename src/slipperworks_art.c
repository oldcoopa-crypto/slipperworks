// src/slipperworks_art.c —— 见 slipperworks_art.h。
// 调色板速查:y黄 o橙 r红 t深红 f粉 p桃红 l粉紫 v淡紫 V紫 G绿 D深绿 m薄荷 B蓝 J深蓝
//           n皮棕 b深棕 e卡其 E黄卡其 c奶油 s肤色 w白 a浅灰 h浅灰 H灰 g青灰 d深灰 x黑 K黑;k 勾线。
#include "slipperworks_art.h"

// T0 鞋垫 10x6 —— 起点:一块小鞋垫。
static const char *const ROWS_T0[] = {
    "...kkkk...",
    "..keeeek..",
    ".keeEEEek.",
    ".keeEEEek.",
    "..keeeek..",
    "...kkkk...",
};

// T1 鞋坯 12x8 —— 没上色的素坯。
static const char *const ROWS_T1[] = {
    "...kkkkkk...",
    "..kaaaaaak..",
    ".kaakkkkaak.",
    ".kaak..kaak.",
    "kaak....kaak",
    "kaak....kaak",
    "kwwwwwwwwwwk",
    ".kkkkkkkkkk.",
};

// T2 小布拖 16x11 —— 猫的小布拖,蓝色布面奶油衬里。
static const char *const ROWS_T2[] = {
    ".....kkkkkk.....",
    "....kBBBBBBk....",
    "...kBccccccBk...",
    "..kBBkkkkkkBBk..",
    ".kBBk......kBBk.",
    ".kBBk......kBBk.",
    "kBBBk......kBBk.",
    "kBBBBk....kBBBk.",
    "kBBBBBBkkkkBBBBk",
    "kwwwwwwwwwwwwwwk",
    ".kkkkkkkkkkkkkk.",
};

// T3 大黄拖 20x13 —— 大黄的爪爪拖,黄面棕掌印。
static const char *const ROWS_T3[] = {
    "......kkkkkk........",
    ".....kyyyyyyk.......",
    "....kyyyyyyyyk......",
    "...kyykkkkkkyyk.....",
    "..kyyyk....kyyyk....",
    "..knyyk....kyyyk....",
    ".kynnyk....kyyyk....",
    ".kynnyk....kyyyyk...",
    "kyyyyk....kyyyyyk...",
    "kyyyyyykkkkyyyyyyk..",
    "kyyyyyyyyyyyyyyyyk..",
    ".kwwwwwwwwwwwwwwk...",
    "..kkkkkkkkkkkkkk....",
};

// T4 库巴拖 24x15 —— 库巴的红拖白底,鞋面两颗金星。
static const char *const ROWS_T4[] = {
    ".........kkkkkk.........",
    "........kyyyyyyk........",
    ".......kyrrrrrryk.......",
    "......krrkkkkkkrrk......",
    ".....krrrk....krrrk.....",
    "....krrrrk....krrrrk....",
    "...kryrrrk....krrrrrk...",
    "...krrrrrk....kryrrrk...",
    "..krrrrrrk....krrrrrrk..",
    "..krrrrrrrk..krrrrrrrk..",
    "..krrrrrrrrkkrrrrrrrrk..",
    "..krrrrrrrrrrrrrrrrrrk..",
    "..kryyrrrrrrrrrryyrrrk..",
    "..kyyyyyyyyyyyyyyyyyyk..",
    ".kwwwwwwwwwwwwwwwwwwwwk.",
    "...kkkkkkkkkkkkkkkkkk...",
};

// T5 爸爸拖 28x17 —— 爸爸的牛皮大拖,深棕面带铜扣。
static const char *const ROWS_T5[] = {
    "..........kkkkkkkk..........",
    ".........kbbbbbbbbk.........",
    "........knnnbbbbnnnk........",
    ".......kbbbkkkkkkbbbk.......",
    "......kbbbbk....kbbbbk......",
    "......kbbbbk....kybbbk......",
    ".....kbbbbbk....kbbbbbk.....",
    ".....kbbbbbk....kbbbbbk.....",
    "....kbbbbbbk....kbbbbbbk....",
    "....kbbbbbbk....kbbbbbbk....",
    "...kbbbbbbbk....kbbbbbbbk...",
    "...kbbbbbbbk...kbbbbbbbbk...",
    "..kbbbbbbbbkkkkbbbbbbbbbk...",
    "..kbbbbbbbbbbbbbbbbbbbbbk...",
    "..knbbbbbbbbbbbbbbbbbbbnk...",
    "..kwwwwwwwwwwwwwwwwwwwwwwk..",
    "...kkkkkkkkkkkkkkkkkkkkkk...",
};

// T6 爷爷拖 32x19 —— 爷爷的灰毡拖,补丁方块。
static const char *const ROWS_T6[] = {
    "............kkkkkkkk............",
    "...........kaaaaaaaak...........",
    "..........kahhhhhhaak...........",
    ".........kaahkkkkhhaak..........",
    "........kaaak....kaaaak.........",
    ".......kaaaak....kaaaak.........",
    ".......kaaak......kaaaak........",
    "......kaaaak......kaaaak........",
    "......kaaak........kaaaak.......",
    ".....kaaaak........kaaaak.......",
    ".....kaaaak........kaaaaak......",
    "....kaaaaak........kaaaaak......",
    "....kaaaaak......kkaaaaaak......",
    "...kaaaaaakkkkkkkaaaaaaaak......",
    "...kaaaaaaaaaaaaaaaaaaaaak......",
    "...kaaaaaaaaaaaaaaaaaaaaak......",
    "...kahhaaaaaahhaaaaaaaaahk......",
    "...kgggggggggggggggggggggk......",
    "....kkkkkkkkkkkkkkkkkkkkkk......",
};

// T7 奶奶拖 38x21 —— 奶奶的绣花拖,粉面红花。
static const char *const ROWS_T7[] = {
    "..............kkkkkkkk................",
    ".............kffffffffk...............",
    "............kffpwwwwpffk..............",
    "...........kffpkkkkkkpffk.............",
    "..........kffpfk....kfpffk............",
    "..........kffpfk....kffpffk...........",
    ".........kffffk......kffffk...........",
    ".........kffrkryk....kffffk...........",
    "........kffryryrk....kfffffk..........",
    "........kffryryrk....kfffffk..........",
    ".......kfffkrkrk......kfffffk.........",
    ".......kffffk..........kfffffk........",
    "......kfffffk..........kfffffpk.......",
    "......kffffk............kffffpk.......",
    ".....kffffffk..........kfffffffpk.....",
    ".....kfffffk..........kfffffffffpk....",
    "....kffffffffkkkkkkkkkfffffffffpk.....",
    "....kfffffffffffffffffffffffffpk......",
    "....kpfffffffffffffffffffffffpk.......",
    "....kwwwwwwwwwwwwwwwwwwwwwwwwwk.......",
    ".....kkkkkkkkkkkkkkkkkkkkkkkkk........",
};

// T8 金拖鞋 44x23 —— 金拖鞋,黄面高光,白星闪。
static const char *const ROWS_T8[] = {
    "................kkkkkkkk....................",
    "...............kyyyyyyyyk...................",
    "..............kyEEyyyyyyok..................",
    ".............kyEyykkkkkyyok.................",
    "............kyEyyk....kyyok.................",
    "............kEyyk......kyyok................",
    "...........kyEyk........kyyok...............",
    "...........kyyk..........kyok...............",
    "..........kyEyk..........kyyok..............",
    "..........kyyk............kyok..............",
    ".........kyEyk............kyyok.............",
    ".........kyyk..............kyok.............",
    "........kyEyyk............kyyyok............",
    "........kyyyyk............kyyyok............",
    ".......kyyyyyk..........kyyyyyok............",
    ".......kyyyyyyk........kyyyyyyyok...........",
    ".......kyyyyyyykkkkkkkkyyyyyyyyok...........",
    ".......kyyyyyyyyyyyyyyyyyyyyyyok............",
    ".......kyyyyyyyyyyyyyyyyyyyyyyok............",
    ".......kyyywyyyyyyyyyywyyyyyyyok............",
    ".......kooooooooooooooooooooook.............",
    ".......kwwwwwwwwwwwwwwwwwwwwwwk.............",
    "........kkkkkkkkkkkkkkkkkkkkkk..............",
};

// T9 回旋拖鞋 44x25 —— 传说武器:奶奶的回旋拖鞋,金底红面带风旋。
static const char *const ROWS_T9[] = {
    "G...........................................",
    "................kkkkkkkk....................",
    "...............krrrrrrrrk...................",
    "..............kryyrrrrrryk..................",
    ".......m.....kryrrkkkkkrryk.................",
    "......GG....kryrrk....krryk.................",
    "......m.....kyrrk......krryk................",
    ".....GG....kryrk........krryk...............",
    ".....m.....krrk..........kryk...............",
    "....GG....kryrk..........krryk..............",
    "....m.....krrk............kryk..............",
    "...GG....kryrk............krryk.............",
    "...m.....krrk..............kryk.............",
    "..GG....kryrrk............krrryk............",
    "........krrrrk............krrryk............",
    ".......krrrrrk..........krrrrryk............",
    ".......krrrrrrk........krrrrrrryk...........",
    ".......krrrrrrrkkkkkkkkrrrrrrrryk...........",
    ".......krrrrrrrrrrrrrrrrrrrrrryk............",
    ".......krrrrrrrrrrrrrrrrrrrrrryk............",
    ".......krrrwrrrrrrrrrrwrrrrrrryk............",
    ".......kEEEEEEEEEEEEEEEEEEEEEEk.............",
    ".......kEEEEEEEEEEEEEEEEEEEEEEk.............",
    "........kkkkkkkkkkkkkkkkkkkkkk..............",
    ".m..........................................",
};

static const spr_art_t ART_TIER[10] = {
    {10, 6, ROWS_T0},   {12, 8, ROWS_T1},   {16, 11, ROWS_T2},  {20, 13, ROWS_T3},
    {24, 16, ROWS_T4},  {28, 17, ROWS_T5},  {32, 19, ROWS_T6},  {38, 21, ROWS_T7},
    {44, 23, ROWS_T8},  {44, 25, ROWS_T9},
};

// 原料:一个线卷(做鞋的线料)。
static const char *const ROWS_SPOOL[] = {
    ".kkkkkkkkkk.",
    ".kbbbbbbbbk.",
    "..keeeeeek..",
    "..kyyyyyyk..",
    "..keeeeeek..",
    "..kyyyyyyk..",
    "..keeeeeek..",
    "..kyyyyyyk..",
    ".kbbbbbbbbk.",
    ".kkkkkkkkkk.",
};

// 拖鞋篮(升级用):编织小篮。
static const char *const ROWS_BASKET[] = {
    "..kkkkkkkk..",
    ".kbbbbbbbbk.",
    "kbebbebbebbk",
    "kbbbbbbbbbbk",
    ".kbebbebbek.",
    ".kbbbbbbbbk.",
    "..kbbbbbbk..",
    "...kkkkkk...",
};

// 大锤:敲一下,把堆敲实下去。
static const char *const ROWS_HAMMER[] = {
    ".kkkkkkkkkk...",
    ".kaaaaaaaHk...",
    ".kaaaaaaaHk...",
    ".kaaaaaaaHk...",
    ".kkkkbkkkkk...",
    "....kbk.......",
    "....kbk.......",
    "....kbk.......",
    "....kbk.......",
    "....kbk.......",
    "....kkk.......",
};

// 合成闪光。
static const char *const ROWS_FX[] = {
    "...y...",
    "...y...",
    ".yywyy.",
    "yyywyyy",
    ".yywyy.",
    "...y...",
    "...y...",
};

const spr_art_t slipperworks_art_spool = {12, 10, ROWS_SPOOL};
const spr_art_t slipperworks_art_hammer = {14, 11, ROWS_HAMMER};
const spr_art_t slipperworks_art_basket = {12, 8, ROWS_BASKET};
const spr_art_t slipperworks_art_fx = {7, 7, ROWS_FX};

const spr_art_t *slipperworks_art_tier(uint8_t tier) {
    return &ART_TIER[tier < 10 ? tier : 0];
}
