/*
 * RobotBuddy — Text Display Implementation
 * ===========================================
 * Renders scrolling text messages and status icons on the bottom
 * portion of the 240x240 LCD screen, below the emotion expression area.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "text_display.h"
#include "robot_events.h"
#include "event_bus.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "text_disp";

/* ============================================================
 * External font data
 * ============================================================ */

extern const uint8_t font_5x7[95][5];

/* ============================================================
 * RGB565 Color Helpers (local, matching emotion_engine.h)
 * ============================================================ */

#define RGB565(r, g, b)  (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_CYAN        RGB565(0, 200, 220)
#define COLOR_GREEN       RGB565(50, 205, 50)
#define COLOR_RED         RGB565(230, 50, 50)
#define COLOR_YELLOW      RGB565(230, 200, 0)
#define COLOR_ORANGE      RGB565(255, 165, 0)
#define COLOR_GRAY        RGB565(128, 128, 128)
#define COLOR_DARK_BG     RGB565(10, 10, 20)

/* Semi-transparent dark bar: blend 50% dark with underlying pixel */
#define TEXT_BAR_COLOR    RGB565(15, 15, 30)

/* ============================================================
 * Message queue entry
 * ============================================================ */

typedef struct {
    char text[TEXT_DISPLAY_MAX_TEXT_LEN];
    uint8_t priority;
    int64_t start_time_ms;          /**< Time when message started displaying (0 = not active) */
} message_entry_t;

/* ============================================================
 * Status icon bitmaps (16x16, 1-bit per pixel, MSB = leftmost)
 * ============================================================ */

static const uint16_t s_icon_wifi_on[16] = {
    0x0000, 0x0000, 0x0180, 0x0420,
    0x0810, 0x1248, 0x2244, 0x4182,
    0x0000, 0x0180, 0x0420, 0x0810,
    0x0000, 0x0180, 0x0000, 0x0000,
};

static const uint16_t s_icon_wifi_off[16] = {
    0x0000, 0x0000, 0x0180, 0x0420,
    0x0810, 0x1248, 0x2244, 0x4182,
    0x4002, 0x2004, 0x1008, 0x0810,
    0x0400, 0x0200, 0x0000, 0x0000,
};

static const uint16_t s_icon_battery_high[16] = {
    0x0000, 0x0000, 0x3FFC, 0x4002,
    0x4FFA, 0x4FFA, 0x4FFA, 0x4FFA,
    0x4FFA, 0x4FFA, 0x4002, 0x3FFC,
    0x0000, 0x0000, 0x0000, 0x0000,
};

static const uint16_t s_icon_battery_medium[16] = {
    0x0000, 0x0000, 0x3FFC, 0x4002,
    0x47FA, 0x47FA, 0x4002, 0x4002,
    0x4002, 0x4002, 0x4002, 0x3FFC,
    0x0000, 0x0000, 0x0000, 0x0000,
};

static const uint16_t s_icon_battery_low[16] = {
    0x0000, 0x0000, 0x3FFC, 0x4002,
    0x4002, 0x4602, 0x4002, 0x4002,
    0x4002, 0x4002, 0x4002, 0x3FFC,
    0x0000, 0x0000, 0x0000, 0x0000,
};

static const uint16_t s_icon_building[16] = {
    0x0000, 0x3FFC, 0x2004, 0x2FF4,
    0x2004, 0x2FF4, 0x2004, 0x2FF4,
    0x2004, 0x2FF4, 0x2004, 0x3FFC,
    0x0000, 0x0000, 0x0000, 0x0000,
};

static const uint16_t s_icon_build_ok[16] = {
    0x0000, 0x0000, 0x0000, 0x0180,
    0x0180, 0x00C0, 0x0060, 0x7E7E,
    0x0180, 0x0180, 0x0180, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000,
};

static const uint16_t s_icon_build_fail[16] = {
    0x0000, 0x0000, 0x0000, 0x6018,
    0x3010, 0x1820, 0x0C40, 0x0680,
    0x0C40, 0x1820, 0x3010, 0x6018,
    0x0000, 0x0000, 0x0000, 0x0000,
};

static const uint16_t s_icon_git_dirty[16] = {
    0x0000, 0x0180, 0x0180, 0x0180,
    0x0180, 0x0180, 0x0180, 0x0180,
    0x0180, 0x0180, 0x0180, 0x0180,
    0x0180, 0x0180, 0x0000, 0x0000,
};

