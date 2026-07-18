# RobotBuddy Pomodoro Timer Component
#
# Pomodoro timer with event bus integration
#
idf_component_register(
    SRCS "pomodoro.c"
    INCLUDE_DIRS "include"
    REQUIRES framework
             esp_log
             esp_timer
             freertos
             log
)
