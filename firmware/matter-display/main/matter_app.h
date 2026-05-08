#pragma once

/**
 * matter_app — C interface for the Matter Air Quality Sensor integration.
 *
 * Exposes a plain-C API so main.c (C) can call into the C++ esp-matter stack
 * without any header pollution.  All implementation lives in matter_app.cpp.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdint.h>

/**
 * Initialise the Matter stack.
 *
 * Creates a Matter node with an Air Quality Sensor endpoint and a
 * CarbonDioxideConcentrationMeasurement cluster (0x040D).
 * Starts BLE/NimBLE advertising so a Matter controller can commission the
 * device.  The QR-code and manual pairing code are printed to the serial
 * console.
 *
 * Must be called after nvs_flash_init() (handled by bsp_init) and before
 * lv_init() so NimBLE gets its memory early.
 *
 * Returns ESP_OK on success.
 */
esp_err_t matter_app_init(void);

/**
 * Push a fresh CO2 reading to the Matter CO2 cluster.
 *
 * Thread-safe — may be called from any FreeRTOS task.
 * No-op when ppm_co2 <= 0.
 */
void matter_app_co2_update(int16_t ppm_co2);

/**
 * Return the QR-code payload string (e.g. "MT:Y3.13 0A0648000…") or NULL.
 * Valid after matter_app_init().  NULL when already commissioned (BLE off).
 */
const char *matter_app_get_qr_code(void);

/**
 * Return the 11-digit manual pairing code (e.g. "749-70-5321") or NULL.
 * Valid after matter_app_init().
 */
const char *matter_app_get_manual_code(void);

#ifdef __cplusplus
}
#endif
