#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bsp/bsp.h"
#include "kni_ui/kni_ui.h"

static const char *TAG = "matter-display";

/* ── Button callback ──────────────────────────────────────────────────────── */

static void on_test_btn(lv_event_t *e)
{
    /* first child of the button is its label */
    lv_obj_t *btn   = lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);

    static bool toggled = false;
    toggled = !toggled;
    lv_label_set_text(label, toggled ? "OK!" : "Test Button");
    ESP_LOGI(TAG, "Button pressed — toggled: %s", toggled ? "ON" : "OFF");
}

/* ── Tab builder ──────────────────────────────────────────────────────────── */

static void build_test_tab(lv_obj_t *panel)
{
    /* Card — display / BSP status */
    lv_obj_t *card = kni_ui_card(panel, "DISPLAY TEST");
    kni_ui_kv_row(card, "Display",  "ST7789 240x320");
    kni_ui_kv_row(card, "Touch",    "CST328 I2C");
    kni_ui_kv_row(card, "BSP",      "waveshare27690");
    kni_ui_kv_row(card, "PSRAM",    "8 MB Octal");
    kni_ui_text(card,
        "If you can read this - display and touch are working.",
        KNI_UI_C_SUBTEXT);

    /* Styled button — stacks below the card thanks to panel flex layout */
    kni_ui_btn(panel, "Test Button", on_test_btn, NULL);
}

/* ── Tab config ───────────────────────────────────────────────────────────── */

static const kni_ui_tab_t tabs[] = {
    { .label = "TEST", .build = build_test_tab },
};

static const kni_ui_cfg_t ui_cfg = {
    .title     = "KNI",
    .subtitle  = "AntyBrainFogUR",
    .logo      = NULL,
    .splash_ms = 2500,
    .tabs      = tabs,
    .tab_count = 1,
};

/* ── Entry point ──────────────────────────────────────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "Booting matter-display v0.1.0");

    ESP_ERROR_CHECK(bsp_init());
    ESP_ERROR_CHECK(bsp_display_start());

    lv_display_t *disp  = NULL;
    lv_indev_t   *indev = NULL;
    ESP_ERROR_CHECK(bsp_lvgl_start(&disp, &indev));

    lv_indev_set_scroll_limit(indev, 3);
    lv_indev_set_scroll_throw(indev, 8);

    if (bsp_lvgl_lock(-1)) {
        ESP_ERROR_CHECK(kni_ui_start(&ui_cfg));
        bsp_lvgl_unlock();
    }

    ESP_LOGI(TAG, "UI running");
}
