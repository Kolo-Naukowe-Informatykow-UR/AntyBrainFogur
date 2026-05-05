/*
 * kni_ui — AntyBrainFogUR UI component for ESP32-S3 Waveshare 27690
 *
 * Screen: 240 × 320 px (portrait)
 * Layout: top bar (28) | scroll panel (234) | bottom tab bar (58)
 *
 * Palette: Deep Carbon / Slate Panel / Frost White / Mint Signal / Sky Hint
 */
#include "kni_ui/kni_ui.h"
#include "bsp/bsp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include <string.h>

static const char *TAG = "kni_ui";

/* ── Runtime state ──────────────────────────────────────────────────────── */
static const kni_ui_cfg_t *s_cfg         = NULL;
static lv_obj_t            *s_splash     = NULL;
static lv_obj_t            *s_main       = NULL;
static lv_obj_t            *s_panels    [KNI_UI_MAX_TABS];
static lv_obj_t            *s_tab_btns  [KNI_UI_MAX_TABS];
static lv_obj_t            *s_tab_lbls  [KNI_UI_MAX_TABS];
static int                  s_active_tab = 0;

/* ════════════════════════════════════════════════════════════════════════
 *  PRIVATE HELPERS
 * ════════════════════════════════════════════════════════════════════════ */

static lv_obj_t *mk_label(lv_obj_t *parent, const char *txt,
                           uint32_t col, const lv_font_t *font)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
    if (font) lv_obj_set_style_text_font(l, font, 0);
    return l;
}

