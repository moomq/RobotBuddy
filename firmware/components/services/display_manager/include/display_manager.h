/*
 * RobotBuddy — Display Manager
 * ==============================
 * Manages the ST7789 LCD display, frame buffer allocation,
 * and frame rendering pipeline.
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
 * Configuration
 * ============================================================ */

#define DISPLAY_WIDTH           240     /**< LCD width in pixels */
#define DISPLAY_HEIGHT          240     /**< LCD height in pixels */
#define DISPLAY_BPP              2      /**< Bytes per pixel (RGB565) */
#define DISPLAY_FRAMEBUFFER_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT * DISPLAY_BPP)
#define DISPLAY_TARGET_FPS       30    /**< Target refresh rate */

/* ============================================================
 * Types
 * ============================================================ */

typedef struct {
    uint16_t width;                /**< Display width */
    uint16_t height;               /**< Display height */
    uint8_t target_fps;            /**< Target FPS */
} display_config_t;

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * @brief Initialize display manager
 *
 * Allocates frame buffer in PSRAM, initializes ST7789 driver,
 * and starts the display refresh task.
 *
 * @param config Display configuration (NULL for defaults)
 * @return ESP_OK on success
 */
esp_err_t display_manager_init(const display_config_t *config);

/**
 * @brief Deinitialize display manager
 *
 * Stops the refresh task and frees frame buffer.
 *
 * @return ESP_OK on success
 */
esp_err_t display_manager_deinit(void);

/**
 * @brief Get the frame buffer pointer
 *
 * Returns a pointer to the PSRAM frame buffer (RGB565 format).
 * The caller can write pixel data directly, then call
 * display_commit_frame() to push to the LCD.
 *
 * @return Pointer to frame buffer (DISPLAY_WIDTH * DISPLAY_HEIGHT * 2 bytes)
 *         NULL if not initialized
 */
uint16_t *display_get_framebuffer(void);

/**
 * @brief Commit the current frame buffer to the display
 *
 * Sends the entire frame buffer to the ST7789 via SPI.
 * This is typically called by the display_task at 30 FPS.
 *
 * @return ESP_OK on success
 */
esp_err_t display_commit_frame(void);

/**
 * @brief Clear the frame buffer with a solid color
 *
 * @param bg_color RGB565 color
 * @return ESP_OK on success
 */
esp_err_t display_clear(uint16_t bg_color);

/**
 * @brief Draw a filled circle on the frame buffer
 *
 * @param cx Center X
 * @param cy Center Y
 * @param r  Radius
 * @param color RGB565 color
 */
void display_draw_filled_circle(uint16_t cx, uint16_t cy, uint16_t r, uint16_t color);

/**
 * @brief Draw a filled ellipse on the frame buffer
 *
 * @param cx Center X
 * @param cy Center Y
 * @param rx Horizontal radius
 * @param ry Vertical radius
 * @param color RGB565 color
 */
void display_draw_filled_ellipse(uint16_t cx, uint16_t cy, uint16_t rx, uint16_t ry, uint16_t color);

/**
 * @brief Draw a single pixel on the frame buffer
 *
 * @param x X coordinate
 * @param y Y coordinate
 * @param color RGB565 color
 */
void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief Get current FPS
 * @return Current frames per second
 */
float display_get_fps(void);

/**
 * @brief Set backlight brightness
 * @param brightness 0-255 (0=off, 255=full)
 * @return ESP_OK on success
 */
esp_err_t display_set_backlight(uint8_t brightness);

#ifdef __cplusplus
}
#endif