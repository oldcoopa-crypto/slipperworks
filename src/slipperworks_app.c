// src/slipperworks_app.c —— 《库巴的拖鞋工坊》入口表。
// 按键:标题页 ▲▼ 选、● 确定;工坊 ▲▼ 挪吊臂、● 丢鞋、● 满桶时敲大锤、
//       长按▼ 收鞋入背包、长按● 开鞋材铺(刚丢的鞋会先收回来);背包页 ● 卖、长按● 卖光一行。
// 存档:saves 分区的 "slipperworks"/"save"(slipperworks_save.h,36 字节)。
#include "slipperworks_logic.h"
#include "slipperworks_save.h"
#include "slipperworks_texts.h"
#include "slipperworks_story.h"
#include "slipperworks_voices.h"
#include "slipperworks_ui.h"

#include <string.h>

#include "esp_random.h"
#include "kit_app.h"
#include "kit_lines.h"
#include "kit_sfx.h"
#include "kit_sprite.h"
#include "kit_store.h"
#include "ui_kit.h"

#define SW_LONG_GRACE_MS 160  // 长按确认期:这之内松手 = 连按误触,不执行

typedef enum { PG_TITLE, PG_WORK, PG_BAG, PG_FAIL, PG_STORY, PG_CONFIRM } page_t;

static slipperworks_save_t s_save;
static slipperworks_game_t s_game;
static page_t s_page;
static uint8_t s_sel;         // 当前页的选择(标题 0..3 / 铺子 0..3 / 背包 0..8)
static bool s_shop;           // 工坊里开着鞋材铺
static bool s_menu;           // 工坊里开着功能菜单(物理暂停)
static bool s_rpg_won;        // 《库巴大冒险》通关徽章
static uint32_t s_anim;       // 自由动画时钟(标题英雄拖鞋)
static int8_t s_swallow = -1;      // 长按之后会跟一声同键 PRESS,吃掉它
static uint32_t s_swallow_at;      // 长按发生的时刻(只在 300ms 内的跟屁 PRESS 才吃)
static uint8_t s_known_mile;  // 已经恭喜过的里程碑位
static uint8_t s_sv_n, s_sv_q, s_sv_h, s_sv_cur;  // 上次落盘的现场指纹(数量/排队/锤子/手里)
static uint16_t s_scene_ms;  // >0:现场有变化,倒计时归零就原样续存一份
static uint8_t s_prev_n;     // 上一帧桶里的鞋数(满桶瞬间提醒用)
static const sw_story_step_t *s_st_steps;  // 当前剧本
static uint8_t s_st_n, s_st_i;             // 步数/当前步
static uint8_t s_story;                    // 工坊里开着故事浮层(物理暂停)
static uint8_t s_story_kind;               // 1 = 教程, 2 = 里程碑夸奖
static uint8_t s_tut_stage;                // 新手教程进行到哪(0 = 完/没开)
static bool s_praise_wait;                 // 里程碑夸奖排队(等工坊空闲)
static bool s_exit_to_firmware;  // 菜单里选了回固件:input 本次返回 true
static uint16_t s_long_wait;     // 长按确认期:连按误触的长按,松手就取消
static uint8_t s_long_action;    // 确认期结束后执行:1 = 开功能菜单, 2 = 收鞋
static uint8_t s_confirm_stage;  // 「新的工坊」确认到第几遍
static uint8_t s_stored_tier; // 已经落过盘的最高合出档
static uint16_t s_lost_bin, s_lost_bag;  // 翻桶洒掉的鞋(展示用)

// ------------------------------------------------------------- 存档与里程碑

static void sv_persist(void) {
    kit_sv_blob_store(SLIPPERWORKS_NS, SLIPPERWORKS_KEY, &s_save, sizeof(s_save));
}

// 成就名字(位序见 slipperworks_save.h)
// 全部里程碑:存档账本里的 + 从现有进度推导的
static uint8_t mile_full(void) {
    return (uint8_t)(s_save.mile_sticky | slipperworks_milestones_of(&s_save));
}

