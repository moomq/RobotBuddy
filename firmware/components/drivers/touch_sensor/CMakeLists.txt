idf_component_register(
    SRCS "touch_sensor.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_log freertos framework
)
