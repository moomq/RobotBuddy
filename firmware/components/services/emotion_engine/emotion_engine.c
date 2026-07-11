/*
 * RobotBuddy — Emotion Engine Implementation
 * ==============================================
 * Renders facial expressions (eyes) on the display frame buffer.
 * Supports 6 MVP emotions with blink animation.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "emotion_engine.h"
#include "display_manager.h"

#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "emotion";

/* ============================================================
 * Emotion Configurations (6 MVP emotions)
 * ============================================================ */

static const emotion_config_t s_emotion_configs[EMOTION_COUNT] = {
    /* IDLE — relaxed, occasional blink */
    [EMOTION_IDLE] = {
        .id = EMOTION_IDLE,
        .name = "IDLE",
        .eye_color = COLOR_CYAN,
        .bg_color = COLOR_DARK_BG,
        .blink_probability = EMOTION_BLINK_PROBABILITY,
        .animation_interval_ms = 50,
    },
    /* LISTENING — wide eyes, focused */
    [EMOTION_LISTENING] = {
        .id = EMOTION_LISTENING,
        .name = "LISTENING",
        .eye_color = COLOR_CYAN,
        .bg_color = COLOR_DARK_BG,
        .blink_probability = 1,   /* Rare blink when listening */
        .animation_interval_ms = 40,
    },
    /* THINKING — eyes move side to side */
    [EMOTION_THINKING] = {
        .id = EMOTION_THINKING,
        .name = "THINKING",
        .eye_color = COLOR_BLUE,
        .bg_color = COLOR_DARK_BG,
        .blink_probability = 2,
        .animation_interval_ms = 60,
    },
    /* ANSWERING — eyes move with speech rhythm */
    [EMOTION_ANSWERING] = {
        .id = EMOTION_ANSWERING,
        .name = "ANSWERING",
        .eye_color = COLOR_CYAN,
        .bg_color = COLOR_DARK_BG,
        .blink_probability = 2,
        .animation_interval_ms = 50,
    },
    /* HAPPY — crescent moon eyes (smile) */
    [EMOTION_HAPPY] = {
        .id = EMOTION_HAPPY,
        .name = "HAPPY",
        .eye_color = COLOR_GREEN,
        .bg_color = COLOR_DARK_BG,
        .blink_probability = 0,   /* No blink when happy */
        .animation_interval_ms = 50,
    },
    /* CONFUSED — tilted, question mark look */
    [EMOTION_CONFUSED] = {
        .id = EMOTION_CONFUSED,
        .name = "CONFUSED",
        .eye_color = COLOR_ORANGE,
        .bg_color = COLOR_DARK_BG,
        .blink_probability = 4,
        .animation_interval_ms = 60,
    },
    /* WARNING — yellow flashing */
    [EMOTION_WARNING] = {
        .id = EMOTION_WARNING,
        .name = "WARNING",
        .eye_color = COLOR_YELLOW,
        .bg_color = COLOR_DARK_BG,
        .blink_probability = 0,
        .animation_interval_ms = 30,
    },
    /* ERROR — red angry eyes */
    [EMOTION_ERROR] = {
        .id = EMOTION_ERROR,
        .name = "ERROR",
        .eye_color = COLOR_RED,
        .bg_color = COLOR_DARK_BG,
        .blink_probability = 0,
        .animation_interval_ms = 30,
    },
    /* FOCUS — half-closed, timer display */
    [EMOTION_FOCUS] = {
        .id = EMOTION_FOCUS,
        .name = "FOCUS",
        .eye_color = COLOR_CYAN,
        .bg_color = COLOR_DARK_BG,
        .blink_probability = 1,
        .animation_interval_ms = 80,
    },
    /* SLEEP — closed eyes, slow breathing */
    [EMOTION_SLEEP] = {
        .id = EMOTION_SLEEP,
        .name = "SLEEP",
        .eye_color = COLOR_DARK_CYAN,
        .bg_color = COLOR_DARK_BG,
        .blink_probability = 0,
        .animation_interval_ms = 100,
    },
    /* EXCITED — bouncing animation */
    [EMOTION_EXCITED] = {
        .id = EMOTION_EXCITED,
        .name = "EXCITED",
        .eye_color = COLOR_CYAN,
        .bg_color = COLOR_DARK_BG,
        .blink_probability = 2,
        .animation_interval_ms = 40,
    },
};

