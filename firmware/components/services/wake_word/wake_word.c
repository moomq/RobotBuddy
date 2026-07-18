/*
 * RobotBuddy — Wake Word Detection Implementation
 * ==================================================
 * Offline wake word detection using ESP-SR (WakeNet).
 * Falls back to audio energy detection when ESP-SR is unavailable.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "wake_word.h"
#include "audio_manager.h"
#include "event_bus.h"
#include "robot_events.h"

#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

/* Conditional ESP-SR includes */
#ifdef CONFIG_ENABLE_ESP_SR
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_speech_commands.h"
#endif

static const char *TAG = "wake_word";

/* ============================================================
 * Module State
 * ============================================================ */

static bool s_initialized = false;
static bool s_listening = false;
static bool s_paused_by_tts = false;
static wake_word_config_t s_config;

/* Task */
static TaskHandle_t s_wake_task = NULL;

/* Atomic flag for listening state (thread-safe reads) */
static volatile bool s_listening_flag = false;

#ifdef CONFIG_ENABLE_ESP_SR
/* ============================================================
 * ESP-SR Engine State
 * ============================================================ */

static const esp_wn_iface_t *s_wn_iface = NULL;
static model_iface_data_t *s_wn_model_data = NULL;
static esp_wn_model_t s_wn_model = {0};

/* Audio chunk size required by WakeNet */
static int s_wn_chunk_size = 0;

#endif /* CONFIG_ENABLE_ESP_SR */

/* Stub engine state (used when ESP-SR unavailable) */
#ifndef CONFIG_ENABLE_ESP_SR
#define WAKE_WORD_STUB_ENERGY_THRESHOLD  800.0f   /* RMS energy threshold */
#define WAKE_WORD_STUB_TRIGGER_MS        500       /* Sustained energy duration to trigger */
#define WAKE_WORD_STUB_READ_SIZE         1024      /* Bytes per I2S read */
#define WAKE_WORD_STUB_COOLDOWN_MS       3000      /* Cooldown between detections */
#define WAKE_WORD_STUB_SAMPLE_SIZE       2         /* 16-bit = 2 bytes per sample */

static int64_t s_energy_above_ts = 0;              /* Timestamp when energy first exceeded threshold */
static int64_t s_last_detection_ts = 0;            /* Timestamp of last detection (for cooldown) */
#endif

/* ============================================================
 * Event Handlers
 * ============================================================ */

/**
 * @brief Handle audio playback start — pause wake word detection
 */
static void on_audio_play_start(const robot_event_t *event)
{
    (void)event;
    if (!s_initialized) {
        return;
    }

    ESP_LOGD(TAG, "TTS started, pausing wake word detection");
    s_paused_by_tts = true;
    s_listening_flag = false;
}

/**
 * @brief Handle audio playback done — resume wake word detection
 */
static void on_audio_play_done(const robot_event_t *event)
{
    (void)event;
    if (!s_initialized) {
        return;
    }

    ESP_LOGD(TAG, "TTS finished, resuming wake word detection");
    s_paused_by_tts = false;
    /* Only resume if we were supposed to be listening */
    if (s_listening) {
        s_listening_flag = true;
    }
}

/* ============================================================
 * ESP-SR Detection Task
 * ============================================================ */

#ifdef CONFIG_ENABLE_ESP_SR

