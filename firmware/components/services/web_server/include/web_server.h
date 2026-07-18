/*
 * RobotBuddy — Web Server
 * ========================
 * Local HTTP console for configuration and status monitoring.
 *
 * Provides a web-based dashboard and REST API for:
 *   - Robot status monitoring (battery, emotion, WiFi, etc.)
 *   - WiFi credential configuration
 *   - AI API key management
 *   - OTA firmware upgrade
 *   - Emotion and motion control
 *   - Pomodoro timer control
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

/* ============================================================
 * Configuration Defaults
 * ============================================================ */

#define WEB_SERVER_DEFAULT_PORT           80    /**< Default HTTP port */
#define WEB_SERVER_DEFAULT_MAX_CONNECTIONS 4    /**< Default max concurrent connections */
#define WEB_SERVER_USERNAME_MAX_LEN       32   /**< Maximum username length */
#define WEB_SERVER_PASSWORD_MAX_LEN       32   /**< Maximum password length */

/* ============================================================
 * Types
 * ============================================================ */

/**
 * @brief Web server configuration
 *
 * Passed to web_server_init(). Set auth_enabled to true and provide
 * username/password to enable HTTP Basic Authentication on all endpoints.
 * Pass NULL to web_server_init() for defaults (no auth, port 80).
 */
typedef struct {
    uint16_t port;                                        /**< HTTP port (default 80) */
    uint8_t  max_connections;                             /**< Max concurrent connections (default 4) */
    bool     auth_enabled;                                /**< Enable HTTP Basic Auth */
    char     username[WEB_SERVER_USERNAME_MAX_LEN];       /**< Auth username */
    char     password[WEB_SERVER_PASSWORD_MAX_LEN];       /**< Auth password */
} web_server_config_t;

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * @brief Initialize the web server
 *
 * Prepares the HTTP server configuration. Does NOT start the server —
 * call web_server_start() afterwards.
 * Safe to call multiple times; subsequent calls return ESP_OK.
 *
 * @param config Configuration with defaults. Pass NULL for defaults.
 * @return ESP_OK on success
 *         ESP_ERR_NO_MEM if resource allocation fails
 */
esp_err_t web_server_init(const web_server_config_t *config);

/**
 * @brief Deinitialize the web server
 *
 * Stops the server if running and releases all resources.
 *
 * @return ESP_OK on success
 */
esp_err_t web_server_deinit(void);

/**
 * @brief Start the HTTP server
 *
 * Starts listening on the configured port and registers all
 * REST API endpoints and the embedded dashboard page.
 * Must be called after web_server_init().
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_STATE if not initialized or already running
 *         ESP_FAIL on server start error
 */
esp_err_t web_server_start(void);

/**
 * @brief Stop the HTTP server
 *
 * Gracefully stops the server and unregisters all handlers.
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_STATE if not running
 */
esp_err_t web_server_stop(void);

/**
 * @brief Check whether the web server is currently running
 *
 * @return true if the server is listening and handling requests
 */
bool web_server_is_running(void);

#ifdef __cplusplus
}
#endif
