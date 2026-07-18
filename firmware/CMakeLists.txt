# RobotBuddy ESP-IDF Project
#
# This is the main CMakeLists.txt for the RobotBuddy firmware.
# Target: ESP32-S3 with 16MB Flash and 8MB PSRAM (OCTAL mode)
#
cmake_minimum_required(VERSION 3.16)

# Set the component directory where additional components are located.
# ESP-IDF searches ONE level deep from each path, so we list every
# component subdirectory explicitly.
set(EXTRA_COMPONENT_DIRS
    "components/framework"
    "components/bsp"
    "components/drivers/st7789"
    "components/drivers/drv8833"
    "components/drivers/mpu6050"
    "components/drivers/ir_sensor"
    "components/drivers/battery"
    "components/drivers/vl53l0x"
    "components/drivers/touch_sensor"
    "components/services/wifi_manager"
    "components/services/display_manager"
    "components/services/emotion_engine"
    "components/services/audio_manager"
    "components/services/cloud_manager"
    "components/services/motion_manager"
    "components/services/sensor_manager"
    "components/services/battery_monitor"
    "components/services/text_display"
    "components/services/mqtt_client"
    "components/services/web_server"
    "components/services/ota_service"
    "components/services/wake_word"
    "components/services/power_manager"
    "components/app/behavior_system"
    "components/app/ai_dialog"
    "components/app/pomodoro"
)

# Include the IDF project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(robotbuddy)