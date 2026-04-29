#pragma once
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── AntyBrainFogUR design palette ───────────────────────────────────────────
 *  Redefine any constant before #include to override.
 * ─────────────────────────────────────────────────────────────────────────── */
#ifndef KNI_UI_C_BG
#define KNI_UI_C_BG        0x0A0E12
#endif
#ifndef KNI_UI_C_SURFACE
#define KNI_UI_C_SURFACE   0x13181F
#endif
#ifndef KNI_UI_C_CARD
#define KNI_UI_C_CARD      0x13181F
#endif
#ifndef KNI_UI_C_TEXT
#define KNI_UI_C_TEXT      0xE8EEF2
#endif
#ifndef KNI_UI_C_SUBTEXT
#define KNI_UI_C_SUBTEXT   0x6B7E8F
#endif
#ifndef KNI_UI_C_TEAL
#define KNI_UI_C_TEAL      0x5EE6C1
#endif
#ifndef KNI_UI_C_TEAL_DIM
#define KNI_UI_C_TEAL_DIM  0x3BA88A
#endif
#ifndef KNI_UI_C_BLUE
#define KNI_UI_C_BLUE      0x7AA8FF
#endif
#ifndef KNI_UI_C_ALERT
#define KNI_UI_C_ALERT     0xFF8A6B
#endif
#ifndef KNI_UI_C_BORDER
#define KNI_UI_C_BORDER    0x1C2530
#endif
#ifndef KNI_UI_C_BAR
#define KNI_UI_C_BAR       0x0D1318
#endif
#ifndef KNI_UI_C_INACTIVE
#define KNI_UI_C_INACTIVE  0x3D5068
#endif

#define KNI_UI_MAX_TABS   5

/* ── Tab descriptor ──────────────────────────────────────────────────────────
 *  build() is called once when the main screen is created.
 *  The panel passed in is a vertical flex container — just add cards to it.
 * ─────────────────────────────────────────────────────────────────────────── */
typedef struct {
    const char *label;                    /* menu bar text — short, uppercase  */
    void       (*build)(lv_obj_t *panel); /* content builder callback          */
} kni_ui_tab_t;

/* ── Top-level config ────────────────────────────────────────────────────── */
typedef struct {
    const char           *title;       /* top bar left  — e.g. "KNI"          */
    const char           *subtitle;    /* top bar right — e.g. "AntyBrainFogUR" */
    const lv_image_dsc_t *logo;        /* splash logo image (NULL = text only) */
    uint32_t              splash_ms;   /* loading bar animation duration (ms)  */
    const kni_ui_tab_t   *tabs;
    int                   tab_count;   /* 1 – KNI_UI_MAX_TABS                 */
} kni_ui_cfg_t;

/* ── Lifecycle ───────────────────────────────────────────────────────────────
 *  Call from within bsp_lvgl_lock() / bsp_lvgl_unlock().
 *  Shows splash → animated bar → fade-in main screen.
 * ─────────────────────────────────────────────────────────────────────────── */
esp_err_t kni_ui_start(const kni_ui_cfg_t *cfg);

/* ── Content helpers — call inside tab build() callbacks ─────────────────────
 *
 *  Typical pattern:
 *
 *    static void build_home(lv_obj_t *panel) {
 *        lv_obj_t *card = kni_ui_card(panel, "STATUS");
 *        lv_obj_t *val  = kni_ui_kv_row(card, "CO2", "--- ppm");
 *        // store val — update with: lv_label_set_text_fmt(val, "%d ppm", co2);
 *    }
 * ─────────────────────────────────────────────────────────────────────────── */

/** Styled card with a Mint title. Returns the card object. */
lv_obj_t *kni_ui_card(lv_obj_t *panel, const char *title);

/**
 * Add a key–value row inside a card.
 * Returns the VALUE label — keep the pointer to update it later.
 */
lv_obj_t *kni_ui_kv_row(lv_obj_t *card, const char *key, const char *value);

/** Wrapped text paragraph inside a card. Returns the label. */
lv_obj_t *kni_ui_text(lv_obj_t *card, const char *text, uint32_t color);

/**
 * Transparent flex-column wrapper inside a panel.
 * Use when a tab needs multiple stacked cards.
 */
lv_obj_t *kni_ui_col(lv_obj_t *panel);

/**
 * Full-width styled button inside a panel or card.
 * Returns the button object; the label is its first child.
 */
lv_obj_t *kni_ui_btn(lv_obj_t *parent, const char *label,
                     lv_event_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif
