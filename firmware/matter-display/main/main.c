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

/* Wait up to 3 s for "T<unix_timestamp>\n" on UART0.
 * Send from Pi:  echo "T$(date +%s)" > /dev/ttyACM0   */
static void sync_time_uart(void)
{
    ESP_LOGI(TAG, "Waiting 3 s for UART time sync  (send: T%llu)",
             (unsigned long long)time(NULL));

    fd_set rfds;
    struct timeval tv = { .tv_sec = 3, .tv_usec = 0 };
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);

    if (select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv) <= 0) {
        ESP_LOGI(TAG, "No UART sync — keeping compile-time clock");
        return;
    }

    char buf[24] = {0};
    int  n = (int)read(STDIN_FILENO, buf, sizeof(buf) - 1);
    if (n < 2 || buf[0] != 'T') return;

    for (int i = 0; i < n; i++) {
        if (buf[i] == '\r' || buf[i] == '\n') { buf[i] = '\0'; break; }
    }

    time_t ts = (time_t)atol(buf + 1);
    if (ts < 1700000000L) return;

    struct timeval stv = { .tv_sec = ts };
    settimeofday(&stv, NULL);
    ESP_LOGI(TAG, "RTC synced via UART: %s", ctime(&ts));
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

    init_time();
    sync_time_uart();

    ESP_ERROR_CHECK(bsp_init());
    ESP_ERROR_CHECK(bsp_display_start());

    /* Start Matter BEFORE LVGL — NimBLE BLE stack needs memory early.
     * Logs the QR code + manual pairing code to the serial console.        */
    ESP_ERROR_CHECK(matter_app_init());

    lv_display_t *disp  = NULL;
    lv_indev_t   *indev = NULL;
    ESP_ERROR_CHECK(bsp_lvgl_start(&disp, &indev));

    lv_indev_set_scroll_limit(indev, 3);
    lv_indev_set_scroll_throw(indev, 8);

    if (bsp_lvgl_lock(-1)) {
        ESP_ERROR_CHECK(ui_start(2500));
        bsp_lvgl_unlock();
    }

    xTaskCreate(co2_task, "co2", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "UI running");
}
