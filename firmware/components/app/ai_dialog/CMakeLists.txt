# RobotBuddy AI Dialog Component
#
# Voice interaction pipeline: ASR → LLM → TTS
#
idf_component_register(
    SRCS "ai_dialog.c"
    INCLUDE_DIRS "include"
    REQUIRES audio_manager
             cloud_manager
             framework
             freertos
             log
             heap
)