/* Icon color mapping */
static const uint16_t s_icon_colors[STATUS_ICON_COUNT] = {
    [STATUS_ICON_NONE]         = 0,
    [STATUS_ICON_WIFI_ON]      = COLOR_CYAN,
    [STATUS_ICON_WIFI_OFF]     = COLOR_GRAY,
    [STATUS_ICON_BATTERY_HIGH] = COLOR_GREEN,
    [STATUS_ICON_BATTERY_MEDIUM] = COLOR_YELLOW,
    [STATUS_ICON_BATTERY_LOW]  = COLOR_RED,
    [STATUS_ICON_BUILDING]     = COLOR_YELLOW,
    [STATUS_ICON_BUILD_OK]     = COLOR_GREEN,
    [STATUS_ICON_BUILD_FAIL]   = COLOR_RED,
    [STATUS_ICON_GIT_DIRTY]    = COLOR_ORANGE,
};

/* Icon bitmap mapping */
static const uint16_t *s_icon_bitmaps[STATUS_ICON_COUNT] = {
    [STATUS_ICON_NONE]         = NULL,
    [STATUS_ICON_WIFI_ON]      = s_icon_wifi_on,
    [STATUS_ICON_WIFI_OFF]     = s_icon_wifi_off,
    [STATUS_ICON_BATTERY_HIGH] = s_icon_battery_high,
    [STATUS_ICON_BATTERY_MEDIUM] = s_icon_battery_medium,
    [STATUS_ICON_BATTERY_LOW]  = s_icon_battery_low,
    [STATUS_ICON_BUILDING]     = s_icon_building,
    [STATUS_ICON_BUILD_OK]     = s_icon_build_ok,
    [STATUS_ICON_BUILD_FAIL]   = s_icon_build_fail,
    [STATUS_ICON_GIT_DIRTY]    = s_icon_git_dirty,
};

/* ============================================================
 * Module State
 * ============================================================ */

static bool s_initialized = false;

/* Configuration */
static uint16_t s_y_start = TEXT_DISPLAY_Y_START_DEFAULT;
static uint16_t s_height = TEXT_DISPLAY_HEIGHT_DEFAULT;
static uint8_t  s_scroll_speed = TEXT_DISPLAY_SCROLL_SPEED_DEFAULT;
static uint32_t s_display_timeout_ms = TEXT_DISPLAY_TIMEOUT_MS_DEFAULT;
static uint8_t  s_max_messages = TEXT_DISPLAY_MAX_MESSAGES_DEFAULT;

/* Message queue (circular buffer) */
static message_entry_t *s_messages = NULL;
static uint8_t  s_msg_head = 0;       /**< Next write position */
static uint8_t  s_msg_tail = 0;       /**< Current read position */
static uint8_t  s_msg_count = 0;      /**< Number of messages in queue */

/* Scroll state */
static int16_t  s_scroll_x = 0;       /**< Current X scroll offset (pixels) */

/* Status icon state */
static status_icon_t s_status_icon = STATUS_ICON_NONE;

/* Mutex for thread-safe queue access */
static SemaphoreHandle_t s_mutex = NULL;

/* ============================================================
 * Internal: Get current time in ms
 * ============================================================ */

static int64_t get_time_ms(void)
{
    return esp_timer_get_time() / 1000;
}

/* ============================================================
 * Internal: Draw a single character from font_5x7
 * ============================================================ */

static void draw_char(uint16_t *fb, uint16_t width, uint16_t height,
                      int16_t x, int16_t y, char ch, uint16_t color)
{
    if (ch < 0x20 || ch > 0x7E) {
        ch = 0x20;  /* Replace non-printable with space */
    }

    const uint8_t *glyph = font_5x7[ch - 0x20];

    for (int row = 0; row < 7; row++) {
        uint8_t row_data = glyph[row];
        int16_t py = y + row;
        if (py < 0 || py >= height) continue;

        for (int col = 0; col < 5; col++) {
            if (row_data & (0x10 >> col)) {
                int16_t px = x + col;
                if (px >= 0 && px < width) {
                    fb[py * width + px] = color;
                }
            }
        }
    }
}

/* ============================================================
 * Internal: Draw a text string at given position
 * ============================================================ */

static void draw_text(uint16_t *fb, uint16_t width, uint16_t height,
                      int16_t x, int16_t y, const char *text, uint16_t color)
{
    if (text == NULL) return;

    while (*text) {
        /* Only draw if character might be visible (partially or fully) */
        if (x + 5 > 0 && x < width) {
            draw_char(fb, width, height, x, y, *text, color);
        }
        x += TEXT_DISPLAY_CHAR_WIDTH;
        text++;
    }
}

/* ============================================================
 * Internal: Draw a 16x16 status icon
 * ============================================================ */

