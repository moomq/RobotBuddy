# RobotBuddy Behavior System Component
#
# Central behavior orchestrator with state machine
#
idf_component_register(
    SRCS "behavior_system.c"
    INCLUDE_DIRS "include"
    REQUIRES emotion_engine
             framework
             freertos
             log
             esp_timer
)