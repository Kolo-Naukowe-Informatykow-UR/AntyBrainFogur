#include "mhz19.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "mhz19";

/* MH-Z19C UART settings */
#define MHZ19_BAUD       9600
#define MHZ19_BUF        256
#define MHZ19_TIMEOUT_MS 120

/* Read-CO2 command (9 bytes, checksum pre-computed = 0x79) */
static const uint8_t CMD_READ_CO2[9] = {
    0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79
};

static uint8_t checksum(const uint8_t *buf)
{
    uint8_t s = 0;
    for (int i = 1; i <= 7; i++) s += buf[i];
    return (uint8_t)(0xFF - s + 1);
}

esp_err_t mhz19_init(const mhz19_cfg_t *cfg)
{
    const uart_config_t ucfg = {
        .baud_rate  = MHZ19_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_RETURN_ON_ERROR(uart_driver_install(cfg->port, MHZ19_BUF, 0, 0, NULL, 0),
                        TAG, "uart_driver_install");
    ESP_RETURN_ON_ERROR(uart_param_config(cfg->port, &ucfg),
                        TAG, "uart_param_config");
    ESP_RETURN_ON_ERROR(uart_set_pin(cfg->port, cfg->gpio_tx, cfg->gpio_rx,
                                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE),
                        TAG, "uart_set_pin");

    ESP_LOGI(TAG, "ready on UART%d  RX=GPIO%d  TX=GPIO%d",
             cfg->port, cfg->gpio_rx, cfg->gpio_tx);
    return ESP_OK;
}

esp_err_t mhz19_read_co2(uart_port_t port, int16_t *ppm)
{
    uart_flush(port);
    uart_write_bytes(port, (const char *)CMD_READ_CO2, sizeof(CMD_READ_CO2));

    uint8_t resp[9] = {0};
    int got = uart_read_bytes(port, resp, sizeof(resp),
                              pdMS_TO_TICKS(MHZ19_TIMEOUT_MS));

    if (got < 9) {
        ESP_LOGW(TAG, "timeout — got %d/9 bytes", got);
        return ESP_ERR_TIMEOUT;
    }
    if (resp[0] != 0xFF || resp[1] != 0x86) {
        ESP_LOGW(TAG, "bad header: %02X %02X", resp[0], resp[1]);
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (resp[8] != checksum(resp)) {
        ESP_LOGW(TAG, "checksum mismatch");
        return ESP_ERR_INVALID_CRC;
    }

    *ppm = (int16_t)(((uint16_t)resp[2] << 8) | resp[3]);
    ESP_LOGD(TAG, "CO2 = %d ppm", *ppm);
    return ESP_OK;
}
