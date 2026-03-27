// main/temp_monitor.c

#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "sensor_ds18b20.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "uart_drv.h"
#include "line_proto.h"

#define TEMP_HISTORY_SIZE 10

static float temp_history[TEMP_HISTORY_SIZE];
static int temp_index = 0;
static int temp_count = 0;

static float g_threshold = 30.0;
static const char *TAG = "app";

static uart_drv_t g_uart = {
    .uart_num = UART_NUM_0,
    .tx_pin   = 1,
    .rx_pin   = 3,
    .baud     = 115200,
    .rx_queue = NULL
};

static bool g_echo = true;
static uint32_t g_temp_period_ms = 5000;
static float g_last_temp = 25.0f;

/* ----------------- utils ----------------- */

static void str_trim(char *s)
{
    if (!s) return;

    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);

    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[n - 1] = 0;
        n--;
    }
}

static void cli_write_ln(uart_drv_t *u, const char *msg)
{
    if (!u || !msg) return;
    uart_drv_write(u, (const uint8_t *)msg, strlen(msg));
    uart_drv_write(u, (const uint8_t *)"\r\n", 2);
}

/* ----------------- CLI types ----------------- */

typedef int (*cli_fn_t)(uart_drv_t *u, int argc, char **argv);

typedef struct {
    const char *name;
    cli_fn_t fn;
    const char *help;
} cli_cmd_t;

/* ----------------- command declarations ----------------- */

static int cmd_help(uart_drv_t *u, int argc, char **argv);
static int cmd_status(uart_drv_t *u, int argc, char **argv);
static int cmd_echo(uart_drv_t *u, int argc, char **argv);
static int cmd_temp(uart_drv_t *u, int argc, char **argv);
static int cmd_stats(uart_drv_t *u, int argc, char **argv);
static int cmd_set(uart_drv_t *u, int argc, char **argv);
static int cmd_get(uart_drv_t *u, int argc, char **argv);

/* ----------------- command table ----------------- */

static const cli_cmd_t g_cmds[] = {
    {"help",   cmd_help,   "Show commands"},
    {"status", cmd_status, "System status"},
    {"echo",   cmd_echo,   "echo on|off"},
    {"temp",   cmd_temp,   "Read current temperature"},
    {"stats",  cmd_stats,  "Show UART configuration"},
    {"set",    cmd_set,    "set period <ms>"},
    {"get",    cmd_get,    "get period"},
};

static const int g_cmds_n = (int)(sizeof(g_cmds) / sizeof(g_cmds[0]));

/* ----------------- tokenizer ----------------- */

static int cli_tokenize(char *line, char **argv, int max_argv)
{
    int argc = 0;
    char *p = line;

    while (*p && argc < max_argv) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        argv[argc++] = p;

        while (*p && !isspace((unsigned char)*p)) p++;
        if (*p) *p++ = 0;
    }

    return argc;
}

/* ----------------- command handlers ----------------- */

static int cmd_help(uart_drv_t *u, int argc, char **argv)
{
    (void)argc;
    (void)argv;

    cli_write_ln(u, "Commands:");
    for (int i = 0; i < g_cmds_n; i++) {
        uart_drv_write(u, (const uint8_t *)"  ", 2);
        uart_drv_write(u, (const uint8_t *)g_cmds[i].name, strlen(g_cmds[i].name));
        uart_drv_write(u, (const uint8_t *)" - ", 3);
        uart_drv_write(u, (const uint8_t *)g_cmds[i].help, strlen(g_cmds[i].help));
        uart_drv_write(u, (const uint8_t *)"\r\n", 2);
    }
    return 0;
}

static int cmd_status(uart_drv_t *u, int argc, char **argv)
{
    (void)argc;
    (void)argv;

    char buf[128];
    snprintf(buf, sizeof(buf),
             "OK: uart=%d baud=%d echo=%s period=%lu ms temp=%.2f C",
             (int)g_uart.uart_num,
             g_uart.baud,
             g_echo ? "on" : "off",
             (unsigned long)g_temp_period_ms,
             g_last_temp);

    cli_write_ln(u, buf);
    return 0;
}

static int cmd_echo(uart_drv_t *u, int argc, char **argv)
{
    if (argc != 2) {
        cli_write_ln(u, "usage: echo on|off");
        return -1;
    }

    if (strcmp(argv[1], "on") == 0) {
        g_echo = true;
        cli_write_ln(u, "echo=on");
        return 0;
    }

    if (strcmp(argv[1], "off") == 0) {
        g_echo = false;
        cli_write_ln(u, "echo=off");
        return 0;
    }

    cli_write_ln(u, "usage: echo on|off");
    return -1;
}

static int cmd_temp(uart_drv_t *u, int argc, char **argv)
{
    (void)argc;
    (void)argv;

    char buf[64];
    snprintf(buf, sizeof(buf), "temperature=%.2f C", g_last_temp);
    cli_write_ln(u, buf);
    return 0;
}

