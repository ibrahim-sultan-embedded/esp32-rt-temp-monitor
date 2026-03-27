#ifndef SENSOR_DS18B20_H
#define SENSOR_DS18B20_H

#include <stdbool.h>

bool ds18b20_init(int gpio_num);
bool ds18b20_read_temp(float *temp_c);

#endif