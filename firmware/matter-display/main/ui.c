#include "ui.h"
#include "matter_app.h"
#include "kni_logo.h"
#include "bsp/bsp.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_app_desc.h"
#include "esp_timer.h"
#include "esp_system.h"
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

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
typedef enum { SCR_SPLASH, SCR_CLOCK, SCR_DASHBOARD, SCR_CO2, SCR_INFO } screen_t;

static screen_t  s_current    = SCR_SPLASH;
static lv_obj_t *s_scr_clock  = NULL;
static lv_obj_t *s_scr_dash   = NULL;
static lv_obj_t *s_scr_co2    = NULL;
static lv_obj_t *s_scr_info   = NULL;

/* live-updated labels on the Device Info screen */
static lv_obj_t *s_lbl_info_ip     = NULL;
static lv_obj_t *s_lbl_info_heap   = NULL;
static lv_obj_t *s_lbl_info_uptime = NULL;

/* labels updated by timers / sensor */
static lv_obj_t *s_lbl_time         = NULL;
static lv_obj_t *s_lbl_date         = NULL;
static lv_obj_t *s_lbl_ppm          = NULL;
static lv_obj_t *s_lbl_status       = NULL;
static lv_obj_t *s_badge_co2        = NULL;   /* pill badge container      */
static lv_obj_t *s_arc_co2          = NULL;
static lv_obj_t *s_lbl_live         = NULL;   /* topbar LIVE / SETUP label */

/* Matter dashboard tile references */
static lv_obj_t *s_tile_matter       = NULL;
static lv_obj_t *s_ico_matter        = NULL;   /* big icon glyph in the tile */
static lv_obj_t *s_lbl_matter_status = NULL;   /* label text under the icon  */
static bool      s_matter_commissioned = false;

/* Factory-reset progress overlay (created lazily, lives on the top layer) */
static lv_obj_t *s_reset_overlay = NULL;
static lv_obj_t *s_reset_bar     = NULL;
static lv_obj_t *s_reset_pct     = NULL;

/* ── CO2 history ─────────────────────────────────────────────────────────
 * Readings are bucketed into 5-minute slots.
 * 48 slots stored = ~4 h; CO2_HIST_SHOW newest are drawn on screen.      */
#define CO2_HIST_SLOTS  48
#define CO2_HIST_SHOW   16
#define CO2_SLOT_SECS   300   /* 5 minutes */

typedef struct { int32_t sum; uint16_t cnt; } co2_slot_t;

static co2_slot_t  s_hist[CO2_HIST_SLOTS];
static uint8_t     s_hist_head = 0;
static time_t      s_hist_slot_ts = 0;
static lv_obj_t   *s_hist_sq[CO2_HIST_SHOW];

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
    go_to(s_scr_co2, SCR_CO2);   /* CO2 is the key feature — always land here */
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

    lv_obj_t *hint = mk_label(s_scr_clock, "tap for CO2", C_INACTIVE, &lv_font_montserrat_14);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -14);

    lv_timer_create(clock_tick_cb,  1000, NULL);
    lv_timer_create(inactivity_cb,  5000, NULL);
    clock_tick_cb(NULL);
}

/* ════════════════════════════════════════════════════════════════════════
 *  DASHBOARD SCREEN
 * ════════════════════════════════════════════════════════════════════════ */
static void on_co2_tile(lv_event_t *e) { (void)e; go_to(s_scr_co2, SCR_CO2); }

/* ── Matter pairing popup ──────────────────────────────────────────────── */
static void on_matter_popup_close(lv_event_t *e)
{
    lv_obj_t *popup = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_del(popup);
}

/* Build a modal popup with QR + manual pairing code (or "Paired" message
 * when commissioned).  Layout (card 220×280, padding 12 → inner 196×256):
 *
 *   y=0    title       (icon + "MATTER SETUP")
 *   y=20   separator   (180×1)
 *   y=28   QR code     (140×140 + 6px white quiet zone) → ends y=180
 *   y=190  "Manual code" hint (mont_14)
 *   y=210  manual code string (mont_16, teal)
 *   y=224  close button (100×32, bottom-aligned, ends y=256)
 *
 * No overlap.  `s_matter_commissioned` chooses commissioned variant.        */
