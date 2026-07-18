/*
 * RobotBuddy — MQTT Client
 * =========================
 * MQTT broker connection management with command subscription,
 * status publishing, and event-bus integration.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Constants
 * ============================================================ */

#define MQTT_BROKER_URL_MAX_LEN  128   /**< Max broker URL length */
#define MQTT_CLIENT_ID_MAX_LEN   64    /**< Max client ID length */
#define MQTT_USERNAME_MAX_LEN    64    /**< Max username length */
#define MQTT_PASSWORD_MAX_LEN    64    /**< Max password length */
#define MQTT_DEVICE_ID_MAX_LEN   32    /**< Max device ID length */
#define MQTT_DEFAULT_KEEPALIVE   60    /**< Default keepalive (seconds) */
#define MQTT_DEFAULT_QOS         1     /**< Default QoS level */
#define MQTT_STATUS_INTERVAL_SEC 30    /**< Status publish interval (seconds) */

/* ============================================================
 * Types
 * ============================================================ */

/**
 * @brief MQTT client configuration
 */
typedef struct {
    char broker_url[MQTT_BROKER_URL_MAX_LEN];  /**< Broker URL (mqtt://host:port) */
    char client_id[MQTT_CLIENT_ID_MAX_LEN];     /**< Client ID */
    char username[MQTT_USERNAME_MAX_LEN];        /**< Auth username (empty = no auth) */
    char password[MQTT_PASSWORD_MAX_LEN];        /**< Auth password (empty = no auth) */
    uint16_t keepalive_sec;                      /**< Keepalive interval (default 60) */
    uint8_t qos;                                 /**< QoS level (default 1) */
    char device_id[MQTT_DEVICE_ID_MAX_LEN];      /**< Device identifier for topic routing */
} mqtt_config_t;

/**
 * @brief Incoming MQTT message callback
 *
 * Called when a message arrives on a subscribed topic.
 *
 * @param topic   Topic string (null-terminated)
 * @param payload Message payload
 * @param len     Payload length
 */
typedef void (*mqtt_message_cb_t)(const char *topic,
                                  const char *payload,
                                  size_t len);

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * @brief Initialize the MQTT client
 *
 * Stores configuration and prepares internal state.
 * Does NOT connect — call mqtt_client_start() afterwards.
 * Safe to call multiple times; subsequent calls return ESP_OK.
 *
 * @param config Pointer to MQTT configuration (must not be NULL)
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_STATE if already initialized
 *         ESP_ERR_INVALID_ARG if config is NULL
 *         ESP_ERR_NO_MEM if mutex creation fails
 */
esp_err_t mqtt_client_init(const mqtt_config_t *config);

/**
 * @brief Deinitialize the MQTT client
 *
 * Disconnects (if connected), destroys the client handle,
 * and frees all resources.
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t mqtt_client_deinit(void);

/**
 * @brief Start the MQTT client
 *
 * Connects to the broker using the configuration provided
 * in mqtt_client_init(). Auto-reconnect is handled internally
 * by the ESP-IDF MQTT library with exponential backoff.
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_STATE if not initialized or already started
 *         ESP_FAIL on internal error
 */
esp_err_t mqtt_client_start(void);

/**
 * @brief Stop the MQTT client
 *
 * Disconnects from the broker and stops the client.
 * Cancels the periodic status timer.
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t mqtt_client_stop(void);

/**
 * @brief Publish a message to an MQTT topic
 *
 * Thread-safe. The topic is prefixed with
 * "robotbuddy/{device_id}/status/" automatically if the
 * given topic does not already start with "robotbuddy/".
 *
 * @param topic Topic suffix or full topic path
 * @param data  Payload data
 * @param len   Payload length
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_STATE if not initialized or not connected
 *         ESP_ERR_INVALID_ARG if topic or data is NULL
 *         ESP_FAIL on publish failure
 */
esp_err_t mqtt_client_publish(const char *topic,
                               const char *data,
                               size_t len);

/**
 * @brief Check whether the MQTT client is currently connected
 *
 * Thread-safe.
 *
 * @return true if connected to the broker
 */
bool mqtt_client_is_connected(void);

#ifdef __cplusplus
}
#endif
