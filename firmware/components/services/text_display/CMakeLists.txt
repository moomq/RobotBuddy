# RobotBuddy Text Display Component
#
# Scrolling text messages and status icons for the bottom display area
#
idf_component_register(
    SRCS "text_display.c" "font_5x7.c"
    INCLUDE_DIRS "include"
    REQUIRES framework
             freertos
             log
)
