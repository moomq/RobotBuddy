# RobotBuddy BSP (Board Support Package) Component
#
# Pin definitions, board initialization, and hardware abstraction
#
idf_component_register(
    SRCS "bsp_board.c"
    INCLUDE_DIRS "include"
    REQUIRES driver
             esp_timer
             esp_rom
             log
             freertos
)