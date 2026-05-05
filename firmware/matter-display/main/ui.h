#pragma once
#include "esp_err.h"
#include <stdint.h>

esp_err_t ui_start(uint32_t splash_ms);

/* Call from UART task inside bsp_lvgl_lock() when a sensor reading arrives. */
void ui_co2_update(int16_t ppm);