static void wake_word_task_fn(void *arg)
{
    ESP_LOGI(TAG, "Wake word task started (ESP-SR mode)");

    /* Allocate audio buffer for WakeNet chunk size */
    int16_t *audio_buf = (int16_t *)heap_caps_malloc(s_wn_chunk_size, MALLOC_CAP_SPIRAM);
    if (audio_buf == NULL) {
        audio_buf = (int16_t *)malloc(s_wn_chunk_size);
    }
    if (audio_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        s_listening_flag = false;
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        if (!s_listening_flag) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        /* Read audio data from capture pipeline */
        size_t bytes_read = audio_capture_read(audio_buf, s_wn_chunk_size, 100);
        if (bytes_read < (size_t)s_wn_chunk_size) {
            /* Not enough data, wait and retry */
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        /* Feed to WakeNet engine */
        int wake_word_id = s_wn_iface->detect(s_wn_model_data, audio_buf);
        if (wake_word_id >= 0) {
            /* Wake word detected */
            float confidence = 0.0f;
            /* Try to get confidence score if API supports it */
            /* esp_wn_iface_t does not expose confidence directly in all versions,
             * so we report the detection threshold as a proxy */
            confidence = s_config.detection_threshold;

            ESP_LOGI(TAG, "Wake word detected (id=%d, confidence=%.2f)", wake_word_id, confidence);

            /* Publish detection event */
            wake_word_event_t payload = {0};
            snprintf(payload.keyword, sizeof(payload.keyword), "keyword_%d", wake_word_id);
            payload.confidence = confidence;

            robot_event_t event = {
                .id = EVENT_WAKE_WORD_DETECTED,
                .payload = &payload,
                .payload_len = sizeof(payload),
            };
            event_bus_publish(&event);

            /* Brief cooldown to avoid rapid re-triggering */
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        esp_task_wdt_reset();
    }

    /* Unreachable, but satisfy compiler */
    free(audio_buf);
}

/* ============================================================
 * Stub Detection Task (Energy-based VAD)
 * ============================================================ */

#else /* !CONFIG_ENABLE_ESP_SR */

/**
 * @brief Calculate RMS energy of 16-bit PCM audio buffer
 */
static float calculate_rms(const int16_t *samples, size_t num_samples)
{
    double sum = 0.0;
    for (size_t i = 0; i < num_samples; i++) {
        double s = (double)samples[i];
        sum += s * s;
    }
    return (float)sqrt(sum / (double)num_samples);
}

static void wake_word_task_fn(void *arg)
{
    ESP_LOGI(TAG, "Wake word task started (stub/energy-VAD mode)");

    /* Allocate audio read buffer */
    int16_t *audio_buf = (int16_t *)malloc(WAKE_WORD_STUB_READ_SIZE);
    if (audio_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        s_listening_flag = false;
        vTaskDelete(NULL);
        return;
    }

    size_t num_samples = WAKE_WORD_STUB_READ_SIZE / WAKE_WORD_STUB_SAMPLE_SIZE;

    while (1) {
        if (!s_listening_flag) {
            vTaskDelay(pdMS_TO_TICKS(50));
            s_energy_above_ts = 0;
            continue;
        }

        /* Read audio data from capture pipeline */
        size_t bytes_read = audio_capture_read(audio_buf, WAKE_WORD_STUB_READ_SIZE, 100);
        if (bytes_read == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        /* Adjust sample count for actual bytes read */
        size_t actual_samples = bytes_read / WAKE_WORD_STUB_SAMPLE_SIZE;

        /* Calculate RMS energy */
        float rms = calculate_rms(audio_buf, actual_samples);

        int64_t now = esp_timer_get_time() / 1000; /* Convert to ms */

        /* Check cooldown from last detection */
        if (s_last_detection_ts > 0 &&
            (now - s_last_detection_ts) < WAKE_WORD_STUB_COOLDOWN_MS) {
            s_energy_above_ts = 0;
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (rms > WAKE_WORD_STUB_ENERGY_THRESHOLD) {
            /* Energy above threshold */
            if (s_energy_above_ts == 0) {
                /* First frame above threshold */
                s_energy_above_ts = now;
            } else if ((now - s_energy_above_ts) >= WAKE_WORD_STUB_TRIGGER_MS) {
                /* Sustained energy above threshold for > 500ms — trigger wake event */
                ESP_LOGI(TAG, "Voice activity detected (RMS=%.1f, threshold=%.1f)",
                         rms, WAKE_WORD_STUB_ENERGY_THRESHOLD);

                /* Publish detection event */
                wake_word_event_t payload = {0};
                snprintf(payload.keyword, sizeof(payload.keyword), "voice_activity");
                payload.confidence = rms / (WAKE_WORD_STUB_ENERGY_THRESHOLD * 3.0f);
                if (payload.confidence > 1.0f) {
                    payload.confidence = 1.0f;
                }

                robot_event_t event = {
                    .id = EVENT_WAKE_WORD_DETECTED,
                    .payload = &payload,
                    .payload_len = sizeof(payload),
                };
                event_bus_publish(&event);

                s_last_detection_ts = now;
                s_energy_above_ts = 0;
            }
        } else {
            /* Energy below threshold — reset sustained timer */
            s_energy_above_ts = 0;
        }

        esp_task_wdt_reset();
    }

    /* Unreachable, but satisfy compiler */
    free(audio_buf);
}

#endif /* CONFIG_ENABLE_ESP_SR */

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t wake_word_init(const wake_word_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Wake word already initialized");
        return ESP_OK;
    }

    /* Apply config */
    if (config != NULL) {
        s_config = *config;
    } else {
        memset(&s_config, 0, sizeof(s_config));
        strncpy(s_config.model_name, "hilexin", sizeof(s_config.model_name) - 1);
        s_config.detection_threshold = WAKE_WORD_DEFAULT_THRESHOLD;
        s_config.channel = 0;
    }

    /* Ensure threshold is within valid range */
    if (s_config.detection_threshold <= 0.0f || s_config.detection_threshold > 1.0f) {
        s_config.detection_threshold = WAKE_WORD_DEFAULT_THRESHOLD;
    }

#ifdef CONFIG_ENABLE_ESP_SR
    /* ---- ESP-SR Initialization ---- */
    ESP_LOGI(TAG, "Initializing ESP-SR wake word engine (model: %s)", s_config.model_name);

    /* Load WakeNet model interface */
    s_wn_iface = &ESP_WN_WAKENET_MODEL;
    if (s_wn_iface == NULL) {
        ESP_LOGE(TAG, "Failed to get WakeNet interface");
        return ESP_FAIL;
    }

    /* Configure WakeNet model */
    s_wn_model = (esp_wn_model_t) {
        .model_name = s_config.model_name,
        .wake_word_num = 1,
    };

    /* Create model data from the interface */
    s_wn_model_data = s_wn_iface->create(&s_wn_model, s_config.detection_threshold);
    if (s_wn_model_data == NULL) {
        ESP_LOGE(TAG, "Failed to create WakeNet model data");
        return ESP_FAIL;
    }

    /* Get the audio chunk size required by the model */
    s_wn_chunk_size = s_wn_iface->get_samp_chunksize(s_wn_model_data) * sizeof(int16_t);
    ESP_LOGI(TAG, "WakeNet chunk size: %d bytes", s_wn_chunk_size);

#else
    /* ---- Stub Initialization ---- */
    ESP_LOGI(TAG, "ESP-SR unavailable, using energy-based VAD stub");
#endif

    /* Subscribe to audio playback events to pause/resume detection */
    esp_err_t ret;
    ret = event_bus_subscribe(EVENT_AUDIO_PLAY_START, on_audio_play_start);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to subscribe to AUDIO_PLAY_START");
    }

    ret = event_bus_subscribe(EVENT_AUDIO_PLAY_DONE, on_audio_play_done);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to subscribe to AUDIO_PLAY_DONE");
    }

    /* Create wake word detection task */
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        wake_word_task_fn, "wake_word",
        WAKE_WORD_TASK_STACK_SIZE, NULL,
        WAKE_WORD_TASK_PRIORITY, &s_wake_task,
        WAKE_WORD_TASK_CORE_ID
    );
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create wake word task");

#ifdef CONFIG_ENABLE_ESP_SR
        if (s_wn_model_data != NULL) {
            s_wn_iface->destroy(s_wn_model_data);
            s_wn_model_data = NULL;
        }
#endif

        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    s_listening = false;
    s_listening_flag = false;
    s_paused_by_tts = false;

    ESP_LOGI(TAG, "Wake word module initialized (threshold=%.2f, channel=%d)",
             s_config.detection_threshold, s_config.channel);
    return ESP_OK;
}

