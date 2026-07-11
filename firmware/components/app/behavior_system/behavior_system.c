/*
 * RobotBuddy — Behavior System Implementation
 * ==============================================
 * State machine driven behavior orchestrator.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "behavior_system.h"
#include "emotion_engine.h"
#include "robot_events.h"
#include "event_bus.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "behavior";

/* ============================================================
 * Module State
 * ============================================================ */

static behavior_state_t s_current_state = BEHAVIOR_STATE_IDLE;
static TaskHandle_t s_task_handle = NULL;
static QueueHandle_t s_event_queue = NULL;
static bool s_initialized = false;
static volatile bool s_running = false;

/* Timers */
static int64_t s_last_activity_time = 0;    /* Last user interaction (ms) */
static int64_t s_state_enter_time = 0;       /* Time current state was entered (ms) */
static bool s_wifi_connected = false;

/* ============================================================
 * State names for logging
 * ============================================================ */

static const char *state_name(behavior_state_t state)
{
    switch (state) {
        case BEHAVIOR_STATE_IDLE:       return "IDLE";
        case BEHAVIOR_STATE_LISTENING:  return "LISTENING";
        case BEHAVIOR_STATE_THINKING:   return "THINKING";
        case BEHAVIOR_STATE_ANSWERING:  return "ANSWERING";
        case BEHAVIOR_STATE_HAPPY:      return "HAPPY";
        case BEHAVIOR_STATE_WARNING:    return "WARNING";
        case BEHAVIOR_STATE_ERROR:      return "ERROR";
        case BEHAVIOR_STATE_SLEEP:      return "SLEEP";
        default:                        return "UNKNOWN";
    }
}

/* ============================================================
 * Internal: State transition
 * ============================================================ */

static void transition_to(behavior_state_t new_state)
{
    if (new_state == s_current_state) {
        return;
    }

    behavior_state_t old_state = s_current_state;
    s_current_state = new_state;
    s_state_enter_time = esp_timer_get_time() / 1000;  /* ms */

    ESP_LOGI(TAG, "State: %s -> %s", state_name(old_state), state_name(new_state));

    /* Map behavior state to emotion */
    emotion_id_t emotion_map[] = {
        [BEHAVIOR_STATE_IDLE]       = EMOTION_IDLE,
        [BEHAVIOR_STATE_LISTENING]  = EMOTION_LISTENING,
        [BEHAVIOR_STATE_THINKING]   = EMOTION_THINKING,
        [BEHAVIOR_STATE_ANSWERING]  = EMOTION_ANSWERING,
        [BEHAVIOR_STATE_HAPPY]      = EMOTION_HAPPY,
        [BEHAVIOR_STATE_WARNING]    = EMOTION_WARNING,
        [BEHAVIOR_STATE_ERROR]      = EMOTION_ERROR,
        [BEHAVIOR_STATE_SLEEP]      = EMOTION_SLEEP,
    };

    if (new_state < sizeof(emotion_map) / sizeof(emotion_map[0])) {
        emotion_set_state(emotion_map[new_state]);
    }

    /* Publish state change event */
    robot_event_t event = {
        .id = EVENT_BEHAVIOR_STATE_CHANGE,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };
    event_bus_publish(&event);
}

/* ============================================================
 * Internal: Update activity timestamp
 * ============================================================ */

static void mark_activity(void)
{
    s_last_activity_time = esp_timer_get_time() / 1000;
}

/* ============================================================
 * Internal: Process incoming events
 * ============================================================ */

