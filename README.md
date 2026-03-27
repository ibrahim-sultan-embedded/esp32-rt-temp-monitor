# ESP32 Temperature Monitor

Real-time embedded system built with ESP32 and FreeRTOS.

## Features
- UART driver
- CLI command interface
- DS18B20 temperature sensor integration
- 1-Wire communication
- FreeRTOS multi-tasking
- Average temperature calculation
- Threshold warning logic

## Commands
- `help`
- `temp`
- `avg`
- `status`
- `set_th <value>`
- `set period <ms>`
- `get period`

## Technologies
- C
- ESP-IDF
- FreeRTOS
- UART
- GPIO
- 1-Wire

## Project Structure
- `main/` - application logic
- `components/uart_drv/` - UART driver
- `components/line_proto/` - line parser
- `main/sensor_ds18b20.*` - DS18B20 sensor layer
