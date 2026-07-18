# RobotBuddy OTA Service Component
#
# Over-The-Air firmware upgrade via HTTP download with SHA256 verification
#
idf_component_register(
    SRCS "ota_service.c"
    INCLUDE_DIRS "include"
    REQUIRES framework
             esp_http_client
             esp_ota
             mbedtls
             nvs_flash
             esp_log
)
