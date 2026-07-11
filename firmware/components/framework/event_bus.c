/*
 * RobotBuddy — Event Bus Implementation
 * =======================================
 * Centralized event distribution using a FreeRTOS queue and dispatch task.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "event_bus.h"
#include "robot_events.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "event_bus";

/* ============================================================
 * Subscriber list for one event type
 * ============================================================ */

typedef struct {
    robot_event_id_t event_id;              /**< The event type */
    event_handler_t handlers[EVENT_BUS_MAX_SUBSCRIBERS]; /**< Registered handlers */
    uint8_t handler_count;                   /**< Number of registered handlers */
} subscriber_entry_t;

/* ============================================================
 * Module state
 * ============================================================ */

static QueueHandle_t s_event_queue = NULL;
static TaskHandle_t s_dispatch_task = NULL;
static subscriber_entry_t s_subscribers[EVENT_BUS_MAX_EVENT_TYPES];
static uint8_t s_subscriber_count = 0;
static SemaphoreHandle_t s_mutex = NULL;
static bool s_initialized = false;
static volatile bool s_running = false;  /**< Graceful shutdown flag */

/* ============================================================
 * Dispatch task
 * ============================================================ */

static void event_bus_dispatch_task(void *arg)
{
    robot_event_t event;

    ESP_LOGI(TAG, "Event bus dispatch task started");

    while (s_running) {
        if (xQueueReceive(s_event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            /* Find subscribers for this event type.
             * Snapshot the handler list under mutex protection so that
             * handlers may safely call subscribe/unsubscribe without
             * corrupting the iteration. */
            event_handler_t handlers[EVENT_BUS_MAX_SUBSCRIBERS];
            uint8_t handler_count = 0;

            xSemaphoreTake(s_mutex, portMAX_DELAY);

            for (uint8_t i = 0; i < s_subscriber_count; i++) {
                if (s_subscribers[i].event_id == event.id) {
                    handler_count = s_subscribers[i].handler_count;
                    if (handler_count > EVENT_BUS_MAX_SUBSCRIBERS) {
                        handler_count = EVENT_BUS_MAX_SUBSCRIBERS;
                    }
                    memcpy(handlers, s_subscribers[i].handlers,
                           handler_count * sizeof(event_handler_t));
                    break;
                }
            }

            xSemaphoreGive(s_mutex);

            /* Invoke all handlers from the snapshot (no mutex held).
             * Handlers may safely call subscribe/unsubscribe/publish. */
            for (uint8_t h = 0; h < handler_count; h++) {
                if (handlers[h] != NULL) {
                    handlers[h](&event);
                }
            }

            /* Free payload if allocated */
            if (event.payload != NULL && event.payload_len > 0) {
                free(event.payload);
            }
        }
    }
}

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t event_bus_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Event bus already initialized");
        return ESP_OK;
    }

    /* Create mutex for subscriber list protection */
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Create event queue */
    s_event_queue = xQueueCreate(EVENT_BUS_QUEUE_DEPTH, sizeof(robot_event_t));
    if (s_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    /* Initialize subscriber list */
    memset(s_subscribers, 0, sizeof(s_subscribers));
    s_subscriber_count = 0;

    /* Create dispatch task */
    BaseType_t ret = xTaskCreatePinnedToCore(
        event_bus_dispatch_task,
        "event_bus",
        EVENT_BUS_TASK_STACK_SIZE,
        NULL,
        EVENT_BUS_TASK_PRIORITY,
        &s_dispatch_task,
        1  /* Core 1 */
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create dispatch task");
        vQueueDelete(s_event_queue);
        s_event_queue = NULL;
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_running = true;
    s_initialized = true;
    ESP_LOGI(TAG, "Event bus initialized (queue depth=%d)", EVENT_BUS_QUEUE_DEPTH);

    return ESP_OK;
}

esp_err_t event_bus_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    /* Mark as uninitialized first to reject new publish/unsubscribe calls */
    s_initialized = false;

    /* Signal dispatch task to exit gracefully, then wait briefly */
    s_running = false;
    if (s_dispatch_task != NULL) {
        vTaskDelay(pdMS_TO_TICKS(200));  /* Give task time to exit loop */
        vTaskDelete(s_dispatch_task);
        s_dispatch_task = NULL;
    }

    /* Drain queue and free any pending payloads before deleting */
    if (s_event_queue != NULL) {
        robot_event_t pending;
        while (xQueueReceive(s_event_queue, &pending, 0) == pdTRUE) {
            if (pending.payload != NULL && pending.payload_len > 0) {
                free(pending.payload);
            }
        }
        vQueueDelete(s_event_queue);
        s_event_queue = NULL;
    }

    /* Clear subscriber list */
    s_subscriber_count = 0;
    memset(s_subscribers, 0, sizeof(s_subscribers));

    /* Delete mutex */
    if (s_mutex != NULL) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }

    ESP_LOGI(TAG, "Event bus deinitialized");
    return ESP_OK;
}

