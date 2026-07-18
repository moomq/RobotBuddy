/*
 * RobotBuddy — Text Display
 * ===========================
 * Renders scrolling text messages and status icons on the bottom
 * portion of the 240x240 LCD screen, below the emotion expression area.
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

#define TEXT_DISPLAY_Y_START_DEFAULT       180   /**< Default Y start for text area (pixels) */
#define TEXT_DISPLAY_HEIGHT_DEFAULT         60   /**< Default text area height (pixels) */
#define TEXT_DISPLAY_SCROLL_SPEED_DEFAULT    2   /**< Default scroll speed (px/frame) */
#define TEXT_DISPLAY_TIMEOUT_MS_DEFAULT  10000   /**< Default message display timeout (ms) */
#define TEXT_DISPLAY_MAX_MESSAGES_DEFAULT    5   /**< Default max queued messages */
#define TEXT_DISPLAY_MAX_TEXT_LEN          200   /**< Max text length per message */
#define TEXT_DISPLAY_ICON_SIZE              16   /**< Status icon size (pixels, square) */
#define TEXT_DISPLAY_ICON_MARGIN             4   /**< Margin around status icons (pixels) */
#define TEXT_DISPLAY_CHAR_WIDTH              6   /**< 5px font + 1px spacing */
#define TEXT_DISPLAY_CHAR_HEIGHT             7   /**< 5x7 font height */

/**
 * @brief Default configuration initializer
 */
#define TEXT_DISPLAY_CONFIG_DEFAULT() { \
    .y_start = TEXT_DISPLAY_Y_START_DEFAULT, \
    .height = TEXT_DISPLAY_HEIGHT_DEFAULT, \
    .scroll_speed = TEXT_DISPLAY_SCROLL_SPEED_DEFAULT, \
    .display_timeout_ms = TEXT_DISPLAY_TIMEOUT_MS_DEFAULT, \
    .max_messages = TEXT_DISPLAY_MAX_MESSAGES_DEFAULT \
}

/* ============================================================
 * Types
 * ============================================================ */

/**
 * @brief Status icon identifiers
 */
typedef enum {
    STATUS_ICON_NONE = 0,           /**< No icon */
    STATUS_ICON_WIFI_ON,            /**< WiFi connected */
    STATUS_ICON_WIFI_OFF,           /**< WiFi disconnected */
    STATUS_ICON_BATTERY_HIGH,       /**< Battery > 75% */
    STATUS_ICON_BATTERY_MEDIUM,     /**< Battery 25-75% */
    STATUS_ICON_BATTERY_LOW,        /**< Battery < 25% */
    STATUS_ICON_BUILDING,           /**< Build in progress */
    STATUS_ICON_BUILD_OK,           /**< Build succeeded */
    STATUS_ICON_BUILD_FAIL,         /**< Build failed */
    STATUS_ICON_GIT_DIRTY,          /**< Uncommitted changes */
    STATUS_ICON_COUNT,              /**< Number of icon types */
} status_icon_t;

/**
 * @brief Text display configuration
 */
typedef struct {
    uint16_t y_start;               /**< Y position where text area begins */
    uint16_t height;                /**< Height of text area in pixels */
    uint8_t  scroll_speed;          /**< Scroll speed in pixels per frame */
    uint32_t display_timeout_ms;    /**< Auto-clear timeout per message (ms) */
    uint8_t  max_messages;          /**< Maximum queued messages */
} text_display_config_t;

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * @brief Initialize text display module
 *
 * Allocates message queue, subscribes to relevant events.
 * Call before any other text_display functions.
 *
 * @param config Configuration (NULL for defaults)
 * @return ESP_OK on success, ESP_ERR_NO_MEM if allocation fails
 */
esp_err_t text_display_init(const text_display_config_t *config);

/**
 * @brief Deinitialize text display module
 *
 * Unsubscribes from events, frees message queue.
 *
 * @return ESP_OK on success
 */
esp_err_t text_display_deinit(void);

/**
 * @brief Show a text message in the scrolling display
 *
 * Adds the message to the queue. If priority >= 2 (high), the message
 * is pushed to the front of the queue.
 *
 * @param text Message text (up to 200 chars, truncated if longer)
 * @param priority 0=low, 1=medium, 2=high (high pushes to front)
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t text_display_show_message(const char *text, uint8_t priority);

/**
 * @brief Clear all messages from the display
 *
 * Removes all queued messages and resets scroll state.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t text_display_clear(void);

/**
 * @brief Render text display to frame buffer
 *
 * Called by display_task each frame. Renders status icons in the
 * top-right corner and scrolling text in the bottom area.
 *
 * @param fb Frame buffer (RGB565, width*height*2 bytes)
 * @param width Frame buffer width
 * @param height Frame buffer height
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t text_display_render(uint16_t *fb, uint16_t width, uint16_t height);

/**
 * @brief Set a status icon to display
 *
 * Status icons are rendered in the top-right corner of the text area.
 * Only one icon of each type is shown; setting STATUS_ICON_NONE clears it.
 *
 * @param icon Icon type to set
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t text_display_set_status_icon(status_icon_t icon);

/**
 * @brief Check if text display is initialized
 * @return true if initialized
 */
bool text_display_is_initialized(void);

#ifdef __cplusplus
}
#endif
