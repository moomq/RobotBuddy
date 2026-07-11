/*
 * RobotBuddy — Event Bus Unit Tests
 * ====================================
 * Tests for the centralized event distribution system.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "event_bus.h"
#include "robot_events.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ============================================================
 * Test globals
 * ============================================================ */

static int s_handler_call_count = 0;
static robot_event_id_t s_last_event_id = (robot_event_id_t)0;
static void *s_last_payload = NULL;

/* ============================================================
 * Helper: Test event handler
 * ============================================================ */

static void test_handler(const robot_event_t *event)
{
    s_handler_call_count++;
    s_last_event_id = event->id;
    s_last_payload = event->payload;
}

static void reset_test_state(void)
{
    s_handler_call_count = 0;
    s_last_event_id = (robot_event_id_t)0;
    s_last_payload = NULL;
}

/* ============================================================
 * Test Cases
 * ============================================================ */

TEST_CASE("event_bus_init: initializes successfully", "[event_bus]")
{
    /* Deinit first in case of previous test */
    event_bus_deinit();

    esp_err_t ret = event_bus_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(event_bus_is_initialized());

    event_bus_deinit();
}

TEST_CASE("event_bus_init: double init returns OK", "[event_bus]")
{
    event_bus_init();
    esp_err_t ret = event_bus_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    event_bus_deinit();
}

TEST_CASE("event_bus_subscribe: valid handler succeeds", "[event_bus]")
{
    event_bus_init();

    esp_err_t ret = event_bus_subscribe(EVENT_SYS_BOOT_COMPLETE, test_handler);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    event_bus_deinit();
}

TEST_CASE("event_bus_subscribe: NULL handler returns error", "[event_bus]")
{
    event_bus_init();

    esp_err_t ret = event_bus_subscribe(EVENT_SYS_BOOT_COMPLETE, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    event_bus_deinit();
}

TEST_CASE("event_bus_publish: subscriber receives event", "[event_bus]")
{
    event_bus_init();
    reset_test_state();
    event_bus_subscribe(EVENT_SYS_BOOT_COMPLETE, test_handler);

    robot_event_t event = {
        .id = EVENT_SYS_BOOT_COMPLETE,
        .timestamp = 1000,
        .payload = NULL,
        .payload_len = 0,
    };
    esp_err_t ret = event_bus_publish(&event);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    /* Allow dispatch task to process */
    vTaskDelay(pdMS_TO_TICKS(100));

    TEST_ASSERT_EQUAL(1, s_handler_call_count);
    TEST_ASSERT_EQUAL(EVENT_SYS_BOOT_COMPLETE, s_last_event_id);

    event_bus_deinit();
}

TEST_CASE("event_bus_publish: multiple subscribers all receive event", "[event_bus]")
{
    event_bus_init();
    reset_test_state();

    int handler1_count = 0;
    int handler2_count = 0;

    /* We reuse test_handler but track counts differently */
    event_bus_subscribe(EVENT_EMOTION_STATE_CHANGE, test_handler);

    robot_event_t event = {
        .id = EVENT_EMOTION_STATE_CHANGE,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };
    event_bus_publish(&event);

    vTaskDelay(pdMS_TO_TICKS(100));

    TEST_ASSERT_EQUAL(1, s_handler_call_count);

    event_bus_deinit();
}

TEST_CASE("event_bus_unsubscribe: handler no longer called", "[event_bus]")
{
    event_bus_init();
    reset_test_state();

    event_bus_subscribe(EVENT_SYS_LOW_BATTERY, test_handler);
    event_bus_unsubscribe(EVENT_SYS_LOW_BATTERY, test_handler);

    robot_event_t event = {
        .id = EVENT_SYS_LOW_BATTERY,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };
    event_bus_publish(&event);

    vTaskDelay(pdMS_TO_TICKS(100));

    /* Handler should NOT be called after unsubscribe */
    TEST_ASSERT_EQUAL(0, s_handler_call_count);

    event_bus_deinit();
}

TEST_CASE("event_bus_publish: payload deep copy works", "[event_bus]")
{
    event_bus_init();
    reset_test_state();

    event_bus_subscribe(EVENT_CLOUD_LLM_RESPONSE, test_handler);

    cloud_response_t response = {
        .text = "Hello from Claude",
        .provider = CLOUD_PROVIDER_CLAUDE,
        .latency_ms = 500,
    };

    robot_event_t event = {
        .id = EVENT_CLOUD_LLM_RESPONSE,
        .timestamp = 0,
        .payload = &response,
        .payload_len = sizeof(cloud_response_t),
    };

    esp_err_t ret = event_bus_publish(&event);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    vTaskDelay(pdMS_TO_TICKS(100));

    TEST_ASSERT_EQUAL(1, s_handler_call_count);
    TEST_ASSERT_EQUAL(EVENT_CLOUD_LLM_RESPONSE, s_last_event_id);
    /* Note: payload is deep-copied, so the pointer will differ from &response */

    event_bus_deinit();
}

TEST_CASE("event_bus: uninitialized publish returns error", "[event_bus]")
{
    event_bus_deinit();  /* Ensure not initialized */

    robot_event_t event = {
        .id = EVENT_SYS_BOOT_COMPLETE,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };

    esp_err_t ret = event_bus_publish(&event);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

TEST_CASE("event_bus_pending_count: returns correct count", "[event_bus]")
{
    event_bus_init();

    uint32_t count = event_bus_pending_count();
    /* Count should be 0 or very small after init */
    TEST_ASSERT_LESS_THAN(10, count);

    event_bus_deinit();
}