#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise all screens and start the splash animation.
 * Call once, inside bsp_lvgl_lock(), before bsp_lvgl_unlock(). */
esp_err_t ui_start(uint32_t splash_ms);

/* Push a fresh CO2 reading to the display.
 * Call from any task that holds bsp_lvgl_lock(). */
void ui_co2_update(int16_t ppm);

/* Update the Matter commissioning status indicator.
 * Thread-safe — acquires the LVGL lock internally.
 * commissioned = true  → tile/LIVE turn teal, label "Paired"
 * commissioned = false → tile/LIVE turn alert-orange, label "Pair Now" */
void ui_matter_set_commissioned(bool commissioned);

/* Show / update / hide a fullscreen overlay used during the BOOT-button
 * factory-reset long-press.  Thread-safe — acquires LVGL lock internally.
 * pct = 0..100 (0 = just started, 100 = about to fire).                    */
void ui_factory_reset_progress_show(uint8_t pct);
void ui_factory_reset_progress_hide(void);

#ifdef __cplusplus
}
#endif
