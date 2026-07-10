# RobotBuddy ESP-IDF Project
#
# This is the main CMakeLists.txt for the RobotBuddy firmware.
# Target: ESP32-S3 with 16MB Flash and 8MB PSRAM (OCTAL mode)
#
cmake_minimum_required(VERSION 3.16)

# Set the component directory where additional components are located
set(EXTRA_COMPONENT_DIRS "components")

# Include the IDF project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(robotbuddy)