static int cmd_stats(uart_drv_t *u, int argc, char **argv)
{
    (void)argc;
    (void)argv;

    char buf[128];
    snprintf(buf, sizeof(buf),
             "UART=%d BAUD=%d TX=%d RX=%d ECHO=%s",
             (int)g_uart.uart_num,
             g_uart.baud,
             g_uart.tx_pin,
             g_uart.rx_pin,
             g_echo ? "on" : "off");

    cli_write_ln(u, buf);
    return 0;
}

static int cmd_set(uart_drv_t *u, int argc, char **argv)
{
    if (argc != 3) {
        cli_write_ln(u, "usage: set period <ms>");
        return -1;
    }

    if (strcmp(argv[1], "period") != 0) {
        cli_write_ln(u, "usage: set period <ms>");
        return -1;
    }

    int period = atoi(argv[2]);
    if (period < 100 || period > 10000) {
        cli_write_ln(u, "ERR: period must be 100..10000 ms");
        return -1;
    }

    g_temp_period_ms = (uint32_t)period;
    cli_write_ln(u, "OK: period updated");
    return 0;
}

static int cmd_get(uart_drv_t *u, int argc, char **argv)
{
    (void)argc;
    (void)argv;

    char buf[64];
    snprintf(buf, sizeof(buf), "period=%lu ms", (unsigned long)g_temp_period_ms);
    cli_write_ln(u, buf);
    return 0;
}

static float get_avg_temp(void)
{
    if (temp_count == 0) return 0;

    float sum = 0;
    for (int i = 0; i < temp_count; i++) {
        sum += temp_history[i];
    }

    return sum / temp_count;
}

/* ----------------- CLI dispatcher ----------------- */

static void cli_handle_line(uart_drv_t *u, const char *line_in)
{
    char line[128];
    strncpy(line, line_in, sizeof(line) - 1);
    line[sizeof(line) - 1] = 0;

    str_trim(line);
    if (!line[0]) return;

    char *argv[8];
    int argc = cli_tokenize(line, argv, 8);
    if (argc == 0) return;

    for (int i = 0; i < g_cmds_n; i++) {
        if (strcmp(argv[0], g_cmds[i].name) == 0) {
            g_cmds[i].fn(u, argc, argv);
            return;
        }
    }

    if (strcmp(line, "avg") == 0) {
    float avg = get_avg_temp();
    char buf[64];
    snprintf(buf, sizeof(buf), "avg=%.2f C", avg);
    cli_write_ln(u, buf);
    }
    else if (strncmp(line, "set_th ", 7) == 0) {
    float th = atof(line + 7);
    g_threshold = th;

    char buf[64];
    snprintf(buf, sizeof(buf), "threshold=%.2f C", g_threshold);
    cli_write_ln(u, buf);
    }
    else if (strcmp(line, "status") == 0) {
    char buf[128];
    snprintf(buf, sizeof(buf),
        "Temp=%.2f C | Period=%lu ms | Threshold=%.2f",
        g_last_temp,
        (unsigned long)g_temp_period_ms,
        g_threshold);

    cli_write_ln(u, buf);
    } else {
    cli_write_ln(u, "ERR: unknown command (type: help)");
    }
}

/* ----------------- tasks ----------------- */

static void uart_rx_task(void *arg)
{
    (void)arg;

    uint8_t rx_buf[64];
    char line_storage[256];
    line_proto_t proto;

    line_proto_init(&proto, line_storage, sizeof(line_storage));

    cli_write_ln(&g_uart, "CLI ready. type: help");

    while (1) {
        int n = uart_drv_read(&g_uart, rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(200));

        if (n > 0) {
            for (int i = 0; i < n; i++) {
                uint8_t b = rx_buf[i];

                if (g_echo) {
                    uart_drv_write(&g_uart, &b, 1);
                }
                if(b== '\r'){
                    b= '\n';
                }
                if (line_proto_feed(&proto, (char)b)) {
                    const char *line = line_proto_get_line(&proto);
                    if (line) {
                        cli_handle_line(&g_uart, line);
                    }
                    line_proto_reset(&proto);
                }
            }
        }
    }
}

static void temperature_task(void *arg)
{
    (void)arg;

    float temp = 0.0f;

    ds18b20_init(4);

    while (1) {
        if (ds18b20_read_temp(&temp)) {
            g_last_temp = temp;
            // save history
            temp_history[temp_index] = temp;
            temp_index = (temp_index + 1) % TEMP_HISTORY_SIZE;

            if (temp_count < TEMP_HISTORY_SIZE) {
                  temp_count++;
            }

            // threshold check
            if (temp > g_threshold) {
                 ESP_LOGW(TAG, "WARNING: High temperature %.2f C", temp);
            } 
           //ESP_LOGI(TAG, "Temp: %.2f C", temp);
             } else {
                    ESP_LOGE(TAG, "Temp read error");
             }

        vTaskDelay(pdMS_TO_TICKS(g_temp_period_ms));
    }
}

/* ----------------- app_main ----------------- */

void app_main(void)
{
    ESP_LOGI(TAG, "Starting...");

    ESP_ERROR_CHECK(uart_drv_init(&g_uart));

    xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 5, NULL);
    xTaskCreate(temperature_task, "temp_task", 4096, NULL, 5, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}