// 存档有实质变化:先把新达成的里程碑请出来,再落盘。
static void sv_commit(void) {
    s_stored_tier = s_save.best_tier;
    uint8_t now = mile_full();
    s_save.mile_sticky = now;
    if (now & (uint8_t)~s_known_mile) {
        slipperworks_ui_toast(DT_MILE, UK_GOLD, 2500);
        sfx_play(SFX_LEVEL_UP);
        if (s_page == PG_WORK) s_praise_wait = true;  // 等场面安静了奶奶再夸
    }
    s_known_mile = now;
    sv_persist();
}

// ------------------------------------------------------------- 页面切换

static void story_show(uint8_t id, uint8_t full);
static bool slipperworks_won(void);  // 感恩剧情的触发条件在 to_work 里用

// 页面切换前必须调用:收起故事浮层,避免旧界面对象悬空
static void story_hide(void) {
    slipperworks_ui_story_hide();
    s_story = 0;
}  // 前置:to_work 的教程第一课要用

static void to_title(void) {
    story_hide();
    s_page = PG_TITLE;
    s_sel = 0;
    slipperworks_ui_title(s_sel, &s_save, s_rpg_won, s_anim);
}

static void to_work(void) {
    story_hide();
    s_page = PG_WORK;
    s_shop = false;
    s_menu = false;
    s_long_wait = 0;
    if (!slipperworks_restore(&s_game, &s_save)) {
        slipperworks_start(&s_game, esp_random(), kit_heat_from(s_save.heat),
                           s_save.basket_lv);  // 种子只在开局取
    }
    s_prev_n = s_game.n;  // 满桶提醒的基线跟上本局起点,不然第二局装满就不提醒了
    slipperworks_ui_work(&s_game);
    slipperworks_ui_work_update(&s_game, &s_save);
    sfx_play(SFX_START);
    // 新手教程:第一次进工坊(或新的工坊之后),奶奶开讲
    if (!(s_save.flags & 0x02)) {
        s_tut_stage = 1;
        story_show(SW_ST_T1, 0);
        return;
    }
    // 全里程碑达成:奶奶的感恩剧情,一场只演一次(flags 位5 记账)
    if (slipperworks_won() && !(s_save.flags & 0x20u)) {
        s_save.flags |= 0x20u;
        sv_persist();
        story_show(SW_ST_GRAT, 0);
    }
}

static uint16_t bag_total_count(void) {
    uint16_t n = 0;
    for (uint8_t i = 0; i < SW_TIERS; i++) n = (uint16_t)(n + s_save.bag[i]);
    return n;
}

static void to_fail(void) {
    story_hide();
    s_lost_bin = (uint16_t)(s_game.n + s_game.queue_n);
    s_lost_bag = bag_total_count();
    s_game.jam_ms = 0;
    s_game.combo = 0;
    s_game.combo_ms = 0;
    s_game.n = 0;
    s_game.queue_n = 0;
    memset(s_save.bag, 0, sizeof(s_save.bag));
    slipperworks_export(&s_game, &s_save);  // 现场也清掉(排队的购买鞋同样洒了)
    sv_commit();
    sfx_play(SFX_GAME_OVER);
    s_page = PG_FAIL;
    slipperworks_ui_fail(s_lost_bin, s_lost_bag);
}

static void to_work_menu(void) {
    to_work();  // 恢复/开现场并建屏
    s_menu = true;
    s_sel = 0;
    slipperworks_ui_menu(s_sel);
}

// 播一段故事:full = 全页(开场),否则工坊浮层。步骤推进在 story_next。
static void story_show(uint8_t id, uint8_t full) {

    const sw_story_t *st = slipperworks_story(id);
    s_st_steps = st->steps;
    s_st_n = st->n;
    s_st_i = 0;
    s_story = full ? 0 : 1;
    s_story_kind = (uint8_t)(id == SW_ST_PRAISE ? 2 : id == SW_ST_GRAT ? 3 : 1);
    if (full) {
        s_page = PG_STORY;
        slipperworks_ui_story_page(&st->steps[0]);
    } else {
        slipperworks_ui_story_modal(&st->steps[0]);
    }
}

