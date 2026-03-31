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
#include "freertos/queue.h"

#include "esp_log.h"

#include "uart_drv.h"
#include "line_proto.h"

#define TEMP_HISTORY_SIZE 10
#define TEMP_QUEUE_LEN    8

typedef enum {
    STATE_NORMAL = 0,
    STATE_WARNING,
    STATE_CRITICAL
} system_state_t;

typedef struct {
    float temperature;
    TickType_t timestamp;
    bool valid;
} temp_msg_t;

typedef struct {
    float last_temp;
    float avg_temp;
    float threshold;
    system_state_t state;
    uint32_t sample_count;
    uint32_t error_count;
} monitor_data_t;

static float temp_history[TEMP_HISTORY_SIZE];
static int temp_index = 0;
static int temp_count = 0;

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
static QueueHandle_t g_temp_queue = NULL;

static monitor_data_t g_monitor = {
    .last_temp = 25.0f,
    .avg_temp = 25.0f,
    .threshold = 30.0f,
    .state = STATE_NORMAL,
    .sample_count = 0,
    .error_count = 0
};

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

static const char *state_to_str(system_state_t s)
{
    switch (s) {
        case STATE_NORMAL:   return "NORMAL";
        case STATE_WARNING:  return "WARNING";
        case STATE_CRITICAL: return "CRITICAL";
        default:             return "UNKNOWN";
    }
}

static system_state_t calc_state(float temp, float threshold)
{
    if (temp >= threshold + 5.0f) {
        return STATE_CRITICAL;
    }
    if (temp >= threshold) {
        return STATE_WARNING;
    }
    return STATE_NORMAL;
}

static float get_avg_temp(void)
{
    if (temp_count == 0) return 0.0f;

    float sum = 0.0f;
    for (int i = 0; i < temp_count; i++) {
        sum += temp_history[i];
    }

    return sum / temp_count;
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

    cli_write_ln(u, "  avg - Show average temperature");
    cli_write_ln(u, "  set_th <temp> - Set warning threshold");
    return 0;
}

static int cmd_status(uart_drv_t *u, int argc, char **argv)
{
    (void)argc;
    (void)argv;

    char buf[192];
    snprintf(buf, sizeof(buf),
             "OK: uart=%d baud=%d echo=%s period=%lu ms temp=%.2f C avg=%.2f C threshold=%.2f C state=%s errors=%lu samples=%lu",
             (int)g_uart.uart_num,
             g_uart.baud,
             g_echo ? "on" : "off",
             (unsigned long)g_temp_period_ms,
             g_monitor.last_temp,
             g_monitor.avg_temp,
             g_monitor.threshold,
             state_to_str(g_monitor.state),
             (unsigned long)g_monitor.error_count,
             (unsigned long)g_monitor.sample_count);

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
    snprintf(buf, sizeof(buf), "temperature=%.2f C", g_monitor.last_temp);
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
        char buf[64];
        snprintf(buf, sizeof(buf), "avg=%.2f C", g_monitor.avg_temp);
        cli_write_ln(u, buf);
    }
    else if (strncmp(line, "set_th ", 7) == 0) {
        float th = atof(line + 7);

        if (th < -55.0f || th > 125.0f) {
            cli_write_ln(u, "ERR: threshold out of range");
            return;
        }

        g_monitor.threshold = th;

        char buf[64];
        snprintf(buf, sizeof(buf), "threshold=%.2f C", g_monitor.threshold);
        cli_write_ln(u, buf);
    }
    else {
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

                if (b == '\r') {
                    b = '\n';
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
    temp_msg_t msg;

    if (!ds18b20_init(4)) {
        ESP_LOGE(TAG, "DS18B20 init failed");
    }

    while (1) {
        msg.timestamp = xTaskGetTickCount();

        if (ds18b20_read_temp(&temp)) {
            msg.temperature = temp;
            msg.valid = true;
        } else {
            msg.temperature = 0.0f;
            msg.valid = false;
        }

        if (g_temp_queue != NULL) {
            if (xQueueSend(g_temp_queue, &msg, pdMS_TO_TICKS(50)) != pdTRUE) {
                ESP_LOGW(TAG, "temp queue full");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(g_temp_period_ms));
    }
}

static void processing_task(void *arg)
{
    (void)arg;

    temp_msg_t msg;

    while (1) {
        if (xQueueReceive(g_temp_queue, &msg, portMAX_DELAY) == pdTRUE) {
            if (!msg.valid) {
                g_monitor.error_count++;
                ESP_LOGE(TAG, "Temp read error");
                continue;
            }

            g_monitor.last_temp = msg.temperature;
            g_monitor.sample_count++;

            temp_history[temp_index] = msg.temperature;
            temp_index = (temp_index + 1) % TEMP_HISTORY_SIZE;

            if (temp_count < TEMP_HISTORY_SIZE) {
                temp_count++;
            }

            g_monitor.avg_temp = get_avg_temp();
            g_monitor.state = calc_state(msg.temperature, g_monitor.threshold);

            if (g_monitor.state == STATE_WARNING) {
                ESP_LOGW(TAG, "WARNING: High temperature %.2f C", msg.temperature);
            } else if (g_monitor.state == STATE_CRITICAL) {
                ESP_LOGE(TAG, "CRITICAL: Temperature %.2f C", msg.temperature);
            } else {
                ESP_LOGI(TAG, "Temp: %.2f C | Avg: %.2f C | State: %s",
                         g_monitor.last_temp,
                         g_monitor.avg_temp,
                         state_to_str(g_monitor.state));
            }
        }
    }
}

/* ----------------- app_main ----------------- */

void app_main(void)
{
    ESP_LOGI(TAG, "Starting...");

    ESP_ERROR_CHECK(uart_drv_init(&g_uart));

    g_temp_queue = xQueueCreate(TEMP_QUEUE_LEN, sizeof(temp_msg_t));
    if (g_temp_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create temp queue");
        return;
    }

    xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 5, NULL);
    xTaskCreate(temperature_task, "temp_task", 4096, NULL, 5, NULL);
    xTaskCreate(processing_task, "proc_task", 4096, NULL, 5, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}