static void show_matter_popup(void)
{
    lv_obj_t *overlay = lv_obj_create(lv_scr_act());
    lv_obj_set_size(overlay, 240, 320);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(overlay, on_matter_popup_close, LV_EVENT_CLICKED, overlay);

    lv_obj_t *card = lv_obj_create(overlay);
    lv_obj_set_size(card, 220, 300);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(C_SURFACE), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(C_TEAL), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    /* stop clicks bubbling to the overlay (which closes the popup) */
    lv_obj_clear_flag(card, LV_OBJ_FLAG_EVENT_BUBBLE);

    /* Title with bluetooth icon */
    lv_obj_t *title = mk_label(card,
        s_matter_commissioned ? LV_SYMBOL_BLUETOOTH "  MATTER PAIRED"
                              : LV_SYMBOL_BLUETOOTH "  MATTER SETUP",
        s_matter_commissioned ? C_TEAL : C_TEXT,
        &lv_font_montserrat_16);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    /* Separator */
    lv_obj_t *sep = lv_obj_create(card);
    lv_obj_set_size(sep, 180, 1);
    lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, 22);
    lv_obj_set_style_bg_color(sep, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_pad_all(sep, 0, 0);

    if (s_matter_commissioned) {
        /* Big OK glyph + "Device Paired" message + reset hint */
        lv_obj_t *check = mk_label(card, LV_SYMBOL_OK, C_TEAL, &lv_font_montserrat_48);
        lv_obj_align(check, LV_ALIGN_CENTER, 0, -50);
        lv_obj_t *msg = mk_label(card, "Device is paired\nwith a controller",
                                 C_TEXT, &lv_font_montserrat_14);
        lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(msg, LV_ALIGN_CENTER, 0, 8);
        /* Factory-reset hint */
        lv_obj_t *hint = mk_label(card,
            LV_SYMBOL_WARNING " Hold BOOT 8 s\nto factory reset",
            C_ALERT, &lv_font_montserrat_14);
        lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -52);
    } else {
        const char *qr_payload = matter_app_get_qr_code();
        const char *manual     = matter_app_get_manual_code();

        if (qr_payload) {
            char qr_uri[128];
            snprintf(qr_uri, sizeof(qr_uri), "MT:%s", qr_payload);
            lv_obj_t *qr = lv_qrcode_create(card);
            lv_qrcode_set_size(qr, 140);
            lv_qrcode_set_dark_color(qr, lv_color_hex(0x000000));
            lv_qrcode_set_light_color(qr, lv_color_hex(0xFFFFFF));
            lv_qrcode_update(qr, qr_uri, strlen(qr_uri));
            /* 6px white quiet zone — required by most QR scanners */
            lv_obj_set_style_border_color(qr, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_border_width(qr, 6, 0);
            lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, 28);
        } else {
            lv_obj_t *boot = mk_label(card, LV_SYMBOL_REFRESH "  Booting...",
                                      C_MUTED, &lv_font_montserrat_14);
            lv_obj_align(boot, LV_ALIGN_TOP_MID, 0, 90);
        }

        if (manual) {
            /* Anchor from bottom so code never collides with the close button.
             * Stack: hint @ -68, code @ -42, button @ 0 — gives 12 px gap
             * between code and button, and 8 px between hint and code.       */
            lv_obj_t *hint = mk_label(card, "manual code",
                                      C_MUTED, &lv_font_montserrat_14);
            lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -68);
            lv_obj_t *code = mk_label(card, manual, C_TEAL, &lv_font_montserrat_16);
            lv_obj_align(code, LV_ALIGN_BOTTOM_MID, 0, -44);
        }
    }

    /* Close button — full-width, anchored to bottom of card */
    lv_obj_t *btn = lv_button_create(card);
    lv_obj_set_size(btn, 120, 32);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(C_TEAL_DIM), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, on_matter_popup_close, LV_EVENT_CLICKED, overlay);
    lv_obj_center(mk_label(btn, LV_SYMBOL_CLOSE "  Close",
                           C_TEXT, &lv_font_montserrat_14));
}

