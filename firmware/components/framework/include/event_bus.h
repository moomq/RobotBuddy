/*
 * RobotBuddy — Event Bus
 * =======================
 * Centralized event distribution system for inter-task communication.
 * Tasks publish events and subscribe to event types they care about.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "robot_events.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Configuration
 * ============================================================ */

#define EVENT_BUS_MAX_SUBSCRIBERS   8     /**< Max subscribers per event type */
#define EVENT_BUS_MAX_EVENT_TYPES   64    /**< Max distinct event types */
#define EVENT_BUS_QUEUE_DEPTH       32    /**< Central event queue depth */
#define EVENT_BUS_TASK_STACK_SIZE   3072  /**< Event bus dispatch task stack (bytes) */
#define EVENT_BUS_TASK_PRIORITY     2     /**< Event bus dispatch task priority */

/* ============================================================
 * Types
 * ============================================================ */

/**
 * @brief Event handler callback type
 *
 * @param event Pointer to the event data (do NOT free, owned by bus)
 */
typedef void (*event_handler_t)(const robot_event_t *event);

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * @brief Initialize the event bus
 *
 * Creates the central event queue and dispatch task.
 * Must be called before any publish/subscribe operations.
 *
 * @return ESP_OK on success, ESP_ERR_NO_MEM if allocation fails
 */
esp_err_t event_bus_init(void);

/**
 * @brief Deinitialize the event bus
 *
 * Stops the dispatch task and frees all resources.
 *
 * @return ESP_OK on success
 */
esp_err_t event_bus_deinit(void);

/**
 * @brief Subscribe to an event type
 *
 * Register a callback to be invoked when the specified event is published.
 * The callback is invoked from the event bus dispatch task context (not ISR).
 *
 * @param event_id The event type to subscribe to
 * @param handler   Callback function
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if handler is NULL
 *         ESP_ERR_NO_MEM if max subscribers reached
 */
esp_err_t event_bus_subscribe(robot_event_id_t event_id, event_handler_t handler);

/**
 * @brief Unsubscribe from an event type
 *
 * @param event_id The event type to unsubscribe from
 * @param handler   The handler to remove
 * @return ESP_OK on success
 *         ESP_ERR_NOT_FOUND if handler not registered
 */
esp_err_t event_bus_unsubscribe(robot_event_id_t event_id, event_handler_t handler);

/**
 * @brief Publish an event (task context)
 *
 * Copies the event data into the central queue.
 * If payload is non-NULL, it is deep-copied; the caller retains ownership.
 * The dispatch task will invoke all subscribers for this event type.
 *
 * @param event Pointer to the event data (stack/local variable is fine)
 * @return ESP_OK on success
 *         ESP_ERR_NO_MEM if the queue is full
 */
esp_err_t event_bus_publish(const robot_event_t *event);

/**
 * @brief Publish an event from ISR context
 *
 * ISR-safe version of publish. Uses xQueueSendFromISR internally.
 *
 * @param event Pointer to the event data (must persist until dispatched)
 * @return ESP_OK on success
 *         ESP_ERR_NO_MEM if the queue is full
 */
esp_err_t event_bus_publish_from_isr(const robot_event_t *event);

/**
 * @brief Get the number of pending events in the queue
 *
 * @return Number of events waiting to be dispatched
 */
uint32_t event_bus_pending_count(void);

/**
 * @brief Check if the event bus is initialized
 *
 * @return true if initialized
 */
bool event_bus_is_initialized(void);

#ifdef __cplusplus
}
#endif