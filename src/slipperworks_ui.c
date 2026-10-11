// src/slipperworks_ui.c —— 见 slipperworks_ui.h。ui_kit 的小工具 + 原创字符画自绘(lv_image)。
#include "slipperworks_ui.h"

#include <string.h>

#include "kit_sprite.h"
#include "slipperworks_art.h"
#include "kit_sfx.h"
#include "slipperworks_story.h"
#include "slipperworks_texts.h"
#include "ui_kit.h"

// ---------------------------------------------------------------- 通用小工具

// 原料数字显示:≥10 万切「万」单位
static void mat_text(char *buf, size_t n, uint32_t mat);

static const char *const NAMES[SW_TIERS] = {DT_T0, DT_T1, DT_T2, DT_T3, DT_T4,
                                            DT_T5, DT_T6, DT_T7, DT_T8, DT_T9};

// 原创字符画 → lv_image(会话池里解码一次);scale256 是 256 基准的缩放。
static lv_obj_t *art_img(lv_obj_t *parent, const spr_art_t *a, int scale256) {
    lv_obj_t *o = lv_image_create(parent);
    lv_image_set_src(o, kit_img(a, spr_decode));
    lv_image_set_antialias(o, false);
    lv_image_set_pivot(o, a->w / 2, a->h / 2);
    if (scale256 != 256) lv_image_set_scale(o, (int32_t)scale256);
    return o;
}

// 以中心点摆位。
static void art_cpos(lv_obj_t *o, const spr_art_t *a, int cx, int cy, int scale256) {
    lv_obj_set_pos(o, cx - a->w * scale256 / 512, cy - a->h * scale256 / 512);
}

// 换图:同一个 art 不重复 set_src(省一次失效)。
static void art_set(lv_obj_t *o, const spr_art_t **cache, const spr_art_t *a) {
    if (*cache == a) return;
    *cache = a;
    lv_image_set_src(o, kit_img(a, spr_decode));
    lv_image_set_pivot(o, a->w / 2, a->h / 2);
    lv_image_set_scale(o, 256);
}

static lv_obj_t *centered(lv_obj_t *parent, const lv_font_t *f, uint32_t color, const char *s, int y) {
    lv_obj_t *t = uk_text(parent, f, color, s);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, y);
    return t;
}

static void bag_total(const slipperworks_save_t *sv, uint16_t *total) {
    uint16_t n = 0;
    for (uint8_t i = 0; i < SW_TIERS; i++) n = (uint16_t)(n + sv->bag[i]);
    *total = n;
}

// ---------------------------------------------------------------- 标题页

#define T_PILLS 4

static lv_obj_t *s_t_hero;
static lv_obj_t *s_t_pills[T_PILLS];

void slipperworks_ui_title(uint8_t sel, const slipperworks_save_t *sv, bool rpg_won, uint32_t anim_ms) {
    lv_obj_t *scr = uk_screen_new(UK_WALL);
    centered(scr, UK_F24, UK_INK, DT_TITLE, 28);
    if (rpg_won) {
        lv_obj_t *b = uk_text(scr, UK_F12, UK_GOLD, DT_BADGE);
        lv_obj_align(b, LV_ALIGN_TOP_RIGHT, -8, 10);
    }

    s_t_hero = art_img(scr, slipperworks_art_tier(9), 512);  // 44x25 → 88x50
    slipperworks_ui_title_frame(anim_ms);

    centered(scr, UK_F12, UK_INK, DT_SUB, 146);
    char buf[48];
    uint16_t total;
    bag_total(sv, &total);
    mat_text(buf, sizeof(buf), slipperworks_mat_of(sv));
    int used = (int)strlen(buf);
    snprintf(buf + used, sizeof(buf) - (size_t)used, DT_INCOME_PAD, (unsigned)sv->sales);
    centered(scr, UK_F12, UK_INK, buf, 166);

    const char *heat_name = sv->heat == KIT_HEAT_MILD ? DT_HEAT_MILD
                            : sv->heat == KIT_HEAT_HOT ? DT_HEAT_HOT
                                                       : DT_HEAT_MID;
    static char heat_item[24];
    snprintf(heat_item, sizeof(heat_item), DT_HEAT_ITEM, heat_name);
    const char *labels[T_PILLS] = {DT_GO, heat_item, DT_NW_TITLE, DT_MENU};
    for (int i = 0; i < T_PILLS; i++) {
        s_t_pills[i] = uk_pill(scr, UK_F12, sel == i ? UK_RED : UK_LINE, UK_WHITE, labels[i]);
        lv_obj_align(s_t_pills[i], LV_ALIGN_TOP_MID, 0, 186 + i * 30);
    }
    static const char *const T_DESCS[T_PILLS] = {DT_TD_GO, DT_TD_HEAT, DT_TD_NEW, DT_TD_MENU};
    centered(scr, UK_F12, UK_GRAY, T_DESCS[sel], UK_H - 20);
    uk_screen_swap(scr);
}