/* ============================================================
 * Module State
 * ============================================================ */

static emotion_id_t s_current_emotion = EMOTION_IDLE;
static bool s_initialized = false;

/* Display geometry */
static uint16_t s_display_width = DISPLAY_WIDTH;
static uint16_t s_display_height = DISPLAY_HEIGHT;
static uint16_t s_left_eye_cx = 80;    /* Left eye center X */
static uint16_t s_left_eye_cy = 120;   /* Left eye center Y */
static uint16_t s_right_eye_cx = 160;  /* Right eye center X */
static uint16_t s_right_eye_cy = 120;  /* Right eye center Y */
static uint16_t s_eye_radius = EMOTION_EYE_RADIUS_DEFAULT;

/* Animation state */
static eye_state_t s_left_eye;
static eye_state_t s_right_eye;
static uint32_t s_frame_counter = 0;
static uint8_t s_blink_timer = 0;    /* Countdown for blink frames */

/* Pseudo-random for blink (simple LCG) */
static uint32_t s_rng_state = 42;

static uint8_t rng_next(void)
{
    s_rng_state = (s_rng_state * 1103515245 + 12345) & 0xFF;
    return (uint8_t)(s_rng_state);
}

/* ============================================================
 * Internal: Eye rendering helpers
 * ============================================================ */

/**
 * @brief Draw a single eye (sclera + iris + pupil + highlight)
 */
static void render_eye(uint16_t *fb, uint16_t width, uint16_t height,
                        const eye_state_t *eye, uint16_t bg_color)
{
    int16_t cx = (int16_t)eye->center_x + eye->offset_x;
    int16_t cy = (int16_t)eye->center_y + eye->offset_y;

    /* Squish factor: 100 = round, lower = squished */
    uint16_t effective_ry = (eye->radius * eye->squish_y) / 100;

    if (eye->is_blinking || eye->squish_y < 10) {
        /* Blinking: draw a thin line instead of eye */
        /* Draw horizontal line at eye center */
        int16_t line_width = (int16_t)(eye->radius * 0.8f);
        for (int16_t x = cx - line_width; x <= cx + line_width; x++) {
            if (x >= 0 && x < width && cy >= 0 && cy < height) {
                fb[cy * width + x] = eye->eye_color;
            }
            /* Thicken line by 1 pixel */
            if (cy + 1 >= 0 && cy + 1 < height && x >= 0 && x < width) {
                fb[(cy + 1) * width + x] = eye->eye_color;
            }
        }
        return;
    }

    /* Draw sclera (white ellipse) */
    int32_t rx = (int32_t)eye->radius;
    int32_t ry = (int32_t)effective_ry;
    int32_t rx_sq = rx * rx;
    int32_t ry_sq = ry * ry;

    for (int16_t y = (int16_t)(cy - ry); y <= (int16_t)(cy + ry); y++) {
        if (y < 0 || y >= height) continue;
        for (int16_t x = (int16_t)(cx - rx); x <= (int16_t)(cx + rx); x++) {
            if (x < 0 || x >= width) continue;
            int32_t dx = x - cx;
            int32_t dy = y - cy;
            if (dx * dx * ry_sq + dy * dy * rx_sq <= rx_sq * ry_sq) {
                fb[y * width + x] = COLOR_WHITE;
            }
        }
    }

    /* Draw iris (colored circle, offset by look direction) */
    int16_t iris_cx = cx + eye->offset_x;
    int16_t iris_cy = cy + eye->offset_y;
    int16_t iris_r = (int16_t)eye->iris_radius;

    for (int16_t y = iris_cy - iris_r; y <= iris_cy + iris_r; y++) {
        if (y < 0 || y >= height) continue;
        for (int16_t x = iris_cx - iris_r; x <= iris_cx + iris_r; x++) {
            if (x < 0 || x >= width) continue;
            int32_t dx = x - iris_cx;
            int32_t dy = y - iris_cy;
            if (dx * dx + dy * dy <= (int32_t)iris_r * iris_r) {
                fb[y * width + x] = eye->eye_color;
            }
        }
    }

    /* Draw pupil (dark circle inside iris) */
    int16_t pupil_cx = iris_cx;
    int16_t pupil_cy = iris_cy;
    int16_t pupil_r = (int16_t)eye->pupil_radius;

    for (int16_t y = pupil_cy - pupil_r; y <= pupil_cy + pupil_r; y++) {
        if (y < 0 || y >= height) continue;
        for (int16_t x = pupil_cx - pupil_r; x <= pupil_cx + pupil_r; x++) {
            if (x < 0 || x >= width) continue;
            int32_t dx = x - pupil_cx;
            int32_t dy = y - pupil_cy;
            if (dx * dx + dy * dy <= (int32_t)pupil_r * pupil_r) {
                fb[y * width + x] = eye->pupil_color;
            }
        }
    }

    /* Draw highlight (specular reflection - small white circle) */
    int16_t hl_cx = iris_cx - (iris_r / 3);
    int16_t hl_cy = iris_cy - (iris_r / 3);
    int16_t hl_r = 3;  /* Small highlight dot */

    for (int16_t y = hl_cy - hl_r; y <= hl_cy + hl_r; y++) {
        if (y < 0 || y >= height) continue;
        for (int16_t x = hl_cx - hl_r; x <= hl_cx + hl_r; x++) {
            if (x < 0 || x >= width) continue;
            int32_t dx = x - hl_cx;
            int32_t dy = y - hl_cy;
            if (dx * dx + dy * dy <= hl_r * hl_r) {
                fb[y * width + x] = eye->highlight_color;
            }
        }
    }
}

