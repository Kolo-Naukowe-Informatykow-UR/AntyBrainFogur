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
#include "bsp/bsp.h"
#include "ui.h"

static const char *TAG = "matter-display";

/* Fallback: set RTC to compile time if not already set. */
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

/* Wait up to 3 s for "T<unix_timestamp>\n" on stdin (UART0).
 * Send from Pi shell:  echo "T$(date +%s)" > /dev/ttyACM0
 * This overwrites the compile-time fallback with the real wall clock. */
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

    /* strip newline / CR */
    for (int i = 0; i < n; i++) {
        if (buf[i] == '\r' || buf[i] == '\n') { buf[i] = '\0'; break; }
    }

    time_t ts = (time_t)atol(buf + 1);
    if (ts < 1700000000L) return;          /* sanity: must be after Nov 2023 */

    struct timeval stv = { .tv_sec = ts };
    settimeofday(&stv, NULL);
    ESP_LOGI(TAG, "RTC synced via UART: %s", ctime(&ts));
}

void app_main(void)
{
    ESP_LOGI(TAG, "Booting matter-display...");

    init_time();
    sync_time_uart();

    ESP_ERROR_CHECK(bsp_init());
    ESP_ERROR_CHECK(bsp_display_start());

    lv_display_t *disp  = NULL;
    lv_indev_t   *indev = NULL;
    ESP_ERROR_CHECK(bsp_lvgl_start(&disp, &indev));

    lv_indev_set_scroll_limit(indev, 3);
    lv_indev_set_scroll_throw(indev, 8);

    if (bsp_lvgl_lock(-1)) {
        ESP_ERROR_CHECK(ui_start(2500));
        bsp_lvgl_unlock();
    }

    ESP_LOGI(TAG, "UI running");
}