esp_err_t event_bus_subscribe(robot_event_id_t event_id, event_handler_t handler)
{
    if (handler == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        ESP_LOGE(TAG, "Event bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* Check if event type already has a subscriber entry */
    for (uint8_t i = 0; i < s_subscriber_count; i++) {
        if (s_subscribers[i].event_id == event_id) {
            if (s_subscribers[i].handler_count >= EVENT_BUS_MAX_SUBSCRIBERS) {
                xSemaphoreGive(s_mutex);
                ESP_LOGE(TAG, "Max subscribers reached for event 0x%04X", event_id);
                return ESP_ERR_NO_MEM;
            }
            s_subscribers[i].handlers[s_subscribers[i].handler_count++] = handler;
            xSemaphoreGive(s_mutex);
            ESP_LOGD(TAG, "Subscribed handler %p to event 0x%04X", handler, event_id);
            return ESP_OK;
        }
    }

    /* New event type — create subscriber entry */
    if (s_subscriber_count >= EVENT_BUS_MAX_EVENT_TYPES) {
        xSemaphoreGive(s_mutex);
        ESP_LOGE(TAG, "Max event types reached");
        return ESP_ERR_NO_MEM;
    }

    subscriber_entry_t *entry = &s_subscribers[s_subscriber_count++];
    entry->event_id = event_id;
    entry->handlers[0] = handler;
    entry->handler_count = 1;

    xSemaphoreGive(s_mutex);
    ESP_LOGD(TAG, "Created subscriber entry for event 0x%04X", event_id);
    return ESP_OK;
}

esp_err_t event_bus_unsubscribe(robot_event_id_t event_id, event_handler_t handler)
{
    if (handler == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || s_mutex == NULL) {
        ESP_LOGE(TAG, "Event bus not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    for (uint8_t i = 0; i < s_subscriber_count; i++) {
        if (s_subscribers[i].event_id == event_id) {
            for (uint8_t h = 0; h < s_subscribers[i].handler_count; h++) {
                if (s_subscribers[i].handlers[h] == handler) {
                    /* Shift remaining handlers */
                    for (uint8_t k = h; k < s_subscribers[i].handler_count - 1; k++) {
                        s_subscribers[i].handlers[k] = s_subscribers[i].handlers[k + 1];
                    }
                    s_subscribers[i].handler_count--;
                    xSemaphoreGive(s_mutex);
                    return ESP_OK;
                }
            }
            xSemaphoreGive(s_mutex);
            return ESP_ERR_NOT_FOUND;
        }
    }

    xSemaphoreGive(s_mutex);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t event_bus_publish(const robot_event_t *event)
{
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || s_event_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    robot_event_t copy = *event;

    /* Deep-copy payload if present */
    if (event->payload != NULL && event->payload_len > 0) {
        void *payload_copy = malloc(event->payload_len);
        if (payload_copy == NULL) {
            ESP_LOGE(TAG, "Failed to allocate payload copy (%zu bytes)", event->payload_len);
            return ESP_ERR_NO_MEM;
        }
        memcpy(payload_copy, event->payload, event->payload_len);
        copy.payload = payload_copy;
    }

    /* Add timestamp if not set */
    if (copy.timestamp == 0) {
        copy.timestamp = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    }

    BaseType_t ret = xQueueSend(s_event_queue, &copy, pdMS_TO_TICKS(10));
    if (ret != pdTRUE) {
        ESP_LOGW(TAG, "Event queue full, dropping event 0x%04X", event->id);
        if (copy.payload != NULL) {
            free(copy.payload);
        }
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t event_bus_publish_from_isr(const robot_event_t *event)
{
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || s_event_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* ISR version: do NOT deep-copy payload (ISR context, no malloc).
     * Caller must ensure payload persists or is NULL. */
    robot_event_t copy = *event;
    copy.timestamp = (uint32_t)(xTaskGetTickCountFromISR() * portTICK_PERIOD_MS);

    BaseType_t higher_priority_task_woken = pdFALSE;
    BaseType_t ret = xQueueSendFromISR(s_event_queue, &copy, &higher_priority_task_woken);

    if (ret != pdTRUE) {
        return ESP_ERR_NO_MEM;
    }

    if (higher_priority_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }

    return ESP_OK;
}

uint32_t event_bus_pending_count(void)
{
    if (s_event_queue == NULL) {
        return 0;
    }
    return (uint32_t)uxQueueMessagesWaiting(s_event_queue);
}

bool event_bus_is_initialized(void)
{
    return s_initialized;
}