# RobotBuddy Main Component
#
# Core application entry point and initialization
#
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES bsp
             nvs_flash
             esp_wifi
             esp_netif
             esp_event
             driver
             esp_timer
             freertos
             log
)

# Target chip verification
if(NOT CONFIG_IDF_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR "RobotBuddy requires ESP32-S3 target. Run: idf.py set-target esp32s3")
endif()