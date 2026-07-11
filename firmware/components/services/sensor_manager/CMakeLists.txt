# RobotBuddy Sensor Manager Component
#
# Polls IMU, IR sensors, and battery; publishes events
#
idf_component_register(
    SRCS "sensor_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES mpu6050
             ir_sensor
             battery
             bsp
             framework
             freertos
             log
)