// 工坊浮层故事推进一步(●):打字机没完先显示完,完了下一步,最后收掉
static void story_next(void) {
    if (!slipperworks_ui_story_done()) {
        slipperworks_ui_story_finish();
        return;
    }
    s_st_i++;
    if (s_st_i < s_st_n) {
        slipperworks_ui_story_step(&s_st_steps[s_st_i]);
        return;
    }
    slipperworks_ui_story_hide();
    s_story = 0;
    if (s_story_kind == 2) s_praise_wait = false;
    if (s_story_kind == 3) {
        // 剧情说完,奶奶拖真进背包
        s_save.bag[7]++;
        slipperworks_ui_toast(DT_GRAT_GIFT, UK_GOLD, 3000);
        sfx_play(SFX_LEVEL_UP);
        sv_commit();
    }
    // 故事可能在任意页面开着(比如背包教学):只有工坊页才能刷工坊对象,别的页对象早换了屏
    if (s_page == PG_WORK) slipperworks_ui_work_update(&s_game, &s_save);
}

static void to_bag(void) {
    story_hide();
    slipperworks_export(&s_game, &s_save);  // 离开工坊:现场存档(桶里的鞋、排队、锤子)
    sv_persist();
    s_page = PG_BAG;
    s_sel = 0;
    slipperworks_ui_bag(s_sel, &s_save);
    if (!(s_save.tips_done & 0x02u)) {  // 第一次进背包:提示条教学
        s_save.tips_done |= 0x02u;
        sv_persist();
        sv_persist();
        story_show(SW_ST_SELL, 0);
    }
}

static void close_shop(void) {
    if (!s_shop) return;
    s_shop = false;
    slipperworks_ui_shop_hide();
    slipperworks_ui_work_update(&s_game, &s_save);
}

// ------------------------------------------------------------- 工坊的按键

static void work_ok(void) {
    if (s_game.jam_ms >= SW_WARN_MS && slipperworks_jam_column(&s_game, s_game.hanger_x)) {
        // 站在压线那列底下:● 变成压桶锤,把堆敲下去敲实;站别处就还是丢鞋
        if (slipperworks_hammer(&s_game)) {
            sfx_play(SFX_HIT);
            slipperworks_ui_shake();
        } else {
            slipperworks_ui_toast(DT_NO_HAMMER, UK_ORANGE, 2500);
        }
    } else if (slipperworks_drop(&s_game)) {
        sfx_play(SFX_TAP);
        if (s_game.n == SW_MAX_PIECES) {  // 这一手正好装满:当场喊一声,别等下一帧
            slipperworks_ui_toast(DT_BIN_FULL, UK_RED, 3000);
            sfx_play(SFX_TICK);
        }
        if (s_tut_stage == 1) {  // 第一丢:教合成
            s_tut_stage = 2;
            story_show(SW_ST_T2, 0);
        }
    } else if (s_game.n >= SW_MAX_PIECES) {
        slipperworks_ui_toast(DT_BIN_FULL, UK_RED, 3000);  // 桶到上限:收鞋或合成腾地方
        sfx_play(SFX_TICK);
    } else if (s_game.jam_ms && slipperworks_jam_column(&s_game, s_game.hanger_x)) {
        slipperworks_ui_toast(DT_JAMMED, UK_RED, 3000);  // 冒线倒计时里这列锁着
        sfx_play(SFX_TICK);
    }
    // 剩下的只有丢完 350ms 冷却里的连按:静默放过,不打断丢鞋节奏
    slipperworks_ui_work_update(&s_game, &s_save);
}

static void work_collect(void) {
    if (slipperworks_collect(&s_game, &s_save)) {
        sfx_play(SFX_PERFECT);
        sv_commit();
        if (s_tut_stage == 3) {  // 第一收:教菜单
            s_tut_stage = 4;
            story_show(SW_ST_T4, 0);
        }
    }
    slipperworks_ui_work_update(&s_game, &s_save);
}

static void close_menu(void) {
    if (!s_menu) return;
    s_menu = false;
    slipperworks_ui_menu_hide();
    slipperworks_ui_work_update(&s_game, &s_save);
}

static void work_open_menu(void) {
    // 长按●前的那声 PRESS 已经把鞋丢出去了:不收回 —— 它引发的连锁照算
    if (s_tut_stage == 4) {  // 第一次开菜单:新手教程毕业
        s_save.flags |= 0x02;
        s_tut_stage = 0;
        sv_commit();
    }
    s_menu = true;
    s_sel = 0;
    slipperworks_ui_menu(s_sel);
}

