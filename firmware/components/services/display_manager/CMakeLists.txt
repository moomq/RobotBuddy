# RobotBuddy Display Manager Component
#
# Manages ST7789 LCD, frame buffer, and rendering primitives
#
idf_component_register(
    SRCS "display_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp
             st7789
             freertos
             log
             esp_timer
)