static void on_matter_tile(lv_event_t *e) { (void)e; show_matter_popup(); }
static void on_live_tap(lv_event_t *e)   { (void)e; show_matter_popup(); }

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

static void on_info_tile(lv_event_t *e) { (void)e; if (s_scr_info) go_to(s_scr_info, SCR_INFO); }

static void build_dashboard_screen(void)
{
    s_scr_dash = mk_screen();
    mk_topbar(s_scr_dash, "KNI", "AntyBrainFogUR");

    /* 2×2 grid: tile=104, gap=8, total=216 → x_off=12, y_off=66 */
    int x0 = 12, y0 = 66, step = 112;

    /* Tile 1 — CO2 Monitor (active) */
    lv_obj_t *t;
    t = mk_tile(s_scr_dash, "CO2", "CO2 Monitor", true, on_co2_tile);
    lv_obj_set_pos(t, x0, y0);

    /* Tile 2 — Matter integration.  Always "active" so it pulses its
     * pressed style; colour swaps between ALERT (needs pairing) and TEAL
     * (paired) inside ui_matter_set_commissioned().                         */
    s_tile_matter = mk_tile(s_scr_dash, LV_SYMBOL_BLUETOOTH,
                            "Pair Now", true, on_matter_tile);
    lv_obj_set_pos(s_tile_matter, x0 + step, y0);
    /* Tile children: [icon, label] — store refs so we can recolour both */
    s_ico_matter        = lv_obj_get_child(s_tile_matter, 0);
    s_lbl_matter_status = lv_obj_get_child(s_tile_matter, 1);
    /* Initial colours derived from s_matter_commissioned (set by main.c
     * before the splash finishes via ui_matter_set_commissioned()).        */
    uint32_t init_accent = s_matter_commissioned ? C_TEAL : C_ALERT;
    lv_obj_set_style_border_color(s_tile_matter, lv_color_hex(init_accent), 0);
    if (s_ico_matter) {
        lv_obj_set_style_text_color(s_ico_matter, lv_color_hex(init_accent), 0);
    }
    if (s_lbl_matter_status) {
        lv_label_set_text(s_lbl_matter_status,
                          s_matter_commissioned ? "Paired" : "Pair Now");
        lv_obj_set_style_text_color(s_lbl_matter_status, lv_color_hex(C_TEXT), 0);
    }

    /* Tile 3 — Device Info (active) */
    t = mk_tile(s_scr_dash, LV_SYMBOL_SETTINGS,
                "Device\nInfo", true, on_info_tile);
    lv_obj_set_pos(t, x0, y0 + step);

    /* Tile 4 — placeholder */
    t = mk_tile(s_scr_dash, LV_SYMBOL_PLUS, "Coming Soon", false, NULL);
    lv_obj_set_pos(t, x0 + step, y0 + step);
}

/* ════════════════════════════════════════════════════════════════════════
 *  CO2 SCREEN
 * ════════════════════════════════════════════════════════════════════════ */

/* Map a ppm average to a palette colour (shared by badge and squares). */
static uint32_t ppm_to_col(int avg)
{
    if (avg < 800)  return C_TEAL;
    if (avg < 1000) return 0xA8D060;
    if (avg < 1500) return 0xFFB347;
    return C_ALERT;
}

/* Redraw the 16 history squares from the circular buffer. */
static void hist_refresh(void)
{
    if (!s_hist_sq[0]) return;
    for (int i = 0; i < CO2_HIST_SHOW; i++) {
        /* i=0 oldest, i=CO2_HIST_SHOW-1 newest */
        int slot = ((int)s_hist_head - (CO2_HIST_SHOW - 1 - i)
                    + CO2_HIST_SLOTS) % CO2_HIST_SLOTS;
        co2_slot_t *sl = &s_hist[slot];
        uint32_t col = (sl->cnt > 0)
                       ? ppm_to_col((int)(sl->sum / sl->cnt))
                       : C_BG;   /* empty = transparent-ish, border stays visible */
        lv_obj_set_style_bg_color(s_hist_sq[i], lv_color_hex(col), 0);
    }
}