// 菜单里按 ●:按当前项执行
static void menu_ok(void) {
    switch (s_sel) {
        case 0:  // 继续做工
            close_menu();
            break;
        case 1:  // 鞋材铺
            s_menu = false;
            slipperworks_ui_menu_hide();
            s_shop = true;
            s_sel = 0;
            slipperworks_ui_shop(s_sel, s_game.queue_n, s_game.basket_lv);
            if (!(s_save.flags & 0x20u)) {  // 第一次开铺子:奶奶对话教学
                s_save.flags |= 0x20u;
                sv_persist();
                story_show(SW_ST_SHOP, 0);
            }
            break;
        case 2:  // 背包
            s_menu = false;
            slipperworks_ui_menu_hide();
            to_bag();
            break;
        case 3:  // 收鞋入背包
            if (slipperworks_collect(&s_game, &s_save)) {
                sfx_play(SFX_PERFECT);
                sv_commit();
            }
            close_menu();
            slipperworks_ui_work_update(&s_game, &s_save);
            break;
        case 4:  // 大锤敲一下
            if (slipperworks_hammer(&s_game)) {
                sfx_play(SFX_HIT);
                slipperworks_ui_shake();
                close_menu();
                slipperworks_ui_work_update(&s_game, &s_save);
            } else {
                slipperworks_ui_toast(DT_NO_HAMMER, UK_ORANGE, 2500);
            }
            break;
        case 5:  // 回标题(离场:现场先落盘)
            s_menu = false;
            slipperworks_ui_menu_hide();
            slipperworks_export(&s_game, &s_save);
            sv_persist();
            to_title();
            break;
        default:  // 回菜单(固件)
            slipperworks_export(&s_game, &s_save);
            sv_persist();
            s_exit_to_firmware = true;
            break;
    }
}

static void shop_ok(void) {
    sw_buy_t r = slipperworks_buy(&s_game, &s_save, s_sel);
    if (r == SW_BUY_OK) {
        sfx_play(SFX_TAP);
        sv_commit();
        if (s_sel == 4) {
            // 篮子升级:桶壁变宽了,工坊画面整个重画一遍再铺上铺子
            char tb[48];
            snprintf(tb, sizeof(tb), DT_BASKET_UP, (unsigned)s_game.basket_lv);
            slipperworks_ui_toast(tb, UK_GOLD, 2200);
            slipperworks_ui_work(&s_game);
            slipperworks_ui_work_update(&s_game, &s_save);
            slipperworks_ui_shop(s_sel, s_game.queue_n, s_game.basket_lv);
            sfx_play(SFX_LEVEL_UP);
            return;
        }
        slipperworks_ui_shop_update(s_sel, s_game.queue_n, s_game.basket_lv);
        slipperworks_ui_work_update(&s_game, &s_save);
    } else if (r == SW_BUY_FULL) {
        slipperworks_ui_toast(DT_HAMMER_FULL, UK_ORANGE, 2500);
    } else if (r == SW_BUY_QFULL) {
        slipperworks_ui_toast(DT_QFULL, UK_ORANGE, 2500);
    } else {
        slipperworks_ui_toast(DT_NO_MAT, UK_RED, 2500);
        sfx_play(SFX_DODGE);
    }
}

// ------------------------------------------------------------- 背包的按键

static void bag_upgrade(void) {
    // 长按前必有一声 PRESS 已经把光标挪到了下一行:先挪回去再升级这一行
    // (顺序不能反:回旋拖鞋是最后一行,按下后正好挪进「回到功能界面」)
    s_sel = (uint8_t)((s_sel + SW_BAG_SEL_BACK) % (SW_BAG_SEL_BACK + 1u));
    slipperworks_ui_bag_update(s_sel, &s_save);
    if (s_sel == SW_BAG_SEL_BACK) return;  // 挪回来还在返回行 = 本来就停在返回行
    uint8_t tier = (uint8_t)(s_sel + SW_COLLECT_TIER);
    if (slipperworks_upgrade(&s_save, tier)) {
        char tb[48];
        snprintf(tb, sizeof(tb), DT_UP_OK, (unsigned)slipperworks_price_of(&s_save, tier));
        slipperworks_ui_toast(tb, UK_GOLD, 2000);
        sfx_play(SFX_LEVEL_UP);
        sv_commit();
    } else if (slipperworks_up_of(&s_save, tier) >= SW_UP_MAX) {
        slipperworks_ui_toast(DT_UP_MAXED, UK_ORANGE, 2500);
    } else {
        slipperworks_ui_toast(DT_NO_MAT, UK_RED, 2500);
        sfx_play(SFX_DODGE);
    }
    slipperworks_ui_bag_update(s_sel, &s_save);
}

