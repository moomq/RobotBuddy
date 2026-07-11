# RobotBuddy Emotion Engine Component
#
# Facial expression engine with 6+ emotions and blink animation
#
idf_component_register(
    SRCS "emotion_engine.c"
    INCLUDE_DIRS "include"
    REQUIRES display_manager
             framework
             freertos
             log
)