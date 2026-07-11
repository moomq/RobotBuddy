/*
 * RobotBuddy — Display Manager Implementation
 * ==============================================
 * Manages ST7789 LCD, PSRAM frame buffer, and rendering primitives.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "display_manager.h"
#include "st7789.h"
#include "bsp_pinmap.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = "display_mgr";

/* ============================================================
 * Module State
 * ============================================================ */

static uint16_t *s_frame_buffer = NULL;    /**< PSRAM frame buffer */
static bool s_initialized = false;
static uint16_t s_width = DISPLAY_WIDTH;
static uint16_t s_height = DISPLAY_HEIGHT;
static float s_fps = 0.0f;
static uint32_t s_frame_count = 0;
static int64_t s_fps_timestamp = 0;

/* ============================================================
 * Internal: FPS calculation
 * ============================================================ */

static void update_fps(void)
{
    s_frame_count++;
    int64_t now = esp_timer_get_time() / 1000; /* ms */
    int64_t elapsed = now - s_fps_timestamp;

    if (elapsed >= 1000) { /* Update FPS every second */
        s_fps = (float)s_frame_count / ((float)elapsed / 1000.0f);
        s_frame_count = 0;
        s_fps_timestamp = now;
    }
}

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t display_manager_init(const display_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Display manager already initialized");
        return ESP_OK;
    }

    if (config != NULL) {
        s_width = config->width;
        s_height = config->height;
    }

    /* Allocate frame buffer in PSRAM */
    size_t fb_size = (size_t)s_width * s_height * DISPLAY_BPP;
    s_frame_buffer = (uint16_t *)heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    if (s_frame_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate frame buffer (%zu bytes)", fb_size);
        return ESP_ERR_NO_MEM;
    }

    /* Clear frame buffer to black */
    memset(s_frame_buffer, 0, fb_size);
    ESP_LOGI(TAG, "Frame buffer allocated: %zu bytes in PSRAM", fb_size);

    /* Initialize ST7789 driver */
    st7789_config_t lcd_config = {
        .spi_host = BSP_LCD_SPI_HOST,
        .pin_cs = BSP_PIN_LCD_CS,
        .pin_dc = BSP_PIN_LCD_DC,
        .pin_rst = BSP_PIN_LCD_RST,
        .pin_bl = BSP_PIN_LCD_BL,
        .spi_freq_hz = BSP_LCD_SPI_FREQ_HZ,
        .width = s_width,
        .height = s_height,
    };

    esp_err_t ret = st7789_init(&lcd_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ST7789 init failed: %s", esp_err_to_name(ret));
        heap_caps_free(s_frame_buffer);
        s_frame_buffer = NULL;
        return ret;
    }

    /* Set backlight to medium brightness */
    st7789_set_backlight(128);

    /* Push initial black frame */
    ret = st7789_draw_bitmap(0, 0, s_width, s_height, s_frame_buffer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Initial frame push failed: %s", esp_err_to_name(ret));
    }

    s_fps_timestamp = esp_timer_get_time() / 1000;
    s_initialized = true;

    ESP_LOGI(TAG, "Display manager initialized (%dx%d)", s_width, s_height);
    return ESP_OK;
}

esp_err_t display_manager_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    /* Turn off backlight */
    st7789_set_backlight(0);

    /* Deinitialize ST7789 */
    st7789_deinit();

    /* Free frame buffer */
    if (s_frame_buffer != NULL) {
        heap_caps_free(s_frame_buffer);
        s_frame_buffer = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "Display manager deinitialized");
    return ESP_OK;
}

uint16_t *display_get_framebuffer(void)
{
    if (!s_initialized) {
        return NULL;
    }
    return s_frame_buffer;
}

