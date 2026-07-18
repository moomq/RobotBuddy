/*
 * RobotBuddy — Cloud Manager
 * ============================
 * Cloud service abstraction layer for ESP32-S3.
 *
 * Provides a unified interface for Automatic Speech Recognition (ASR),
 * Large Language Model (LLM) chat, and Text-To-Speech (TTS) synthesis
 * across multiple cloud providers (Claude, OpenAI, DeepSeek).
 *
 * Features:
 *   - Provider abstraction with runtime switching
 *   - Automatic failover on 5xx errors or timeouts
 *   - Retry with exponential backoff (3 attempts)
 *   - WiFi connectivity check before requests
 *   - NVS-backed API key persistence
 *   - Thread-safe: mutex-serialized requests
 *   - Event bus integration for async notifications
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

/** Maximum length for ASR result text (bytes) */
#define CLOUD_ASR_TEXT_MAX         256

/** Maximum length for LLM response text (bytes) */
#define CLOUD_LLM_RESPONSE_MAX    2048

/** Maximum length for TTS audio buffer (bytes) */
#define CLOUD_TTS_AUDIO_MAX       (64 * 1024)

/** Default HTTP request timeout (ms) */
#define CLOUD_DEFAULT_TIMEOUT_MS   15000

/** Maximum retry attempts per request */
#define CLOUD_MAX_RETRIES          3

/** Maximum provider failover attempts */
#define CLOUD_MAX_FAILOVER         3

/** NVS namespace for persistent cloud config */
#define CLOUD_NVS_NAMESPACE        "cloud_mgr"

/** NVS key for API key storage */
#define CLOUD_NVS_API_KEY_PREFIX   "api_key_"

/* ============================================================
 * Provider Enumeration (defined in robot_events.h)
 * ============================================================ */

/**
 * @brief Number of supported cloud providers
 *
 * Must match the number of entries in cloud_provider_t
 * (CLOUD_PROVIDER_CLAUDE, CLOUD_PROVIDER_OPENAI, CLOUD_PROVIDER_DEEPSEEK).
 */
#define CLOUD_PROVIDER_COUNT  3

/* ============================================================
 * Configuration Structure
 * ============================================================ */

/**
 * @brief Cloud manager configuration
 *
 * Passed to cloud_manager_init(). API keys provided here serve as
 * defaults and are persisted to NVS; subsequent boots load from NVS.
 * Set api_key to "" to skip the default and rely on menuconfig/NVS only.
 */
typedef struct {
    cloud_provider_t provider;      /**< Default provider */
    char api_key[128];              /**< Default API key (may be overridden per-provider) */
    char endpoint[256];             /**< Default base endpoint URL (optional override) */
    uint16_t timeout_ms;            /**< HTTP request timeout in ms (0 = use default) */
} cloud_config_t;

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * @brief Initialize the cloud manager
 *
 * Initializes the internal mutex, loads persisted API keys from NVS,
 * and publishes EVENT_CLOUD_CONNECTED if WiFi is available.
 * Must be called before any other cloud_manager function.
 * Safe to call multiple times; subsequent calls return ESP_OK.
 *
 * @param config Configuration with defaults. Pass NULL for menuconfig defaults.
 * @return ESP_OK on success
 *         ESP_ERR_NO_MEM if mutex allocation fails
 *         ESP_ERR_INVALID_STATE if already initialized (still returns ESP_OK)
 */
esp_err_t cloud_manager_init(const cloud_config_t *config);

/**
 * @brief Deinitialize the cloud manager
 *
 * Releases all resources, cancels any in-flight request tracking,
 * and publishes EVENT_CLOUD_DISCONNECTED.
 *
 * @return ESP_OK on success
 */
esp_err_t cloud_manager_deinit(void);

/**
 * @brief Send audio data for Automatic Speech Recognition
 *
 * POSTs audio as multipart/form-data to the provider's Whisper-compatible
 * ASR endpoint and parses the JSON response to extract transcribed text.
 *
 * On success, publishes EVENT_CLOUD_ASR_RESULT with an asr_result_t payload.
 * On failure, publishes EVENT_CLOUD_ERROR and attempts failover.
 *
 * @param audio_data Pointer to raw audio bytes (PCM 16-bit LE, 16 kHz mono)
 * @param len        Length of audio data in bytes
 * @param text_out   Output buffer for transcribed text (caller-allocated)
 * @param text_max   Size of text_out buffer (recommended ≥ CLOUD_ASR_TEXT_MAX)
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if audio_data or text_out is NULL
 *         ESP_ERR_INVALID_STATE if not initialized or WiFi not connected
 *         ESP_FAIL on HTTP or JSON parse error after all retries/failovers
 */
