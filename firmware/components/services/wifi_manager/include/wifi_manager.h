/*
 * RobotBuddy — WiFi Manager
 * ==========================
 * WiFi connection management service for ESP32-S3.
 *
 * Provides automatic STA connection with NVS-stored credentials,
 * SmartConfig (ESP-TOUCH) provisioning, exponential-backoff
 * reconnection, and state-change callback notifications.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "robot_events.h"   /* wifi_state_t is defined here */
#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Constants
 * ============================================================ */

#define WIFI_MANAGER_NVS_NAMESPACE  "wifi_mgr"   /**< NVS namespace */
#define WIFI_MANAGER_NVS_SSID_KEY    "ssid"        /**< NVS key for SSID */
#define WIFI_MANAGER_NVS_PASS_KEY   "pass"        /**< NVS key for password */
#define WIFI_MANAGER_MAX_CALLBACKS  4             /**< Max registered callbacks */

/* ============================================================
 * Types
 * ============================================================ */

/**
 * @brief WiFi state change callback
 *
 * Called when the WiFi state machine transitions.
 *
 * @param state New WiFi state
 * @param arg   User-provided context (registered with the callback)
 */
typedef void (*wifi_event_cb_t)(wifi_state_t state, void *arg);

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * @brief Initialize the WiFi manager
 *
 * Initializes NVS, netif, event loop, and WiFi STA driver.
 * Does NOT start the connection — call wifi_manager_start() afterwards.
 * Safe to call multiple times; subsequent calls return ESP_OK.
 *
 * @return ESP_OK on success
 *         ESP_FAIL on internal initialization error
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Start WiFi connection
 *
 * Attempts to connect using credentials stored in NVS.
 * If no credentials are stored, falls through to the DISCONNECTED state.
 * Must be called after wifi_manager_init().
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t wifi_manager_start(void);

/**
 * @brief Stop WiFi and disconnect
 *
 * Disconnects STA, stops the WiFi driver, and cancels any
 * pending reconnection timers. Transitions to IDLE state.
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t wifi_manager_stop(void);

/**
 * @brief Get the current WiFi state
 *
 * Thread-safe.
 *
 * @return Current wifi_state_t value
 */
wifi_state_t wifi_manager_get_state(void);

/**
 * @brief Start SmartConfig (ESP-TOUCH) provisioning
 *
 * Enters SMARTCONFIG state and waits for a mobile app to
 * send credentials. On success, credentials are stored to NVS
 * and the manager transitions to CONNECTING → CONNECTED.
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_STATE if not initialized or already provisioning
 */
esp_err_t wifi_manager_start_smartconfig(void);

/**
 * @brief Register a state-change callback
 *
 * Up to WIFI_MANAGER_MAX_CALLBACKS callbacks may be registered.
 * Callbacks are invoked from the WiFi event task context — avoid
 * blocking or heavy work inside the callback.
 *
 * @param cb  Callback function (must not be NULL)
 * @param arg User context passed to the callback (may be NULL)
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if cb is NULL
 *         ESP_ERR_NO_MEM if no free callback slots
 */
esp_err_t wifi_manager_register_callback(wifi_event_cb_t cb, void *arg);

/**
 * @brief Check whether WiFi is currently connected (has IP)
 *
 * @return true if state == WIFI_STATE_CONNECTED
 */
bool wifi_manager_is_connected(void);

#ifdef __cplusplus
}
#endif