static void draw_icon_16x16(uint16_t *fb, uint16_t width, uint16_t height,
                            int16_t x, int16_t y, const uint16_t *bitmap,
                            uint16_t color)
{
    if (bitmap == NULL) return;

    for (int row = 0; row < 16; row++) {
        uint16_t row_data = bitmap[row];
        int16_t py = y + row;
        if (py < 0 || py >= height) continue;

        for (int col = 0; col < 16; col++) {
            if (row_data & (0x8000 >> col)) {
                int16_t px = x + col;
                if (px >= 0 && px < width) {
                    fb[py * width + px] = color;
                }
            }
        }
    }
}

/* ============================================================
 * Internal: Draw semi-transparent dark bar
 * ============================================================ */

static void draw_text_bar(uint16_t *fb, uint16_t width, uint16_t height,
                          uint16_t y_start, uint16_t bar_height)
{
    for (uint16_t y = y_start; y < y_start + bar_height && y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            uint16_t pixel = fb[y * width + x];
            /* Blend: 50% dark overlay */
            uint8_t r = ((pixel >> 11) & 0x1F) >> 1;
            uint8_t g = ((pixel >> 5) & 0x3F) >> 1;
            uint8_t b = (pixel & 0x1F) >> 1;
            fb[y * width + x] = RGB565(r << 3, g << 2, b << 3);
        }
    }
}

/* ============================================================
 * Internal: Calculate pixel width of a text string
 * ============================================================ */

static int16_t text_pixel_width(const char *text)
{
    if (text == NULL) return 0;
    int len = 0;
    while (*text) {
        len++;
        text++;
    }
    return len * TEXT_DISPLAY_CHAR_WIDTH;
}

/* ============================================================
 * Internal: Queue operations (must hold mutex)
 * ============================================================ */

static bool queue_is_full(void)
{
    return s_msg_count >= s_max_messages;
}

static bool queue_is_empty(void)
{
    return s_msg_count == 0;
}

static void queue_push_back(const char *text, uint8_t priority)
{
    if (queue_is_full()) {
        /* Drop oldest message (advance tail) */
        s_msg_tail = (s_msg_tail + 1) % s_max_messages;
        s_msg_count--;
    }

    message_entry_t *entry = &s_messages[s_msg_head];
    strncpy(entry->text, text, TEXT_DISPLAY_MAX_TEXT_LEN - 1);
    entry->text[TEXT_DISPLAY_MAX_TEXT_LEN - 1] = '\0';
    entry->priority = priority;
    entry->start_time_ms = 0;

    s_msg_head = (s_msg_head + 1) % s_max_messages;
    s_msg_count++;
}

static void queue_push_front(const char *text, uint8_t priority)
{
    if (queue_is_full()) {
        /* Drop oldest message (advance tail) */
        s_msg_tail = (s_msg_tail + 1) % s_max_messages;
        s_msg_count--;
    }

    /* Insert before tail */
    uint8_t insert_idx;
    if (s_msg_tail == 0) {
        insert_idx = s_max_messages - 1;
    } else {
        insert_idx = s_msg_tail - 1;
    }

    message_entry_t *entry = &s_messages[insert_idx];
    strncpy(entry->text, text, TEXT_DISPLAY_MAX_TEXT_LEN - 1);
    entry->text[TEXT_DISPLAY_MAX_TEXT_LEN - 1] = '\0';
    entry->priority = priority;
    entry->start_time_ms = 0;

    s_msg_tail = insert_idx;
    s_msg_count++;
}

static message_entry_t *queue_peek(void)
{
    if (queue_is_empty()) return NULL;
    return &s_messages[s_msg_tail];
}

static void queue_pop(void)
{
    if (queue_is_empty()) return;
    s_msg_tail = (s_msg_tail + 1) % s_max_messages;
    s_msg_count--;
}

static void queue_clear(void)
{
    s_msg_head = 0;
    s_msg_tail = 0;
    s_msg_count = 0;
}

/* ============================================================
 * Internal: Event handler
 * ============================================================ */