esp_err_t wake_word_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    /* Stop listening first */
    s_listening = false;
    s_listening_flag = false;

    /* Unsubscribe from events */
    event_bus_unsubscribe(EVENT_AUDIO_PLAY_START, on_audio_play_start);
    event_bus_unsubscribe(EVENT_AUDIO_PLAY_DONE, on_audio_play_done);

    /* Delete task */
    if (s_wake_task != NULL) {
        vTaskDelete(s_wake_task);
        s_wake_task = NULL;
    }

#ifdef CONFIG_ENABLE_ESP_SR
    /* Release ESP-SR resources */
    if (s_wn_model_data != NULL) {
        s_wn_iface->destroy(s_wn_model_data);
        s_wn_model_data = NULL;
    }
    s_wn_iface = NULL;
    s_wn_chunk_size = 0;
#else
    /* Reset stub state */
    s_energy_above_ts = 0;
    s_last_detection_ts = 0;
#endif

    s_initialized = false;
    s_paused_by_tts = false;

    ESP_LOGI(TAG, "Wake word module deinitialized");
    return ESP_OK;
}

esp_err_t wake_word_start(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_listening && !s_paused_by_tts) {
        ESP_LOGD(TAG, "Already listening");
        return ESP_OK;
    }

    s_listening = true;

    /* Only set the flag if TTS is not currently playing */
    if (!s_paused_by_tts) {
        s_listening_flag = true;
    }

    ESP_LOGI(TAG, "Wake word detection started");
    return ESP_OK;
}

esp_err_t wake_word_stop(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_listening = false;
    s_listening_flag = false;

#ifndef CONFIG_ENABLE_ESP_SR
    s_energy_above_ts = 0;
#endif

    ESP_LOGI(TAG, "Wake word detection stopped");
    return ESP_OK;
}

bool wake_word_is_listening(void)
{
    /* Read the volatile flag for thread safety */
    return s_listening_flag;
}
