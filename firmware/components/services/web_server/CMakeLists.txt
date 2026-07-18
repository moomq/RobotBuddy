idf_component_register(
    SRCS "web_server.c"
    INCLUDE_DIRS "include"
    REQUIRES "framework" "esp_http_server" "cjson" "nvs_flash" "esp_log" "freertos" "emotion_engine" "mqtt_client" "ota_service"
)