static void process_event(const robot_event_t *event)
{
    mark_activity();

    switch (event->id) {
        /* WiFi events */
        case EVENT_SYS_WIFI_CONNECTED:
            s_wifi_connected = true;
            if (s_current_state == BEHAVIOR_STATE_ERROR) {
                transition_to(BEHAVIOR_STATE_IDLE);
            }
            break;

        case EVENT_SYS_WIFI_DISCONNECTED:
            s_wifi_connected = false;
            break;

        /* Audio events */
        case EVENT_AUDIO_CAPTURE_START:
            transition_to(BEHAVIOR_STATE_LISTENING);
            break;

        case EVENT_AUDIO_CAPTURE_STOP:
            /* Will transition to THINKING when ASR result arrives */
            if (s_current_state == BEHAVIOR_STATE_LISTENING) {
                transition_to(BEHAVIOR_STATE_THINKING);
            }
            break;

        case EVENT_AUDIO_PLAY_START:
            transition_to(BEHAVIOR_STATE_ANSWERING);
            break;

        case EVENT_AUDIO_PLAY_DONE:
        case EVENT_AUDIO_PLAY_STOP:
            if (s_current_state == BEHAVIOR_STATE_ANSWERING) {
                transition_to(BEHAVIOR_STATE_IDLE);
            }
            break;

        case EVENT_AUDIO_PLAY_ERROR:
            if (s_current_state == BEHAVIOR_STATE_ANSWERING) {
                transition_to(BEHAVIOR_STATE_ERROR);
            }
            break;

        /* Cloud events */
        case EVENT_CLOUD_LLM_RESPONSE:
            /* LLM response received, will start playing */
            if (s_current_state == BEHAVIOR_STATE_THINKING) {
                /* Stay in THINKING until audio playback begins */
            }
            break;

        case EVENT_CLOUD_ERROR:
            if (s_current_state == BEHAVIOR_STATE_THINKING) {
                transition_to(BEHAVIOR_STATE_ERROR);
            }
            break;

        /* Sensor events */
        case EVENT_SENSOR_OBSTACLE:
            /* Obstacle detected — stop motion, show warning */
            transition_to(BEHAVIOR_STATE_WARNING);
            /* Also send emergency stop to motion — no payload needed,
             * emergency_stop() directly halts motors bypassing the queue. */
            {
                robot_event_t stop_event = {
                    .id = EVENT_MOTION_EMERGENCY_STOP,
                    .timestamp = 0,
                    .payload = NULL,
                    .payload_len = 0,
                };
                event_bus_publish(&stop_event);
            }
            break;

        case EVENT_SENSOR_EDGE:
            /* Edge detected — emergency stop */
            transition_to(BEHAVIOR_STATE_WARNING);
            {
                robot_event_t stop_event = {
                    .id = EVENT_MOTION_EMERGENCY_STOP,
                    .timestamp = 0,
                    .payload = NULL,
                    .payload_len = 0,
                };
                event_bus_publish(&stop_event);
            }
            break;

        case EVENT_SENSOR_FALL_DETECTED:
            transition_to(BEHAVIOR_STATE_ERROR);
            break;

        /* Battery events */
        case EVENT_SYS_LOW_BATTERY:
            transition_to(BEHAVIOR_STATE_WARNING);
            break;

        case EVENT_SYS_CRITICAL_BATTERY:
            /* Critical battery — go to sleep */
            transition_to(BEHAVIOR_STATE_SLEEP);
            break;

        /* Emotion state change (from external source) */
        case EVENT_EMOTION_STATE_CHANGE:
            /* External emotion override — behavior follows */
            if (event->payload != NULL) {
                emotion_event_t *emo = (emotion_event_t *)event->payload;
                if (emo->emotion_id == EMOTION_HAPPY) {
                    transition_to(BEHAVIOR_STATE_HAPPY);
                } else if (emo->emotion_id == EMOTION_ERROR) {
                    transition_to(BEHAVIOR_STATE_ERROR);
                }
            }
            break;

        default:
            /* Unhandled events — no state change */
            break;
    }
}

/* ============================================================
 * Internal: Periodic state checks (timeout transitions)
 * ============================================================ */

static void check_timeouts(void)
{
    int64_t now = esp_timer_get_time() / 1000;  /* ms */
    int64_t state_duration = now - s_state_enter_time;
    int64_t idle_duration = now - s_last_activity_time;

    /* Auto-recover from ERROR after timeout */
    if (s_current_state == BEHAVIOR_STATE_ERROR &&
        state_duration > BEHAVIOR_ERROR_RECOVERY_MS) {
        transition_to(BEHAVIOR_STATE_IDLE);
    }

    /* Auto-recover from HAPPY after timeout */
    if (s_current_state == BEHAVIOR_STATE_HAPPY &&
        state_duration > BEHAVIOR_HAPPY_RECOVERY_MS) {
        transition_to(BEHAVIOR_STATE_IDLE);
    }

    /* Auto-recover from WARNING after timeout */
    if (s_current_state == BEHAVIOR_STATE_WARNING &&
        state_duration > BEHAVIOR_WARNING_RECOVERY_MS) {
        transition_to(BEHAVIOR_STATE_IDLE);
    }

    /* Idle → SLEEP after timeout */
    if (s_current_state == BEHAVIOR_STATE_IDLE &&
        idle_duration > BEHAVIOR_IDLE_TIMEOUT_MS) {
        transition_to(BEHAVIOR_STATE_SLEEP);
    }

    /* SLEEP → IDLE on activity (handled in process_event via mark_activity) */
    if (s_current_state == BEHAVIOR_STATE_SLEEP &&
        idle_duration < BEHAVIOR_SLEEP_WAKE_MS) {
        /* Recent activity — wake up */
        transition_to(BEHAVIOR_STATE_IDLE);
    }
}

