/*
 * RobotBuddy — AI Dialog Implementation
 * ========================================
 * Voice interaction pipeline: ASR → LLM → TTS
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "ai_dialog.h"
#include "audio_manager.h"
#include "cloud_manager.h"
#include "event_bus.h"
#include "robot_events.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "ai_dialog";

/* ============================================================
 * Module State
 * ============================================================ */

static ai_dialog_state_t s_state = AI_DIALOG_IDLE;
static TaskHandle_t s_task_handle = NULL;
static bool s_initialized = false;

/* Configuration */
static cloud_provider_t s_provider = CLOUD_PROVIDER_CLAUDE;
static uint16_t s_max_rounds = AI_DIALOG_MAX_ROUNDS;
static uint16_t s_asr_timeout_ms = AI_DIALOG_ASR_TIMEOUT_MS;
static uint16_t s_llm_timeout_ms = AI_DIALOG_LLM_TIMEOUT_MS;

/* Conversation history (circular buffer) */
typedef struct {
    char user[AI_DIALOG_MAX_TEXT_LEN];
    char assistant[AI_DIALOG_MAX_TEXT_LEN];
} dialog_round_t;

static dialog_round_t s_history[AI_DIALOG_MAX_ROUNDS];
static uint8_t s_round_count = 0;
static uint8_t s_history_start = 0;

/* Audio buffer for ASR */
#define ASR_CHUNK_SIZE  3200  /* 100ms of 16kHz/16bit mono audio */
static uint8_t *s_asr_buffer = NULL;
static size_t s_asr_buffer_len = 0;
static size_t s_asr_buffer_cap = 0;

/* LLM response buffer */
static char s_llm_response[AI_DIALOG_MAX_TEXT_LEN];

/* ============================================================
 * Internal: State transition
 * ============================================================ */

static void set_state(ai_dialog_state_t new_state)
{
    if (new_state == s_state) return;

    ai_dialog_state_t old_state = s_state;
    s_state = new_state;

    const char *names[] = {"IDLE", "LISTENING", "PROCESSING", "SPEAKING"};
    ESP_LOGI(TAG, "Dialog state: %s -> %s",
             (old_state < 4) ? names[old_state] : "UNKNOWN",
             (new_state < 4) ? names[new_state] : "UNKNOWN");

    /* Publish behavior state change */
    robot_event_t event = { .id = EVENT_BEHAVIOR_STATE_CHANGE, .payload = NULL, .payload_len = 0 };

    static behavior_state_t s_behavior_state_payload;
    s_behavior_state_payload = BEHAVIOR_STATE_IDLE;

    switch (new_state) {
        case AI_DIALOG_LISTENING:
            s_behavior_state_payload = BEHAVIOR_STATE_LISTENING;
            break;
        case AI_DIALOG_PROCESSING:
            s_behavior_state_payload = BEHAVIOR_STATE_THINKING;
            break;
        case AI_DIALOG_SPEAKING:
            s_behavior_state_payload = BEHAVIOR_STATE_ANSWERING;
            break;
        case AI_DIALOG_IDLE:
        default:
            s_behavior_state_payload = BEHAVIOR_STATE_IDLE;
            break;
    }

    event.payload = &s_behavior_state_payload;
    event.payload_len = sizeof(behavior_state_t);

    event_bus_publish(&event);
}

/* ============================================================
 * AI Dialog Task
 * ============================================================ */