static void bag_sell(uint16_t count) {
    uint8_t tier = (uint8_t)(s_sel + SW_COLLECT_TIER);
    uint16_t sold = slipperworks_sell(&s_save, tier, count);
    if (sold) {
        sfx_play(SFX_DODGE);  // 收款的「叮」
        sv_commit();
    }
    slipperworks_ui_bag_update(s_sel, &s_save);
}

// ------------------------------------------------------------- kit_app_t 回调

// 旧 8 位账本 → 新 16 位:0卖→0 | 2爸→1 | 4奶→2 | 5金→3 | 7回旋→4 | 3爷→9 | 6收入万→10
static uint8_t mile_remap(uint8_t o) {
    uint16_t m = (uint16_t)(o & 0x01u);
    if (o & 0x04u) m |= 1u << 1;
    if (o & 0x10u) m |= 1u << 2;
    if (o & 0x20u) m |= 1u << 3;
    if (o & 0x80u) m |= 1u << 4;
    if (o & 0x08u) m |= 1u << 9;
    if (o & 0x40u) m |= 1u << 10;
    return (uint8_t)m;  // 旧账本只有平台位,高 8 位补 0 由调用方负责
}

static void slipperworks_init(void) {
    static slipperworks_save_t scratch;
    if (!kit_sv_blob_load(SLIPPERWORKS_NS, SLIPPERWORKS_KEY, &s_save, sizeof(s_save),
                          slipperworks_save_valid, &scratch)) {
        // 20 只桶时代的老档(160;bin 数组在中段):整块读进新布局会把排队/原料高位
        // 全部读成垃圾(余额凭空多出几百万就是它),必须读进 scratch 再按老布局解包
        if (kit_sv_blob_load(SLIPPERWORKS_NS, SLIPPERWORKS_KEY, &scratch,
                             SLIPPERWORKS_SAVE_BIN20_SIZE, slipperworks_save_valid_bin20, &scratch)) {
            slipperworks_save_from_bin20(&s_save, &scratch);
            s_save.mile_sticky = mile_remap(s_save.mile_sticky);  // 旧位序 → 新位序
        } else if (kit_sv_blob_load(SLIPPERWORKS_NS, SLIPPERWORKS_KEY, &scratch,
                                    SLIPPERWORKS_SAVE_V3_SIZE, slipperworks_save_valid_bin20, &scratch)) {
            // 155 字节档连尾部账本都没有:解包出的残留尾巴清零
            slipperworks_save_from_bin20(&s_save, &scratch);
            s_save.mile_sticky = 0;
            s_save.mat_hi = 0;
            s_save.tips_done = 0;
            s_save.mile_hi = 0;
        } else {
            slipperworks_save_reset(&s_save);
        }
    }
    // 旧档迁移:提示位清一次(教学/提示对话随修复更新,重看一遍);tips_done 记账后不再重播
    if (!s_save.tips_done) s_save.flags = (uint8_t)(s_save.flags & 0x1Fu);  // 保留位4(拖鞋雨)
    uint8_t before = s_save.mile_sticky;
    s_save.mile_sticky |= slipperworks_milestones_of(&s_save);
    s_known_mile = s_save.mile_sticky;
    s_stored_tier = s_save.best_tier;
    if (s_save.mile_sticky != before) sv_persist();  // 旧档补记账才写,平时开机不写
}

static void slipperworks_enter(const kit_links_t *links) {
    s_rpg_won = links->rpg_won;
    s_anim = 0;
    to_title();
}