/* ============================================================
 * Internal: Animation update per emotion
 * ============================================================ */

static void update_emotion_animation(emotion_id_t emotion)
{
    const emotion_config_t *cfg = &s_emotion_configs[emotion];
    s_frame_counter++;

    /* Reset eyes to default */
    s_left_eye.radius = s_eye_radius;
    s_right_eye.radius = s_eye_radius;
    s_left_eye.squish_y = 100;   /* 100 = round */
    s_right_eye.squish_y = 100;
    s_left_eye.offset_x = 0;
    s_left_eye.offset_y = 0;
    s_right_eye.offset_x = 0;
    s_right_eye.offset_y = 0;
    s_left_eye.eye_color = cfg->eye_color;
    s_right_eye.eye_color = cfg->eye_color;

    /* Emotion-specific animation */
    switch (emotion) {
        case EMOTION_IDLE:
            /* Subtle idle animation: slight random look */
            if (s_frame_counter % 60 == 0) {
                s_left_eye.offset_x = (rng_next() % 5) - 2;
                s_right_eye.offset_x = s_left_eye.offset_x;
                s_left_eye.offset_y = (rng_next() % 3) - 1;
                s_right_eye.offset_y = s_left_eye.offset_y;
            }
            break;

        case EMOTION_LISTENING:
            /* Wide eyes, focused */
            s_left_eye.radius = s_eye_radius + 4;
            s_right_eye.radius = s_eye_radius + 4;
            s_left_eye.iris_radius = EMOTION_IRIS_RADIUS_DEFAULT + 2;
            s_right_eye.iris_radius = EMOTION_IRIS_RADIUS_DEFAULT + 2;
            break;

        case EMOTION_THINKING:
            /* Eyes look up and side to side */
            s_left_eye.offset_x = (int16_t)(5 * sin(s_frame_counter * 0.05f));
            s_right_eye.offset_x = s_left_eye.offset_x;
            s_left_eye.offset_y = -5;
            s_right_eye.offset_y = -5;
            break;

        case EMOTION_ANSWERING:
            /* Eyes follow speech rhythm (subtle movement) */
            s_left_eye.offset_x = (int16_t)(3 * sin(s_frame_counter * 0.1f));
            s_right_eye.offset_x = s_left_eye.offset_x;
            break;

        case EMOTION_HAPPY:
            /* Crescent moon eyes (squished to ~20% height) */
            s_left_eye.squish_y = 25;
            s_right_eye.squish_y = 25;
            break;

        case EMOTION_CONFUSED:
            /* One eye slightly up, one slightly down */
            s_left_eye.offset_y = -3;
            s_right_eye.offset_y = 3;
            s_left_eye.offset_x = 3;
            s_right_eye.offset_x = -3;
            break;

        case EMOTION_WARNING:
            /* Flashing: alternate between normal and bright */
            if (s_frame_counter % 10 < 5) {
                s_left_eye.eye_color = COLOR_YELLOW;
                s_right_eye.eye_color = COLOR_YELLOW;
            } else {
                s_left_eye.eye_color = COLOR_ORANGE;
                s_right_eye.eye_color = COLOR_ORANGE;
            }
            break;

        case EMOTION_ERROR:
            /* X-shaped eyes (squished to thin line + diagonal) */
            s_left_eye.squish_y = 15;
            s_right_eye.squish_y = 15;
            s_left_eye.offset_x = 4;
            s_right_eye.offset_x = -4;
            break;

        case EMOTION_FOCUS:
            /* Half-closed eyes */
            s_left_eye.squish_y = 50;
            s_right_eye.squish_y = 50;
            s_left_eye.offset_y = 3;
            s_right_eye.offset_y = 3;
            break;

        case EMOTION_SLEEP:
            /* Closed eyes (thin line) */
            s_left_eye.squish_y = 8;
            s_right_eye.squish_y = 8;
            /* Slow breathing: subtle width oscillation */
            s_left_eye.radius = s_eye_radius - 2 + (rng_next() % 3);
            s_right_eye.radius = s_left_eye.radius;
            break;

        case EMOTION_EXCITED:
            /* Bigger eyes, bouncing */
            s_left_eye.radius = s_eye_radius + 6;
            s_right_eye.radius = s_eye_radius + 6;
            s_left_eye.offset_y = (int16_t)(4 * sin(s_frame_counter * 0.15f));
            s_right_eye.offset_y = s_left_eye.offset_y;
            break;

        default:
            break;
    }

    /* Blink logic */
    if (s_blink_timer > 0) {
        /* Currently in blink animation */
        s_left_eye.is_blinking = true;
        s_right_eye.is_blinking = true;
        s_left_eye.squish_y = (s_blink_timer > EMOTION_BLINK_FRAMES / 2) ? 8 : 30;
        s_right_eye.squish_y = s_left_eye.squish_y;
        s_blink_timer--;
    } else {
        s_left_eye.is_blinking = false;
        s_right_eye.is_blinking = false;

        /* Random blink trigger */
        if (cfg->blink_probability > 0 && rng_next() < cfg->blink_probability) {
            s_blink_timer = EMOTION_BLINK_FRAMES;
        }
    }
}

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t emotion_engine_init(const emotion_engine_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Emotion engine already initialized");
        return ESP_OK;
    }

    if (config != NULL) {
        s_display_width = config->display_width;
        s_display_height = config->display_height;
        s_left_eye_cx = config->left_eye_cx;
        s_left_eye_cy = config->left_eye_cy;
        s_right_eye_cx = config->right_eye_cx;
        s_right_eye_cy = config->right_eye_cy;
        s_eye_radius = config->eye_radius;
    }

    /* Initialize eye states */
    s_left_eye.center_x = s_left_eye_cx;
    s_left_eye.center_y = s_left_eye_cy;
    s_left_eye.radius = s_eye_radius;
    s_left_eye.iris_radius = EMOTION_IRIS_RADIUS_DEFAULT;
    s_left_eye.pupil_radius = EMOTION_PUPIL_RADIUS_DEFAULT;
    s_left_eye.eye_color = COLOR_CYAN;
    s_left_eye.pupil_color = COLOR_BLACK;
    s_left_eye.highlight_color = COLOR_WHITE;
    s_left_eye.squish_y = 100;
    s_left_eye.offset_x = 0;
    s_left_eye.offset_y = 0;
    s_left_eye.is_blinking = false;
    s_left_eye.blink_frame = 0;

    s_right_eye.center_x = s_right_eye_cx;
    s_right_eye.center_y = s_right_eye_cy;
    s_right_eye.radius = s_eye_radius;
    s_right_eye.iris_radius = EMOTION_IRIS_RADIUS_DEFAULT;
    s_right_eye.pupil_radius = EMOTION_PUPIL_RADIUS_DEFAULT;
    s_right_eye.eye_color = COLOR_CYAN;
    s_right_eye.pupil_color = COLOR_BLACK;
    s_right_eye.highlight_color = COLOR_WHITE;
    s_right_eye.squish_y = 100;
    s_right_eye.offset_x = 0;
    s_right_eye.offset_y = 0;
    s_right_eye.is_blinking = false;
    s_right_eye.blink_frame = 0;

    s_current_emotion = EMOTION_IDLE;
    s_frame_counter = 0;
    s_blink_timer = 0;

    /* Seed RNG with hardware random for non-deterministic blink patterns */
    s_rng_state = esp_random();

    s_initialized = true;
    ESP_LOGI(TAG, "Emotion engine initialized (L: %d,%d  R: %d,%d  r: %d)",
             s_left_eye_cx, s_left_eye_cy, s_right_eye_cx, s_right_eye_cy, s_eye_radius);
    return ESP_OK;
}