void slipperworks_ui_title_frame(uint32_t anim_ms) {
    if (!s_t_hero) return;
    int bob = uk_tri(anim_ms % 1200u, 1200u, 10);
    art_cpos(s_t_hero, slipperworks_art_tier(9), UK_W / 2, 97 + bob, 512);
}

// ---------------------------------------------------------------- 工坊

static lv_obj_t *s_bin_pieces[SW_MAX_PIECES];
static const spr_art_t *s_bin_art[SW_MAX_PIECES];
static bool s_bin_pop[SW_MAX_PIECES];
static lv_obj_t *s_fly_img[SW_FLY_N];
static const spr_art_t *s_fly_art[SW_FLY_N];
static lv_obj_t *s_rope;
static lv_obj_t *s_hanger;
static const spr_art_t *s_hanger_art;
static lv_obj_t *s_next;
static const spr_art_t *s_next_art;
static lv_obj_t *s_mat_txt;
static lv_obj_t *s_rim[6];
static lv_obj_t *s_bag_txt;
static lv_obj_t *s_ham_txt;
static lv_obj_t *s_hint_txt;

// 原料数字显示:≥10 万切「万」单位,免得长数字撑爆界面
static lv_obj_t *s_my_toast;   // 自己的提示条(uk_toast 在部分页面会隐身,不赌它)
static lv_obj_t *s_my_toast_scr;  // 提示条挂在哪块屏(换屏=旧条随屏销毁,指针作废)
static uint32_t s_my_toast_ms;