esp_err_t display_commit_frame(void)
{
    if (!s_initialized || s_frame_buffer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Frame buffer is allocated with MALLOC_CAP_DMA flag which ensures
     * cache coherency for DMA transfers from PSRAM on ESP-IDF v5.5.
     * TODO: When upgrading to a newer ESP-IDF with esp_cache.h, add:
     *   esp_cache_msync(s_frame_buffer, fb_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M); */

    esp_err_t ret = st7789_draw_bitmap(0, 0, s_width, s_height, s_frame_buffer);
    if (ret == ESP_OK) {
        update_fps();
    }
    return ret;
}

esp_err_t display_clear(uint16_t bg_color)
{
    if (!s_initialized || s_frame_buffer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Optimized fill: use 32-bit paired writes where possible (~2× speedup).
     * For solid colors (R=G=B), use memset which is even faster. */
    size_t total_pixels = (size_t)s_width * s_height;

    if ((bg_color & 0xFF) == ((bg_color >> 8) & 0xFF)) {
        /* High and low bytes identical — use fast memset */
        memset(s_frame_buffer, bg_color & 0xFF, total_pixels * 2);
    } else {
        /* 32-bit paired write: two pixels per store */
        uint32_t val32 = ((uint32_t)bg_color << 16) | bg_color;
        uint32_t *p32 = (uint32_t *)s_frame_buffer;
        size_t count = total_pixels / 2;
        for (size_t i = 0; i < count; i++) {
            p32[i] = val32;
        }
        /* Handle odd pixel */
        if (total_pixels & 1) {
            s_frame_buffer[total_pixels - 1] = bg_color;
        }
    }

    return ESP_OK;
}

void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (!s_initialized || s_frame_buffer == NULL) {
        return;
    }
    if (x >= s_width || y >= s_height) {
        return;
    }
    s_frame_buffer[y * s_width + x] = color;
}

void display_draw_filled_circle(uint16_t cx, uint16_t cy, uint16_t r, uint16_t color)
{
    if (!s_initialized || s_frame_buffer == NULL) {
        return;
    }

    int32_t x_start = (int32_t)cx - (int32_t)r;
    int32_t x_end = (int32_t)cx + (int32_t)r;
    int32_t y_start = (int32_t)cy - (int32_t)r;
    int32_t y_end = (int32_t)cy + (int32_t)r;
    uint32_t r_sq = (uint32_t)r * (uint32_t)r;

    for (int32_t y = y_start; y <= y_end; y++) {
        if (y < 0 || y >= s_height) continue;
        for (int32_t x = x_start; x <= x_end; x++) {
            if (x < 0 || x >= s_width) continue;
            int32_t dx = x - (int32_t)cx;
            int32_t dy = y - (int32_t)cy;
            if ((uint32_t)(dx * dx + dy * dy) <= r_sq) {
                s_frame_buffer[y * s_width + x] = color;
            }
        }
    }
}

void display_draw_filled_ellipse(uint16_t cx, uint16_t cy, uint16_t rx, uint16_t ry, uint16_t color)
{
    if (!s_initialized || s_frame_buffer == NULL) {
        return;
    }
    if (rx == 0 || ry == 0) {
        return;
    }

    int32_t x_start = (int32_t)cx - (int32_t)rx;
    int32_t x_end = (int32_t)cx + (int32_t)rx;
    int32_t y_start = (int32_t)cy - (int32_t)ry;
    int32_t y_end = (int32_t)cy + (int32_t)ry;

    /* Use fixed-point to avoid float in embedded */
    /* Check: (x-cx)^2/rx^2 + (y-cy)^2/ry^2 <= 1 */
    uint32_t rx_sq = (uint32_t)rx * (uint32_t)rx;
    uint32_t ry_sq = (uint32_t)ry * (uint32_t)ry;

    for (int32_t y = y_start; y <= y_end; y++) {
        if (y < 0 || y >= s_height) continue;
        int32_t dy = y - (int32_t)cy;
        for (int32_t x = x_start; x <= x_end; x++) {
            if (x < 0 || x >= s_width) continue;
            int32_t dx = x - (int32_t)cx;
            /* (dx^2 * ry^2 + dy^2 * rx^2) <= rx^2 * ry^2 */
            uint32_t lhs = (uint32_t)(dx * dx) * ry_sq + (uint32_t)(dy * dy) * rx_sq;
            uint32_t rhs = rx_sq * ry_sq;
            if (lhs <= rhs) {
                s_frame_buffer[y * s_width + x] = color;
            }
        }
    }
}

float display_get_fps(void)
{
    return s_fps;
}

esp_err_t display_set_backlight(uint8_t brightness)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    return st7789_set_backlight(brightness);
}