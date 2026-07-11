/*
 * RobotBuddy — Emotion Engine
 * =============================
 * Manages robot facial expressions (eye animations) and renders
 * them to the display frame buffer.
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

#define EMOTION_EYE_RADIUS_DEFAULT   35    /**< Default eye radius (pixels) */
#define EMOTION_PUPIL_RADIUS_DEFAULT  12    /**< Default pupil radius */
#define EMOTION_IRIS_RADIUS_DEFAULT   22    /**< Default iris radius */
#define EMOTION_BLINK_PROBABILITY      3    /**< Blink probability per frame (out of 256) */
#define EMOTION_BLINK_FRAMES           4    /**< Number of frames for blink animation */

/* ============================================================
 * Types
 * ============================================================ */

/**
 * @brief Emotion configuration
 */
typedef struct {
    emotion_id_t id;                 /**< Emotion identifier */
    const char *name;                /**< Human-readable name */
    uint16_t eye_color;              /**< Default iris color (RGB565) */
    uint16_t bg_color;               /**< Default background color (RGB565) */
    uint8_t blink_probability;       /**< Blink probability per frame (0-255) */
    uint16_t animation_interval_ms;  /**< Animation frame interval (ms) */
} emotion_config_t;

/**
 * @brief Eye state for animation
 */
typedef struct {
    uint16_t center_x;              /**< Eye center X */
    uint16_t center_y;              /**< Eye center Y */
    uint16_t radius;                 /**< Eye radius */
    uint16_t iris_radius;            /**< Iris radius */
    uint16_t pupil_radius;            /**< Pupil radius */
    int16_t  offset_x;               /**< Horizontal offset (for look direction) */
    int16_t  offset_y;               /**< Vertical offset (for look direction) */
    uint16_t squish_y;               /**< Vertical squish factor (0=flat, 100=round) */
    uint16_t eye_color;              /**< Current iris color */
    uint16_t pupil_color;            /**< Pupil color */
    uint16_t highlight_color;         /**< Highlight (specular) color */
    bool     is_blinking;             /**< Blink animation active */
    uint8_t  blink_frame;             /**< Current blink frame */
} eye_state_t;

/**
 * @brief Emotion engine configuration
 */
typedef struct {
    uint16_t display_width;           /**< Display width */
    uint16_t display_height;          /**< Display height */
    uint16_t left_eye_cx;             /**< Left eye center X */
    uint16_t left_eye_cy;             /**< Left eye center Y */
    uint16_t right_eye_cx;            /**< Right eye center X */
    uint16_t right_eye_cy;            /**< Right eye center Y */
    uint16_t eye_radius;              /**< Default eye radius */
} emotion_engine_config_t;

/* ============================================================
 * RGB565 Color Helpers
 * ============================================================ */

#define RGB565(r, g, b)  (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

/* Pre-defined colors */
#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_CYAN        RGB565(0, 200, 220)
#define COLOR_DARK_CYAN   RGB565(0, 120, 140)
#define COLOR_BLUE        RGB565(30, 100, 230)
#define COLOR_RED         RGB565(230, 50, 50)
#define COLOR_YELLOW      RGB565(230, 200, 0)
#define COLOR_GREEN       RGB565(50, 205, 50)
#define COLOR_ORANGE      RGB565(255, 165, 0)
#define COLOR_GRAY        RGB565(128, 128, 128)
#define COLOR_DARK_BG     RGB565(10, 10, 20)

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * @brief Initialize emotion engine
 *
 * @param config Configuration (NULL for defaults)
 * @return ESP_OK on success
 */
esp_err_t emotion_engine_init(const emotion_engine_config_t *config);

/**
 * @brief Deinitialize emotion engine
 * @return ESP_OK on success
 */
esp_err_t emotion_engine_deinit(void);

/**
 * @brief Set current emotion state
 *
 * @param emotion Emotion ID to switch to
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if emotion ID out of range
 */
esp_err_t emotion_set_state(emotion_id_t emotion);

/**
 * @brief Get current emotion state
 * @return Current emotion ID
 */
emotion_id_t emotion_get_state(void);

/**
 * @brief Get emotion name string
 * @param emotion Emotion ID
 * @return Name string (e.g., "IDLE", "HAPPY")
 */
const char *emotion_get_name(emotion_id_t emotion);

/**
 * @brief Render current emotion frame to frame buffer
 *
 * Called by display_task at 30 FPS. Renders the current emotion
 * animation (eyes, blink, movement) to the provided frame buffer.
 *
 * @param fb Frame buffer (RGB565, width*height*2 bytes)
 * @param width Frame buffer width
 * @param height Frame buffer height
 * @return ESP_OK on success
 */
esp_err_t emotion_render_frame(uint16_t *fb, uint16_t width, uint16_t height);

/**
 * @brief Check if emotion engine is initialized
 * @return true if initialized
 */
bool emotion_engine_is_initialized(void);

#ifdef __cplusplus
}
#endif