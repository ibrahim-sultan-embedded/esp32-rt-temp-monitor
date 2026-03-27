#include "sensor_ds18b20.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"

#include "onewire_bus.h"
#include "ds18b20.h"

#define TAG "ds18b20_drv"

static int s_gpio = -1;
static bool s_initialized = false;

static onewire_bus_handle_t s_bus = NULL;
static ds18b20_device_handle_t s_sensor = NULL;

bool ds18b20_init(int gpio_num)
{
    if (s_initialized) {
        return true;
    }

    s_gpio = gpio_num;

    onewire_bus_config_t bus_config = {
        .bus_gpio_num = s_gpio,
        .flags = {
            .en_pull_up = true,
        }
    };

    onewire_bus_rmt_config_t rmt_config = {
        .max_rx_bytes = 10,
    };

    esp_err_t err = onewire_new_bus_rmt(&bus_config, &rmt_config, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "onewire_new_bus_rmt failed: %s", esp_err_to_name(err));
        return false;
    }

    onewire_device_iter_handle_t iter = NULL;
    onewire_device_t next_dev;
    err = onewire_new_device_iter(s_bus, &iter);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "onewire_new_device_iter failed: %s", esp_err_to_name(err));
        return false;
    }

    bool found = false;
    do {
        err = onewire_device_iter_get_next(iter, &next_dev);
        if (err == ESP_OK) {
            ds18b20_config_t ds_cfg = {0};
            if (ds18b20_new_device_from_enumeration(&next_dev, &ds_cfg, &s_sensor) == ESP_OK) {
                onewire_device_address_t address = 0;
                ds18b20_get_device_address(s_sensor, &address);
                ESP_LOGI(TAG, "Found DS18B20 addr=%016llX", (unsigned long long)address);
                found = true;
                break;
            }
        }
    } while (err != ESP_ERR_NOT_FOUND);

    onewire_del_device_iter(iter);

    if (!found) {
        ESP_LOGE(TAG, "No DS18B20 found on GPIO %d", s_gpio);
        return false;
    }

    s_initialized = true;
    return true;
}

bool ds18b20_read_temp(float *temp_c)
{
    if (temp_c == NULL) {
        return false;
    }

    if (!s_initialized || s_sensor == NULL || s_bus == NULL) {
        ESP_LOGE(TAG, "Sensor not initialized");
        return false;
    }

    esp_err_t err = ds18b20_trigger_temperature_conversion_for_all(s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "trigger conversion failed: %s", esp_err_to_name(err));
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(800));

    err = ds18b20_get_temperature(s_sensor, temp_c);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "read temperature failed: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}