static void co2_apply_level(int16_t ppm)
{
    uint32_t    col;
    const char *status;

    if (ppm < 0) {
        lv_label_set_text(s_lbl_ppm,    "---");
        lv_label_set_text(s_lbl_status, "NO DATA");
        lv_obj_set_style_text_color(s_lbl_ppm,    lv_color_hex(C_MUTED), 0);
        lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(C_MUTED), 0);
        if (s_badge_co2)
            lv_obj_set_style_border_color(s_badge_co2, lv_color_hex(C_INACTIVE), 0);
        if (s_arc_co2) {
            lv_arc_set_value(s_arc_co2, 0);
            lv_obj_set_style_arc_color(s_arc_co2, lv_color_hex(C_INACTIVE),
                                       LV_PART_INDICATOR);
        }
        return;
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", ppm);
    lv_label_set_text(s_lbl_ppm, buf);

    if      (ppm < 800)  { col = C_TEAL;   status = "GOOD";     }
    else if (ppm < 1000) { col = 0xA8D060; status = "OK";       }
    else if (ppm < 1500) { col = 0xFFB347; status = "HIGH";     }
    else                 { col = C_ALERT;  status = "CRITICAL"; }

    lv_label_set_text(s_lbl_status, status);
    lv_obj_set_style_text_color(s_lbl_ppm,    lv_color_hex(col), 0);
    lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(col), 0);
    if (s_badge_co2)
        lv_obj_set_style_border_color(s_badge_co2, lv_color_hex(col), 0);
    if (s_arc_co2) {
        lv_arc_set_value(s_arc_co2, ppm > 2500 ? 100 : (int)(ppm * 100 / 2500));
        lv_obj_set_style_arc_color(s_arc_co2, lv_color_hex(col), LV_PART_INDICATOR);
    }
}

static void on_co2_back(lv_event_t *e)
{
    (void)e;
    go_to(s_scr_dash, SCR_DASHBOARD);
}

