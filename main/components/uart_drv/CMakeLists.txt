idf_component_register(
    SRCS "uart_drv.c"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos
)