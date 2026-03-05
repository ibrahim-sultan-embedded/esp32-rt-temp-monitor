#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

QueueHandle_t temp_queue;

void sensor_task(void *pv)
{
    int temperature = 25;

    while (1)
    {
        temperature++;

        xQueueSend(temp_queue, &temperature, portMAX_DELAY);

        printf("Sensor sent: %d\n", temperature);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void logger_task(void *pv)
{
    int received_temp;

    while (1)
    {
        if (xQueueReceive(temp_queue, &received_temp, portMAX_DELAY))
        {
            printf("Logger received: %d\n", received_temp);
        }
    }
}

void app_main(void)
{
    temp_queue = xQueueCreate(5, sizeof(int));

    xTaskCreate(sensor_task, "sensor", 2048, NULL, 5, NULL);
    xTaskCreate(logger_task, "logger", 2048, NULL, 5, NULL);
}