static void text_display_event_handler(const robot_event_t *event)
{
    if (!s_initialized || event == NULL) return;

    switch (event->id) {
        case EVENT_DISPLAY_TEXT_MSG: {
            if (event->payload != NULL) {
                text_msg_event_t *msg = (text_msg_event_t *)event->payload;
                text_display_show_message(msg->text, msg->priority);
            }
            break;
        }

        case EVENT_DISPLAY_CLEAR_TEXT: {
            text_display_clear();
            break;
        }

        case EVENT_BUILD_STATUS: {
            if (event->payload != NULL) {
                build_status_event_t *bs = (build_status_event_t *)event->payload;
                /* status: 0=running, 1=success, 2=fail, 3=warning */
                switch (bs->status) {
                    case 0: /* Running */
                        text_display_set_status_icon(STATUS_ICON_BUILDING);
                        text_display_show_message("Build: running...", 1);
                        break;
                    case 1: /* Success */
                        text_display_set_status_icon(STATUS_ICON_BUILD_OK);
                        text_display_show_message("Build: success", 1);
                        break;
                    case 2: /* Fail */
                        text_display_set_status_icon(STATUS_ICON_BUILD_FAIL);
                        text_display_show_message("Build: FAILED", 2);
                        break;
                    case 3: /* Warning */
                        text_display_set_status_icon(STATUS_ICON_BUILD_FAIL);
                        text_display_show_message("Build: warning", 1);
                        break;
                    default:
                        break;
                }
                if (bs->msg[0] != '\0') {
                    text_display_show_message(bs->msg, 1);
                }
            }
            break;
        }

        case EVENT_GIT_STATUS: {
            if (event->payload != NULL) {
                git_status_event_t *gs = (git_status_event_t *)event->payload;
                if (gs->uncommitted > 0) {
                    text_display_set_status_icon(STATUS_ICON_GIT_DIRTY);
                    char buf[64];
                    snprintf(buf, sizeof(buf), "Git: %d uncommitted", gs->uncommitted);
                    text_display_show_message(buf, 0);
                } else {
                    text_display_set_status_icon(STATUS_ICON_NONE);
                }
                if (gs->conflicts > 0) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "Git: %d conflicts!", gs->conflicts);
                    text_display_show_message(buf, 2);
                }
            }
            break;
        }

        default:
            break;
    }
}

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t text_display_init(const text_display_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Text display already initialized");
        return ESP_OK;
    }

    /* Apply configuration */
    if (config != NULL) {
        s_y_start = config->y_start;
        s_height = config->height;
        s_scroll_speed = config->scroll_speed;
        s_display_timeout_ms = config->display_timeout_ms;
        s_max_messages = config->max_messages;
    }

    /* Create mutex */
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Allocate message queue */
    s_messages = calloc(s_max_messages, sizeof(message_entry_t));
    if (s_messages == NULL) {
        ESP_LOGE(TAG, "Failed to allocate message queue");
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    /* Initialize queue state */
    s_msg_head = 0;
    s_msg_tail = 0;
    s_msg_count = 0;
    s_scroll_x = 0;
    s_status_icon = STATUS_ICON_NONE;

    /* Subscribe to events */
    event_bus_subscribe(EVENT_DISPLAY_TEXT_MSG, text_display_event_handler);
    event_bus_subscribe(EVENT_DISPLAY_CLEAR_TEXT, text_display_event_handler);
    event_bus_subscribe(EVENT_BUILD_STATUS, text_display_event_handler);
    event_bus_subscribe(EVENT_GIT_STATUS, text_display_event_handler);

    s_initialized = true;
    ESP_LOGI(TAG, "Text display initialized (y=%d, h=%d, speed=%d, timeout=%lu)",
             s_y_start, s_height, s_scroll_speed, (unsigned long)s_display_timeout_ms);
    return ESP_OK;
}

esp_err_t text_display_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    /* Unsubscribe from events */
    event_bus_unsubscribe(EVENT_DISPLAY_TEXT_MSG, text_display_event_handler);
    event_bus_unsubscribe(EVENT_DISPLAY_CLEAR_TEXT, text_display_event_handler);
    event_bus_unsubscribe(EVENT_BUILD_STATUS, text_display_event_handler);
    event_bus_unsubscribe(EVENT_GIT_STATUS, text_display_event_handler);

    /* Free message queue */
    if (s_messages != NULL) {
        free(s_messages);
        s_messages = NULL;
    }

    /* Delete mutex */
    if (s_mutex != NULL) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "Text display deinitialized");
    return ESP_OK;
}

esp_err_t text_display_show_message(const char *text, uint8_t priority)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (text == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for show_message");
        return ESP_ERR_TIMEOUT;
    }

    if (priority >= 2) {
        /* High priority: push to front */
        queue_push_front(text, priority);
        /* Reset scroll to show new message immediately */
        s_scroll_x = 0;
    } else {
        queue_push_back(text, priority);
    }

    /* If no message currently displaying, start from the new one */
    if (s_msg_count == 1) {
        s_scroll_x = 0;
        message_entry_t *current = queue_peek();
        if (current != NULL) {
            current->start_time_ms = get_time_ms();
        }
    }

    ESP_LOGI(TAG, "Message queued (pri=%d): %.40s%s", priority,
             text, strlen(text) > 40 ? "..." : "");

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t text_display_clear(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire mutex for clear");
        return ESP_ERR_TIMEOUT;
    }

    s_msg_head = 0;
    s_msg_tail = 0;
    s_msg_count = 0;
    s_scroll_x = 0;

    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Messages cleared");
    return ESP_OK;
}

