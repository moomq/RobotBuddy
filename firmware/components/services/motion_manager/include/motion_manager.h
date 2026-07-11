/*
 * RobotBuddy — Motion Manager
 * =============================
 * Controls DRV8833 dual H-bridge for differential drive motion.
 * Provides PID-based speed control and motion commands.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"
#include "robot_events.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Configuration
 * ============================================================ */

#define MOTION_TASK_STACK_SIZE   2048
#define MOTION_TASK_PRIORITY     3        /* Medium-high priority */
#define MOTION_TASK_CORE_ID      1        /* Core 1 */
#define MOTION_TASK_PERIOD_MS    10       /* 100 Hz control loop */
#define MOTION_QUEUE_DEPTH        10
#define MOTION_MAX_SPEED          255
#define MOTION_DEFAULT_SPEED      150     /* Default speed (0-255) */

/* ============================================================
 * Types
 * ============================================================ */

typedef struct {
    int pin_ain1, pin_ain2, pin_bin1, pin_bin2;
    uint32_t pwm_freq;
    uint16_t max_speed;            /* Maximum speed (0-255) */
    uint16_t default_speed;        /* Default speed for commands */
} motion_config_t;

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * @brief Initialize motion manager
 *
 * Initializes DRV8833 driver and creates motion control task.
 *
 * @param config Configuration (NULL for defaults from bsp_pinmap)
 * @return ESP_OK on success
 */
esp_err_t motion_manager_init(const motion_config_t *config);

/**
 * @brief Deinitialize motion manager
 * @return ESP_OK on success
 */
esp_err_t motion_manager_deinit(void);

/**
 * @brief Execute a motion command
 *
 * Thread-safe. Sends command to motion task via queue.
 *
 * @param cmd Motion command type
 * @param speed Speed (0-255), use 0 for default
 * @param angle Rotation angle for ROTATE command (±360°)
 * @param duration_ms Duration in ms (0 = until next command)
 * @return ESP_OK on success, ESP_ERR_NO_MEM if queue full
 */
esp_err_t motion_execute(motion_cmd_t cmd, int16_t speed, int16_t angle, uint16_t duration_ms);

/**
 * @brief Emergency stop — immediately stops all motors
 *
 * Can be called from ISR context.
 *
 * @return ESP_OK on success
 */
esp_err_t motion_emergency_stop(void);

/**
 * @brief Check if robot is currently moving
 * @return true if motors are active
 */
bool motion_is_moving(void);

#ifdef __cplusplus
}
#endif