static bool slipperworks_input(const kit_input_t *in) {
    if (!in->long_press) {
        if ((int8_t)in->key == s_swallow && kit_clock_ms() - s_swallow_at < 300u) {
            s_long_wait = 0;  // 长按后很快就松手:连按误触,取消待发的长按动作
            return false;
        }
        s_swallow = -1;  // 隔久了或按了别的键:标记作废,这是正常按键
        s_long_wait = 0;
    }
    if (in->long_press) {
        s_swallow = (int8_t)in->key;
        s_swallow_at = kit_clock_ms();
    }
    if (s_story) {  // 故事浮层开着:● 推进(先显示完整句),其他键忽略
        if (in->key == KIT_KEY_OK && !in->long_press) story_next();
        return false;
    }

    switch (s_page) {
        case PG_TITLE:
            if (in->key == KIT_KEY_OK) {
                    if (in->long_press) break;  // 标题页长按不重复触发
                if (s_sel == 0) {
                    if (s_save.flags & 0x40u) {
                        to_work();
                    } else {
                        // 第一次开工:先播开场剧,演完自动进工坊带新手教程
                        s_save.flags |= 0x40u;
                        sv_persist();
                        story_show(SW_ST_OPEN, 1);
                    }
                }
                else if (s_sel == 1) {
                    s_save.heat = (uint8_t)kit_heat_next(kit_heat_from(s_save.heat));
                    sv_persist();
                    slipperworks_ui_title(s_sel, &s_save, s_rpg_won, s_anim);
                } else if (s_sel == 2) {
                    s_confirm_stage = 1;
                    s_page = PG_CONFIRM;
                    slipperworks_ui_confirm(1);
                } else {
                    return true;  // 回菜单
                }
            } else {
                s_sel = (uint8_t)((s_sel + (in->key == KIT_KEY_UP ? 3u : 1u)) % 4u);
                slipperworks_ui_title(s_sel, &s_save, s_rpg_won, s_anim);
            }
            break;

        case PG_CONFIRM:
            if (in->long_press) break;  // 长按不做事(按下那声已经推进了确认)
            if (in->key == KIT_KEY_OK) {
                if (s_confirm_stage == 1) {
                    s_confirm_stage = 2;
                    slipperworks_ui_confirm(2);
                } else {
                    // 两遍都确认了:清空进度;星星账本永久保留
                    uint8_t keep_stars = s_save.mile_sticky;
                    slipperworks_save_reset(&s_save);
                    s_save.mile_sticky = keep_stars;
                    s_known_mile = keep_stars;
                    s_stored_tier = 0;
                    sv_persist();
                    story_show(SW_ST_OPEN, 1);
                }
            } else {
                to_title();  // ▲▼ 取消
            }
            break;

        case PG_STORY:
            if (in->key == KIT_KEY_OK && !in->long_press) {
                if (!slipperworks_ui_story_done()) {
                    slipperworks_ui_story_finish();
                    return false;
                }
                s_st_i++;
                if (s_st_i < s_st_n) {
                    slipperworks_ui_story_step(&s_st_steps[s_st_i]);
                } else {
                    slipperworks_ui_story_hide();
                    if (!(s_save.flags & 0x01)) {
                        s_save.flags |= 0x01;
                        sv_persist();
                    }
                    to_work();
                }
            }
            break;

        case PG_WORK:
            if (s_story) {
                if (in->key == KIT_KEY_OK && !in->long_press) story_next();
            } else if (s_menu) {
                if (in->long_press) {
                    // 菜单里长按不做事(长按前必有一声 PRESS,会误触选项)
                } else if (in->key != KIT_KEY_OK) {
                    s_sel = (uint8_t)((s_sel + (in->key == KIT_KEY_UP ? SW_MENU_N - 1u : 1u)) % SW_MENU_N);
                    slipperworks_ui_menu_update(s_sel);
                } else {
                    menu_ok();
                }
            } else if (s_shop) {
                if (in->long_press) {
                    // 铺子里长按●不做事:长按前必有一声 PRESS,会把选中的东西买掉
                } else if (in->key != KIT_KEY_OK) {
                    s_sel = (uint8_t)((s_sel + (in->key == KIT_KEY_UP ? SW_SHOP_CLOSE : 1u)) %
                                      (SW_SHOP_CLOSE + 1u));
                    slipperworks_ui_shop_update(s_sel, s_game.queue_n, s_game.basket_lv);
                } else if (s_sel == SW_SHOP_BAG) {
                    s_shop = false;
                    slipperworks_ui_shop_hide();
                    to_bag();
                } else if (s_sel == SW_SHOP_CLOSE) {
                    close_shop();
                    s_menu = true;
                    s_sel = 0;
                    slipperworks_ui_menu(s_sel);
                } else {
                    shop_ok();
                }
            } else if (in->key == KIT_KEY_UP) {
                slipperworks_hanger(&s_game, -1);
                slipperworks_ui_work_update(&s_game, &s_save);
            } else if (in->key == KIT_KEY_DOWN && !in->long_press) {
                slipperworks_hanger(&s_game, +1);
                slipperworks_ui_work_update(&s_game, &s_save);
            } else if (in->key == KIT_KEY_DOWN) {
                if (!s_long_wait) {
                    s_long_wait = SW_LONG_GRACE_MS;  // 长按▼:确认期过了再收鞋
                    s_long_action = 2;
                }
            } else if (in->long_press) {
                if (!s_long_wait) {
                    s_long_wait = SW_LONG_GRACE_MS;  // 长按●:确认期过了再开菜单
                    s_long_action = 1;
                }
            } else {
                work_ok();
            }
            break;

        case PG_FAIL:
            if (in->key == KIT_KEY_OK && !in->long_press) to_work();
            break;

        case PG_BAG:
            if (in->key == KIT_KEY_DOWN && in->long_press) {
                bag_upgrade();
            } else if (in->key != KIT_KEY_OK) {
                s_sel = (uint8_t)((s_sel + (in->key == KIT_KEY_UP ? SW_BAG_SEL_BACK : 1u)) %
                                  (SW_BAG_SEL_BACK + 1u));
                slipperworks_ui_bag_update(s_sel, &s_save);
            } else if (s_sel == SW_BAG_SEL_BACK) {
                to_work_menu();
            } else if (in->long_press) {
                bag_sell(0);  // 卖光这一行
            } else {
                bag_sell(1);
            }
            break;
    }
    if (s_exit_to_firmware) {
        s_exit_to_firmware = false;
        return true;
    }
    return false;
}

