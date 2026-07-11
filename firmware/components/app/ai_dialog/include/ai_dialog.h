/*
 * RobotBuddy — AI Dialog
 * =========================
 * Orchestrates the full voice interaction pipeline:
 * ASR (record) → LLM (think) → TTS (speak)
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

#define AI_DIALOG_TASK_STACK_SIZE   8192
#define AI_DIALOG_TASK_PRIORITY      4      /* Same as cloud_task */
#define AI_DIALOG_TASK_CORE_ID       0      /* Core 0 (WiFi core) */
#define AI_DIALOG_MAX_ROUNDS         5      /* Conversation history depth */
#define AI_DIALOG_MAX_TEXT_LEN        512    /* Max text buffer size */
#define AI_DIALOG_ASR_TIMEOUT_MS      10000  /* 10s recording timeout */
#define AI_DIALOG_LLM_TIMEOUT_MS      30000  /* 30s LLM timeout */

/* ============================================================
 * Types
 * ============================================================ */

typedef enum {
    AI_DIALOG_IDLE = 0,
    AI_DIALOG_LISTENING,    /**< Recording audio */
    AI_DIALOG_PROCESSING,   /**< Sending to ASR/LLM */
    AI_DIALOG_SPEAKING,     /**< Playing TTS response */
} ai_dialog_state_t;

typedef struct {
    cloud_provider_t provider;          /**< AI provider to use */
    uint16_t max_rounds;                /**< Max conversation rounds */
    uint16_t asr_timeout_ms;            /**< Recording timeout */
    uint16_t llm_timeout_ms;            /**< LLM request timeout */
} ai_dialog_config_t;

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * @brief Initialize AI dialog system
 * @param config Configuration (NULL for defaults)
 * @return ESP_OK on success
 */
esp_err_t ai_dialog_init(const ai_dialog_config_t *config);

/**
 * @brief Deinitialize AI dialog system
 * @return ESP_OK on success
 */
esp_err_t ai_dialog_deinit(void);

/**
 * @brief Start listening (begin recording)
 * @return ESP_OK on success
 */
esp_err_t ai_dialog_start_listening(void);

/**
 * @brief Stop listening and process (send to ASR)
 * @return ESP_OK on success
 */
esp_err_t ai_dialog_stop_listening(void);

/**
 * @brief Cancel current dialog
 * @return ESP_OK on success
 */
esp_err_t ai_dialog_cancel(void);

/**
 * @brief Get current dialog state
 * @return Current state
 */
ai_dialog_state_t ai_dialog_get_state(void);

/**
 * @brief Clear conversation history
 * @return ESP_OK on success
 */
esp_err_t ai_dialog_clear_history(void);

/**
 * @brief Get current conversation round count
 * @return Number of completed rounds
 */
uint8_t ai_dialog_get_round_count(void);

#ifdef __cplusplus
}
#endif