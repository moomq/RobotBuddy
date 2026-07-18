# RobotBuddy Wake Word Detection Component
#
# Offline wake word detection using ESP-SR (WakeNet).
# Falls back to audio energy detection when ESP-SR is unavailable.
#
idf_component_register(
    SRCS "wake_word.c"
    INCLUDE_DIRS "include"
    REQUIRES framework
             esp_log
             freertos
             esp_timer
)