// 全页面通用提示条:挂在当前屏、自动置顶、自带计时。
// 换屏后旧条随旧屏销毁,下次调用按屏重建,绝不引用悬空对象。
void slipperworks_ui_toast(const char *text, uint32_t bg, uint32_t ms) {
    lv_obj_t *scr = lv_screen_active();
    if (!s_my_toast || s_my_toast_scr != scr) {
        s_my_toast = uk_text(scr, UK_F12, UK_WHITE, "");
        s_my_toast_scr = scr;
        lv_obj_set_style_bg_opa(s_my_toast, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_hor(s_my_toast, 8, 0);
        lv_obj_set_style_pad_ver(s_my_toast, 3, 0);
        lv_obj_align(s_my_toast, LV_ALIGN_TOP_MID, 0, 2);
    }
    lv_obj_set_style_bg_color(s_my_toast, lv_color_hex(bg), 0);
    uk_set_text(s_my_toast, text);
    lv_obj_move_foreground(s_my_toast);  // 菜单/铺子浮层盖不住它
    uk_show(s_my_toast, true);
    s_my_toast_ms = ms;
}

void slipperworks_ui_toast_frame(uint32_t dt) {
    if (!s_my_toast_ms) return;
    s_my_toast_ms = s_my_toast_ms > dt ? s_my_toast_ms - dt : 0;
    if (!s_my_toast_ms && s_my_toast && s_my_toast_scr == lv_screen_active())
        uk_show(s_my_toast, false);
}

static void mat_text(char *buf, size_t n, uint32_t mat) {
    if (mat >= 100000u) snprintf(buf, n, DT_MAT_WAN, (unsigned)(mat / 10000u));
    else snprintf(buf, n, DT_MAT, (unsigned)mat);
}

// 背包页用:纯数字(线卷图标就是「原料」的意思,不用文字前缀),贴右最宽 76px 不压图标
static void mat_text_plain(char *buf, size_t n, uint32_t mat) {
    if (mat >= 100000u) snprintf(buf, n, DT_MAT_WAN, (unsigned)(mat / 10000u));
    else snprintf(buf, n, "%u", (unsigned)mat);
}

// HUD 用的:数字大了以后「原料」前缀会挤到「下一只」,直接省掉前缀
static void mat_text_hud(char *buf, size_t n, uint32_t mat) {
    if (mat >= 100000u) snprintf(buf, n, DT_MAT_WAN, (unsigned)(mat / 10000u));
    else snprintf(buf, n, DT_MAT, (unsigned)mat);
}
static int s_bx0, s_bx1;  // 桶内壁(建屏时按辣度定)
static uint16_t s_shake_ms;  // 大锤震屏剩余时间
static lv_obj_t *s_cap_txt;  // 警戒线倒计时(大红数字,秒)

void slipperworks_ui_work(const slipperworks_game_t *g) {
    lv_obj_t *scr = uk_screen_new(UK_WALL);
    int inner = slipperworks_bin_inner(g);
    s_bx0 = (UK_W - inner) / 2;
    s_bx1 = s_bx0 + inner;

    // 装饰:库巴守在左边,猫蹲在右边
    lv_obj_t *kid = uk_sprite(scr, SPR_KID, 2);
    uk_spos(kid, 4, 150);
    lv_obj_t *cat = uk_sprite(scr, SPR_MON_CAT, 1);
    uk_spos(cat, s_bx1 + 9, 140);

    // 桶:内壁(比墙面深一档,衬得清鞋)、左右壁、桶底
    uk_rect(scr, s_bx0, SW_RIM_Y, inner, SW_FLOOR_Y - SW_RIM_Y, UK_WAINSCOT);
    uk_rect(scr, s_bx0 - 6, 84, 6, UK_H - 84, UK_FLOOR);
    uk_rect(scr, s_bx1, 84, 6, UK_H - 84, UK_FLOOR);
    uk_rect(scr, s_bx0 - 6, SW_FLOOR_Y, inner + 12, UK_H - SW_FLOOR_Y, UK_FLOOR_D);

    // 警戒线(虚线)
    int period = inner / 6;
    for (int i = 0; i < 6; i++) {
        s_rim[i] = uk_rect(scr, s_bx0 + i * period, SW_RIM_Y - 1, period * 3 / 5, 2, UK_LINE);
    }

    // 吊绳 + 吊挂 + 下一只预览
    s_rope = uk_rect(scr, g->hanger_x, 34, 1, SW_DROP_Y - 34, UK_INK);
    s_hanger_art = NULL;
    s_hanger = art_img(scr, slipperworks_art_tier(g->cur), 256);
    s_next_art = NULL;
    s_next = art_img(scr, slipperworks_art_tier(g->nxt), 256);

    // HUD:原料(线卷图标 + 数字)、下一只
    lv_obj_t *mi = art_img(scr, &slipperworks_art_spool, 512);  // 12x10 → 24x20
    art_cpos(mi, &slipperworks_art_spool, 22, 17, 512);
    s_mat_txt = uk_text(scr, UK_F24, UK_INK, "");
    lv_obj_align(s_mat_txt, LV_ALIGN_TOP_LEFT, 42, 6);
    lv_obj_t *nl = uk_text(scr, UK_F12, UK_INK, DT_NEXT);
    lv_obj_align(nl, LV_ALIGN_TOP_LEFT, 148, 8);

    // 装满倒计时的大数字(平时藏着)
    s_cap_txt = uk_text(scr, UK_F36, UK_RED, "");
    lv_obj_align(s_cap_txt, LV_ALIGN_TOP_MID, 0, 150);

    // 底条:背包数(左)和大锤数(右),白字画在桶底色上
    s_bag_txt = uk_text(scr, UK_F12, UK_WHITE, "");
    lv_obj_set_pos(s_bag_txt, s_bx0 + 2, SW_FLOOR_Y + 1);
    s_ham_txt = uk_text(scr, UK_F12, UK_WHITE, "");
    lv_obj_set_pos(s_ham_txt, s_bx1 - 46, SW_FLOOR_Y + 1);
    // 操作提示轮播(约 5 秒换一条)
    s_hint_txt = uk_text(scr, UK_F12, UK_WHITE, "");
    s_my_toast = uk_text(scr, UK_F12, UK_WHITE, "");
    lv_obj_set_style_bg_color(s_my_toast, lv_color_hex(UK_RED), 0);
    lv_obj_set_style_bg_opa(s_my_toast, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(s_my_toast, 8, 0);
    lv_obj_set_style_pad_ver(s_my_toast, 3, 0);
    lv_obj_align(s_my_toast, LV_ALIGN_TOP_MID, 0, 2);
    uk_show(s_my_toast, false);
    lv_obj_align(s_hint_txt, LV_ALIGN_TOP_MID, 0, SW_FLOOR_Y + 12);

    // 鞋和飞行动画的精灵池
    for (int i = 0; i < SW_MAX_PIECES; i++) {
        s_bin_art[i] = NULL;
        s_bin_pop[i] = false;
        s_bin_pieces[i] = art_img(scr, slipperworks_art_tier(0), 256);
        uk_show(s_bin_pieces[i], false);
    }
    for (int i = 0; i < SW_FLY_N; i++) {
        s_fly_art[i] = NULL;
        s_fly_img[i] = art_img(scr, slipperworks_art_tier(0), 256);
        uk_show(s_fly_img[i], false);
    }
    uk_screen_swap(scr);
}

void slipperworks_ui_shake(void) {
    s_shake_ms = 220;
}

void slipperworks_ui_work_update(const slipperworks_game_t *g, const slipperworks_save_t *sv) {
    char buf[24];
    if (!s_mat_txt || !s_bag_txt || !s_hanger || !s_rope || !s_next || !s_hint_txt || !s_cap_txt) return;

    if (s_shake_ms) {
        s_shake_ms = (uint16_t)(s_shake_ms > 16 ? s_shake_ms - 16 : 0);
        lv_obj_set_x(lv_screen_active(), (s_shake_ms / 30u) % 2u ? 3 : -3);
        if (!s_shake_ms) lv_obj_set_x(lv_screen_active(), 0);
    }

    // 吊挂和绳
    const spr_art_t *ha = slipperworks_art_tier(g->cur);
    if (!s_hanger || !s_rope || !s_next) return;  // 防御:页面切换后旧指针不可用
    art_cpos(s_hanger, ha, g->hanger_x, SW_DROP_Y, 256);
    int top = SW_DROP_Y - ha->h / 2;
    lv_obj_set_x(s_rope, g->hanger_x);
    lv_obj_set_height(s_rope, top > 34 ? top - 34 : 1);

    // 下一只(缩到约 22px 见方):后台排队的队首优先
    const spr_art_t *na = slipperworks_art_tier(g->queue_n ? g->queue[0] : g->nxt);
    art_set(s_next, &s_next_art, na);
    int s256 = 22 * 256 / (na->w > na->h ? na->w : na->h);
    lv_image_set_scale(s_next, (int32_t)s256);
    art_cpos(s_next, na, 210, 18, s256);

    // 桶里的鞋
    for (uint8_t i = 0; i < SW_MAX_PIECES; i++) {
        lv_obj_t *o = s_bin_pieces[i];
        if (!o) continue;
        if (i >= g->n) {
            uk_show(o, false);
            continue;
        }
        const sw_piece_t *p = &g->pcs[i];
        if (p->x != p->x || p->y != p->y || p->x < -50 || p->x > 290 || p->y < -50 || p->y > 350) {
            uk_show(o, false);
            continue;
        }
        const spr_art_t *a = slipperworks_art_tier(p->tier);
        uk_show(o, true);
        art_set(o, &s_bin_art[i], a);
        int px = (int)p->x, py = (int)p->y;
        if (px < -30) px = -30;
        if (px > 270) px = 270;
        if (py < -30) py = -30;
        if (py > 350) py = 350;
        art_cpos(o, a, px, py, 256);
        if (p->born) {
            lv_image_set_scale(o, 256 + p->born * 88 / SW_BORN_MS);
            s_bin_pop[i] = true;
        } else if (s_bin_pop[i]) {
            lv_image_set_scale(o, 256);
            s_bin_pop[i] = false;
        }
    }

    // 收鞋飞进背包
    for (uint8_t i = 0; i < SW_FLY_N; i++) {
        lv_obj_t *o = s_fly_img[i];
        if (!o || g->fly[i].t >= SW_FLY_MS) {
            uk_show(o, false);
            continue;
        }
        const sw_fly_t *f = &g->fly[i];
        const spr_art_t *a = slipperworks_art_tier(f->tier);
        art_set(o, &s_fly_art[i], a);
        int k = f->t * 256 / SW_FLY_MS;
        int x = (int)f->x + ((s_bx0 + 8) - (int)f->x) * k / 256;
        int y = (int)f->y + ((SW_FLOOR_Y + 10) - (int)f->y) * k / 256;
        y -= uk_tri(f->t, SW_FLY_MS, 26);
        art_cpos(o, a, x, y, 256);
        uk_show(o, true);
    }

    // 警戒线倒计时:压线时显示还剩几秒(离开线数字自动消失)
    if (g->jam_ms) {
        char buf[4];
        uint32_t left = (SW_FAIL_MS - g->jam_ms + 999u) / 1000u;
        if (left > 9u) left = 9;
        snprintf(buf, sizeof(buf), "%u", (unsigned)left);
        uk_set_text(s_cap_txt, buf);
        uk_show(s_cap_txt, true);
    } else {
        uk_show(s_cap_txt, false);
    }

    // 警戒线:堆一冒头就闪红,快翻桶时急促
    bool flash = g->jam_ms > 0 &&
                 (g->anim_ms / (g->jam_ms >= SW_WARN_MS ? 110u : 260u)) % 2u == 0;
    for (int i = 0; i < 6; i++) {
        lv_obj_set_style_bg_color(s_rim[i], lv_color_hex(flash ? UK_RED : UK_LINE), 0);
    }

    mat_text_hud(buf, sizeof(buf), slipperworks_mat_of(sv));
    uk_set_text(s_mat_txt, buf);
    uint16_t total;
    bag_total(sv, &total);
    snprintf(buf, sizeof(buf), DT_BAG_N, (unsigned)total);
    uk_set_text(s_bag_txt, buf);
    snprintf(buf, sizeof(buf), DT_HAM_N, (unsigned)g->hammers);
    uk_set_text(s_ham_txt, buf);
    // 操作提示轮播
    static const char *const HINTS[3] = {DT_HINT_DROP, DT_HINT_COLLECT, DT_HINT_MENU};
    uk_set_text(s_hint_txt, HINTS[(g->anim_ms / 5000u) % 3u]);
}

// ---------------------------------------------------------------- 功能菜单浮层

static lv_obj_t *s_menu_root;
static lv_obj_t *s_menu_hl;
static lv_obj_t *s_menu_desc;
static const char *const MENU_ITEMS[SW_MENU_N] = {DT_M_CONT, DT_SHOP, DT_BAG, DT_M_COLLECT,
                                                  DT_M_HAMMER, DT_M_TITLE, DT_MENU};
static const char *const MENU_DESCS[SW_MENU_N] = {DT_MD_CONT, DT_MD_SHOP, DT_MD_BAG, DT_MD_COLLECT,
                                                  DT_MD_HAMMER, DT_MD_TITLE, DT_MD_EXIT};

void slipperworks_ui_menu(uint8_t sel) {
    s_menu_root = uk_panel(lv_screen_active(), 40, 44, 160, 236);
    lv_obj_t *p = s_menu_root;
    centered(p, UK_F24, UK_INK, DT_FUNC, 6);
    s_menu_hl = uk_rect(p, 6, 0, 148, 26, UK_WAINSCOT);
    for (int i = 0; i < SW_MENU_N; i++) {
        lv_obj_t *row = uk_text(p, UK_F12, UK_INK, MENU_ITEMS[i]);
        lv_obj_set_pos(row, 22, 42 + i * 26);
    }
    s_menu_desc = uk_text(p, UK_F12, UK_GRAY, "");
    lv_obj_set_pos(s_menu_desc, 22, 216);
    slipperworks_ui_menu_update(sel);
}

void slipperworks_ui_menu_update(uint8_t sel) {
    if (!s_menu_root) return;
    lv_obj_set_pos(s_menu_hl, 6, 38 + sel * 26);
    uk_set_text(s_menu_desc, MENU_DESCS[sel]);
}

void slipperworks_ui_menu_hide(void) {
    if (s_menu_root) {
        lv_obj_del(s_menu_root);
        s_menu_root = NULL;
    }
}

// ---------------------------------------------------------------- 鞋材铺浮层

#define SHOP_ROWS SW_SHOP_N

static lv_obj_t *s_shop_root;
static lv_obj_t *s_shop_hl;
static lv_obj_t *s_shop_bag_pill;
static lv_obj_t *s_shop_close_pill;
static lv_obj_t *s_shop_qtxt;

static lv_obj_t *s_shop_name[SHOP_ROWS];
static lv_obj_t *s_shop_price[SHOP_ROWS];

void slipperworks_ui_shop(uint8_t sel, uint8_t queue_n, uint8_t basket_lv) {
    s_shop_root = uk_panel(lv_screen_active(), 20, 32, 200, 252);
    lv_obj_t *p = s_shop_root;
    centered(p, UK_F24, UK_INK, DT_SHOP, 4);
    s_shop_qtxt = uk_text(p, UK_F12, UK_INK, "");
    lv_obj_align(s_shop_qtxt, LV_ALIGN_TOP_RIGHT, -10, 12);
    s_shop_hl = uk_rect(p, 6, 0, 188, 28, UK_WAINSCOT);
    for (int i = 0; i < SHOP_ROWS; i++) {
        int y = 40 + i * 30;
        const spr_art_t *a = i < 3 ? slipperworks_art_tier(slipperworks_shop_tier[i])
                                   : i == 3 ? &slipperworks_art_hammer
                                            : &slipperworks_art_basket;
        lv_obj_t *img = art_img(p, a, 256);
        art_cpos(img, a, 30, y + 12, 256);
        s_shop_name[i] = uk_text(p, UK_F12, UK_INK, "");
        lv_obj_set_pos(s_shop_name[i], 52, y + 6);
        s_shop_price[i] = uk_text(p, UK_F12, UK_INK, "");
        lv_obj_set_pos(s_shop_price[i], 132, y + 6);
    }
    s_shop_bag_pill = uk_pill(p, UK_F12, UK_LINE, UK_WHITE, DT_GO_BAG);
    lv_obj_align(s_shop_bag_pill, LV_ALIGN_TOP_LEFT, 14, 194);
    s_shop_close_pill = uk_pill(p, UK_F12, UK_LINE, UK_WHITE, DT_SHOP_CLOSE);
    lv_obj_align(s_shop_close_pill, LV_ALIGN_TOP_RIGHT, -14, 194);
    centered(p, UK_F12, UK_INK, DT_SHOP_TIP, 226);
    slipperworks_ui_shop_update(sel, queue_n, basket_lv);
}

void slipperworks_ui_shop_update(uint8_t sel, uint8_t queue_n, uint8_t basket_lv) {
    if (!s_shop_root) return;
    char qb[16];
    snprintf(qb, sizeof(qb), DT_SHOP_QUEUE, (unsigned)queue_n);
    uk_set_text(s_shop_qtxt, qb);
    lv_obj_set_pos(s_shop_hl, 6, 36 + sel * 30);
    uk_show(s_shop_hl, sel < SHOP_ROWS);
    // 鞋材与大锤行的名和价
    for (int i = 0; i < 3; i++) {
        uk_set_text(s_shop_name[i], NAMES[slipperworks_shop_tier[i]]);
        char pb[16];
        snprintf(pb, sizeof(pb), DT_UNIT, (unsigned)slipperworks_shop_price[i]);
        uk_set_text(s_shop_price[i], pb);
    }
    uk_set_text(s_shop_name[3], DT_HAM_NAME);
    char hb[16];
    snprintf(hb, sizeof(hb), DT_UNIT, (unsigned)slipperworks_shop_price[3]);
    uk_set_text(s_shop_price[3], hb);
    // 篮子行:当前级 → 下一级;满级显示 MAX
    uint8_t lv = basket_lv;
    char bb[20];
    if (lv >= SW_BASKET_MAX) {
        uk_set_text(s_shop_name[4], DT_BASKET_MAX_ROW);
        uk_set_text(s_shop_price[4], "");
    } else {
        snprintf(bb, sizeof(bb), DT_BASKET_ROW, (unsigned)lv, (unsigned)(lv + 1));
        uk_set_text(s_shop_name[4], bb);
        char pp[16];
        snprintf(pp, sizeof(pp), DT_UNIT, (unsigned)slipperworks_basket_price[lv - 1]);
        uk_set_text(s_shop_price[4], pp);
    }
    lv_obj_set_style_bg_color(s_shop_bag_pill,
                              lv_color_hex(sel == SW_SHOP_BAG ? UK_RED : UK_LINE), 0);
    lv_obj_set_style_bg_color(s_shop_close_pill,
                              lv_color_hex(sel == SW_SHOP_CLOSE ? UK_RED : UK_LINE), 0);
}

void slipperworks_ui_shop_hide(void) {
    if (s_shop_root) {
        lv_obj_del(s_shop_root);
        s_shop_root = NULL;
    }
}

// ---------------------------------------------------------------- 失败页

void slipperworks_ui_fail(uint16_t lost_bin, uint16_t lost_bag) {
    lv_obj_t *scr = uk_screen_new(UK_WALL);
    centered(scr, UK_F36, UK_RED, DT_FAIL_TITLE, 52);
    centered(scr, UK_F12, UK_INK, DT_FAIL_SUB, 118);
    char buf[40];
    snprintf(buf, sizeof(buf), DT_FAIL_LOST, (unsigned)lost_bin, (unsigned)lost_bag);
    centered(scr, UK_F24, UK_INK, buf, 150);
    lv_obj_t *kid = uk_sprite(scr, SPR_KID, 2);
    uk_spos(kid, (UK_W - 32) / 2, 196);
    lv_obj_t *pill = uk_pill(scr, UK_F24, UK_RED, UK_WHITE, DT_FAIL_AGAIN);
    lv_obj_align(pill, LV_ALIGN_TOP_MID, 0, 250);
    uk_screen_swap(scr);
}

// ---------------------------------------------------------------- 「新的工坊」确认页

void slipperworks_ui_confirm(uint8_t stage) {
    lv_obj_t *scr = uk_screen_new(UK_WALL);
    centered(scr, UK_F36, UK_RED, DT_NW_TITLE, 44);
    centered(scr, UK_F12, UK_INK, DT_NW_W1, 108);
    centered(scr, UK_F12, UK_RED, DT_NW_W2, 130);
    centered(scr, UK_F12, UK_RED, DT_NW_W3, 152);
    lv_obj_t *pill = uk_pill(scr, UK_F24, UK_RED, UK_WHITE, stage == 1 ? DT_NW_C1 : DT_NW_C2);
    lv_obj_align(pill, LV_ALIGN_TOP_MID, 0, 200);
    centered(scr, UK_F12, UK_INK, DT_NW_CANCEL, 262);
    uk_screen_swap(scr);
}

// ---------------------------------------------------------------- 剧情播放器

static lv_obj_t *s_st_back;   // 全页背景板(浮层模式没有)
static lv_obj_t *s_st_face;
static lv_obj_t *s_st_tag;
static lv_obj_t *s_st_panel;
static lv_obj_t *s_st_text;
static char s_st_buf[200];    // 当前整句
static uint16_t s_st_len, s_st_shown;
static uint32_t s_st_t;
static uint32_t s_st_voice_ms;  // 这句的配音时长(0 = 无配音,按 48ms/字打)
static uint32_t s_st_silence;   // 开头静默(上一句语音还在放)
static uint8_t s_st_pending;    // 静默期过后要播的配音
static const kit_line_t *s_st_voice;

// 台词按 UTF-8 字符边界截断
static void utf8_cut(char *s, uint32_t bytes) {
    uint32_t i = bytes;
    while (i > 0 && (((unsigned char)s[i]) & 0xC0u) == 0x80u) i--;
    s[i] = 0;
}

static uint8_t s_story_full;  // 当前故事是否全页(开场)
static void story_build(lv_obj_t *parent, const struct sw_story_step *st, uint8_t full_page);

void slipperworks_ui_story_page(const struct sw_story_step *st) {
    s_story_full = 1;
    lv_obj_t *scr = uk_screen_new(UK_WALL);
    centered(scr, UK_F24, UK_INK, DT_TITLE, 16);
    story_build(scr, st, 1);
    uk_screen_swap(scr);
}

void slipperworks_ui_story_modal(const struct sw_story_step *st) {
    s_story_full = 0;
    story_build(lv_screen_active(), st, 0);
}

static void story_build(lv_obj_t *parent, const struct sw_story_step *st, uint8_t full_page) {
    s_st_silence = st->voice ? 250u : 0u;  // 开头静默一下:上一句语音还在放,别顶掉
    if (full_page) s_st_back = uk_rect(parent, 0, 56, UK_W, UK_H - 56, UK_WALL);
    s_st_face = uk_sprite(parent, st->who == SW_W_KUBA ? SPR_KID : SPR_GRANNY, 3);
    uk_spos(s_st_face, (UK_W - 48) / 2, full_page ? 64 : 104);
    s_st_panel = uk_panel(parent, 8, 172, 224, 112);
    lv_obj_clear_flag(s_st_panel, LV_OBJ_FLAG_SCROLLABLE);
    uint32_t tag_col = st->who == SW_W_KUBA ? UK_BLUE : st->who == SW_W_GRANNY ? UK_RED : UK_GRAY;
    s_st_tag = uk_pill(s_st_panel, UK_F12, tag_col, UK_WHITE,
                       st->who == SW_W_KUBA ? DT_W_KUBA : st->who == SW_W_GRANNY ? DT_W_GRANNY : DT_W_NONE);
    lv_obj_align(s_st_tag, LV_ALIGN_TOP_LEFT, 8, 6);
    s_st_text = uk_text(s_st_panel, UK_F12, UK_INK, "");
    lv_obj_set_pos(s_st_text, 12, 30);
// 台词已手动分行,标签自动尺寸
    uk_set_text(s_st_text, "");
    strncpy(s_st_buf, st->text, sizeof(s_st_buf) - 1);
    s_st_buf[sizeof(s_st_buf) - 1] = 0;
    s_st_len = (uint16_t)strlen(s_st_buf);
    s_st_shown = 0;
    s_st_t = 0;
    s_st_voice = st->voice;
    // 声音文字同步:有配音时,文字显示速度 = 台词长度 ÷ 配音时长(语音播完,字也显完)
    s_st_voice_ms = st->voice ? st->voice->samples / 16u : 0u;
    s_st_pending = s_st_voice ? 1u : 0u;  // 静默期过了再播,别顶掉上一句
}

void slipperworks_ui_story_step(const struct sw_story_step *st) {
    // 捋一遍旧对象再建新的(头像/说话人/文本都可能变)
    if (s_st_back) { lv_obj_del(s_st_back); s_st_back = NULL; }
    if (s_st_panel) { lv_obj_del(s_st_panel); s_st_panel = NULL; }
    if (s_st_face) { lv_obj_del(s_st_face); s_st_face = NULL; }
    story_build(lv_screen_active(), st, s_story_full);
}

bool slipperworks_ui_story_tick(uint32_t dt) {
    if (!s_st_panel) return true;
    s_st_t += dt;
    if (s_st_pending) {  // 静默期结束:现在播这句的配音
        s_st_pending = 0;
        if (s_st_voice) sfx_play_line(s_st_voice, SFX_NONE, false);
    }
    // 有配音:按配音时长等比例出字(语音播完,字也显完);没配音:48ms/字
    // 出字时间从静默期之后起算(不然静默期文字会跑到声音前面)
    uint32_t eff = s_st_t > s_st_silence ? s_st_t - s_st_silence : 0;
    uint32_t dur = s_st_voice_ms ? s_st_voice_ms : (uint32_t)s_st_len * 48u;
    uint32_t want = s_st_len;
    if (eff < dur) want = (uint32_t)((uint64_t)s_st_len * eff / dur);
    if (want < s_st_len) {
        char buf[200];
        strncpy(buf, s_st_buf, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;
        utf8_cut(buf, want);
        uk_set_text(s_st_text, buf);
        return false;
    }
    uk_set_text(s_st_text, s_st_buf);
    return true;
}

void slipperworks_ui_story_finish(void) {
    s_st_shown = s_st_len;
    // 时钟直接跳到"播完":有配音的句子打字按配音时长走,比 len*48 慢,
    // 时钟若设回 len*48 会倒退,文字就缩回去重打(看起来像往回退)
    uint32_t dur = s_st_voice_ms ? s_st_voice_ms : (uint32_t)s_st_len * 48u;
    s_st_t = s_st_silence + dur;
    uk_set_text(s_st_text, s_st_buf);
}

bool slipperworks_ui_story_done(void) {
    if (!s_st_panel) return true;
    uint32_t dur = s_st_voice_ms ? s_st_voice_ms : (uint32_t)s_st_len * 48u;
    return s_st_t >= s_st_silence + dur;
}

// 细节教学提示:语音 + 顶部提示条
void slipperworks_ui_tip_voice(const struct sw_story_step *st) {
    if (st->voice) sfx_play_line(st->voice, SFX_NONE, false);
}

void slipperworks_ui_story_hide(void) {
    if (s_st_back) { lv_obj_del(s_st_back); s_st_back = NULL; }
    if (s_st_panel) { lv_obj_del(s_st_panel); s_st_panel = NULL; }
    if (s_st_face) { lv_obj_del(s_st_face); s_st_face = NULL; }
    s_st_len = s_st_shown = 0;
}

// ---------------------------------------------------------------- 背包页

#define SW_BAG_ROWS (SW_TIERS - SW_COLLECT_TIER)  // 8 行:小布拖..回旋拖鞋
#define SW_BAG_SEL_BACK SW_BAG_ROWS                  // 最后一格 = 回标题

static lv_obj_t *s_bg_hl;
static lv_obj_t *s_bg_price[SW_BAG_ROWS];
static lv_obj_t *s_bg_lv[SW_BAG_ROWS];
static lv_obj_t *s_bg_cnt[SW_BAG_ROWS];
static lv_obj_t *s_bg_back;
static lv_obj_t *s_bg_mat;
static lv_obj_t *s_bag_tip;

void slipperworks_ui_bag(uint8_t sel, const slipperworks_save_t *sv) {
    lv_obj_t *scr = uk_screen_new(UK_WALL);
    lv_obj_t *bt = uk_text(scr, UK_F24, UK_INK, DT_BAG);
    lv_obj_align(bt, LV_ALIGN_TOP_LEFT, 8, 6);
    lv_obj_t *mi = art_img(scr, &slipperworks_art_spool, 384);  // 12x10 → 18x15
    art_cpos(mi, &slipperworks_art_spool, 140, 20, 384);
    s_bg_mat = uk_text(scr, UK_F24, UK_INK, "");
    lv_obj_align(s_bg_mat, LV_ALIGN_TOP_RIGHT, -6, 8);  // 贴右:数字再长也压不到线卷头上

    s_bg_hl = uk_rect(scr, 8, 0, 224, 25, UK_WAINSCOT);
    for (int i = 0; i < SW_BAG_ROWS; i++) {
        uint8_t tier = (uint8_t)(i + SW_COLLECT_TIER);
        int y = 44 + i * 27;
        const spr_art_t *a = slipperworks_art_tier(tier);
        lv_obj_t *img = art_img(scr, a, 256);
        art_cpos(img, a, 24, y + 12, 256);
        lv_obj_t *name = uk_text(scr, UK_F12, UK_INK, NAMES[tier]);
        lv_obj_set_pos(name, 44, y + 6);
        s_bg_cnt[i] = uk_text(scr, UK_F12, UK_INK, "");
        lv_obj_set_pos(s_bg_cnt[i], 100, y + 6);
        s_bg_lv[i] = uk_text(scr, UK_F12, UK_INK, "");
        lv_obj_set_pos(s_bg_lv[i], 134, y + 6);
        s_bg_price[i] = uk_text(scr, UK_F12, UK_INK, "");
        lv_obj_set_pos(s_bg_price[i], 170, y + 6);
    }
    s_bg_back = uk_pill(scr, UK_F12, sel == SW_BAG_SEL_BACK ? UK_RED : UK_LINE, UK_WHITE, DT_BAG_BACK);
    lv_obj_align(s_bg_back, LV_ALIGN_TOP_MID, 0, 264);
    s_bag_tip = centered(scr, UK_F12, UK_INK, "", UK_H - 20);
    uk_screen_swap(scr);
    slipperworks_ui_bag_update(sel, sv);
}

void slipperworks_ui_bag_update(uint8_t sel, const slipperworks_save_t *sv) {
    char buf[64];  // 提示文案一行 ~30 字节,16 会拦腰截断(此前"长按● 卖光"就是被它吃了)
    for (int i = 0; i < SW_BAG_ROWS; i++) {
        uint8_t tier = (uint8_t)(i + SW_COLLECT_TIER);
        snprintf(buf, sizeof(buf), DT_COUNT, (unsigned)sv->bag[tier]);
        uk_set_text(s_bg_cnt[i], buf);
        uint8_t up = slipperworks_up_of(sv, tier);
        snprintf(buf, sizeof(buf), DT_LV, (unsigned)up);
        uk_set_text(s_bg_lv[i], buf);
        snprintf(buf, sizeof(buf), DT_UNIT, (unsigned)slipperworks_price_of(sv, tier));
        uk_set_text(s_bg_price[i], buf);
        uint32_t col = up ? UK_GOLD : UK_INK;
        lv_obj_set_style_text_color(s_bg_lv[i], lv_color_hex(col), 0);
        lv_obj_set_style_text_color(s_bg_price[i], lv_color_hex(col), 0);
    }
    mat_text_plain(buf, sizeof(buf), slipperworks_mat_of(sv));
    uk_set_text(s_bg_mat, buf);
    // 底部提示随选中行变化:行里有货先教卖光,空行教升级,然后满级 / 返回行
    if (sel < SW_BAG_SEL_BACK) {
        uint8_t tier = (uint8_t)(sel + SW_COLLECT_TIER);
        if (sv->bag[tier] > 0) {
            uk_set_text(s_bag_tip, DT_BAG_TIP_SELL);
        } else if (slipperworks_up_of(sv, tier) < SW_UP_MAX) {
            snprintf(buf, sizeof(buf), DT_BAG_TIP_UP,
                     (unsigned)slipperworks_upgrade_cost(sv, tier));
            uk_set_text(s_bag_tip, buf);
        } else {
            uk_set_text(s_bag_tip, DT_BAG_TIP_MAX);
        }
    } else {
        uk_set_text(s_bag_tip, DT_BAG_TIP_BACK);
    }
    if (sel < SW_BAG_SEL_BACK) {
        lv_obj_set_y(s_bg_hl, 43 + sel * 27);
        uk_show(s_bg_hl, true);
    } else {
        uk_show(s_bg_hl, false);
    }
    lv_obj_set_style_bg_color(s_bg_back, lv_color_hex(sel == SW_BAG_SEL_BACK ? UK_RED : UK_LINE), 0);
}
