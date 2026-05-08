#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "bsp/bsp.h"
#include "mhz19.h"
#include "ui.h"
#include "matter_app.h"

static const char *TAG = "matter-display";

/* MH-Z19C wiring */
#define MHZ19_UART  UART_NUM_1
#define MHZ19_RX    18   /* GPIO18 ← sensor TX */
#define MHZ19_TX    15   /* GPIO15 → sensor RX */

/* BOOT button on Waveshare ESP32-S3-Touch-LCD-2.8 = GPIO0 (active low). */
#define BOOT_BTN_GPIO     GPIO_NUM_0
#define RESET_HOLD_MS     8000   /* total hold time → factory reset    */
#define RESET_GRACE_MS    1000   /* ignore the first second (debounce) */
#define RESET_POLL_MS     100

/* ── RTC helpers ────────────────────────────────────────────────────────── */

static void init_time(void)
{
    time_t now;
    time(&now);
    if (now > 1000000000UL) return;

    struct tm t = {};
    char mon[4];
    int  day, year, h, m, s;
    sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &h,   &m,   &s);

    static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *p = strstr(months, mon);
    t.tm_mon   = p ? (int)(p - months) / 3 : 0;
    t.tm_mday  = day;
    t.tm_year  = year - 1900;
    t.tm_hour  = h;
    t.tm_min   = m;
    t.tm_sec   = s;
    t.tm_isdst = -1;

    struct timeval tv = { .tv_sec = mktime(&t) };
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG, "RTC set to compile time: %s %s", __DATE__, __TIME__);
}

/* Start SNTP so the clock auto-syncs once Matter brings up WiFi.
 * Polish timezone — DST (CEST) handled automatically per POSIX TZ rules.
 * pool.ntp.org resolves & connects whenever lwip has internet; before then
 * the clock stays at the compile-time fallback set by init_time().          */
static void start_sntp(void)
{
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    cfg.start = true;
    esp_err_t err = esp_netif_sntp_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SNTP init failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "SNTP started — clock will sync once WiFi connects");
}

/* ── BOOT-button factory-reset task ─────────────────────────────────────
 * Polls GPIO0 every 100 ms.  When held continuously for RESET_HOLD_MS,
 * shows a fullscreen progress overlay and triggers
 * matter_app_factory_reset() which wipes the chip_* NVS partitions and
 * reboots.  Press shorter than RESET_GRACE_MS is ignored as debounce.    */
static void factory_reset_task(void *arg)
{
    (void)arg;

    /* GPIO0 has external pull-up on the dev board; we still enable internal
     * pull-up as a belt-and-braces measure.                                */
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BOOT_BTN_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));

    int  held_ms        = 0;
    bool overlay_shown  = false;

    while (1) {
        if (gpio_get_level(BOOT_BTN_GPIO) == 0) {
            held_ms += RESET_POLL_MS;

            if (held_ms > RESET_GRACE_MS) {
                int span = RESET_HOLD_MS - RESET_GRACE_MS;
                int pct  = (held_ms - RESET_GRACE_MS) * 100 / span;
                if (pct > 100) pct = 100;

                ui_factory_reset_progress_show((uint8_t)pct);
                overlay_shown = true;

                if (held_ms >= RESET_HOLD_MS) {
                    ESP_LOGW(TAG, "BOOT held %d ms — triggering factory reset",
                             held_ms);
                    matter_app_factory_reset();   /* schedules reboot */
                    /* Stay in the overlay until reboot lands us back at boot ROM */
                    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
                }
            }
        } else {
            if (overlay_shown) {
                ui_factory_reset_progress_hide();
                overlay_shown = false;
            }
            held_ms = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(RESET_POLL_MS));
    }
}

/* ── CO2 sensor task ─────────────────────────────────────────────────────
 * Reads MH-Z19C every 5 s and pushes ppm to the UI.
 * On error keeps the last known value; first 3 failures → shows "---".   */
static void co2_task(void *arg)
{
    (void)arg;

    const mhz19_cfg_t cfg = {
        .port    = MHZ19_UART,
        .gpio_rx = MHZ19_RX,
        .gpio_tx = MHZ19_TX,
    };
    ESP_ERROR_CHECK(mhz19_init(&cfg));

    /* MH-Z19C needs up to 3 min warm-up; first reads may be 0 or garbage.
     * We still push them — UI shows "---" for ppm < 0, which we leave as-is
     * until we get a plausible value (> 0). */
    int16_t last_ppm = -1;
    int     errors   = 0;

    while (1) {
        int16_t ppm;
        esp_err_t err = mhz19_read_co2(MHZ19_UART, &ppm);

        if (err == ESP_OK && ppm > 0) {
            last_ppm = ppm;
            errors   = 0;
        } else {
            errors++;
            ESP_LOGW(TAG, "CO2 read error #%d: %s", errors, esp_err_to_name(err));
            if (errors >= 3) last_ppm = -1;   /* show "---" after 3 consecutive fails */
        }

        if (bsp_lvgl_lock(100)) {
            ui_co2_update(last_ppm);
            bsp_lvgl_unlock();
        }

        /* Push the same reading to the Matter CO2 cluster */
        matter_app_co2_update(last_ppm);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/* ── Entry point ─────────────────────────────────────────────────────────── */
void app_main(void)
{
    ESP_LOGI(TAG, "Booting matter-display...");

    /* NVS must be initialised before WiFi, BLE, or Matter touch it. */
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs erase — reflashing");
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);

    init_time();   /* compile-time fallback until SNTP syncs */

    ESP_ERROR_CHECK(bsp_init());
    ESP_ERROR_CHECK(bsp_display_start());

    /* Start Matter BEFORE LVGL — NimBLE BLE stack needs memory early.
     * Logs the QR code + manual pairing code to the serial console.        */
    ESP_ERROR_CHECK(matter_app_init());

    /* SNTP after Matter so esp_netif/lwip are already running.  Sync happens
     * asynchronously the moment WiFi STA gets an IP.                       */
    start_sntp();

    lv_display_t *disp  = NULL;
    lv_indev_t   *indev = NULL;
    ESP_ERROR_CHECK(bsp_lvgl_start(&disp, &indev));

    lv_indev_set_scroll_limit(indev, 3);
    lv_indev_set_scroll_throw(indev, 8);

    if (bsp_lvgl_lock(-1)) {
        ESP_ERROR_CHECK(ui_start(2500));
        bsp_lvgl_unlock();
    }

    /* Push the current commissioning state to the UI once the splash is
     * done.  matter_app emits kCommissioningComplete only on transitions,
     * so on a cold boot of an already-paired device it would otherwise
     * never fire and the tile would stay in "Pair Now" alert state.       */
    ui_matter_set_commissioned(matter_app_is_commissioned());

    xTaskCreate(co2_task, "co2", 4096, NULL, 5, NULL);
    xTaskCreate(factory_reset_task, "fact_reset", 3072, NULL, 4, NULL);

    ESP_LOGI(TAG, "UI running");
}
