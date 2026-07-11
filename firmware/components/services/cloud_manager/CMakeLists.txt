# RobotBuddy Cloud Manager Component
#
# Cloud service abstraction for ASR, LLM, and TTS with provider failover
#
idf_component_register(
    SRCS "cloud_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client
             esp_event
             nvs_flash
             json
             esp_timer
             framework
             wifi_manager
             freertos
             log
)