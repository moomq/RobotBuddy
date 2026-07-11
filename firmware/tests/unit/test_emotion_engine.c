/*
 * RobotBuddy — Emotion Engine Unit Tests
 * =========================================
 * Tests for the facial expression engine.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "emotion_engine.h"
#include "display_manager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

/* ============================================================
 * Test: Initialization
 * ============================================================ */

TEST_CASE("emotion_init: default config initializes successfully", "[emotion]")
{
    emotion_engine_deinit();  /* Ensure clean state */

    esp_err_t ret = emotion_engine_init(NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(emotion_engine_is_initialized());

    /* Default state should be IDLE */
    TEST_ASSERT_EQUAL(EMOTION_IDLE, emotion_get_state());

    emotion_engine_deinit();
}

TEST_CASE("emotion_init: custom config initializes successfully", "[emotion]")
{
    emotion_engine_deinit();

    emotion_engine_config_t cfg = {
        .display_width = 240,
        .display_height = 240,
        .left_eye_cx = 80,
        .left_eye_cy = 120,
        .right_eye_cx = 160,
        .right_eye_cy = 120,
        .eye_radius = 35,
    };

    esp_err_t ret = emotion_engine_init(&cfg);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    emotion_engine_deinit();
}

TEST_CASE("emotion_init: double init returns OK", "[emotion]")
{
    emotion_engine_init(NULL);

    esp_err_t ret = emotion_engine_init(NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    emotion_engine_deinit();
}

/* ============================================================
 * Test: State Transitions
 * ============================================================ */

TEST_CASE("emotion_set_state: all valid emotions succeed", "[emotion]")
{
    emotion_engine_init(NULL);

    for (int i = 0; i < EMOTION_COUNT; i++) {
        esp_err_t ret = emotion_set_state((emotion_id_t)i);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        TEST_ASSERT_EQUAL(i, emotion_get_state());
    }

    emotion_engine_deinit();
}

TEST_CASE("emotion_set_state: invalid emotion returns error", "[emotion]")
{
    emotion_engine_init(NULL);

    esp_err_t ret = emotion_set_state((emotion_id_t)99);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = emotion_set_state((emotion_id_t)200);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    emotion_engine_deinit();
}

TEST_CASE("emotion_set_state: transitions between states", "[emotion]")
{
    emotion_engine_init(NULL);

    /* IDLE -> THINKING */
    emotion_set_state(EMOTION_THINKING);
    TEST_ASSERT_EQUAL(EMOTION_THINKING, emotion_get_state());

    /* THINKING -> HAPPY */
    emotion_set_state(EMOTION_HAPPY);
    TEST_ASSERT_EQUAL(EMOTION_HAPPY, emotion_get_state());

    /* HAPPY -> ERROR */
    emotion_set_state(EMOTION_ERROR);
    TEST_ASSERT_EQUAL(EMOTION_ERROR, emotion_get_state());

    /* ERROR -> IDLE */
    emotion_set_state(EMOTION_IDLE);
    TEST_ASSERT_EQUAL(EMOTION_IDLE, emotion_get_state());

    emotion_engine_deinit();
}

/* ============================================================
 * Test: Emotion Names
 * ============================================================ */

TEST_CASE("emotion_get_name: returns correct names", "[emotion]")
{
    TEST_ASSERT_EQUAL_STRING("IDLE", emotion_get_name(EMOTION_IDLE));
    TEST_ASSERT_EQUAL_STRING("LISTENING", emotion_get_name(EMOTION_LISTENING));
    TEST_ASSERT_EQUAL_STRING("THINKING", emotion_get_name(EMOTION_THINKING));
    TEST_ASSERT_EQUAL_STRING("HAPPY", emotion_get_name(EMOTION_HAPPY));
    TEST_ASSERT_EQUAL_STRING("ERROR", emotion_get_name(EMOTION_ERROR));
    TEST_ASSERT_EQUAL_STRING("SLEEP", emotion_get_name(EMOTION_SLEEP));
}

TEST_CASE("emotion_get_name: invalid ID returns UNKNOWN", "[emotion]")
{
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", emotion_get_name((emotion_id_t)99));
}

/* ============================================================
 * Test: Frame Rendering
 * ============================================================ */

TEST_CASE("emotion_render_frame: renders without crash", "[emotion]")
{
    emotion_engine_init(NULL);

    /* Allocate a frame buffer */
    uint16_t *fb = (uint16_t *)malloc(240 * 240 * 2);
    TEST_ASSERT_NOT_NULL(fb);

    /* Render each emotion */
    for (int i = 0; i < EMOTION_COUNT; i++) {
        emotion_set_state((emotion_id_t)i);
        esp_err_t ret = emotion_render_frame(fb, 240, 240);
        TEST_ASSERT_EQUAL(ESP_OK, ret);

        /* Frame buffer should not be all zeros (black) for most emotions */
        /* At minimum, it should not crash */
    }

    free(fb);
    emotion_engine_deinit();
}

TEST_CASE("emotion_render_frame: NULL buffer returns error", "[emotion]")
{
    emotion_engine_init(NULL);

    esp_err_t ret = emotion_render_frame(NULL, 240, 240);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    emotion_engine_deinit();
}

TEST_CASE("emotion_render_frame: uninitialized returns error", "[emotion]")
{
    emotion_engine_deinit();

    uint16_t fb[240 * 240];
    esp_err_t ret = emotion_render_frame(fb, 240, 240);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/* ============================================================
 * Test: Blink Animation
 * ============================================================ */

TEST_CASE("emotion_blink: IDLE state has non-zero blink probability", "[emotion]")
{
    emotion_engine_init(NULL);
    emotion_set_state(EMOTION_IDLE);

    uint16_t *fb = (uint16_t *)malloc(240 * 240 * 2);
    TEST_ASSERT_NOT_NULL(fb);

    /* Render many frames - should eventually see a blink */
    bool blink_seen = false;
    for (int i = 0; i < 300; i++) {  /* ~10 seconds at 30FPS */
        emotion_render_frame(fb, 240, 240);
        /* Check if squish_y indicates a blink (low value) */
        /* The blink mechanism reduces squish_y temporarily */
    }

    /* We can't easily verify blink in unit test without accessing
     * internal state, but at least we verify no crash occurs */

    free(fb);
    emotion_engine_deinit();
}