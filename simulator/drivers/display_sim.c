/*
 * RobotBuddy — Display Manager PC Simulator (SDL2)
 * ====================================================
 * Implements the display_manager.h interface using SDL2
 * for rendering the emotion engine frame buffer on a PC window.
 *
 * This file replaces the ESP32 display_manager.c which uses
 * PSRAM allocation and SPI output to ST7789 LCD.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "display_manager.h"
#include "display_sim.h"
#include "esp_log.h"
#include <SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "display_sim";

/* ============================================================
 * Module State
 * ============================================================ */

static uint16_t *s_frame_buffer = NULL;
static bool s_initialized = false;
/* Note: All module state is unprotected — this simulator is
 * single-threaded (main loop drives all logic). Do not call
 * API from multiple threads simultaneously. */
static uint16_t s_width = DISPLAY_WIDTH;
static uint16_t s_height = DISPLAY_HEIGHT;
static float s_fps = 0.0f;
static uint32_t s_frame_count = 0;
static uint32_t s_fps_timestamp = 0;

/* SDL2 resources */
static SDL_Window *s_window = NULL;
static SDL_Renderer *s_renderer = NULL;
static SDL_Texture *s_texture = NULL;
static int s_scale = 2;  /* Default window scale factor */

/* ============================================================
 * Internal: FPS calculation
 * ============================================================ */

static void update_fps(void)
{
    s_frame_count++;
    uint32_t now = SDL_GetTicks();
    uint32_t elapsed = now - s_fps_timestamp;

    if (elapsed >= 1000) {
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

    /* Allocate frame buffer (PC: standard malloc, no PSRAM needed) */
    size_t fb_size = (size_t)s_width * (size_t)s_height * DISPLAY_BPP;
    s_frame_buffer = (uint16_t *)malloc(fb_size);
    if (s_frame_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate frame buffer (%zu bytes)", fb_size);
        return ESP_ERR_NO_MEM;
    }
    memset(s_frame_buffer, 0, fb_size);
    ESP_LOGI(TAG, "Frame buffer allocated: %zu bytes", fb_size);

    /* Initialize SDL2 */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        ESP_LOGE(TAG, "SDL_Init failed: %s", SDL_GetError());
        free(s_frame_buffer);
        s_frame_buffer = NULL;
        return ESP_FAIL;
    }

    /* Create window */
    char title[128];
    snprintf(title, sizeof(title),
             "RobotBuddy Emotion Simulator | IDLE | FPS: -- | "
             "Keys: 1-0/Q=emotion, ESC=quit");
    s_window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        s_width * s_scale, s_height * s_scale,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (s_window == NULL) {
        ESP_LOGE(TAG, "SDL_CreateWindow failed: %s", SDL_GetError());
        free(s_frame_buffer);
        s_frame_buffer = NULL;
        SDL_Quit();
        return ESP_FAIL;
    }

    /* Create renderer */
    s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_ACCELERATED);
    if (s_renderer == NULL) {
        ESP_LOGE(TAG, "SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(s_window);
        s_window = NULL;
        free(s_frame_buffer);
        s_frame_buffer = NULL;
        SDL_Quit();
        return ESP_FAIL;
    }

    /* Set logical size so the texture scales to fill the window */
    SDL_RenderSetLogicalSize(s_renderer, s_width, s_height);

    /* Create streaming texture for RGB565 pixel data */
    s_texture = SDL_CreateTexture(
        s_renderer,
        SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING,
        s_width, s_height
    );
    if (s_texture == NULL) {
        ESP_LOGE(TAG, "SDL_CreateTexture failed: %s", SDL_GetError());
        SDL_DestroyRenderer(s_renderer);
        s_renderer = NULL;
        SDL_DestroyWindow(s_window);
        s_window = NULL;
        free(s_frame_buffer);
        s_frame_buffer = NULL;
        SDL_Quit();
        return ESP_FAIL;
    }

    /* Clear window to black */
    SDL_SetRenderDrawColor(s_renderer, 0, 0, 0, 255);
    SDL_RenderClear(s_renderer);
    SDL_RenderPresent(s_renderer);

    s_fps_timestamp = SDL_GetTicks();
    s_initialized = true;

    ESP_LOGI(TAG, "Display simulator initialized (%dx%d, scale=%dx)",
             s_width, s_height, s_scale);
    return ESP_OK;
}

esp_err_t display_manager_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    if (s_texture != NULL) {
        SDL_DestroyTexture(s_texture);
        s_texture = NULL;
    }
    if (s_renderer != NULL) {
        SDL_DestroyRenderer(s_renderer);
        s_renderer = NULL;
    }
    if (s_window != NULL) {
        SDL_DestroyWindow(s_window);
        s_window = NULL;
    }
    SDL_Quit();

    if (s_frame_buffer != NULL) {
        free(s_frame_buffer);
        s_frame_buffer = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "Display simulator deinitialized");
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

    /* Update SDL texture with the frame buffer data */
    int result = SDL_UpdateTexture(s_texture, NULL, s_frame_buffer, s_width * 2);
    if (result < 0) {
        ESP_LOGE(TAG, "SDL_UpdateTexture failed: %s", SDL_GetError());
        return ESP_FAIL;
    }

    /* Render the texture to the window */
    SDL_RenderClear(s_renderer);
    SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
    SDL_RenderPresent(s_renderer);

    update_fps();
    return ESP_OK;
}

esp_err_t display_clear(uint16_t bg_color)
{
    if (!s_initialized || s_frame_buffer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

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

    /* Use 64-bit arithmetic to prevent overflow for large radii.
     * For RobotBuddy's use case (radius <= ~50 pixels), 32-bit
     * would suffice, but the function signature accepts uint16_t. */
    uint64_t rx_sq = (uint64_t)rx * (uint64_t)rx;
    uint64_t ry_sq = (uint64_t)ry * (uint64_t)ry;
    uint64_t rhs = rx_sq * ry_sq;

    for (int32_t y = y_start; y <= y_end; y++) {
        if (y < 0 || y >= s_height) continue;
        int32_t dy = y - (int32_t)cy;
        for (int32_t x = x_start; x <= x_end; x++) {
            if (x < 0 || x >= s_width) continue;
            int32_t dx = x - (int32_t)cx;
            uint64_t lhs = (uint64_t)(dx * dx) * ry_sq + (uint64_t)(dy * dy) * rx_sq;
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
    /* PC: no backlight control, always full brightness */
    (void)brightness;
    return ESP_OK;
}

/* ============================================================
 * Simulator-specific API
 * ============================================================ */

SDL_Window *display_get_window(void)
{
    return s_window;
}

int display_get_scale(void)
{
    return s_scale;
}

void display_set_scale(int scale)
{
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    s_scale = scale;

    if (s_window != NULL) {
        SDL_SetWindowSize(s_window, s_width * s_scale, s_height * s_scale);
    }
}