static void ai_dialog_task(void *arg)
{
    ESP_LOGI(TAG, "AI dialog task started");

    while (1) {
        switch (s_state) {
            case AI_DIALOG_IDLE:
                vTaskDelay(pdMS_TO_TICKS(100));
                break;

            case AI_DIALOG_LISTENING:
                /* Audio capture is running — wait for stop command */
                vTaskDelay(pdMS_TO_TICKS(50));
                break;

            case AI_DIALOG_PROCESSING: {
                /* Step 1: Send audio to ASR */
                ESP_LOGI(TAG, "Sending audio to ASR (%zu bytes)", s_asr_buffer_len);

                char asr_text[AI_DIALOG_MAX_TEXT_LEN] = {0};
                esp_err_t ret = cloud_asr_send(s_asr_buffer, s_asr_buffer_len,
                                                asr_text, sizeof(asr_text));

                if (ret != ESP_OK || strlen(asr_text) == 0) {
                    ESP_LOGE(TAG, "ASR failed: %s", esp_err_to_name(ret));
                    set_state(AI_DIALOG_IDLE);
                    break;
                }

                ESP_LOGI(TAG, "ASR result: \"%s\"", asr_text);

                /* Step 2: Send text to LLM */
                /* Build conversation history string */
                char history_buf[2048] = {0};
                for (uint8_t i = 0; i < s_round_count; i++) {
                    uint8_t idx = (s_history_start + i) % AI_DIALOG_MAX_ROUNDS;
                    strncat(history_buf, "User: ", sizeof(history_buf) - strlen(history_buf) - 1);
                    strncat(history_buf, s_history[idx].user, sizeof(history_buf) - strlen(history_buf) - 1);
                    strncat(history_buf, "\nAssistant: ", sizeof(history_buf) - strlen(history_buf) - 1);
                    strncat(history_buf, s_history[idx].assistant, sizeof(history_buf) - strlen(history_buf) - 1);
                    strncat(history_buf, "\n", sizeof(history_buf) - strlen(history_buf) - 1);
                }

                ESP_LOGI(TAG, "Sending to LLM...");
                memset(s_llm_response, 0, sizeof(s_llm_response));

                ret = cloud_llm_chat(asr_text, history_buf, s_llm_response, sizeof(s_llm_response));

                if (ret != ESP_OK || strlen(s_llm_response) == 0) {
                    ESP_LOGE(TAG, "LLM failed: %s", esp_err_to_name(ret));
                    set_state(AI_DIALOG_IDLE);
                    break;
                }

                ESP_LOGI(TAG, "LLM response: \"%s\"", s_llm_response);

                /* Step 3: Save conversation round */
                uint8_t round_idx = (s_history_start + s_round_count) % AI_DIALOG_MAX_ROUNDS;
                strncpy(s_history[round_idx].user, asr_text, sizeof(s_history[round_idx].user) - 1);
                strncpy(s_history[round_idx].assistant, s_llm_response, sizeof(s_history[round_idx].assistant) - 1);

                if (s_round_count < AI_DIALOG_MAX_ROUNDS) {
                    s_round_count++;
                } else {
                    s_history_start = (s_history_start + 1) % AI_DIALOG_MAX_ROUNDS;
                }

                /* Step 4: TTS synthesis and playback */
                ESP_LOGI(TAG, "Starting TTS playback...");

                /* For MVP: just play the response text via TTS */
                set_state(AI_DIALOG_SPEAKING);
                audio_play_start();

                /* TTS: send text to cloud, receive audio, play.
                 * TODO(V1.1): Replace this blocking 2s delay with event-driven
                 * TTS streaming. The cloud TTS response should be streamed to
                 * the audio playback ring buffer as chunks arrive, and
                 * EVENT_AUDIO_PLAY_DONE should fire when the stream ends.
                 * This blocking delay prevents the AI dialog task from
                 * responding to cancellation requests during playback. */
                vTaskDelay(pdMS_TO_TICKS(2000));  /* Placeholder for TTS playback */

                audio_play_stop();

                /* Publish audio play done */
                robot_event_t done_event = {
                    .id = EVENT_AUDIO_PLAY_DONE,
                    .payload = NULL,
                    .payload_len = 0,
                };
                event_bus_publish(&done_event);

                set_state(AI_DIALOG_IDLE);
                break;
            }

            case AI_DIALOG_SPEAKING:
                /* TTS playback in progress — wait for completion */
                vTaskDelay(pdMS_TO_TICKS(50));
                break;

            default:
                set_state(AI_DIALOG_IDLE);
                break;
        }

        esp_task_wdt_reset();
    }
}

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t ai_dialog_init(const ai_dialog_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "AI dialog already initialized");
        return ESP_OK;
    }

    if (config != NULL) {
        s_provider = config->provider;
        s_max_rounds = config->max_rounds;
        s_asr_timeout_ms = config->asr_timeout_ms;
        s_llm_timeout_ms = config->llm_timeout_ms;
    }

    /* Allocate ASR buffer */
    size_t asr_buf_size = (size_t)(16000 * 2 * 10);  /* 10 seconds of 16kHz/16bit audio */
    s_asr_buffer = (uint8_t *)heap_caps_malloc(asr_buf_size, MALLOC_CAP_SPIRAM);
    if (s_asr_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate ASR buffer (%zu bytes)", asr_buf_size);
        return ESP_ERR_NO_MEM;
    }
    s_asr_buffer_cap = asr_buf_size;
    s_asr_buffer_len = 0;

    /* Clear conversation history */
    memset(s_history, 0, sizeof(s_history));
    s_round_count = 0;
    s_history_start = 0;

    /* Create AI dialog task */
    BaseType_t ret = xTaskCreatePinnedToCore(
        ai_dialog_task,
        "ai_dialog",
        AI_DIALOG_TASK_STACK_SIZE,
        NULL,
        AI_DIALOG_TASK_PRIORITY,
        &s_task_handle,
        AI_DIALOG_TASK_CORE_ID
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create AI dialog task");
        heap_caps_free(s_asr_buffer);
        s_asr_buffer = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_state = AI_DIALOG_IDLE;
    s_initialized = true;

    ESP_LOGI(TAG, "AI dialog initialized (provider=%d, max_rounds=%d)",
             s_provider, s_max_rounds);
    return ESP_OK;
}