esp_err_t cloud_asr_send(const void *audio_data, size_t len,
                          char *text_out, size_t text_max);

/**
 * @brief Send a prompt to the LLM chat endpoint
 *
 * POSTs a JSON payload with prompt and optional conversation history
 * to the provider's chat completion endpoint, then parses the response.
 *
 * On success, publishes EVENT_CLOUD_LLM_RESPONSE with a cloud_response_t payload.
 * On failure, publishes EVENT_CLOUD_ERROR and attempts failover.
 *
 * @param prompt   User prompt text (UTF-8, null-terminated)
 * @param history  JSON-encoded conversation history (may be NULL for no history)
 * @param response Output buffer for LLM response text (caller-allocated)
 * @param resp_max Size of response buffer (recommended ≥ CLOUD_LLM_RESPONSE_MAX)
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if prompt or response is NULL
 *         ESP_ERR_INVALID_STATE if not initialized or WiFi not connected
 *         ESP_FAIL on HTTP or JSON parse error after all retries/failovers
 */
esp_err_t cloud_llm_chat(const char *prompt, const char *history,
                          char *response, size_t resp_max);

/**
 * @brief Synthesize text to speech audio
 *
 * Sends text to the provider's TTS endpoint and receives audio data back.
 * The audio format depends on the provider (typically MP3 or PCM).
 *
 * On success, publishes EVENT_CLOUD_TTS_DATA with the audio buffer as payload.
 * On failure, publishes EVENT_CLOUD_ERROR and attempts failover.
 *
 * @param text      Text to synthesize (UTF-8, null-terminated)
 * @param audio_out Output buffer for audio data (caller-allocated)
 * @param audio_len [in] Size of audio_out buffer; [out] actual audio bytes received
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if text or audio_out is NULL
 *         ESP_ERR_INVALID_STATE if not initialized or WiFi not connected
 *         ESP_FAIL on HTTP error after all retries/failovers
 *         ESP_ERR_NO_MEM if audio_out is too small
 */
esp_err_t cloud_tts_synthesize(const char *text, void *audio_out,
                                size_t *audio_len);

/**
 * @brief Switch the active cloud provider
 *
 * Thread-safe. Takes effect on the next request.
 *
 * @param provider Provider to use for subsequent requests
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if provider is out of range
 *         ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t cloud_set_provider(cloud_provider_t provider);

/**
 * @brief Get the current active cloud provider
 *
 * Thread-safe.
 *
 * @return Current cloud_provider_t value
 */
cloud_provider_t cloud_get_provider(void);

/**
 * @brief Check whether the cloud manager is connected and ready
 *
 * Returns true only if the manager is initialized AND WiFi is connected.
 *
 * @return true if initialized and WiFi connected
 */
bool cloud_manager_is_connected(void);

/* ============================================================
 * V2.0 Streaming TTS API
 * ============================================================ */

/**
 * @brief Stream TTS audio with low-latency playback
 *
 * V2.0 enhancement: Opens a streaming connection to the cloud TTS
 * endpoint (WebSocket or chunked HTTP) and feeds audio data directly
 * to the audio playback ring buffer as chunks arrive, enabling
 * "play-while-download" for reduced end-to-end latency.
 *
 * Falls back to cloud_tts_synthesize() if streaming is unavailable.
 *
 * @param text     Text to synthesize (UTF-8, null-terminated)
 * @param text_len Length of text
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_STATE if not initialized or WiFi not connected
 *         ESP_FAIL on streaming error
 */
esp_err_t cloud_tts_stream(const char *text, size_t text_len);

/**
 * @brief Set the API key for a specific provider
 *
 * Persists the key to NVS and updates the runtime copy.
 * Thread-safe.
 *
 * @param provider Target provider
 * @param api_key  API key string (null-terminated)
 * @return ESP_OK on success
 */
esp_err_t cloud_set_api_key(cloud_provider_t provider, const char *api_key);

/**
 * @brief Get the current API key for a provider (for Web console display)
 *
 * Returns a masked version of the key for security.
 *
 * @param provider Target provider
 * @param buf      Output buffer
 * @param buf_size Size of output buffer
 * @return ESP_OK on success
 */
esp_err_t cloud_get_api_key_masked(cloud_provider_t provider, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif