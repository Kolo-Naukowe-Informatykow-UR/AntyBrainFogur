#pragma once
#include "esp_err.h"
#include "driver/uart.h"
#include <stdint.h>

typedef struct {
    uart_port_t port;   /* UART_NUM_1 or UART_NUM_2          */
    int         gpio_rx; /* ESP32 RX pin ← sensor TX           */
    int         gpio_tx; /* ESP32 TX pin → sensor RX           */
} mhz19_cfg_t;

/**
 * Initialise UART for MH-Z19C.
 * Call once before mhz19_read_co2().
 */
esp_err_t mhz19_init(const mhz19_cfg_t *cfg);

/**
 * Send read-CO2 command and return concentration in ppm.
 * Blocks up to ~120 ms (sensor response time).
 * Returns ESP_ERR_TIMEOUT / ESP_ERR_INVALID_RESPONSE on failure.
 * On success *ppm is in range 0–5000.
 */
esp_err_t mhz19_read_co2(uart_port_t port, int16_t *ppm);
