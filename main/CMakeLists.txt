idf_component_register(
    SRCS "temp_monitor.c" "sensor_ds18b20.c"
    INCLUDE_DIRS "."
    REQUIRES uart_drv line_proto driver freertos
)