# RobotBuddy Audio Manager Component
#
# I2S audio capture and playback with ring buffers
#
idf_component_register(
    SRCS "audio_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp
             framework
             driver
             esp_timer
             freertos
             log
             heap
)