static void slipperworks_frame(uint32_t dt) {
    slipperworks_ui_toast_frame(dt);
    // 故事浮层(任何页面):打字机 + 配音静默计时必须在页面守卫之前推进
    if (s_story || s_page == PG_STORY) slipperworks_ui_story_tick(dt);
    if (s_page == PG_TITLE) {
        s_anim += dt;
        slipperworks_ui_title_frame(s_anim);
        return;
    }
    if (s_page != PG_WORK || s_shop || s_menu) return;

    // 桶刚装满:主动喊一声,别等玩家按了●才发现丢不下去
    if (s_game.n == SW_MAX_PIECES && s_prev_n < SW_MAX_PIECES) {
        slipperworks_ui_toast(DT_BIN_FULL, UK_RED, 3000);
        sfx_play(SFX_TICK);
    }
    s_prev_n = s_game.n;
    // 工坊现场续存:数量/排队/锤子/手里有变化 → 5 秒后原样拍一份落盘,
    // 刷新/断电最多丢这 5 秒;不结算,正在掉的鞋恢复后接着掉。
    if (s_game.n != s_sv_n || s_game.queue_n != s_sv_q || s_game.hammers != s_sv_h ||
        s_game.cur != s_sv_cur) {
        s_sv_n = s_game.n;
        s_sv_q = s_game.queue_n;
        s_sv_h = s_game.hammers;
        s_sv_cur = s_game.cur;
        s_scene_ms = 5000;
    }
    if (s_scene_ms) {
        s_scene_ms = (uint16_t)(s_scene_ms > dt ? s_scene_ms - dt : 0);
        if (!s_scene_ms) {
            slipperworks_snapshot(&s_game, &s_save);
            sv_persist();
        }
    }

    if (s_long_wait) {
        s_long_wait = (uint16_t)(s_long_wait > dt ? s_long_wait - dt : 0);
        if (!s_long_wait) {
            if (s_long_action == 1) work_open_menu();
            else work_collect();
        }
    }

    sw_tick_out_t ev;
    slipperworks_tick(&s_game, &s_save, dt, &ev);
    if (ev.merges) {
        sfx_play(SFX_HIT);
        if (ev.combo >= 2) {
            char cb[16];
            snprintf(cb, sizeof(cb), DT_COMBO, (unsigned)ev.combo);  // 别把 %u 原样上屏
            slipperworks_ui_toast(cb, UK_ORANGE, 1500);
        }
        // 合出第一双"成品鞋"(小布拖起,能收进背包的)才教收鞋:
        // 鞋坯这类半成品收不了,提前教只会让玩家对着没反应的 ▼ 发懵
        if (s_tut_stage == 2 && s_save.best_tier >= SW_COLLECT_TIER) {
            s_tut_stage = 3;
            story_show(SW_ST_T3, 0);
        }
    }
    if (ev.spin) {
        sfx_play_line(&LINE_BOOMERANG, SFX_CHAIN, false);  // 奶奶:拖鞋会拐弯!
        s_save.flags |= 0x10;  // 拖鞋雨里程碑(下面 sv_commit 落盘)
    }
    if (ev.just_warn && !(s_save.tips_done & 0x04u)) {
        // 第一次堆到警戒线:温柔一次 —— 自动弹功能菜单、补一把大锤,奶奶对话教救援
        // 菜单开着物理暂停,时间冻结;关掉菜单后 ~1 秒内不锤不收,才真的算翻桶
        s_save.tips_done |= 0x04u;  // 大锤救援只演一次(翻桶判定照常,只是先教学一遍)
        if (s_game.hammers < SW_HAMMER_MAX) s_game.hammers++;
        sv_persist();
        close_menu();
        close_shop();
        s_menu = true;  // 功能菜单保持可见,奶奶在菜单上讲救援
        s_sel = 4;      // 光标停在大锤上
        slipperworks_ui_menu(s_sel);
        story_show(SW_ST_JAM, 0);
        return;
    }
    if (ev.fail) {
        to_fail();
        return;
    }
    // 合成本身不落盘(太频繁伤闪存):只有合出了新档位(里程碑可能变化)才写
    if (ev.spin || (ev.merges && s_save.best_tier != s_stored_tier)) sv_commit();
    if (ev.spin) {
        slipperworks_ui_toast(DT_SPIN, UK_GOLD, 2600);  // 结算完再显示,免得被里程碑提示顶掉
        sfx_play_line(&slipperworks_voice_G_RAIN, SFX_NONE, false);  // 奶奶:拖鞋雨!
    }
    // 里程碑夸奖:等教程和菜单都空了再演
    if (s_praise_wait && s_tut_stage == 0 && !s_story) {
        s_praise_wait = false;
        sfx_play(SFX_LEVEL_UP);
        story_show(SW_ST_PRAISE, 0);
        return;
    }
    slipperworks_ui_work_update(&s_game, &s_save);
}

