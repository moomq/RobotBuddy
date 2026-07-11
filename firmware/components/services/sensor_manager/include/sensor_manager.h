/*
 * RobotBuddy — Sensor Manager
 * ==============================
 * Polls IMU, IR sensors, and battery at defined intervals.
 * Publishes sensor events to the event bus.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"
#include "robot_events.h"
#include "driver/i2c.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Configuration
 * ============================================================ */

#define SENSOR_TASK_STACK_SIZE    2048
#define SENSOR_TASK_PRIORITY      2
#define SENSOR_TASK_CORE_ID       1
#define SENSOR_POLL_INTERVAL_MS   50      /* 20 Hz */
#define BATTERY_POLL_DIVIDER      20      /* Battery read every 20th cycle (1 Hz) */
#define SENSOR_EVENT_QUEUE_DEPTH  8

/* ============================================================
 * Types
 * ============================================================ */

typedef struct {
    int pin_obstacle_left, pin_obstacle_right;
    int pin_edge_left, pin_edge_right;
    i2c_port_t i2c_port;
    uint8_t mpu6050_addr;
    int adc_pin;
    float battery_divider_ratio;
} sensor_config_t;

typedef void (*sensor_event_cb_t)(robot_event_id_t event, const sensor_data_t *data);

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t sensor_manager_init(const sensor_config_t *config);
esp_err_t sensor_manager_deinit(void);
const sensor_data_t *sensor_get_data(void);
esp_err_t sensor_register_callback(sensor_event_cb_t cb);

#ifdef __cplusplus
}
#endif