# RobotBuddy Power Manager Component
#
# Dynamic power management with automatic sleep states
#
idf_component_register(
    SRCS "power_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES framework
             esp_log
             esp_timer
             esp_sleep
             esp_wifi
             nvs_flash
             display_manager
             cloud_manager
             freertos
)
