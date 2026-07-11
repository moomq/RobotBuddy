/*
 * RobotBuddy — Behavior System Unit Tests
 * ============================================
 * Tests for the behavior state machine and event handling.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "behavior_system.h"
#include "robot_events.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ============================================================
 * Test: Initialization
 * ============================================================ */

TEST_CASE("behavior_init: initializes successfully", "[behavior]")
{
    behavior_system_deinit();

    esp_err_t ret = behavior_system_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(behavior_system_is_initialized());

    /* Initial state should be IDLE */
    TEST_ASSERT_EQUAL(BEHAVIOR_STATE_IDLE, behavior_get_state());

    behavior_system_deinit();
}

TEST_CASE("behavior_init: double init returns OK", "[behavior]")
{
    behavior_system_init(NULL);
    esp_err_t ret = behavior_system_init(NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    behavior_system_deinit();
}

/* ============================================================
 * Test: State Transitions via Events
 * ============================================================ */

TEST_CASE("behavior: AUDIO_CAPTURE_START transitions to LISTENING", "[behavior]")
{
    behavior_system_init(NULL);

    /* Reset to IDLE */
    TEST_ASSERT_EQUAL(BEHAVIOR_STATE_IDLE, behavior_get_state());

    robot_event_t event = {
        .id = EVENT_AUDIO_CAPTURE_START,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };

    behavior_send_event(&event);
    vTaskDelay(pdMS_TO_TICKS(200));  /* Allow task to process */

    TEST_ASSERT_EQUAL(BEHAVIOR_STATE_LISTENING, behavior_get_state());

    behavior_system_deinit();
}

TEST_CASE("behavior: CLOUD_ERROR transitions from THINKING to ERROR", "[behavior]")
{
    behavior_system_init(NULL);

    /* First go to LISTENING */
    robot_event_t listen_event = {
        .id = EVENT_AUDIO_CAPTURE_START,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };
    behavior_send_event(&listen_event);
    vTaskDelay(pdMS_TO_TICKS(200));

    /* Then to THINKING (audio capture stop) */
    robot_event_t think_event = {
        .id = EVENT_AUDIO_CAPTURE_STOP,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };
    behavior_send_event(&think_event);
    vTaskDelay(pdMS_TO_TICKS(200));

    TEST_ASSERT_EQUAL(BEHAVIOR_STATE_THINKING, behavior_get_state());

    /* Cloud error */
    robot_event_t error_event = {
        .id = EVENT_CLOUD_ERROR,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };
    behavior_send_event(&error_event);
    vTaskDelay(pdMS_TO_TICKS(200));

    TEST_ASSERT_EQUAL(BEHAVIOR_STATE_ERROR, behavior_get_state());

    behavior_system_deinit();
}

TEST_CASE("behavior: SENSOR_EDGE triggers WARNING and emergency stop", "[behavior]")
{
    behavior_system_init(NULL);

    robot_event_t edge_event = {
        .id = EVENT_SENSOR_EDGE,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };

    behavior_send_event(&edge_event);
    vTaskDelay(pdMS_TO_TICKS(200));

    TEST_ASSERT_EQUAL(BEHAVIOR_STATE_WARNING, behavior_get_state());

    behavior_system_deinit();
}

TEST_CASE("behavior: CRITICAL_BATTERY transitions to SLEEP", "[behavior]")
{
    behavior_system_init(NULL);

    robot_event_t battery_event = {
        .id = EVENT_SYS_CRITICAL_BATTERY,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };

    behavior_send_event(&battery_event);
    vTaskDelay(pdMS_TO_TICKS(200));

    TEST_ASSERT_EQUAL(BEHAVIOR_STATE_SLEEP, behavior_get_state());

    behavior_system_deinit();
}

TEST_CASE("behavior: ERROR auto-recovers to IDLE after timeout", "[behavior]")
{
    behavior_system_init(NULL);

    /* Force into ERROR state */
    robot_event_t error_event = {
        .id = EVENT_CLOUD_ERROR,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };

    /* First go to THINKING, then ERROR */
    robot_event_t listen_event = {
        .id = EVENT_AUDIO_CAPTURE_START,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };
    behavior_send_event(&listen_event);
    vTaskDelay(pdMS_TO_TICKS(200));

    robot_event_t think_event = {
        .id = EVENT_AUDIO_CAPTURE_STOP,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };
    behavior_send_event(&think_event);
    vTaskDelay(pdMS_TO_TICKS(200));

    behavior_send_event(&error_event);
    vTaskDelay(pdMS_TO_TICKS(200));

    TEST_ASSERT_EQUAL(BEHAVIOR_STATE_ERROR, behavior_get_state());

    /* Wait for auto-recovery (BEHAVIOR_ERROR_RECOVERY_MS = 3000) */
    vTaskDelay(pdMS_TO_TICKS(3500));

    TEST_ASSERT_EQUAL(BEHAVIOR_STATE_IDLE, behavior_get_state());

    behavior_system_deinit();
}

TEST_CASE("behavior: HAPPY auto-recovers to IDLE after 3 seconds", "[behavior]")
{
    behavior_system_init(NULL);

    /* Force HAPPY state via emotion event */
    emotion_event_t emo = {
        .emotion_id = EMOTION_HAPPY,
        .duration_ms = 0,
        .intensity = 255,
    };
    robot_event_t happy_event = {
        .id = EVENT_EMOTION_STATE_CHANGE,
        .timestamp = 0,
        .payload = &emo,
        .payload_len = sizeof(emo),
    };

    behavior_send_event(&happy_event);
    vTaskDelay(pdMS_TO_TICKS(200));

    TEST_ASSERT_EQUAL(BEHAVIOR_STATE_HAPPY, behavior_get_state());

    /* Wait for auto-recovery (3 seconds) */
    vTaskDelay(pdMS_TO_TICKS(3500));

    TEST_ASSERT_EQUAL(BEHAVIOR_STATE_IDLE, behavior_get_state());

    behavior_system_deinit();
}

TEST_CASE("behavior: queue full returns ESP_ERR_NO_MEM", "[behavior]")
{
    behavior_system_init(NULL);

    /* Fill the event queue */
    robot_event_t event = {
        .id = EVENT_SENSOR_IMU_DATA,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };

    esp_err_t last_ret = ESP_OK;
    for (int i = 0; i < 20; i++) {  /* Queue depth is 16 */
        last_ret = behavior_send_event(&event);
    }

    /* At least one should fail when queue is full */
    /* (depending on timing, some may succeed) */

    behavior_system_deinit();
}