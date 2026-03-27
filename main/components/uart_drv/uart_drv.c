#include "uart_drv.h"
#include "esp_log.h"
#include <string.h>
static const char *TAG = "uart_drv";

esp_err_t uart_drv_init(uart_drv_t *d)
{
    if (!d) return ESP_ERR_INVALID_ARG;

    uart_config_t cfg = {
        .baud_rate = d->baud,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };

    ESP_ERROR_CHECK(uart_param_config(d->uart_num, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(d->uart_num, d->tx_pin, d->rx_pin,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // buffer sizes "כמו בעבודה" – נותנים מרווח
    const int rx_buf = 2048;
    const int tx_buf = 2048;
    const int queue_len = 20;

    ESP_ERROR_CHECK(uart_driver_install(d->uart_num, rx_buf, tx_buf,
                                        queue_len, &d->rx_queue, 0));

    ESP_LOGI(TAG, "UART%d init OK (baud=%d, TX=%d, RX=%d)",
             d->uart_num, d->baud, d->tx_pin, d->rx_pin);
    return ESP_OK;
}

int uart_drv_read(uart_drv_t *d, uint8_t *buf, size_t max_len, TickType_t to)
{
    if (!d || !buf || max_len == 0) return -1;
    // קורא bytes מה-RX buffer שהדרייבר של IDF ממלא (ISR פנימי)
    int n = uart_read_bytes(d->uart_num, buf, max_len, to);
    return n;
}

int uart_drv_write(uart_drv_t *d, const uint8_t *data, size_t len)
{
    if (!d || !data || len == 0) return -1;
    int n = uart_write_bytes(d->uart_num, (const char*)data, len);
    return n;
}

int uart_drv_write_str(uart_drv_t *d, const char *s)
{
    if (!d || !s) return -1;
    return uart_drv_write(d, (const uint8_t*)s, strlen(s));
}
esp_err_t uart_drv_deinit(uart_drv_t *d)
{
    if (!d) return ESP_ERR_INVALID_ARG;

    // אם השתמשת ב-queue של אירועים - פה גם מוחקים task/queue שלך
    esp_err_t err = uart_driver_delete(d->uart_num);
    if (err != ESP_OK) return err;

    d->rx_queue = NULL;
    return ESP_OK;
}