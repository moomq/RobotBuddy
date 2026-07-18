# RobotBuddy Web Server Component
#
# HTTP console for configuration and status monitoring
#
idf_component_register(
    SRCS "web_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server
             cjson
             nvs_flash
             mbedtls
             esp_wifi
             esp_netif
             framework
             wifi_manager
             cloud_manager
             battery_monitor
             emotion_engine
             motion_manager
             freertos
             log
)
