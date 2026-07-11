# RobotBuddy Main Component
#
# Core application entry point and initialization
#
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES bsp
             framework
             st7789
             drv8833
             mpu6050
             ir_sensor
             battery
             wifi_manager
             display_manager
             emotion_engine
             audio_manager
             cloud_manager
             motion_manager
             sensor_manager
             battery_monitor
             behavior_system
             ai_dialog
             nvs_flash
             esp_wifi
             esp_netif
             esp_event
             driver
             esp_timer
             freertos
             log
             spi_flash
             heap
)

# Target chip verification
if(NOT CONFIG_IDF_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR "RobotBuddy requires ESP32-S3 target. Run: idf.py set-target esp32s3")
endif()