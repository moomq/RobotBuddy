/*
 * RobotBuddy — Pomodoro Timer
 * =============================
 * Pomodoro timer module that integrates with the emotion engine
 * and audio system via the event bus.
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

#define POMODORO_DEFAULT_WORK_MIN       25      /**< Default work duration (minutes) */
#define POMODORO_DEFAULT_BREAK_MIN      5       /**< Default break duration (minutes) */
#define POMODORO_DEFAULT_MAX_ROUNDS     4       /**< Default rounds before long idle */
#define POMODORO_TASK_STACK_SIZE        4096    /**< Task stack in bytes */
#define POMODORO_TASK_PRIORITY          1       /**< Low priority */
#define POMODORO_TASK_CORE_ID           1       /**< Core 1 */
#define POMODORO_TASK_PERIOD_MS         1000    /**< 1 Hz tick loop */
#define POMODORO_EVENT_QUEUE_DEPTH      8       /**< Event queue depth */

/* ============================================================
 * Types
 * ============================================================ */

/**
 * @brief Pomodoro timer configuration
 */
typedef struct {
    uint8_t work_duration_min;      /**< Work period duration in minutes */
    uint8_t break_duration_min;     /**< Break period duration in minutes */
    uint8_t max_rounds;             /**< Number of work/break rounds */
} pomodoro_config_t;

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * @brief Initialize pomodoro timer module
 *
 * Creates the timer task, mutex, and subscribes to touch/MQTT events.
 * When config is NULL, defaults are used.
 *
 * @param config Configuration (NULL for defaults)
 * @return ESP_OK on success
 */
esp_err_t pomodoro_init(const pomodoro_config_t *config);

/**
 * @brief Deinitialize pomodoro timer module
 *
 * Stops the timer, deletes task and resources.
 *
 * @return ESP_OK on success
 */
esp_err_t pomodoro_deinit(void);

/**
 * @brief Start a pomodoro session
 *
 * Transitions from IDLE to WORKING. If already running, no-op.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t pomodoro_start(void);

/**
 * @brief Pause the current pomodoro session
 *
 * Transitions WORKING/BREAK to PAUSED. If already paused or idle, no-op.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t pomodoro_pause(void);

/**
 * @brief Resume a paused pomodoro session
 *
 * Transitions PAUSED back to the previous state (WORKING or BREAK).
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t pomodoro_resume(void);

/**
 * @brief Stop the current pomodoro session
 *
 * Transitions any state to IDLE and resets round counter.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t pomodoro_stop(void);

/**
 * @brief Get current pomodoro state
 * @return Current pomodoro state
 */
pomodoro_state_t pomodoro_get_state(void);

/**
 * @brief Get remaining seconds in current period
 * @return Remaining seconds (0 if idle)
 */
uint16_t pomodoro_get_remaining_sec(void);

/**
 * @brief Get current round number
 * @return Current round (1-based, 0 if idle)
 */
uint8_t pomodoro_get_round(void);

#ifdef __cplusplus
}
#endif
