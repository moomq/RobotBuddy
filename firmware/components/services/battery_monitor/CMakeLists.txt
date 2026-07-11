# RobotBuddy Battery Monitor Component
#
# High-level battery monitoring with event bus integration
#
idf_component_register(
    SRCS "battery_monitor.c"
    INCLUDE_DIRS "include"
    REQUIRES battery
             bsp
             framework
             freertos
             log
)