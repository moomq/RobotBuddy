/*
 * RobotBuddy — Audio Manager
 * ============================
 * Manages I2S audio capture (INMP441) and playback (MAX98357A).
 * Provides ring buffer based audio streaming pipeline.
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

#define AUDIO_SAMPLE_RATE       16000      /* 16 kHz */
#define AUDIO_BITS_PER_SAMPLE   16         /* 16-bit */
#define AUDIO_CHANNELS          1          /* Mono */
#define AUDIO_CAPTURE_BUF_SIZE  (32 * 1024)  /* 32 KB capture ring buffer */
#define AUDIO_PLAYBACK_BUF_SIZE (64 * 1024)  /* 64 KB playback ring buffer */
#define AUDIO_TASK_STACK_SIZE   8192       /* 8 KB stack */
#define AUDIO_CAPTURE_PRIORITY  8          /* Highest */
#define AUDIO_PLAYBACK_PRIORITY  7
#define AUDIO_TASK_CORE_ID       0          /* Core 0 (WiFi core) */

/* ============================================================
 * Types
 * ============================================================ */

typedef enum {
    AUDIO_STATE_IDLE = 0,
    AUDIO_STATE_CAPTURING,
    AUDIO_STATE_PLAYING,
} audio_state_t;

typedef struct {
    uint32_t sample_rate;
    uint8_t bits_per_sample;
    uint8_t channels;
    size_t capture_buf_size;
    size_t playback_buf_size;
} audio_config_t;

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t audio_manager_init(const audio_config_t *config);
esp_err_t audio_manager_deinit(void);

/* Capture control */
esp_err_t audio_capture_start(void);
esp_err_t audio_capture_stop(void);
size_t audio_capture_read(void *buf, size_t len, uint32_t timeout_ms);

/* Playback control */
esp_err_t audio_play_start(void);
esp_err_t audio_play_stop(void);
esp_err_t audio_play_data(const void *data, size_t len);
esp_err_t audio_play_tone(uint16_t freq, uint16_t duration_ms);

/* State query */
audio_state_t audio_get_state(void);
bool audio_is_capturing(void);
bool audio_is_playing(void);

#ifdef __cplusplus
}
#endif