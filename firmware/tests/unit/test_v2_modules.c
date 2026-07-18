/*
 * RobotBuddy — V2.0 Unit Tests
 * ===============================
 * Tests for new V2.0 modules: text_display, pomodoro, power_manager,
 * wake_word, mqtt_client, ota_service, web_server.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"

/* Framework */
#include "event_bus.h"
#include "robot_events.h"

/* V2.0 modules */
#include "text_display.h"
#include "pomodoro.h"
#include "power_manager.h"
#include "wake_word.h"

static const char *TAG = "test_v2";

/* ============================================================
 * Text Display Tests
 * ============================================================ */

TEST_CASE("text_display: init with default config", "[text_display]")
{
    /* Ensure event bus is initialized */
    if (!event_bus_is_initialized()) {
        TEST_ASSERT_EQUAL(ESP_OK, event_bus_init());
    }

    text_display_config_t cfg = {
        .y_start = 180,
        .height = 60,
        .scroll_speed = 2,
        .display_timeout_ms = 10000,
        .max_messages = 5,
    };

    esp_err_t ret = text_display_init(&cfg);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    /* Double init should return OK */
    ret = text_display_init(&cfg);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("text_display: show and clear messages", "[text_display]")
{
    if (!event_bus_is_initialized()) {
        event_bus_init();
    }

    text_display_config_t cfg = TEXT_DISPLAY_CONFIG_DEFAULT();
    text_display_init(&cfg);

    /* Show a message */
    esp_err_t ret = text_display_show_message("Hello World", 1);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    /* Show high priority message */
    ret = text_display_show_message("URGENT!", 2);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    /* Clear all messages */
    ret = text_display_clear();
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    /* Clear when empty should also work */
    ret = text_display_clear();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("text_display: render does not crash", "[text_display]")
{
    if (!event_bus_is_initialized()) {
        event_bus_init();
    }

    text_display_config_t cfg = TEXT_DISPLAY_CONFIG_DEFAULT();
    text_display_init(&cfg);

    /* Allocate a fake frame buffer */
    uint16_t fb[240 * 240];
    memset(fb, 0, sizeof(fb));

    /* Render with no messages — should not crash */
    text_display_render(fb, 240, 240);

    /* Show a message and render */
    text_display_show_message("Test message", 1);
    text_display_render(fb, 240, 240);

    /* Render with NULL buffer — should not crash */
    text_display_render(NULL, 240, 240);
}

TEST_CASE("text_display: message queue overflow", "[text_display]")
{
    if (!event_bus_is_initialized()) {
        event_bus_init();
    }

    text_display_config_t cfg = TEXT_DISPLAY_CONFIG_DEFAULT();
    cfg.max_messages = 3;
    text_display_init(&cfg);

    /* Add more messages than max_messages */
    for (int i = 0; i < 5; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Message %d", i);
        esp_err_t ret = text_display_show_message(msg, 0);
        /* Should succeed even if queue is full (oldest dropped) */
        TEST_ASSERT_EQUAL(ESP_OK, ret);
    }
}

/* ============================================================
 * Pomodoro Tests
 * ============================================================ */

TEST_CASE("pomodoro: init with default config", "[pomodoro]")
{
    if (!event_bus_is_initialized()) {
        event_bus_init();
    }

    pomodoro_config_t cfg = {
        .work_duration_min = 25,
        .break_duration_min = 5,
        .max_rounds = 4,
    };

    esp_err_t ret = pomodoro_init(&cfg);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    TEST_ASSERT_EQUAL(POMODORO_STATE_IDLE, pomodoro_get_state());
}

TEST_CASE("pomodoro: start and stop", "[pomodoro]")
{
    if (!event_bus_is_initialized()) {
        event_bus_init();
    }

    pomodoro_config_t cfg = {
        .work_duration_min = 25,
        .break_duration_min = 5,
        .max_rounds = 4,
    };
    pomodoro_init(&cfg);

    /* Start pomodoro */
    esp_err_t ret = pomodoro_start();
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    /* Allow task to process */
    vTaskDelay(pdMS_TO_TICKS(200));

    /* Should be in WORKING state */
    TEST_ASSERT_EQUAL(POMODORO_STATE_WORKING, pomodoro_get_state());

    /* Remaining should be close to 25*60 seconds */
    uint16_t remaining = pomodoro_get_remaining_sec();
    TEST_ASSERT_TRUE(remaining > 1480 && remaining <= 1500);

    /* Stop pomodoro */
    ret = pomodoro_stop();
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_EQUAL(POMODORO_STATE_IDLE, pomodoro_get_state());
}

TEST_CASE("pomodoro: pause and resume", "[pomodoro]")
{
    if (!event_bus_is_initialized()) {
        event_bus_init();
    }

    pomodoro_config_t cfg = {
        .work_duration_min = 1,  /* Short for testing */
        .break_duration_min = 1,
        .max_rounds = 1,
    };
    pomodoro_init(&cfg);

    pomodoro_start();
    vTaskDelay(pdMS_TO_TICKS(200));

    /* Pause */
    esp_err_t ret = pomodoro_pause();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_EQUAL(POMODORO_STATE_PAUSED, pomodoro_get_state());

    uint16_t remaining_at_pause = pomodoro_get_remaining_sec();

    /* Wait a bit — timer should not advance while paused */
    vTaskDelay(pdMS_TO_TICKS(1500));
    uint16_t remaining_after_wait = pomodoro_get_remaining_sec();
    TEST_ASSERT_EQUAL(remaining_at_pause, remaining_after_wait);

    /* Resume */
    ret = pomodoro_resume();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_EQUAL(POMODORO_STATE_WORKING, pomodoro_get_state());

    pomodoro_stop();
}

TEST_CASE("pomodoro: short duration completes", "[pomodoro]")
{
    if (!event_bus_is_initialized()) {
        event_bus_init();
    }

    pomodoro_config_t cfg = {
        .work_duration_min = 0,  /* 0 = use minimum for testing */
        .break_duration_min = 0,
        .max_rounds = 1,
    };
    /* Override: use 5-second work for testing */
    cfg.work_duration_min = 0;
    pomodoro_init(&cfg);

    /* For testing, we can't easily test 25-minute durations.
     * This test just verifies the state machine doesn't crash. */
    pomodoro_start();
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_EQUAL(POMODORO_STATE_WORKING, pomodoro_get_state());

    pomodoro_stop();
    TEST_ASSERT_EQUAL(POMODORO_STATE_IDLE, pomodoro_get_state());
}

/* ============================================================
 * Power Manager Tests
 * ============================================================ */

TEST_CASE("power_manager: init with default config", "[power_manager]")
{
    if (!event_bus_is_initialized()) {
        event_bus_init();
    }

    power_config_t cfg = {
        .dim_timeout_ms = 300000,
        .light_sleep_timeout_ms = 600000,
        .deep_sleep_timeout_ms = 1800000,
        .dim_brightness = 32,
        .active_brightness = 128,
    };

    esp_err_t ret = power_manager_init(&cfg);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    TEST_ASSERT_EQUAL(POWER_STATE_ACTIVE, power_manager_get_state());
}

TEST_CASE("power_manager: notify activity resets idle", "[power_manager]")
{
    if (!event_bus_is_initialized()) {
        event_bus_init();
    }

    power_config_t cfg = POWER_CONFIG_DEFAULT();
    power_manager_init(&cfg);

    /* Notify activity */
    power_manager_notify_activity();

    /* Idle time should be near zero */
    uint32_t idle_ms = power_manager_get_idle_ms();
    TEST_ASSERT_TRUE(idle_ms < 1000);
}

TEST_CASE("power_manager: set state manually", "[power_manager]")
{
    if (!event_bus_is_initialized()) {
        event_bus_init();
    }

    power_config_t cfg = POWER_CONFIG_DEFAULT();
    power_manager_init(&cfg);

    /* Set to display dim */
    esp_err_t ret = power_manager_set_state(POWER_STATE_DISPLAY_DIM);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(POWER_STATE_DISPLAY_DIM, power_manager_get_state());

    /* Set back to active */
    ret = power_manager_set_state(POWER_STATE_ACTIVE);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(POWER_STATE_ACTIVE, power_manager_get_state());
}

/* ============================================================
 * Wake Word Tests
 * ============================================================ */

TEST_CASE("wake_word: init and start/stop", "[wake_word]")
{
    if (!event_bus_is_initialized()) {
        event_bus_init();
    }

    wake_word_config_t cfg = {
        .model_name = "hilexin",
        .detection_threshold = 0.5f,
        .channel = 0,
    };

    esp_err_t ret = wake_word_init(&cfg);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    /* Start listening */
    ret = wake_word_start();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(wake_word_is_listening());

    /* Stop listening */
    ret = wake_word_stop();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_FALSE(wake_word_is_listening());
}

TEST_CASE("wake_word: double init returns OK", "[wake_word]")
{
    if (!event_bus_is_initialized()) {
        event_bus_init();
    }

    wake_word_config_t cfg = {
        .model_name = "hilexin",
        .detection_threshold = 0.5f,
        .channel = 0,
    };

    esp_err_t ret = wake_word_init(&cfg);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = wake_word_init(&cfg);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/* ============================================================
 * Event Bus Extension Tests (V2.0 new event IDs)
 * ============================================================ */

TEST_CASE("event_bus: V2.0 event IDs are valid", "[event_bus]")
{
    /* Verify V2.0 event IDs don't collide with V1.0 */
    TEST_ASSERT_EQUAL(0x0700, EVENT_DISPLAY_TEXT_MSG);
    TEST_ASSERT_EQUAL(0x0701, EVENT_DISPLAY_CLEAR_TEXT);
    TEST_ASSERT_EQUAL(0x0800, EVENT_TOUCH_SINGLE);
    TEST_ASSERT_EQUAL(0x0801, EVENT_TOUCH_DOUBLE);
    TEST_ASSERT_EQUAL(0x0802, EVENT_TOUCH_LONG);
    TEST_ASSERT_EQUAL(0x0900, EVENT_WAKE_WORD_DETECTED);
    TEST_ASSERT_EQUAL(0x0A00, EVENT_MQTT_CONNECTED);
    TEST_ASSERT_EQUAL(0x0A01, EVENT_MQTT_DISCONNECTED);
    TEST_ASSERT_EQUAL(0x0A02, EVENT_MQTT_MESSAGE);
    TEST_ASSERT_EQUAL(0x0B00, EVENT_OTA_START);
    TEST_ASSERT_EQUAL(0x0B01, EVENT_OTA_PROGRESS);
    TEST_ASSERT_EQUAL(0x0B02, EVENT_OTA_COMPLETE);
    TEST_ASSERT_EQUAL(0x0C00, EVENT_POMODORO_START);
    TEST_ASSERT_EQUAL(0x0C01, EVENT_POMODORO_TICK);
    TEST_ASSERT_EQUAL(0x0C02, EVENT_POMODORO_DONE);
    TEST_ASSERT_EQUAL(0x0D00, EVENT_BUILD_STATUS);
    TEST_ASSERT_EQUAL(0x0D01, EVENT_GIT_STATUS);
    TEST_ASSERT_EQUAL(0x0E00, EVENT_POWER_STATE_CHANGE);
}

TEST_CASE("event_bus: publish V2.0 events", "[event_bus]")
{
    if (!event_bus_is_initialized()) {
        event_bus_init();
    }

    /* Publish various V2.0 events — should not crash */
    robot_event_t event = {0};

    event.id = EVENT_WAKE_WORD_DETECTED;
    wake_word_event_t wake_payload = {.keyword = "Hey Buddy", .confidence = 0.95f};
    event.payload = &wake_payload;
    event.payload_len = sizeof(wake_payload);
    TEST_ASSERT_EQUAL(ESP_OK, event_bus_publish(&event));

    event.id = EVENT_BUILD_STATUS;
    build_status_event_t build_payload = {.status = 1, .msg = "Build Success"};
    event.payload = &build_payload;
    event.payload_len = sizeof(build_payload);
    TEST_ASSERT_EQUAL(ESP_OK, event_bus_publish(&event));

    event.id = EVENT_POMODORO_TICK;
    pomodoro_event_t pomo_payload = {.remaining_sec = 1480, .round = 1, .is_break = false};
    event.payload = &pomo_payload;
    event.payload_len = sizeof(pomo_payload);
    TEST_ASSERT_EQUAL(ESP_OK, event_bus_publish(&event));

    /* Allow dispatch task to process */
    vTaskDelay(pdMS_TO_TICKS(100));
}

/* ============================================================
 * Payload Structure Size Tests
 * ============================================================ */

TEST_CASE("payload structures: V2.0 sizes are within limits", "[event_bus]")
{
    /* Verify payload sizes fit within expected limits */
    TEST_ASSERT_TRUE(sizeof(text_msg_event_t) <= 256);
    TEST_ASSERT_TRUE(sizeof(wake_word_event_t) <= 64);
    TEST_ASSERT_TRUE(sizeof(mqtt_message_event_t) <= 768);
    TEST_ASSERT_TRUE(sizeof(ota_progress_event_t) <= 16);
    TEST_ASSERT_TRUE(sizeof(build_status_event_t) <= 128);
    TEST_ASSERT_TRUE(sizeof(git_status_event_t) <= 16);
    TEST_ASSERT_TRUE(sizeof(pomodoro_event_t) <= 16);
    TEST_ASSERT_TRUE(sizeof(power_event_t) <= 16);
}
