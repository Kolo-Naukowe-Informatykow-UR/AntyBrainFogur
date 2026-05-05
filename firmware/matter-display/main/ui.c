#include "ui.h"
#include "kni_logo.h"
#include "bsp/bsp.h"
#include "lvgl.h"
#include "esp_log.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "ui";

/* ── Palette — AntyBrainFogUR ─────────────────────────────────────────── */
#define C_BG       0x0A0E12   /* Deep Carbon   — background               */
#define C_SURFACE  0x13181F   /* Slate Panel   — cards / surface          */
#define C_TEXT     0xE8EEF2   /* Frost White   — foreground / text        */
#define C_MUTED    0x6B7E8F   /* muted text                               */
#define C_TEAL     0x5EE6C1   /* Mint Signal   — primary accent           */
#define C_TEAL_DIM 0x3BA88A   /* dimmed teal                              */
#define C_BLUE     0x7AA8FF   /* Sky Hint      — secondary accent         */
#define C_ALERT    0xFF8A6B   /* CO2 High      — alert / warning          */
#define C_BORDER   0x1C2530   /* border / divider                         */
#define C_BAR      0x0D1318   /* top/status bar bg                        */
#define C_INACTIVE 0x3D5068   /* inactive / disabled                      */

/* ── Screen state ─────────────────────────────────────────────────────── */
typedef enum { SCR_SPLASH, SCR_CLOCK, SCR_DASHBOARD, SCR_CO2 } screen_t;

static screen_t  s_current   = SCR_SPLASH;
static lv_obj_t *s_scr_clock = NULL;
static lv_obj_t *s_scr_dash  = NULL;
static lv_obj_t *s_scr_co2   = NULL;

/* labels updated by timers / sensor */
static lv_obj_t *s_lbl_time   = NULL;
static lv_obj_t *s_lbl_date   = NULL;
static lv_obj_t *s_lbl_ppm    = NULL;
static lv_obj_t *s_lbl_status = NULL;
static lv_obj_t *s_ind_co2    = NULL;

static uint32_t  s_splash_ms  = 2500;

/* ── Navigation ──────────────────────────────────────────────────────── */
static void go_to(lv_obj_t *scr, screen_t id)
{
    lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
    s_current = id;
}

/* ── Generic helpers ─────────────────────────────────────────────────── */
static lv_obj_t *mk_label(lv_obj_t *parent, const char *txt,
                           uint32_t col, const lv_font_t *font)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
    if (font) lv_obj_set_style_text_font(l, font, 0);
    return l;
}

static lv_obj_t *mk_screen(void)
{
    lv_obj_t *s = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s, 0, 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    return s;
}

static lv_obj_t *mk_accent_line(lv_obj_t *parent, int y)
{
    lv_obj_t *l = lv_obj_create(parent);
    lv_obj_set_size(l, 240, 2);
    lv_obj_set_pos(l, 0, y);
    lv_obj_set_style_bg_color(l, lv_color_hex(C_TEAL), 0);
    lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(l, 0, 0);
    lv_obj_set_style_radius(l, 0, 0);
    return l;
}