static void build_co2_screen(void)
{
    s_scr_co2 = mk_screen();

    /* ════════════════════════════════════════════════════════════════════
     *  TOPBAR — fixed, 36 px: only back button + LIVE placeholder
     * ════════════════════════════════════════════════════════════════════ */
    lv_obj_t *topbar = lv_obj_create(s_scr_co2);
    lv_obj_set_size(topbar, 240, 36);
    lv_obj_set_pos(topbar, 0, 0);
    lv_obj_set_style_bg_color(topbar, lv_color_hex(C_BAR), 0);
    lv_obj_set_style_bg_opa(topbar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(topbar, lv_color_hex(C_TEAL), 0);
    lv_obj_set_style_border_side(topbar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(topbar, 1, 0);
    lv_obj_set_style_radius(topbar, 0, 0);
    lv_obj_set_style_pad_hor(topbar, 10, 0);
    lv_obj_set_style_pad_ver(topbar, 0, 0);
    lv_obj_clear_flag(topbar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back = lv_button_create(topbar);
    lv_obj_set_size(back, 32, 28);
    lv_obj_set_style_bg_opa(back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(C_TEAL), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(back, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_set_style_shadow_width(back, 0, 0);
    lv_obj_set_style_radius(back, 4, 0);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(back, on_co2_back, LV_EVENT_CLICKED, NULL);
    lv_obj_center(mk_label(back, "<", C_TEAL, &lv_font_montserrat_14));

    /* LIVE / SETUP indicator — right side; updated by ui_matter_set_commissioned().
     * Tap opens the Matter pairing popup (QR + manual code, or paired status). */
    s_lbl_live = mk_label(topbar,
                          s_matter_commissioned ? LV_SYMBOL_BLUETOOTH " PAIRED"
                                                : LV_SYMBOL_BLUETOOTH " SETUP",
                          s_matter_commissioned ? C_TEAL : C_ALERT,
                          &lv_font_montserrat_14);
    lv_obj_align(s_lbl_live, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_flag(s_lbl_live, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_lbl_live, 8);
    lv_obj_add_event_cb(s_lbl_live, on_live_tap, LV_EVENT_CLICKED, NULL);

    /* ════════════════════════════════════════════════════════════════════
     *  SCROLLABLE CONTENT — below topbar, taller than viewport
     *  viewport = 284 px,  content = ~370 px  →  ~86 px scroll travel
     * ════════════════════════════════════════════════════════════════════ */
    lv_obj_t *cnt = lv_obj_create(s_scr_co2);
    lv_obj_set_pos(cnt, 0, 36);
    lv_obj_set_size(cnt, 240, 284);
    lv_obj_set_style_bg_color(cnt, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(cnt, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cnt, 0, 0);
    lv_obj_set_style_pad_all(cnt, 0, 0);
    lv_obj_set_scroll_dir(cnt, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(cnt, LV_SCROLLBAR_MODE_ACTIVE);

    /* ── arc (160×160), x_center=120, y_center=100 in cnt coords ─────────── */
    s_arc_co2 = lv_arc_create(cnt);
    lv_obj_set_size(s_arc_co2, 160, 160);
    lv_obj_set_pos(s_arc_co2, 40, 40);               /* center=(120,120) in cnt */
    lv_arc_set_rotation(s_arc_co2, 135);
    lv_arc_set_bg_angles(s_arc_co2, 0, 270);
    lv_arc_set_range(s_arc_co2, 0, 100);
    lv_arc_set_value(s_arc_co2, 0);
    lv_obj_clear_flag(s_arc_co2, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(s_arc_co2,    LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_width(s_arc_co2,  0,             LV_PART_KNOB);
    lv_obj_set_style_height(s_arc_co2, 0,             LV_PART_KNOB);
    lv_obj_set_style_bg_opa(s_arc_co2, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc_co2, lv_color_hex(C_SURFACE),  LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc_co2, 14,                        LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc_co2, lv_color_hex(C_INACTIVE), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_arc_co2, 14,                        LV_PART_INDICATOR);

    /* ppm — centered in arc (arc center y=100, font_48 half-height≈27) */
    s_lbl_ppm = lv_label_create(cnt);
    lv_label_set_text(s_lbl_ppm, "---");
    lv_obj_set_style_text_color(s_lbl_ppm, lv_color_hex(C_MUTED), 0);
    lv_obj_set_style_text_font(s_lbl_ppm, &lv_font_montserrat_48, 0);
    lv_obj_align(s_lbl_ppm, LV_ALIGN_TOP_MID, 0, 93);

    lv_obj_t *unit = mk_label(cnt, "PPM", C_MUTED, &lv_font_montserrat_14);
    lv_obj_align(unit, LV_ALIGN_TOP_MID, 0, 153);

    /* status pill badge */
    s_badge_co2 = lv_obj_create(cnt);
    lv_obj_set_size(s_badge_co2, 160, 32);
    lv_obj_set_pos(s_badge_co2, 40, 222);
    lv_obj_set_style_bg_color(s_badge_co2,     lv_color_hex(C_SURFACE),  0);
    lv_obj_set_style_bg_opa(s_badge_co2,       LV_OPA_COVER,             0);
    lv_obj_set_style_border_color(s_badge_co2, lv_color_hex(C_INACTIVE), 0);
    lv_obj_set_style_border_width(s_badge_co2, 1,                        0);
    lv_obj_set_style_radius(s_badge_co2,       16,                       0);
    lv_obj_set_style_pad_all(s_badge_co2,      0,                        0);
    lv_obj_clear_flag(s_badge_co2, LV_OBJ_FLAG_SCROLLABLE);

    s_lbl_status = lv_label_create(s_badge_co2);
    lv_label_set_text(s_lbl_status, "NO DATA");
    lv_obj_set_style_text_color(s_lbl_status, lv_color_hex(C_MUTED), 0);
    lv_obj_set_style_text_font(s_lbl_status,  &lv_font_montserrat_14, 0);
    lv_obj_align(s_lbl_status, LV_ALIGN_CENTER, 0, 0);

    /* ── history section ─────────────────────────────────────────────────── */
    lv_obj_t *div = lv_obj_create(cnt);
    lv_obj_set_size(div, 200, 1);
    lv_obj_set_pos(div, 20, 294);
    lv_obj_set_style_bg_color(div, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_set_style_pad_all(div, 0, 0);

    lv_obj_t *hlbl = mk_label(cnt, "LAST 1H", C_MUTED, &lv_font_montserrat_14);
    lv_obj_set_pos(hlbl, 12, 306);

    /* 16 squares  width=12  height=32  gap=2 → total 222px, x_start=9
     * Empty = C_BG fill + C_INACTIVE border → clearly visible outlines    */
    for (int i = 0; i < CO2_HIST_SHOW; i++) {
        s_hist_sq[i] = lv_obj_create(cnt);
        lv_obj_set_size(s_hist_sq[i], 12, 32);
        lv_obj_set_pos(s_hist_sq[i], 9 + i * 14, 328);
        lv_obj_set_style_bg_color(s_hist_sq[i],     lv_color_hex(C_BG),       0);
        lv_obj_set_style_bg_opa(s_hist_sq[i],       LV_OPA_COVER,             0);
        lv_obj_set_style_border_color(s_hist_sq[i], lv_color_hex(C_INACTIVE), 0);
        lv_obj_set_style_border_width(s_hist_sq[i], 1,                        0);
        lv_obj_set_style_radius(s_hist_sq[i],       3,                        0);
        lv_obj_set_style_pad_all(s_hist_sq[i],      0,                        0);
        lv_obj_clear_flag(s_hist_sq[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    /* colour legend: coloured dot + label × 4, y=330 */
    static const uint32_t leg_col[] = { C_TEAL, 0xA8D060, 0xFFB347, C_ALERT };
    static const char    *leg_txt[] = { "< 800", "< 1k",  "< 1.5k", "> 1.5k" };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *dot = lv_obj_create(cnt);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_pos(dot, 10 + i * 55, 376);
        lv_obj_set_style_bg_color(dot,     lv_color_hex(leg_col[i]), 0);
        lv_obj_set_style_bg_opa(dot,       LV_OPA_COVER,             0);
        lv_obj_set_style_border_width(dot, 0,                        0);
        lv_obj_set_style_radius(dot,       2,                        0);
        lv_obj_set_style_pad_all(dot,      0,                        0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *ll = mk_label(cnt, leg_txt[i], C_INACTIVE, &lv_font_montserrat_14);
        lv_obj_set_pos(ll, 22 + i * 55, 372);
    }
}

/* ════════════════════════════════════════════════════════════════════════
 *  DEVICE INFO SCREEN
 * ════════════════════════════════════════════════════════════════════════ */
static void on_info_back(lv_event_t *e) { (void)e; go_to(s_scr_dash, SCR_DASHBOARD); }

/* Add a label/value row at given y. Returns the value label so it can be
 * stored for live updates. */
static lv_obj_t *info_row(lv_obj_t *parent, int y,
                          const char *label, const char *value)
{
    lv_obj_t *l = mk_label(parent, label, C_MUTED, &lv_font_montserrat_14);
    lv_obj_set_pos(l, 12, y);

    lv_obj_t *v = mk_label(parent, value, C_TEXT, &lv_font_montserrat_14);
    lv_obj_align(v, LV_ALIGN_TOP_RIGHT, -12, y);
    return v;
}

static void info_tick_cb(lv_timer_t *t)
{
    (void)t;
    if (s_current != SCR_INFO) return;

    if (s_lbl_info_heap) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%lu KB",
                 (unsigned long)(esp_get_free_heap_size() / 1024));
        lv_label_set_text(s_lbl_info_heap, buf);
    }
    if (s_lbl_info_ip) {
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        esp_netif_ip_info_t ip = {0};
        if (netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr) {
            char buf[20];
            snprintf(buf, sizeof(buf), IPSTR, IP2STR(&ip.ip));
            lv_label_set_text(s_lbl_info_ip, buf);
        } else {
            lv_label_set_text(s_lbl_info_ip, "—");
        }
    }
    if (s_lbl_info_uptime) {
        int up = (int)(esp_timer_get_time() / 1000000);
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                 up / 3600, (up / 60) % 60, up % 60);
        lv_label_set_text(s_lbl_info_uptime, buf);
    }
}

static void build_info_screen(void)
{
    s_scr_info = mk_screen();

    /* Topbar — same shape as CO2 screen for consistency */
    lv_obj_t *topbar = lv_obj_create(s_scr_info);
    lv_obj_set_size(topbar, 240, 36);
    lv_obj_set_pos(topbar, 0, 0);
    lv_obj_set_style_bg_color(topbar, lv_color_hex(C_BAR), 0);
    lv_obj_set_style_bg_opa(topbar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(topbar, lv_color_hex(C_TEAL), 0);
    lv_obj_set_style_border_side(topbar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(topbar, 1, 0);
    lv_obj_set_style_radius(topbar, 0, 0);
    lv_obj_set_style_pad_hor(topbar, 10, 0);
    lv_obj_set_style_pad_ver(topbar, 0, 0);
    lv_obj_clear_flag(topbar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back = lv_button_create(topbar);
    lv_obj_set_size(back, 32, 28);
    lv_obj_set_style_bg_opa(back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(C_TEAL), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(back, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_set_style_shadow_width(back, 0, 0);
    lv_obj_set_style_radius(back, 4, 0);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(back, on_info_back, LV_EVENT_CLICKED, NULL);
    lv_obj_center(mk_label(back, LV_SYMBOL_LEFT, C_TEAL, &lv_font_montserrat_14));

    lv_obj_t *t = mk_label(topbar, LV_SYMBOL_SETTINGS "  DEVICE INFO",
                           C_TEAL, &lv_font_montserrat_14);
    lv_obj_align(t, LV_ALIGN_RIGHT_MID, 0, 0);

    /* Content rows */
    int y    = 50;
    int step = 26;

    /* MAC address */
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char mac_buf[20];
    snprintf(mac_buf, sizeof(mac_buf),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    info_row(s_scr_info, y, LV_SYMBOL_WIFI " MAC", mac_buf);
    y += step;

    s_lbl_info_ip     = info_row(s_scr_info, y, LV_SYMBOL_WIFI " IP",        "—");
    y += step;
    s_lbl_info_heap   = info_row(s_scr_info, y, LV_SYMBOL_DRIVE " HEAP",     "—");
    y += step;
    s_lbl_info_uptime = info_row(s_scr_info, y, LV_SYMBOL_REFRESH " UPTIME", "00:00:00");
    y += step;

    /* App + IDF versions */
    const esp_app_desc_t *desc = esp_app_get_description();
    info_row(s_scr_info, y,
             LV_SYMBOL_FILE " APP",
             (desc && desc->version[0]) ? desc->version : "?");
    y += step;
    info_row(s_scr_info, y,
             LV_SYMBOL_FILE " IDF",
             (desc && desc->idf_ver[0]) ? desc->idf_ver : "?");
    y += step;

    /* Matter cluster */
    info_row(s_scr_info, y, LV_SYMBOL_BLUETOOTH " MATTER", "CO2 0x040D");
    y += step;

    /* Footer hint */
    lv_obj_t *hint = mk_label(s_scr_info,
        "AntyBrainFogUR  •  KNI",
        C_INACTIVE, &lv_font_montserrat_14);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -12);

    lv_timer_create(info_tick_cb, 1000, NULL);
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
    build_info_screen();
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

    /* ── accumulate into history slot ─────────────────────────────────── */
    if (ppm > 0) {
        time_t now = time(NULL);
        if (s_hist_slot_ts == 0) s_hist_slot_ts = now;

        if (now - s_hist_slot_ts >= CO2_SLOT_SECS) {
            /* advance to next slot */
            s_hist_head = (s_hist_head + 1) % CO2_HIST_SLOTS;
            s_hist[s_hist_head].sum = 0;
            s_hist[s_hist_head].cnt = 0;
            s_hist_slot_ts = now;
        }
        s_hist[s_hist_head].sum += ppm;
        s_hist[s_hist_head].cnt++;
        hist_refresh();
    }

    co2_apply_level(ppm);
}

/* ── Matter commissioning status ─────────────────────────────────────────
 * Called from the Matter event task; acquires LVGL lock internally.        */
void ui_matter_set_commissioned(bool commissioned)
{
    s_matter_commissioned = commissioned;

    /* s_lbl_live / s_tile_matter are NULL until CO2 screen is built */
    if (!s_lbl_live && !s_tile_matter) return;

    if (!bsp_lvgl_lock(100)) return;

    /* ── topbar LIVE indicator ─────────────────────────────────────────── */
    if (s_lbl_live) {
        lv_label_set_text(s_lbl_live,
            commissioned ? LV_SYMBOL_BLUETOOTH " PAIRED"
                         : LV_SYMBOL_BLUETOOTH " SETUP");
        lv_obj_set_style_text_color(s_lbl_live,
            lv_color_hex(commissioned ? C_TEAL : C_ALERT), 0);
    }

    /* ── dashboard Matter tile — recolour border, icon and label ──────── */
    uint32_t accent = commissioned ? C_TEAL : C_ALERT;
    if (s_tile_matter) {
        lv_obj_set_style_border_color(s_tile_matter, lv_color_hex(accent), 0);
    }
    if (s_ico_matter) {
        lv_obj_set_style_text_color(s_ico_matter, lv_color_hex(accent), 0);
    }
    if (s_lbl_matter_status) {
        lv_label_set_text(s_lbl_matter_status,
                          commissioned ? "Paired" : "Pair Now");
        lv_obj_set_style_text_color(s_lbl_matter_status,
            lv_color_hex(C_TEXT), 0);
    }

    bsp_lvgl_unlock();
}

/* ── Factory-reset progress overlay (top layer, above modals) ───────────── */
void ui_factory_reset_progress_show(uint8_t pct)
{
    if (pct > 100) pct = 100;
    if (!bsp_lvgl_lock(50)) return;

    if (!s_reset_overlay) {
        /* Create on top layer so it sits above any modal popup */
        s_reset_overlay = lv_obj_create(lv_layer_top());
        lv_obj_set_size(s_reset_overlay, 240, 320);
        lv_obj_set_pos(s_reset_overlay, 0, 0);
        lv_obj_set_style_bg_color(s_reset_overlay, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(s_reset_overlay, LV_OPA_90, 0);
        lv_obj_set_style_border_width(s_reset_overlay, 0, 0);
        lv_obj_set_style_pad_all(s_reset_overlay, 0, 0);
        lv_obj_clear_flag(s_reset_overlay, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *icon = mk_label(s_reset_overlay, LV_SYMBOL_WARNING,
                                  C_ALERT, &lv_font_montserrat_48);
        lv_obj_align(icon, LV_ALIGN_CENTER, 0, -80);

        lv_obj_t *title = mk_label(s_reset_overlay, "Factory Reset",
                                   C_ALERT, &lv_font_montserrat_24);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, -28);

        lv_obj_t *hint = mk_label(s_reset_overlay,
            "Hold BOOT to wipe Matter\nfabrics and reboot",
            C_TEXT, &lv_font_montserrat_14);
        lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(hint, LV_ALIGN_CENTER, 0, 8);

        s_reset_bar = lv_bar_create(s_reset_overlay);
        lv_obj_set_size(s_reset_bar, 180, 8);
        lv_obj_align(s_reset_bar, LV_ALIGN_CENTER, 0, 56);
        lv_bar_set_range(s_reset_bar, 0, 100);
        lv_obj_set_style_bg_color(s_reset_bar, lv_color_hex(C_BORDER), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_reset_bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(s_reset_bar, 4, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_reset_bar, lv_color_hex(C_ALERT), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(s_reset_bar, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_radius(s_reset_bar, 4, LV_PART_INDICATOR);

        s_reset_pct = mk_label(s_reset_overlay, "0%", C_MUTED, &lv_font_montserrat_14);
        lv_obj_align(s_reset_pct, LV_ALIGN_CENTER, 0, 76);
    }

    if (s_reset_bar) lv_bar_set_value(s_reset_bar, pct, LV_ANIM_OFF);
    if (s_reset_pct) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u%%", (unsigned)pct);
        lv_label_set_text(s_reset_pct, buf);
    }

    bsp_lvgl_unlock();
}

void ui_factory_reset_progress_hide(void)
{
    if (!bsp_lvgl_lock(50)) return;
    if (s_reset_overlay) {
        lv_obj_del(s_reset_overlay);
        s_reset_overlay = NULL;
        s_reset_bar     = NULL;
        s_reset_pct     = NULL;
    }
    bsp_lvgl_unlock();
}
