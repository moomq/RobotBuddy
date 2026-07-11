# RobotBuddy Motion Manager Component
#
# Differential drive motor control with command queue
#
idf_component_register(
    SRCS "motion_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES drv8833
             bsp
             framework
             freertos
             log
)