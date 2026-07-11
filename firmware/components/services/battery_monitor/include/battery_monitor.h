/*
 * RobotBuddy — Battery Monitor
 * ==============================
 * High-level battery monitoring that integrates the battery driver
 * with the event bus and power management decisions.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Battery thresholds */
#define BATTERY_CRITICAL_VOLTAGE   3.0f   /**< Deep sleep threshold (V) */
#define BATTERY_LOW_VOLTAGE         3.4f   /**< Low power mode threshold (V) */
#define BATTERY_FULL_VOLTAGE        4.2f   /**< Full charge voltage (V) */
#define BATTERY_EMPTY_VOLTAGE       3.0f   /**< Empty voltage (V) */

/* Power modes */
typedef enum {
    POWER_MODE_ACTIVE,       /**< Full functionality */
    POWER_MODE_LIGHT_SLEEP,  /**< WiFi + display off, sensors on */
    POWER_MODE_DEEP_SLEEP,   /**< Only RTC, wake on GPIO or timer */
} power_mode_t;

/**
 * @brief Initialize battery monitor
 * @return ESP_OK on success
 */
esp_err_t battery_monitor_init(void);

/**
 * @brief Deinitialize battery monitor
 * @return ESP_OK on success
 */
esp_err_t battery_monitor_deinit(void);

/**
 * @brief Get current battery voltage
 * @return Voltage in volts (0.0 if error)
 */
float battery_monitor_get_voltage(void);

/**
 * @brief Get current battery percentage
 * @return Percentage 0-100
 */
uint8_t battery_monitor_get_percentage(void);

/**
 * @brief Check if charger is connected
 * @return true if charging
 */
bool battery_monitor_is_charging(void);

/**
 * @brief Get current power mode
 * @return Current power mode
 */
power_mode_t battery_monitor_get_power_mode(void);

#ifdef __cplusplus
}
#endif