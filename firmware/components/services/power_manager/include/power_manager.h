/*
 * RobotBuddy — Power Manager
 * ============================
 * Dynamic power management with automatic sleep states.
 * Monitors user activity and transitions through power states:
 *   ACTIVE -> DISPLAY_DIM -> WIFI_LIGHT_SLEEP -> DEEP_SLEEP
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
 * Configuration Defaults
 * ============================================================ */

#define POWER_MANAGER_DEFAULT_DIM_TIMEOUT_MS          300000   /**< 5 min idle -> dim */
#define POWER_MANAGER_DEFAULT_LIGHT_SLEEP_TIMEOUT_MS   600000   /**< 10 min idle -> light sleep */
#define POWER_MANAGER_DEFAULT_DEEP_SLEEP_TIMEOUT_MS   1800000   /**< 30 min idle -> deep sleep */
#define POWER_MANAGER_DEFAULT_DIM_BRIGHTNESS              32   /**< Dim backlight level */
#define POWER_MANAGER_DEFAULT_ACTIVE_BRIGHTNESS           128   /**< Active backlight level */
#define POWER_MANAGER_DEEP_SLEEP_WAKEUP_TIMER_SEC          60   /**< Deep sleep timer wake (s) */

/* ============================================================
 * Configuration Structure
 * ============================================================ */

/**
 * @brief Power manager configuration
 *
 * Passed to power_manager_init(). Set fields to 0 for defaults.
 */
typedef struct {
    uint32_t dim_timeout_ms;           /**< Idle ms before dimming display */
    uint32_t light_sleep_timeout_ms;   /**< Idle ms before WiFi light sleep */
    uint32_t deep_sleep_timeout_ms;    /**< Idle ms before deep sleep */
    uint8_t  dim_brightness;           /**< Backlight level when dimmed (0-255) */
    uint8_t  active_brightness;        /**< Backlight level when active (0-255) */
} power_config_t;

/**
 * @brief Default configuration initializer
 */
#define POWER_CONFIG_DEFAULT() { \
    .dim_timeout_ms = POWER_MANAGER_DEFAULT_DIM_TIMEOUT_MS, \
    .light_sleep_timeout_ms = POWER_MANAGER_DEFAULT_LIGHT_SLEEP_TIMEOUT_MS, \
    .deep_sleep_timeout_ms = POWER_MANAGER_DEFAULT_DEEP_SLEEP_TIMEOUT_MS, \
    .dim_brightness = POWER_MANAGER_DEFAULT_DIM_BRIGHTNESS, \
    .active_brightness = POWER_MANAGER_DEFAULT_ACTIVE_BRIGHTNESS \
}

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * @brief Initialize the power manager
 *
 * Starts the periodic check task, subscribes to activity events,
 * and sets the initial state to POWER_STATE_ACTIVE.
 * Safe to call multiple times; subsequent calls return ESP_OK.
 *
 * @param config Configuration (NULL for defaults)
 * @return ESP_OK on success
 */
esp_err_t power_manager_init(const power_config_t *config);

/**
 * @brief Deinitialize the power manager
 *
 * Stops the check task, unsubscribes from events, and releases resources.
 *
 * @return ESP_OK on success
 */
esp_err_t power_manager_deinit(void);

/**
 * @brief Get the current power state
 *
 * Thread-safe.
 *
 * @return Current power_state_t value
 */
power_state_t power_manager_get_state(void);

/**
 * @brief Notify the power manager of user activity
 *
 * Resets the idle timer and may trigger a state transition
 * back to ACTIVE if currently in a lower power state.
 * Thread-safe.
 */
void power_manager_notify_activity(void);

/**
 * @brief Force a power state transition
 *
 * Thread-safe. Performs the entry actions for the target state.
 *
 * @param state Target power state
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t power_manager_set_state(power_state_t state);

/**
 * @brief Get the current idle duration
 *
 * Thread-safe.
 *
 * @return Idle time in milliseconds since last activity
 */
uint32_t power_manager_get_idle_ms(void);

#ifdef __cplusplus
}
#endif
