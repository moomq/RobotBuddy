/*
 * RobotBuddy — OTA Service
 * =========================
 * Over-The-Air firmware upgrade service for ESP32-S3.
 *
 * Downloads firmware images via HTTP, verifies integrity with SHA256,
 * and applies updates using the ESP-IDF OTA partition mechanism.
 * Supports automatic rollback on boot failure.
 *
 * Features:
 *   - HTTP firmware download with 4KB chunked reads (PSRAM-backed)
 *   - SHA256 integrity verification (mbedtls)
 *   - Firmware header magic byte validation
 *   - Optional size check when expected_size > 0
 *   - State machine: IDLE -> DOWNLOADING -> VERIFYING -> APPLYING -> REBOOTING
 *   - Automatic rollback via esp_ota_mark_app_invalid_rollback_and_reboot()
 *   - Progress tracking via event bus (EVENT_OTA_PROGRESS)
 *   - Thread-safe: mutex prevents concurrent OTA operations
 *   - Configurable HTTP timeouts (default 30s connect, 60s total)
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"
#include "robot_events.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Configuration Defaults
 * ============================================================ */

/** HTTP connection timeout in milliseconds */
#define OTA_DEFAULT_CONNECT_TIMEOUT_MS   30000

/** Total HTTP operation timeout in milliseconds */
#define OTA_DEFAULT_TOTAL_TIMEOUT_MS     60000

/** Download chunk size in bytes (allocated in PSRAM) */
#define OTA_DOWNLOAD_CHUNK_SIZE          4096

/** Maximum firmware URL length */
#define OTA_URL_MAX_LEN                  256

/** SHA256 hex string length (64 hex chars + null terminator) */
#define OTA_SHA256_HEX_LEN               65

/** Delay before reboot after successful OTA (ms) */
#define OTA_REBOOT_DELAY_MS              1000

/* ============================================================
 * Configuration Structure
 * ============================================================ */

/**
 * @brief OTA service configuration
 *
 * Passed to ota_service_init(). Provides default timeouts and
 * optional firmware validation parameters.
 */
typedef struct {
    char firmware_url[OTA_URL_MAX_LEN];   /**< Default firmware URL (optional) */
    uint32_t expected_size;                /**< Expected firmware size (0 = skip size check) */
    char expected_sha256[OTA_SHA256_HEX_LEN]; /**< Expected SHA256 hex (empty = skip SHA256 check) */
    uint32_t connect_timeout_ms;           /**< HTTP connect timeout (0 = use default) */
    uint32_t total_timeout_ms;             /**< Total HTTP timeout (0 = use default) */
} ota_config_t;

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * @brief Initialize the OTA service
 *
 * Initializes the internal mutex and resets state to OTA_STATE_IDLE.
 * Safe to call multiple times; subsequent calls return ESP_OK.
 *
 * @param config Configuration with defaults. Pass NULL for defaults.
 * @return ESP_OK on success
 *         ESP_ERR_NO_MEM if mutex allocation fails
 */
esp_err_t ota_service_init(const ota_config_t *config);

/**
 * @brief Deinitialize the OTA service
 *
 * Cancels any in-flight OTA operation, releases resources,
 * and resets state to OTA_STATE_IDLE.
 *
 * @return ESP_OK on success
 */
esp_err_t ota_service_deinit(void);

/**
 * @brief Start an OTA firmware upgrade
 *
 * Downloads firmware from the given URL, verifies SHA256 if provided,
 * and writes to the OTA partition. On success, sets the boot partition
 * and reboots after a 1-second delay.
 *
 * Publishes:
 *   - EVENT_OTA_START when download begins
 *   - EVENT_OTA_PROGRESS periodically during download
 *   - EVENT_OTA_COMPLETE on successful verification and apply
 *   - EVENT_OTA_ERROR on any failure
 *
 * On error, transitions to OTA_STATE_ERROR. If the new firmware fails
 * to boot, the ESP-IDF bootloader will automatically roll back.
 *
 * @param url    HTTP(S) URL to download firmware from (null-terminated)
 * @param sha256 Expected SHA256 hex string (may be NULL or empty to skip)
 * @return ESP_OK on success (device will reboot)
 *         ESP_ERR_INVALID_ARG if url is NULL or empty
 *         ESP_ERR_INVALID_STATE if not initialized or OTA already in progress
 *         ESP_FAIL on download, verification, or write error
 */
esp_err_t ota_start(const char *url, const char *sha256);

/**
 * @brief Cancel an in-flight OTA operation
 *
 * Aborts the current download and resets state to OTA_STATE_IDLE.
 * The partially written OTA partition is not cleaned up; it will be
 * overwritten on the next OTA attempt.
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_STATE if not initialized or no OTA in progress
 */
esp_err_t ota_cancel(void);

/**
 * @brief Get the current download progress percentage
 *
 * Thread-safe.
 *
 * @return Progress percentage (0-100), or 0 if not downloading
 */
uint8_t ota_get_progress(void);

/**
 * @brief Get the current OTA state
 *
 * Thread-safe.
 *
 * @return Current ota_state_t value
 */
ota_state_t ota_get_state(void);

#ifdef __cplusplus
}
#endif
