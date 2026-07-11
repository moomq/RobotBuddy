idf_component_register(
    SRCS "drv8833.c"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos log
)