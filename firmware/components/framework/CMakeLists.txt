# RobotBuddy Framework Component
#
# Event bus, system monitor, and core framework utilities
#
idf_component_register(
    SRCS "event_bus.c"
         "sysmon.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event
             esp_timer
             freertos
             log
             heap
)