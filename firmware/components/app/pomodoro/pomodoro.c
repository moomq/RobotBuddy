/*
 * RobotBuddy — Pomodoro Timer Implementation
 * =============================================
 * State machine driven Pomodoro timer with event bus integration.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "pomodoro.h"
#include "robot_events.h"
#include "event_bus.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "pomodoro";

/* ============================================================
 * Module State
 * ============================================================ */

static pomodoro_config_t s_config = {
    .work_duration_min  = POMODORO_DEFAULT_WORK_MIN,
    .break_duration_min = POMODORO_DEFAULT_BREAK_MIN,
    .max_rounds         = POMODORO_DEFAULT_MAX_ROUNDS,
};

static pomodoro_state_t s_current_state   = POMODORO_STATE_IDLE;
static pomodoro_state_t s_pre_pause_state = POMODORO_STATE_IDLE;
static uint8_t  s_current_round           = 0;
static uint16_t s_remaining_sec           = 0;
static TaskHandle_t  s_task_handle        = NULL;
static QueueHandle_t s_event_queue        = NULL;
static SemaphoreHandle_t s_mutex          = NULL;
static bool s_initialized                 = false;
static volatile bool s_running            = false;

/* ============================================================
 * State names for logging
 * ============================================================ */

static const char *state_name(pomodoro_state_t state)
{
    switch (state) {
        case POMODORO_STATE_IDLE:    return "IDLE";
        case POMODORO_STATE_WORKING: return "WORKING";
        case POMODORO_STATE_BREAK:   return "BREAK";
        case POMODORO_STATE_PAUSED:  return "PAUSED";
        default:                     return "UNKNOWN";
    }
}

/* ============================================================
 * Internal: Publish pomodoro event
 * ============================================================ */

static void publish_event(robot_event_id_t event_id, uint16_t remaining, uint8_t round, bool is_break)
{
    pomodoro_event_t payload = {
        .remaining_sec = remaining,
        .round         = round,
        .is_break      = is_break,
    };

    robot_event_t event = {
        .id          = event_id,
        .timestamp   = 0,
        .payload     = &payload,
        .payload_len = sizeof(pomodoro_event_t),
    };
    event_bus_publish(&event);
}

/* ============================================================
 * Internal: State transition (must be called with mutex held)
 * ============================================================ */

static void transition_to(pomodoro_state_t new_state)
{
    if (new_state == s_current_state) {
        return;
    }

    pomodoro_state_t old_state = s_current_state;
    s_current_state = new_state;

    ESP_LOGI(TAG, "State: %s -> %s (round %u, %u sec remaining)",
             state_name(old_state), state_name(new_state),
             s_current_round, s_remaining_sec);
}

/* ============================================================
 * Internal: Start work period
 * ============================================================ */

static void start_work(void)
{
    s_current_round++;
    s_remaining_sec = (uint16_t)s_config.work_duration_min * 60;
    transition_to(POMODORO_STATE_WORKING);

    ESP_LOGI(TAG, "Work period started (round %u/%u, %u min)",
             s_current_round, s_config.max_rounds, s_config.work_duration_min);

    publish_event(EVENT_POMODORO_TICK, s_remaining_sec, s_current_round, false);
}

/* ============================================================
 * Internal: Start break period
 * ============================================================ */

static void start_break(void)
{
    s_remaining_sec = (uint16_t)s_config.break_duration_min * 60;
    transition_to(POMODORO_STATE_BREAK);

    ESP_LOGI(TAG, "Break period started (%u min)", s_config.break_duration_min);

    publish_event(EVENT_POMODORO_TICK, s_remaining_sec, s_current_round, true);
}

/* ============================================================
 * Internal: Handle 1-second tick
 * ============================================================ */