esp_err_t text_display_render(uint16_t *fb, uint16_t width, uint16_t height)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (fb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* --- Render status icon in top-right corner of text area --- */
    if (s_status_icon != STATUS_ICON_NONE) {
        const uint16_t *bitmap = s_icon_bitmaps[s_status_icon];
        uint16_t color = s_icon_colors[s_status_icon];
        if (bitmap != NULL) {
            int16_t icon_x = width - TEXT_DISPLAY_ICON_SIZE - TEXT_DISPLAY_ICON_MARGIN;
            int16_t icon_y = (int16_t)s_y_start + TEXT_DISPLAY_ICON_MARGIN;
            draw_icon_16x16(fb, width, height, icon_x, icon_y, bitmap, color);
        }
    }

    /* --- Render scrolling text --- */
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        /* Non-critical: skip this frame if mutex unavailable */
        return ESP_OK;
    }

    if (queue_is_empty()) {
        xSemaphoreGive(s_mutex);
        return ESP_OK;
    }

    message_entry_t *current = queue_peek();
    if (current == NULL) {
        xSemaphoreGive(s_mutex);
        return ESP_OK;
    }

    /* Mark start time if this is a new message */
    if (current->start_time_ms == 0) {
        current->start_time_ms = get_time_ms();
        s_scroll_x = 0;
    }

    /* Check auto-timeout */
    int64_t elapsed = get_time_ms() - current->start_time_ms;
    if (s_display_timeout_ms > 0 && elapsed > (int64_t)s_display_timeout_ms) {
        /* Message timed out, advance to next */
        queue_pop();
        s_scroll_x = 0;

        if (!queue_is_empty()) {
            current = queue_peek();
            if (current != NULL) {
                current->start_time_ms = get_time_ms();
            }
        } else {
            xSemaphoreGive(s_mutex);
            return ESP_OK;
        }
    }

    /* Copy current text to local buffer (avoid holding mutex during render) */
    char local_text[TEXT_DISPLAY_MAX_TEXT_LEN];
    strncpy(local_text, current->text, TEXT_DISPLAY_MAX_TEXT_LEN - 1);
    local_text[TEXT_DISPLAY_MAX_TEXT_LEN - 1] = '\0';
    int16_t local_scroll_x = s_scroll_x;

    xSemaphoreGive(s_mutex);

    /* Draw semi-transparent dark bar behind text */
    draw_text_bar(fb, width, height, s_y_start, s_height);

    /* Draw scrolling text */
    int16_t text_y = (int16_t)s_y_start + (s_height - TEXT_DISPLAY_CHAR_HEIGHT) / 2;
    int16_t text_x = -local_scroll_x;

    draw_text(fb, width, height, text_x, text_y, local_text, COLOR_WHITE);

    /* If message is wider than screen, also draw wrapped copy for seamless scroll */
    int16_t text_width = text_pixel_width(local_text);
    if (text_width > width) {
        /* Draw second copy after the first for seamless loop */
        draw_text(fb, width, height, text_x + text_width + TEXT_DISPLAY_CHAR_WIDTH * 2,
                  text_y, local_text, COLOR_WHITE);
    }

    /* Advance scroll */
    s_scroll_x += s_scroll_speed;

    /* Check if message has scrolled completely off screen */
    if (text_width <= width) {
        /* Short message: scroll off right edge */
        if (s_scroll_x > text_width + width) {
            /* Message scrolled off, advance to next */
            if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                queue_pop();
                s_scroll_x = 0;
                if (!queue_is_empty()) {
                    current = queue_peek();
                    if (current != NULL) {
                        current->start_time_ms = get_time_ms();
                    }
                }
                xSemaphoreGive(s_mutex);
            }
        }
    } else {
        /* Long message: seamless loop until timeout */
        if (s_scroll_x >= text_width + TEXT_DISPLAY_CHAR_WIDTH * 2) {
            s_scroll_x = 0;
        }
    }

    return ESP_OK;
}

esp_err_t text_display_set_status_icon(status_icon_t icon)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (icon >= STATUS_ICON_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    s_status_icon = icon;
    return ESP_OK;
}

bool text_display_is_initialized(void)
{
    return s_initialized;
}