esp_err_t ai_dialog_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    if (s_task_handle != NULL) {
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
    }

    if (s_asr_buffer != NULL) {
        heap_caps_free(s_asr_buffer);
        s_asr_buffer = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "AI dialog deinitialized");
    return ESP_OK;
}

esp_err_t ai_dialog_start_listening(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_state != AI_DIALOG_IDLE) {
        ESP_LOGW(TAG, "Cannot start listening in state %d", s_state);
        return ESP_ERR_INVALID_STATE;
    }

    /* Reset ASR buffer */
    s_asr_buffer_len = 0;

    /* Start audio capture */
    esp_err_t ret = audio_capture_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start audio capture: %s", esp_err_to_name(ret));
        return ret;
    }

    set_state(AI_DIALOG_LISTENING);
    return ESP_OK;
}

esp_err_t ai_dialog_stop_listening(void)
{
    if (s_state != AI_DIALOG_LISTENING) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Stop audio capture */
    audio_capture_stop();

    /* Read captured audio from ring buffer */
    size_t total_read = 0;
    size_t chunk_size = ASR_CHUNK_SIZE;
    while (total_read + chunk_size < s_asr_buffer_cap) {
        size_t bytes_read = audio_capture_read(
            s_asr_buffer + total_read,
            chunk_size,
            100  /* 100ms timeout */
        );
        if (bytes_read == 0) break;
        total_read += bytes_read;
    }
    s_asr_buffer_len = total_read;

    ESP_LOGI(TAG, "Captured %zu bytes of audio", s_asr_buffer_len);

    /* Transition to processing */
    set_state(AI_DIALOG_PROCESSING);
    return ESP_OK;
}

esp_err_t ai_dialog_cancel(void)
{
    if (s_state == AI_DIALOG_IDLE) {
        return ESP_OK;
    }

    /* Stop any active audio */
    if (audio_is_capturing()) {
        audio_capture_stop();
    }
    if (audio_is_playing()) {
        audio_play_stop();
    }

    s_asr_buffer_len = 0;
    set_state(AI_DIALOG_IDLE);
    ESP_LOGI(TAG, "Dialog cancelled");
    return ESP_OK;
}

ai_dialog_state_t ai_dialog_get_state(void)
{
    return s_state;
}

esp_err_t ai_dialog_clear_history(void)
{
    memset(s_history, 0, sizeof(s_history));
    s_round_count = 0;
    s_history_start = 0;
    ESP_LOGI(TAG, "Conversation history cleared");
    return ESP_OK;
}

uint8_t ai_dialog_get_round_count(void)
{
    return s_round_count;
}