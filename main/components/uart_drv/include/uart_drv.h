#pragma once
#include <stddef.h>
#include <stdint.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct {
    uart_port_t uart_num;
    int tx_pin;
    int rx_pin;
    int baud;
    QueueHandle_t rx_queue;   // אירועים מהדרייבר של IDF
} uart_drv_t;

esp_err_t uart_drv_init(uart_drv_t *d);
int uart_drv_read(uart_drv_t *d, uint8_t *buf, size_t max_len, TickType_t to);
int uart_drv_write(uart_drv_t *d, const uint8_t *data, size_t len);
int uart_drv_write_str(uart_drv_t *d, const char *s);
esp_err_t uart_drv_deinit(uart_drv_t *d);