static void handle_tick(void)
{
    if (s_current_state != POMODORO_STATE_WORKING &&
        s_current_state != POMODORO_STATE_BREAK) {
        return;
    }

    if (s_remaining_sec > 0) {
        s_remaining_sec--;
    }

    bool is_break = (s_current_state == POMODORO_STATE_BREAK);

    /* Publish tick event */
    publish_event(EVENT_POMODORO_TICK, s_remaining_sec, s_current_round, is_break);

    /* Check for period completion */
    if (s_remaining_sec == 0) {
        if (s_current_state == POMODORO_STATE_WORKING) {
            ESP_LOGI(TAG, "Work period complete (round %u)", s_current_round);
            publish_event(EVENT_POMODORO_DONE, 0, s_current_round, false);

            if (s_current_round >= s_config.max_rounds) {
                /* All rounds complete — return to idle */
                ESP_LOGI(TAG, "All %u rounds complete, session done", s_config.max_rounds);
                s_current_round = 0;
                transition_to(POMODORO_STATE_IDLE);
            } else {
                start_break();
            }
        } else if (s_current_state == POMODORO_STATE_BREAK) {
            ESP_LOGI(TAG, "Break period complete (round %u)", s_current_round);
            publish_event(EVENT_POMODORO_BREAK_DONE, 0, s_current_round, true);
            start_work();
        }
    }
}

/* ============================================================
 * Internal: Process incoming events
 * ============================================================ */

static void process_event(const robot_event_t *event)
{
    switch (event->id) {
        case EVENT_TOUCH_SINGLE:
            /* Toggle pause/resume on single touch */
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            if (s_current_state == POMODORO_STATE_WORKING ||
                s_current_state == POMODORO_STATE_BREAK) {
                s_pre_pause_state = s_current_state;
                transition_to(POMODORO_STATE_PAUSED);
                ESP_LOGI(TAG, "Paused (was %s)", state_name(s_pre_pause_state));
            } else if (s_current_state == POMODORO_STATE_PAUSED) {
                transition_to(s_pre_pause_state);
                ESP_LOGI(TAG, "Resumed (back to %s)", state_name(s_pre_pause_state));
            }
            xSemaphoreGive(s_mutex);
            break;

        case EVENT_POMODORO_START:
            /* External start command (MQTT/Web) */
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            if (s_current_state == POMODORO_STATE_IDLE) {
                start_work();
            }
            xSemaphoreGive(s_mutex);
            break;

        default:
            break;
    }
}

/* ============================================================
 * Pomodoro Task
 * ============================================================ */

static void pomodoro_task(void *arg)
{
    ESP_LOGI(TAG, "Pomodoro task started");

    robot_event_t event;

    while (s_running) {
        /* Wait for events with 1-second timeout — acts as tick */
        if (xQueueReceive(s_event_queue, &event, pdMS_TO_TICKS(POMODORO_TASK_PERIOD_MS)) == pdTRUE) {
            process_event(&event);
        }

        /* Handle 1-second tick (only when timer is active) */
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        handle_tick();
        xSemaphoreGive(s_mutex);

        /* Feed watchdog */
        esp_task_wdt_reset();
    }
}

/* ============================================================
 * Event bus callback
 * ============================================================ */