static unsigned slipperworks_busy(void) {
    return (s_page == PG_WORK && !s_shop && !s_menu) ? KIT_BUSY_TIMING : 0;
}

// won / milestones / status 会被外壳在任意时刻调用:只读状态,不取随机数。
static bool slipperworks_won(void) {
    return mile_full() == 0xFFu;
}

static uint8_t slipperworks_milestones(void) {
    return mile_full();
}

// 调试通道状态行:SW <页> <原料> <背包总数> <大锤>
static size_t slipperworks_status(char *buf, size_t len) {
    int n = snprintf(buf, len, "SW %d %u %u %u %u %u\n", (int)s_page,
                     (unsigned)slipperworks_mat_of(&s_save),
                     (unsigned)bag_total_count(), (unsigned)s_game.hammers, (unsigned)s_game.n,
                     (unsigned)s_game.queue_n);
    return n < 0 ? 0 : ((size_t)n < len ? (size_t)n : len - 1);
}

static void slipperworks_save_now(void) {
    if (s_page == PG_WORK) slipperworks_export(&s_game, &s_save);  // 低电:工坊现场一起落盘
    sv_persist();
}

static const kit_menu_card_t SLIPPERWORKS_CARD = {
    .title = DT_CARD,
    .sub = DT_SUB,
    .art = {SPR_SLIPPER, SPR_KID},
};

const kit_app_t slipperworks_app = {
    .id = "slipperworks",
    // 十级拖鞋链(10..48px)+ 线卷/大锤/闪光 + 库巴/猫/拖鞋/兜底,解码后约 48KB
    .sprite_budget = KIT_SPR_BUDGET(32 * 1024, 24),
    .init = slipperworks_init,
    .enter = slipperworks_enter,
    .input = slipperworks_input,
    .frame = slipperworks_frame,
    .busy = slipperworks_busy,
    .won = slipperworks_won,
    .milestones = slipperworks_milestones,
    .status = slipperworks_status,
    .save_now = slipperworks_save_now,
    .card = &SLIPPERWORKS_CARD,
};
