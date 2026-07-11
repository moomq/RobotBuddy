/*
 * RobotBuddy — Behavior System
 * ==============================
 * Central behavior orchestrator that receives events from all subsystems
 * and coordinates the robot's responses (expression, motion, audio).
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

#define BEHAVIOR_IDLE_TIMEOUT_MS    (5 * 60 * 1000)   /**< 5 min idle → SLEEP */
#define BEHAVIOR_TASK_STACK_SIZE    4096                /**< Task stack in bytes */
#define BEHAVIOR_TASK_PRIORITY      1                   /**< Low priority */
#define BEHAVIOR_TASK_CORE_ID       1                   /**< Core 1 */
#define BEHAVIOR_TASK_PERIOD_MS     100                 /**< 10 Hz decision loop */
#define BEHAVIOR_EVENT_QUEUE_DEPTH  16                  /**< Event queue depth */
#define BEHAVIOR_ERROR_RECOVERY_MS   3000                /**< Auto-recover from ERROR after 3s */
#define BEHAVIOR_HAPPY_RECOVERY_MS   3000                /**< Auto-recover from HAPPY after 3s */
#define BEHAVIOR_WARNING_RECOVERY_MS 3000                /**< Auto-recover from WARNING after 3s */
#define BEHAVIOR_SLEEP_WAKE_MS       5000                /**< Wake from SLEEP if recent activity */

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * @brief Initialize behavior system
 *
 * Creates the behavior task and subscribes to relevant events.
 *
 * @return ESP_OK on success
 */
esp_err_t behavior_system_init(void);

/**
 * @brief Deinitialize behavior system
 * @return ESP_OK on success
 */
esp_err_t behavior_system_deinit(void);

/**
 * @brief Get current behavior state
 * @return Current behavior state
 */
behavior_state_t behavior_get_state(void);

/**
 * @brief Send an event to the behavior system
 *
 * Thread-safe. Can be called from any task or ISR.
 *
 * @param event Event to send
 * @return ESP_OK on success, ESP_ERR_NO_MEM if queue is full
 */
esp_err_t behavior_send_event(const robot_event_t *event);

/**
 * @brief Check if behavior system is initialized
 * @return true if initialized
 */
bool behavior_system_is_initialized(void);

#ifdef __cplusplus
}
#endif