static void pomodoro_event_handler(const robot_event_t *event)
{
    /* Forward event from event bus to pomodoro task queue */
    if (s_event_queue != NULL) {
        BaseType_t ret = xQueueSend(s_event_queue, event, pdMS_TO_TICKS(10));
        if (ret != pdTRUE) {
            ESP_LOGW(TAG, "Event queue full, dropping event 0x%04X", event->id);
        }
    }
}

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t pomodoro_init(const pomodoro_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Pomodoro already initialized");
        return ESP_OK;
    }

    /* Apply configuration (defaults if NULL) */
    if (config != NULL) {
        s_config = *config;
    } else {
        s_config.work_duration_min  = POMODORO_DEFAULT_WORK_MIN;
        s_config.break_duration_min = POMODORO_DEFAULT_BREAK_MIN;
        s_config.max_rounds         = POMODORO_DEFAULT_MAX_ROUNDS;
    }

    /* Create mutex */
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Create event queue */
    s_event_queue = xQueueCreate(POMODORO_EVENT_QUEUE_DEPTH, sizeof(robot_event_t));
    if (s_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    /* Subscribe to relevant events */
    event_bus_subscribe(EVENT_TOUCH_SINGLE, pomodoro_event_handler);
    event_bus_subscribe(EVENT_POMODORO_START, pomodoro_event_handler);

    /* Create pomodoro task */
    BaseType_t ret = xTaskCreatePinnedToCore(
        pomodoro_task,
        "pomodoro",
        POMODORO_TASK_STACK_SIZE,
        NULL,
        POMODORO_TASK_PRIORITY,
        &s_task_handle,
        POMODORO_TASK_CORE_ID
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create pomodoro task");
        vQueueDelete(s_event_queue);
        s_event_queue = NULL;
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_current_state   = POMODORO_STATE_IDLE;
    s_pre_pause_state  = POMODORO_STATE_IDLE;
    s_current_round    = 0;
    s_remaining_sec    = 0;
    s_running          = true;
    s_initialized      = true;

    ESP_LOGI(TAG, "Pomodoro initialized (work=%u min, break=%u min, rounds=%u)",
             s_config.work_duration_min, s_config.break_duration_min, s_config.max_rounds);
    return ESP_OK;
}

esp_err_t pomodoro_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    s_running = false;
    if (s_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(200));
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
    }

    if (s_event_queue != NULL) {
        vQueueDelete(s_event_queue);
        s_event_queue = NULL;
    }

    if (s_mutex != NULL) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "Pomodoro deinitialized");
    return ESP_OK;
}

esp_err_t pomodoro_start(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_current_state == POMODORO_STATE_IDLE) {
        start_work();
    } else {
        ESP_LOGW(TAG, "Cannot start — not idle (state=%s)", state_name(s_current_state));
    }
    xSemaphoreGive(s_mutex);

    return ESP_OK;
}

esp_err_t pomodoro_pause(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_current_state == POMODORO_STATE_WORKING ||
        s_current_state == POMODORO_STATE_BREAK) {
        s_pre_pause_state = s_current_state;
        transition_to(POMODORO_STATE_PAUSED);
        ESP_LOGI(TAG, "Paused (was %s)", state_name(s_pre_pause_state));
    }
    xSemaphoreGive(s_mutex);

    return ESP_OK;
}

esp_err_t pomodoro_resume(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_current_state == POMODORO_STATE_PAUSED) {
        transition_to(s_pre_pause_state);
        ESP_LOGI(TAG, "Resumed (back to %s)", state_name(s_pre_pause_state));
    }
    xSemaphoreGive(s_mutex);

    return ESP_OK;
}

esp_err_t pomodoro_stop(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_current_state != POMODORO_STATE_IDLE) {
        s_current_round = 0;
        s_remaining_sec = 0;
        transition_to(POMODORO_STATE_IDLE);
        ESP_LOGI(TAG, "Session stopped");
    }
    xSemaphoreGive(s_mutex);

    return ESP_OK;
}

pomodoro_state_t pomodoro_get_state(void)
{
    if (!s_initialized) {
        return POMODORO_STATE_IDLE;
    }

    pomodoro_state_t state;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    state = s_current_state;
    xSemaphoreGive(s_mutex);

    return state;
}

uint16_t pomodoro_get_remaining_sec(void)
{
    if (!s_initialized) {
        return 0;
    }

    uint16_t remaining;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    remaining = s_remaining_sec;
    xSemaphoreGive(s_mutex);

    return remaining;
}

uint8_t pomodoro_get_round(void)
{
    if (!s_initialized) {
        return 0;
    }

    uint8_t round;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    round = s_current_round;
    xSemaphoreGive(s_mutex);

    return round;
}
