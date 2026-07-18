idf_component_register(
    SRCS "mqtt_client.c"
    INCLUDE_DIRS "include"
    REQUIRES framework
             esp_mqtt
             cjson
             esp_log
             freertos
             wifi_manager
)