esp_err_t emotion_engine_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }
    s_initialized = false;
    ESP_LOGI(TAG, "Emotion engine deinitialized");
    return ESP_OK;
}

esp_err_t emotion_set_state(emotion_id_t emotion)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (emotion >= EMOTION_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    emotion_id_t prev = s_current_emotion;
    s_current_emotion = emotion;
    s_frame_counter = 0;
    s_blink_timer = 0;

    ESP_LOGI(TAG, "Emotion: %s -> %s",
             emotion_get_name(prev), emotion_get_name(emotion));
    return ESP_OK;
}

emotion_id_t emotion_get_state(void)
{
    return s_current_emotion;
}

const char *emotion_get_name(emotion_id_t emotion)
{
    if (emotion >= EMOTION_COUNT) {
        return "UNKNOWN";
    }
    return s_emotion_configs[emotion].name;
}

esp_err_t emotion_render_frame(uint16_t *fb, uint16_t width, uint16_t height)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (fb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const emotion_config_t *cfg = &s_emotion_configs[s_current_emotion];

    /* Update animation state */
    update_emotion_animation(s_current_emotion);

    /* Clear frame buffer with background color.
     * Use 32-bit writes when possible (writes two pixels at once)
     * for ~2× speedup over per-pixel loops. */
    size_t total_pixels = (size_t)width * height;
    uint16_t bg = cfg->bg_color;
    if ((bg & 0xFF) == ((bg >> 8) & 0xFF)) {
        /* High and low bytes identical — use fast memset */
        memset(fb, bg & 0xFF, total_pixels * 2);
    } else {
        /* 32-bit paired write: two pixels per store */
        uint32_t val32 = ((uint32_t)bg << 16) | bg;
        uint32_t *p32 = (uint32_t *)fb;
        size_t count = total_pixels / 2;
        for (size_t i = 0; i < count; i++) {
            p32[i] = val32;
        }
        /* Handle odd pixel if width*height is odd */
        if (total_pixels & 1) {
            fb[total_pixels - 1] = bg;
        }
    }

    /* Render left eye */
    render_eye(fb, width, height, &s_left_eye, cfg->bg_color);

    /* Render right eye */
    render_eye(fb, width, height, &s_right_eye, cfg->bg_color);

    return ESP_OK;
}

bool emotion_engine_is_initialized(void)
{
    return s_initialized;
}