/*
 * RobotBuddy — Wake Word Detection
 * ==================================
 * Offline wake word detection using ESP-SR (WakeNet).
 * Falls back to audio energy detection when ESP-SR is unavailable.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Configuration
 * ============================================================ */

#define WAKE_WORD_TASK_STACK_SIZE   (20 * 1024)  /* 20 KB stack */
#define WAKE_WORD_TASK_PRIORITY     7             /* Same as audio playback */
#define WAKE_WORD_TASK_CORE_ID      0             /* Core 0 (WiFi core) */
#define WAKE_WORD_DEFAULT_THRESHOLD 0.5f          /* Default detection confidence */
#define WAKE_WORD_MODEL_NAME_LEN    64            /* Max model name length */

/* ============================================================
 * Types
 * ============================================================ */

/**
 * @brief Wake word detection configuration
 */
typedef struct {
    char model_name[WAKE_WORD_MODEL_NAME_LEN];  /**< WakeNet model name (e.g. "hilexin") */
    float detection_threshold;                    /**< Detection threshold (0.0 - 1.0) */
    uint8_t channel;                              /**< I2S channel number for audio input */
} wake_word_config_t;

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * @brief Initialize the wake word detection module
 *
 * Sets up ESP-SR (or stub) engine and creates the detection task.
 * Subscribes to audio playback events to pause/resume detection.
 *
 * @param config Configuration (NULL for defaults)
 * @return ESP_OK on success, ESP_ERR_NO_MEM or ESP_FAIL on failure
 */
esp_err_t wake_word_init(const wake_word_config_t *config);

/**
 * @brief Deinitialize the wake word detection module
 *
 * Stops detection, deletes the task, and releases all resources.
 *
 * @return ESP_OK on success
 */
esp_err_t wake_word_deinit(void);

/**
 * @brief Start wake word listening
 *
 * Begins feeding audio data to the detection engine.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t wake_word_start(void);

/**
 * @brief Stop wake word listening
 *
 * Pauses audio processing in the detection engine.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t wake_word_stop(void);

/**
 * @brief Check if wake word detection is currently listening
 *
 * @return true if listening, false otherwise
 */
bool wake_word_is_listening(void);

#ifdef __cplusplus
}
#endif