/* ============================================================
 * Behavior Task
 * ============================================================ */

static void behavior_task(void *arg)
{
    ESP_LOGI(TAG, "Behavior task started");

    robot_event_t event;
    s_last_activity_time = esp_timer_get_time() / 1000;
    s_state_enter_time = s_last_activity_time;

    while (s_running) {
        /* Check for events with timeout */
        if (xQueueReceive(s_event_queue, &event, pdMS_TO_TICKS(BEHAVIOR_TASK_PERIOD_MS)) == pdTRUE) {
            process_event(&event);
        }

        /* Periodic state checks */
        check_timeouts();

        /* Feed watchdog */
        esp_task_wdt_reset();
    }
}

/* ============================================================
 * Event bus callback
 * ============================================================ */

static void behavior_event_handler(const robot_event_t *event)
{
    /* Forward event from event bus to behavior task queue */
    if (s_event_queue != NULL) {
        BaseType_t higher_priority_woken = pdFALSE;
        BaseType_t ret = xQueueSend(s_event_queue, event, pdMS_TO_TICKS(10));
        if (ret != pdTRUE) {
            ESP_LOGW(TAG, "Event queue full, dropping event 0x%04X", event->id);
        }
    }
}

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t behavior_system_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Behavior system already initialized");
        return ESP_OK;
    }

    /* Create event queue */
    s_event_queue = xQueueCreate(BEHAVIOR_EVENT_QUEUE_DEPTH, sizeof(robot_event_t));
    if (s_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return ESP_ERR_NO_MEM;
    }

    /* Subscribe to relevant events */
    event_bus_subscribe(EVENT_SYS_WIFI_CONNECTED, behavior_event_handler);
    event_bus_subscribe(EVENT_SYS_WIFI_DISCONNECTED, behavior_event_handler);
    event_bus_subscribe(EVENT_SYS_LOW_BATTERY, behavior_event_handler);
    event_bus_subscribe(EVENT_SYS_CRITICAL_BATTERY, behavior_event_handler);
    event_bus_subscribe(EVENT_AUDIO_CAPTURE_START, behavior_event_handler);
    event_bus_subscribe(EVENT_AUDIO_CAPTURE_STOP, behavior_event_handler);
    event_bus_subscribe(EVENT_AUDIO_PLAY_START, behavior_event_handler);
    event_bus_subscribe(EVENT_AUDIO_PLAY_DONE, behavior_event_handler);
    event_bus_subscribe(EVENT_AUDIO_PLAY_STOP, behavior_event_handler);
    event_bus_subscribe(EVENT_AUDIO_PLAY_ERROR, behavior_event_handler);
    event_bus_subscribe(EVENT_CLOUD_LLM_RESPONSE, behavior_event_handler);
    event_bus_subscribe(EVENT_CLOUD_ERROR, behavior_event_handler);
    event_bus_subscribe(EVENT_SENSOR_OBSTACLE, behavior_event_handler);
    event_bus_subscribe(EVENT_SENSOR_EDGE, behavior_event_handler);
    event_bus_subscribe(EVENT_SENSOR_FALL_DETECTED, behavior_event_handler);
    event_bus_subscribe(EVENT_EMOTION_STATE_CHANGE, behavior_event_handler);

    /* Create behavior task */
    BaseType_t ret = xTaskCreatePinnedToCore(
        behavior_task,
        "behavior",
        BEHAVIOR_TASK_STACK_SIZE,
        NULL,
        BEHAVIOR_TASK_PRIORITY,
        &s_task_handle,
        BEHAVIOR_TASK_CORE_ID
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create behavior task");
        vQueueDelete(s_event_queue);
        s_event_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_current_state = BEHAVIOR_STATE_IDLE;
    s_running = true;
    s_initialized = true;

    ESP_LOGI(TAG, "Behavior system initialized");
    return ESP_OK;
}

esp_err_t behavior_system_deinit(void)
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

    s_initialized = false;
    ESP_LOGI(TAG, "Behavior system deinitialized");
    return ESP_OK;
}

behavior_state_t behavior_get_state(void)
{
    return s_current_state;
}

esp_err_t behavior_send_event(const robot_event_t *event)
{
    if (!s_initialized || s_event_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    BaseType_t ret = xQueueSend(s_event_queue, event, pdMS_TO_TICKS(10));
    if (ret != pdTRUE) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

bool behavior_system_is_initialized(void)
{
    return s_initialized;
}