static lv_obj_t *mk_topbar(lv_obj_t *scr, const char *left, const char *right)
{
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, 240, 28);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(C_BAR), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(bar, lv_color_hex(C_TEAL), 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(bar, 1, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_hor(bar, 10, 0);
    lv_obj_set_style_pad_ver(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *l = mk_label(bar, left,  C_TEAL,   &lv_font_montserrat_14);
    lv_obj_set_align(l, LV_ALIGN_LEFT_MID);
    lv_obj_t *r = mk_label(bar, right, C_MUTED, &lv_font_montserrat_14);
    lv_obj_set_align(r, LV_ALIGN_RIGHT_MID);
    return bar;
}

/* ════════════════════════════════════════════════════════════════════════
 *  SPLASH SCREEN
 * ════════════════════════════════════════════════════════════════════════ */
static void bar_anim_cb(void *var, int32_t val)
{
    lv_obj_set_width((lv_obj_t *)var, val);
}

static void on_splash_done(lv_anim_t *a);  /* forward decl */

static void build_splash(lv_obj_t *scr)
{
    mk_accent_line(scr, 0);
    mk_accent_line(scr, 318);

    /* ── Wordmark: "Anty" + "Brain"(teal) + "Fog" + "UR"(teal) ──────── */
    lv_obj_t *wm = lv_obj_create(scr);
    lv_obj_set_width(wm, LV_SIZE_CONTENT);
    lv_obj_set_height(wm, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(wm, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wm, 0, 0);
    lv_obj_set_style_pad_all(wm, 0, 0);
    lv_obj_set_style_pad_column(wm, 0, 0);
    lv_obj_set_layout(wm, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(wm, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wm, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(wm, LV_OBJ_FLAG_SCROLLABLE);
    mk_label(wm, "Anty",  C_TEXT, &lv_font_montserrat_24);
    mk_label(wm, "Brain", C_TEAL, &lv_font_montserrat_24);
    mk_label(wm, "Fog",   C_TEXT, &lv_font_montserrat_24);
    mk_label(wm, "UR",    C_TEAL, &lv_font_montserrat_24);

#if KNI_LOGO_VALID
    /* Layout (screen 240x320, loading bar occupies bottom ~90px):
     * y=36  wordmark  (≈28px tall)
     * y=76  logo      (96px)
     * y=182 tag       (≈16px)                                        */
    lv_obj_align(wm, LV_ALIGN_TOP_MID, 0, 36);

    lv_obj_t *logo_img = lv_image_create(scr);
    lv_image_set_src(logo_img, &kni_logo);
    lv_obj_align(logo_img, LV_ALIGN_TOP_MID, 0, 76);

    lv_obj_t *tag = mk_label(scr, "CO2 CLARITY SENSOR", C_MUTED, &lv_font_montserrat_14);
    lv_obj_align(tag, LV_ALIGN_TOP_MID, 0, 182);
#else
    /* No logo — wordmark + tag centered in upper half */
    lv_obj_align(wm, LV_ALIGN_CENTER, 0, -24);

    lv_obj_t *tag = mk_label(scr, "CO2 CLARITY SENSOR", C_MUTED, &lv_font_montserrat_14);
    lv_obj_align(tag, LV_ALIGN_CENTER, 0, 8);
#endif

    /* ── Loading bar ─────────────────────────────────────────────────── */
    lv_obj_t *sep = lv_obj_create(scr);
    lv_obj_set_size(sep, 100, 1);
    lv_obj_align(sep, LV_ALIGN_BOTTOM_MID, 0, -64);
    lv_obj_set_style_bg_color(sep, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);

    lv_obj_t *init_l = mk_label(scr, "Initializing...", C_MUTED, &lv_font_montserrat_14);
    lv_obj_align(init_l, LV_ALIGN_BOTTOM_MID, 0, -48);

    lv_obj_t *bar_bg = lv_obj_create(scr);
    lv_obj_set_size(bar_bg, 180, 4);
    lv_obj_align(bar_bg, LV_ALIGN_BOTTOM_MID, 0, -28);
    lv_obj_set_style_bg_color(bar_bg, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_bg_opa(bar_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar_bg, 0, 0);
    lv_obj_set_style_radius(bar_bg, 2, 0);
    lv_obj_set_style_pad_all(bar_bg, 0, 0);
    lv_obj_clear_flag(bar_bg, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bar_fill = lv_obj_create(bar_bg);
    lv_obj_set_size(bar_fill, 0, 4);
    lv_obj_set_pos(bar_fill, 0, 0);
    lv_obj_set_style_bg_color(bar_fill, lv_color_hex(C_TEAL), 0);
    lv_obj_set_style_bg_opa(bar_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar_fill, 0, 0);
    lv_obj_set_style_radius(bar_fill, 2, 0);

    lv_obj_t *ver = mk_label(scr, "v0.1.0", C_INACTIVE, &lv_font_montserrat_14);
    lv_obj_align(ver, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, bar_fill);
    lv_anim_set_exec_cb(&a, bar_anim_cb);
    lv_anim_set_values(&a, 0, 180);
    lv_anim_set_duration(&a, (int32_t)s_splash_ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_completed_cb(&a, on_splash_done);
    lv_anim_start(&a);

    lv_screen_load(scr);
    ESP_LOGI(TAG, "splash loaded");
}

/* ════════════════════════════════════════════════════════════════════════
 *  CLOCK SCREEN
 * ════════════════════════════════════════════════════════════════════════ */
#define INACTIVITY_MS  (15 * 60 * 1000)

static void clock_tick_cb(lv_timer_t *t)
{
    (void)t;
    time_t now;
    struct tm ti;
    time(&now);
    localtime_r(&now, &ti);

    /* only redraw when visible content changes — avoid per-second LVGL dirty marks */
    static int prev_min = -1, prev_mday = -1;
    if (ti.tm_min == prev_min && ti.tm_mday == prev_mday) return;
    prev_min  = ti.tm_min;
    prev_mday = ti.tm_mday;

    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
    lv_label_set_text(s_lbl_time, buf);

    static const char *days[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
    snprintf(buf, sizeof(buf), "%s  %02d %s %04d",
             days[ti.tm_wday], ti.tm_mday,
             months[ti.tm_mon], ti.tm_year + 1900);
    lv_label_set_text(s_lbl_date, buf);
}

static void inactivity_cb(lv_timer_t *t)
{
    (void)t;
    if (s_current == SCR_CLOCK) return;
    if (lv_display_get_inactive_time(NULL) >= INACTIVITY_MS)
        go_to(s_scr_clock, SCR_CLOCK);
}

static void on_clock_tap(lv_event_t *e)
{
    (void)e;
    go_to(s_scr_dash, SCR_DASHBOARD);
}

static void build_clock_screen(void)
{
    s_scr_clock = mk_screen();
    lv_obj_add_event_cb(s_scr_clock, on_clock_tap, LV_EVENT_CLICKED, NULL);

    mk_accent_line(s_scr_clock, 0);
    mk_accent_line(s_scr_clock, 318);

    lv_obj_t *kni = mk_label(s_scr_clock, "KNI", C_TEAL, &lv_font_montserrat_14);
    lv_obj_set_pos(kni, 12, 10);

    /* HH:MM — big, centered */
    s_lbl_time = lv_label_create(s_scr_clock);
    lv_label_set_text(s_lbl_time, "00:00");
    lv_obj_set_style_text_color(s_lbl_time, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(s_lbl_time, &lv_font_montserrat_48, 0);
    lv_obj_align(s_lbl_time, LV_ALIGN_CENTER, 0, -16);

    /* date below time */
    s_lbl_date = lv_label_create(s_scr_clock);
    lv_label_set_text(s_lbl_date, "Mon  01 Jan 2026");
    lv_obj_set_style_text_color(s_lbl_date, lv_color_hex(C_MUTED), 0);
    lv_obj_set_style_text_font(s_lbl_date, &lv_font_montserrat_16, 0);
    lv_obj_align(s_lbl_date, LV_ALIGN_CENTER, 0, 40);

    lv_obj_t *hint = mk_label(s_scr_clock, "tap to open", C_INACTIVE, &lv_font_montserrat_14);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -14);

    lv_timer_create(clock_tick_cb,  1000, NULL);
    lv_timer_create(inactivity_cb,  5000, NULL);
    clock_tick_cb(NULL);
}

/* ════════════════════════════════════════════════════════════════════════
 *  DASHBOARD SCREEN
 * ════════════════════════════════════════════════════════════════════════ */
static void on_co2_tile(lv_event_t *e) { (void)e; go_to(s_scr_co2, SCR_CO2); }

static lv_obj_t *mk_tile(lv_obj_t *parent, const char *icon,
                          const char *label, bool active, lv_event_cb_t cb)
{
    lv_obj_t *t = lv_obj_create(parent);
    lv_obj_set_size(t, 104, 104);
    lv_obj_set_style_bg_color(t, lv_color_hex(C_SURFACE), 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(t, lv_color_hex(active ? C_TEAL : C_BORDER), 0);
    lv_obj_set_style_border_width(t, 1, 0);
    lv_obj_set_style_radius(t, 12, 0);
    lv_obj_set_style_pad_all(t, 0, 0);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);

    if (active) {
        lv_obj_set_style_bg_color(t, lv_color_hex(C_TEAL), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(t, LV_OPA_20, LV_STATE_PRESSED);
    }

    lv_obj_t *ico = mk_label(t, icon, active ? C_TEAL : C_INACTIVE, &lv_font_montserrat_24);
    lv_obj_align(ico, LV_ALIGN_CENTER, 0, -14);

    lv_obj_t *lbl = mk_label(t, label, active ? C_TEXT : C_INACTIVE, &lv_font_montserrat_14);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lbl, 90);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 20);

    if (cb) lv_obj_add_event_cb(t, cb, LV_EVENT_CLICKED, NULL);
    return t;
}

static void build_dashboard_screen(void)
{
    s_scr_dash = mk_screen();
    mk_topbar(s_scr_dash, "KNI", "AntyBrainFogUR");

    /* 2x2 grid: tile=104, gap=8, total=216 → x_off=12, y_off=66 */
    int x0 = 12, y0 = 66, step = 112;

    lv_obj_t *t;
    t = mk_tile(s_scr_dash, "CO2", "CO2 Monitor", true,  on_co2_tile);
    lv_obj_set_pos(t, x0,        y0);

    t = mk_tile(s_scr_dash, "-", "Coming Soon", false, NULL);
    lv_obj_set_pos(t, x0 + step, y0);

    t = mk_tile(s_scr_dash, "-", "Coming Soon", false, NULL);
    lv_obj_set_pos(t, x0,        y0 + step);

    t = mk_tile(s_scr_dash, "-", "Coming Soon", false, NULL);
    lv_obj_set_pos(t, x0 + step, y0 + step);
}

/* ════════════════════════════════════════════════════════════════════════
 *  CO2 SCREEN
 * ════════════════════════════════════════════════════════════════════════ */
static void co2_apply_level(int16_t ppm)
{
    uint32_t col;
    const char *status;

    if (ppm < 0) {
        lv_label_set_text(s_lbl_ppm,    "---");
        lv_label_set_text(s_lbl_status, "NO SENSOR");
        lv_obj_set_style_text_color(s_lbl_ppm,    lv_color_hex(C_MUTED), 0);
        lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(C_MUTED), 0);
        lv_obj_set_style_bg_color(s_ind_co2, lv_color_hex(C_BORDER), 0);
        return;
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", ppm);
    lv_label_set_text(s_lbl_ppm, buf);

    if      (ppm < 800)  { col = C_TEAL;   status = "GOOD";    }
    else if (ppm < 1000) { col = 0xA8D060; status = "OK";      }
    else if (ppm < 1500) { col = 0xFFB347; status = "WARNING"; }
    else                 { col = C_ALERT;  status = "HIGH";    }

    lv_label_set_text(s_lbl_status, status);
    lv_obj_set_style_text_color(s_lbl_ppm,    lv_color_hex(col), 0);
    lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(col), 0);
    lv_obj_set_style_bg_color(s_ind_co2, lv_color_hex(col), 0);
}

static void on_co2_back(lv_event_t *e)
{
    (void)e;
    go_to(s_scr_dash, SCR_DASHBOARD);
}

static void build_co2_screen(void)
{
    s_scr_co2 = mk_screen();

    /* top bar with back */
    lv_obj_t *topbar = lv_obj_create(s_scr_co2);
    lv_obj_set_size(topbar, 240, 40);
    lv_obj_set_pos(topbar, 0, 0);
    lv_obj_set_style_bg_color(topbar, lv_color_hex(C_BAR), 0);
    lv_obj_set_style_bg_opa(topbar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(topbar, lv_color_hex(C_TEAL), 0);
    lv_obj_set_style_border_side(topbar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(topbar, 1, 0);
    lv_obj_set_style_radius(topbar, 0, 0);
    lv_obj_set_style_pad_hor(topbar, 8, 0);
    lv_obj_set_style_pad_ver(topbar, 0, 0);
    lv_obj_clear_flag(topbar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back = lv_button_create(topbar);
    lv_obj_set_size(back, 36, 30);
    lv_obj_set_style_bg_opa(back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(C_TEAL), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(back, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_set_style_shadow_width(back, 0, 0);
    lv_obj_set_style_radius(back, 6, 0);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(back, on_co2_back, LV_EVENT_CLICKED, NULL);
    lv_obj_t *blbl = mk_label(back, "<", C_TEAL, &lv_font_montserrat_16);
    lv_obj_center(blbl);

    lv_obj_t *title = mk_label(topbar, "CO2 Monitor", C_TEXT, &lv_font_montserrat_14);
    lv_obj_set_align(title, LV_ALIGN_CENTER);

    /* LIVE */
    lv_obj_t *live = mk_label(s_scr_co2, "LIVE", C_TEAL, &lv_font_montserrat_14);
    lv_obj_align(live, LV_ALIGN_TOP_MID, 0, 58);

    /* big ppm number */
    s_lbl_ppm = lv_label_create(s_scr_co2);
    lv_label_set_text(s_lbl_ppm, "---");
    lv_obj_set_style_text_color(s_lbl_ppm, lv_color_hex(C_MUTED), 0);
    lv_obj_set_style_text_font(s_lbl_ppm, &lv_font_montserrat_48, 0);
    lv_obj_align(s_lbl_ppm, LV_ALIGN_TOP_MID, 0, 74);

    lv_obj_t *unit = mk_label(s_scr_co2, "ppm", C_MUTED, &lv_font_montserrat_16);
    lv_obj_align(unit, LV_ALIGN_TOP_MID, 0, 132);

    /* status */
    s_lbl_status = lv_label_create(s_scr_co2);
    lv_label_set_text(s_lbl_status, "NO SENSOR");
    lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(C_MUTED), 0);
    lv_obj_set_style_text_font(s_lbl_status, &lv_font_montserrat_16, 0);
    lv_obj_align(s_lbl_status, LV_ALIGN_TOP_MID, 0, 158);

    /* color indicator bar */
    lv_obj_t *ind_bg = lv_obj_create(s_scr_co2);
    lv_obj_set_size(ind_bg, 200, 6);
    lv_obj_align(ind_bg, LV_ALIGN_TOP_MID, 0, 184);
    lv_obj_set_style_bg_color(ind_bg, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_bg_opa(ind_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ind_bg, 0, 0);
    lv_obj_set_style_radius(ind_bg, 3, 0);
    lv_obj_set_style_pad_all(ind_bg, 0, 0);
    lv_obj_clear_flag(ind_bg, LV_OBJ_FLAG_SCROLLABLE);

    s_ind_co2 = lv_obj_create(ind_bg);
    lv_obj_set_size(s_ind_co2, 200, 6);
    lv_obj_set_pos(s_ind_co2, 0, 0);
    lv_obj_set_style_bg_color(s_ind_co2, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_bg_opa(s_ind_co2, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ind_co2, 0, 0);
    lv_obj_set_style_radius(s_ind_co2, 3, 0);
}

/* ════════════════════════════════════════════════════════════════════════
 *  SPLASH DONE → build remaining screens
 * ════════════════════════════════════════════════════════════════════════ */
static void on_splash_done(lv_anim_t *a)
{
    (void)a;
    build_clock_screen();
    build_dashboard_screen();
    build_co2_screen();
    go_to(s_scr_clock, SCR_CLOCK);
    ESP_LOGI(TAG, "UI ready");
}

/* ════════════════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ════════════════════════════════════════════════════════════════════════ */
esp_err_t ui_start(uint32_t splash_ms)
{
    s_splash_ms = splash_ms;
    lv_obj_t *splash = mk_screen();
    build_splash(splash);
    return ESP_OK;
}

void ui_co2_update(int16_t ppm)
{
    if (!s_lbl_ppm) return;
    co2_apply_level(ppm);
}