static void apply_card_style(lv_obj_t *o)
{
    lv_obj_set_style_bg_color(o,     lv_color_hex(KNI_UI_C_CARD),   0);
    lv_obj_set_style_bg_opa(o,       LV_OPA_COVER,                  0);
    lv_obj_set_style_border_color(o, lv_color_hex(KNI_UI_C_BORDER), 0);
    lv_obj_set_style_border_width(o, 1,                              0);
    lv_obj_set_style_radius(o,       8,                              0);
    lv_obj_set_style_pad_all(o,      10,                             0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
}

/*
 * Scrollable content panel — 240×234 px, between top bar and tab bar.
 * Uses flex-column so children stack vertically without overlapping.
 */
static lv_obj_t *make_panel(lv_obj_t *scr)
{
    lv_obj_t *p = lv_obj_create(scr);
    lv_obj_set_size(p, 240, 234);
    lv_obj_set_pos(p, 0, 28);
    lv_obj_set_style_bg_color(p,     lv_color_hex(KNI_UI_C_BG), 0);
    lv_obj_set_style_bg_opa(p,       LV_OPA_COVER,              0);
    lv_obj_set_style_border_width(p, 0,                         0);
    lv_obj_set_style_radius(p,       0,                         0);
    lv_obj_set_style_pad_all(p,      10,                        0);
    lv_obj_set_style_pad_row(p,      8,                         0);   /* gap between children */
    lv_obj_set_scroll_dir(p, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(p, LV_SCROLLBAR_MODE_ACTIVE);
    /* Flex column: children stack top-to-bottom, no overlap */
    lv_obj_set_layout(p, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    return p;
}

/* ── Tab switching ──────────────────────────────────────────────────────── */
static void switch_tab(int idx)
{
    if (idx == s_active_tab) return;

    /* deactivate old */
    lv_obj_set_style_border_width(s_tab_btns[s_active_tab], 0, 0);
    lv_obj_set_style_text_color(s_tab_lbls[s_active_tab],
                                lv_color_hex(KNI_UI_C_INACTIVE), 0);
    lv_obj_add_flag(s_panels[s_active_tab], LV_OBJ_FLAG_HIDDEN);

    s_active_tab = idx;

    /* activate new */
    lv_obj_set_style_border_color(s_tab_btns[idx], lv_color_hex(KNI_UI_C_TEAL), 0);
    lv_obj_set_style_border_side(s_tab_btns[idx],  LV_BORDER_SIDE_TOP,          0);
    lv_obj_set_style_border_width(s_tab_btns[idx], 2,                           0);
    lv_obj_set_style_text_color(s_tab_lbls[idx],   lv_color_hex(KNI_UI_C_TEAL), 0);
    lv_obj_clear_flag(s_panels[idx], LV_OBJ_FLAG_HIDDEN);
}

static void on_tab_click(lv_event_t *e)
{
    switch_tab((int)(intptr_t)lv_event_get_user_data(e));
}

/* ════════════════════════════════════════════════════════════════════════
 *  MAIN SCREEN  (240 × 320)
 *  ┌────────────────────────┐  y=0
 *  │  top bar      28 px    │
 *  ├────────────────────────┤  y=28
 *  │                        │
 *  │  scroll panel 234 px   │
 *  │                        │
 *  ├────────────────────────┤  y=262
 *  │  tab bar       58 px   │
 *  └────────────────────────┘  y=320
 * ════════════════════════════════════════════════════════════════════════ */
static void build_main_screen(void)
{
    int n = s_cfg->tab_count;

    s_main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_main, lv_color_hex(KNI_UI_C_BG), 0);
    lv_obj_set_style_bg_opa(s_main,   LV_OPA_COVER,               0);
    lv_obj_clear_flag(s_main, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Top status bar ────────────────────────────────────────────────── */
    lv_obj_t *topbar = lv_obj_create(s_main);
    lv_obj_set_size(topbar, 240, 28);
    lv_obj_set_pos(topbar, 0, 0);
    lv_obj_set_style_bg_color(topbar,    lv_color_hex(KNI_UI_C_BAR),    0);
    lv_obj_set_style_bg_opa(topbar,      LV_OPA_COVER,                  0);
    lv_obj_set_style_border_color(topbar, lv_color_hex(KNI_UI_C_TEAL),  0);
    lv_obj_set_style_border_side(topbar,  LV_BORDER_SIDE_BOTTOM,        0);
    lv_obj_set_style_border_width(topbar, 1,                            0);
    lv_obj_set_style_radius(topbar,       0,                            0);
    lv_obj_set_style_pad_hor(topbar,     10,                            0);
    lv_obj_set_style_pad_ver(topbar,      0,                            0);
    lv_obj_clear_flag(topbar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *tl = mk_label(topbar, s_cfg->title,    KNI_UI_C_TEAL,    &lv_font_montserrat_14);
    lv_obj_set_align(tl, LV_ALIGN_LEFT_MID);
    lv_obj_t *tr = mk_label(topbar, s_cfg->subtitle, KNI_UI_C_SUBTEXT, &lv_font_montserrat_14);
    lv_obj_set_align(tr, LV_ALIGN_RIGHT_MID);

    /* ── Content panels — one per tab ──────────────────────────────────── */
    for (int i = 0; i < n; i++) {
        lv_obj_t *p = make_panel(s_main);
        if (i > 0) lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN);
        s_panels[i] = p;
        s_cfg->tabs[i].build(p);
    }

    /* ── Bottom tab bar ─────────────────────────────────────────────────── */
    int btn_w = 240 / n;
    lv_obj_t *menu = lv_obj_create(s_main);
    lv_obj_set_size(menu, 240, 58);
    lv_obj_set_pos(menu, 0, 262);
    lv_obj_set_style_bg_color(menu,     lv_color_hex(KNI_UI_C_BAR),    0);
    lv_obj_set_style_bg_opa(menu,       LV_OPA_COVER,                  0);
    lv_obj_set_style_border_color(menu, lv_color_hex(KNI_UI_C_BORDER), 0);
    lv_obj_set_style_border_side(menu,  LV_BORDER_SIDE_TOP,            0);
    lv_obj_set_style_border_width(menu, 1,                             0);
    lv_obj_set_style_radius(menu,       0,                             0);
    lv_obj_set_style_pad_all(menu,      0,                             0);
    lv_obj_clear_flag(menu, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < n; i++) {
        lv_obj_t *btn = lv_obj_create(menu);
        lv_obj_set_size(btn, btn_w, 58);
        lv_obj_set_pos(btn, i * btn_w, 0);
        lv_obj_set_style_bg_opa(btn,      LV_OPA_TRANSP,               0);
        lv_obj_set_style_bg_opa(btn,      LV_OPA_10,                   LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(btn,    lv_color_hex(KNI_UI_C_TEAL), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(btn, 0,                          0);
        lv_obj_set_style_radius(btn,       0,                          0);
        lv_obj_set_style_pad_all(btn,      0,                          0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(btn, on_tab_click, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        s_tab_btns[i] = btn;

        bool active = (i == 0);
        if (active) {
            lv_obj_set_style_border_color(btn, lv_color_hex(KNI_UI_C_TEAL), 0);
            lv_obj_set_style_border_side(btn,  LV_BORDER_SIDE_TOP,          0);
            lv_obj_set_style_border_width(btn, 2,                            0);
        }

        uint32_t col = active ? KNI_UI_C_TEAL : KNI_UI_C_INACTIVE;
        lv_obj_t *lbl = mk_label(btn, s_cfg->tabs[i].label, col, &lv_font_montserrat_14);
        lv_obj_center(lbl);
        s_tab_lbls[i] = lbl;
    }

    ESP_LOGI(TAG, "main screen ready (%d tab%s)", n, n == 1 ? "" : "s");
}

/* ════════════════════════════════════════════════════════════════════════
 *  SPLASH SCREEN  (240 × 320)
 *
 *  ┌────────────────────────┐  y=0   — 2 px Mint accent line
 *  │  [logo if set]         │
 *  │                        │
 *  │    KNI  ← Mint 24px    │  vertically centred in top ~half
 *  │    AntyBrainFogUR      │
 *  │    CO2 smart sensor    │
 *  │                        │
 *  │  ─────────────────     │  separator
 *  │  Initializing...       │
 *  │  [████████░░░░░░]      │  animated bar
 *  │  v0.1.0                │
 *  └────────────────────────┘  y=320 — 2 px Mint accent line
 * ════════════════════════════════════════════════════════════════════════ */
static void bar_anim_cb(void *var, int32_t val)
{
    lv_obj_set_width((lv_obj_t *)var, val);
}

static void on_splash_done(lv_anim_t *a)
{
    (void)a;
    build_main_screen();
    lv_screen_load_anim(s_main, LV_SCR_LOAD_ANIM_FADE_IN, 500, 0, true);
}

static void build_splash(void)
{
    s_splash = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_splash, lv_color_hex(KNI_UI_C_BG), 0);
    lv_obj_set_style_bg_opa(s_splash,   LV_OPA_COVER,               0);
    lv_obj_set_style_pad_all(s_splash,  0,                          0);
    lv_obj_clear_flag(s_splash, LV_OBJ_FLAG_SCROLLABLE);

    /* Top accent line */
    lv_obj_t *top_line = lv_obj_create(s_splash);
    lv_obj_set_size(top_line, 240, 2);
    lv_obj_set_pos(top_line, 0, 0);
    lv_obj_set_style_bg_color(top_line,    lv_color_hex(KNI_UI_C_TEAL), 0);
    lv_obj_set_style_bg_opa(top_line,      LV_OPA_COVER,                0);
    lv_obj_set_style_border_width(top_line, 0,                          0);

    /* Bottom accent line */
    lv_obj_t *bot_line = lv_obj_create(s_splash);
    lv_obj_set_size(bot_line, 240, 2);
    lv_obj_set_pos(bot_line, 0, 318);
    lv_obj_set_style_bg_color(bot_line,    lv_color_hex(KNI_UI_C_TEAL), 0);
    lv_obj_set_style_bg_opa(bot_line,      LV_OPA_COVER,                0);
    lv_obj_set_style_border_width(bot_line, 0,                          0);

    /* ── Logo (optional) ─────────────────────────────────────────────── */
    int content_top = 40;
    if (s_cfg->logo) {
        lv_obj_t *img = lv_image_create(s_splash);
        lv_image_set_src(img, s_cfg->logo);
        lv_obj_align(img, LV_ALIGN_TOP_MID, 0, content_top);
        content_top += (int)s_cfg->logo->header.h + 12;
    }

    /* ── Title ───────────────────────────────────────────────────────── */
    lv_obj_t *title = lv_label_create(s_splash);
    lv_label_set_text(title, s_cfg->title);
    lv_obj_set_style_text_color(title, lv_color_hex(KNI_UI_C_TEAL), 0);
    lv_obj_set_style_text_font(title,  &lv_font_montserrat_24,       0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, content_top);

    /* ── Subtitle ────────────────────────────────────────────────────── */
    lv_obj_t *sub = lv_label_create(s_splash);
    lv_label_set_text(sub, s_cfg->subtitle);
    lv_obj_set_style_text_color(sub, lv_color_hex(KNI_UI_C_TEXT), 0);
    lv_obj_set_style_text_font(sub,  &lv_font_montserrat_16,       0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, content_top + 34);

    /* ── Tagline ─────────────────────────────────────────────────────── */
    lv_obj_t *tag = lv_label_create(s_splash);
    lv_label_set_text(tag, "Smart CO2 Monitor");
    lv_obj_set_style_text_color(tag, lv_color_hex(KNI_UI_C_SUBTEXT), 0);
    lv_obj_set_style_text_font(tag,  &lv_font_montserrat_14,          0);
    lv_obj_align(tag, LV_ALIGN_TOP_MID, 0, content_top + 58);

    /* ── Separator ───────────────────────────────────────────────────── */
    lv_obj_t *sep = lv_obj_create(s_splash);
    lv_obj_set_size(sep, 120, 1);
    lv_obj_align(sep, LV_ALIGN_BOTTOM_MID, 0, -62);
    lv_obj_set_style_bg_color(sep,     lv_color_hex(KNI_UI_C_BORDER), 0);
    lv_obj_set_style_bg_opa(sep,       LV_OPA_COVER,                  0);
    lv_obj_set_style_border_width(sep, 0,                             0);

    /* ── "Initializing..." label ─────────────────────────────────────── */
    lv_obj_t *init_lbl = lv_label_create(s_splash);
    lv_label_set_text(init_lbl, "Initializing...");
    lv_obj_set_style_text_color(init_lbl, lv_color_hex(KNI_UI_C_SUBTEXT), 0);
    lv_obj_set_style_text_font(init_lbl,  &lv_font_montserrat_14,          0);
    lv_obj_align(init_lbl, LV_ALIGN_BOTTOM_MID, 0, -46);

    /* ── Loading bar background ──────────────────────────────────────── */
    lv_obj_t *bar_bg = lv_obj_create(s_splash);
    lv_obj_set_size(bar_bg, 180, 4);
    lv_obj_align(bar_bg, LV_ALIGN_BOTTOM_MID, 0, -26);
    lv_obj_set_style_bg_color(bar_bg,     lv_color_hex(KNI_UI_C_BORDER), 0);
    lv_obj_set_style_bg_opa(bar_bg,       LV_OPA_COVER,                  0);
    lv_obj_set_style_border_width(bar_bg, 0,                             0);
    lv_obj_set_style_radius(bar_bg,       2,                             0);
    lv_obj_set_style_pad_all(bar_bg,      0,                             0);
    lv_obj_clear_flag(bar_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Loading bar fill (animated) ─────────────────────────────────── */
    lv_obj_t *bar_fill = lv_obj_create(bar_bg);
    lv_obj_set_size(bar_fill, 0, 4);
    lv_obj_set_pos(bar_fill, 0, 0);
    lv_obj_set_style_bg_color(bar_fill,     lv_color_hex(KNI_UI_C_TEAL), 0);
    lv_obj_set_style_bg_opa(bar_fill,       LV_OPA_COVER,                0);
    lv_obj_set_style_border_width(bar_fill, 0,                           0);
    lv_obj_set_style_radius(bar_fill,       2,                           0);

    /* ── Version ─────────────────────────────────────────────────────── */
    lv_obj_t *ver = lv_label_create(s_splash);
    lv_label_set_text(ver, "v0.1.0");
    lv_obj_set_style_text_color(ver, lv_color_hex(KNI_UI_C_TEAL_DIM), 0);
    lv_obj_set_style_text_font(ver,  &lv_font_montserrat_14,            0);
    lv_obj_align(ver, LV_ALIGN_BOTTOM_MID, 0, -8);

    /* ── Animate bar → trigger main screen when done ─────────────────── */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, bar_fill);
    lv_anim_set_exec_cb(&a, bar_anim_cb);
    lv_anim_set_values(&a, 0, 180);
    lv_anim_set_duration(&a, (int32_t)s_cfg->splash_ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_completed_cb(&a, on_splash_done);
    lv_anim_start(&a);

    lv_screen_load(s_splash);
    ESP_LOGI(TAG, "splash screen loaded (%lu ms)", (unsigned long)s_cfg->splash_ms);
}

/* ════════════════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ════════════════════════════════════════════════════════════════════════ */

esp_err_t kni_ui_start(const kni_ui_cfg_t *cfg)
{
    if (!cfg || cfg->tab_count < 1 || cfg->tab_count > KNI_UI_MAX_TABS) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!cfg->title || !cfg->subtitle || !cfg->tabs) {
        return ESP_ERR_INVALID_ARG;
    }

    s_cfg        = cfg;
    s_active_tab = 0;
    memset(s_panels,   0, sizeof(s_panels));
    memset(s_tab_btns, 0, sizeof(s_tab_btns));
    memset(s_tab_lbls, 0, sizeof(s_tab_lbls));

    build_splash();
    return ESP_OK;
}

/* ── Content helpers ────────────────────────────────────────────────────── */

lv_obj_t *kni_ui_card(lv_obj_t *panel, const char *title)
{
    lv_obj_t *c = lv_obj_create(panel);
    lv_obj_set_width(c,  lv_pct(100));
    lv_obj_set_height(c, LV_SIZE_CONTENT);
    apply_card_style(c);
    lv_obj_set_layout(c, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c, 5, 0);
    mk_label(c, title, KNI_UI_C_TEAL, &lv_font_montserrat_14);
    return c;
}

lv_obj_t *kni_ui_kv_row(lv_obj_t *card, const char *key, const char *value)
{
    lv_obj_t *row = lv_obj_create(card);
    lv_obj_set_width(row,  lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row,      LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0,            0);
    lv_obj_set_style_pad_all(row,      2,            0);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                               LV_FLEX_ALIGN_CENTER,
                               LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);

    mk_label(row, key,   KNI_UI_C_SUBTEXT, &lv_font_montserrat_14);
    lv_obj_t *val = mk_label(row, value, KNI_UI_C_TEXT, &lv_font_montserrat_14);
    return val;
}

lv_obj_t *kni_ui_text(lv_obj_t *card, const char *text, uint32_t color)
{
    lv_obj_t *l = mk_label(card, text, color, &lv_font_montserrat_14);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_text_line_space(l, 3, 0);
    return l;
}

lv_obj_t *kni_ui_col(lv_obj_t *panel)
{
    lv_obj_t *col = lv_obj_create(panel);
    lv_obj_set_width(col,  lv_pct(100));
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(col,      LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0,            0);
    lv_obj_set_style_pad_all(col,      0,            0);
    lv_obj_set_style_pad_row(col,      8,            0);
    lv_obj_set_layout(col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_CLICKABLE);
    return col;
}

lv_obj_t *kni_ui_btn(lv_obj_t *parent, const char *label,
                     lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_width(btn,  lv_pct(100));
    lv_obj_set_height(btn, 48);
    lv_obj_set_style_bg_color(btn,     lv_color_hex(KNI_UI_C_TEAL),     0);
    lv_obj_set_style_bg_color(btn,     lv_color_hex(KNI_UI_C_TEAL_DIM), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn,       8,                               0);
    lv_obj_set_style_border_width(btn, 0,                               0);
    lv_obj_set_style_shadow_width(btn, 0,                               0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_hex(KNI_UI_C_BG),        0);
    lv_obj_set_style_text_font(lbl,  &lv_font_montserrat_14,            0);
    lv_obj_center(lbl);

    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    return btn;
}
