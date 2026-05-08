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
 * commissioned = true  → "● PAIRED"  (teal)
 * commissioned = false → "○ SETUP"   (muted) */
void ui_matter_set_commissioned(bool commissioned);

#ifdef __cplusplus
}
#endif
