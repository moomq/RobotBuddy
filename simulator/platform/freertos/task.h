/*
 * FreeRTOS Task Compatibility — PC Simulator
 * =============================================
 * Provides minimal FreeRTOS task API compatibility.
 * The simulator uses a single-threaded main loop, so real
 * multitasking is not needed.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * FreeRTOS Type Definitions (PC compatibility)
 * ============================================================ */

typedef void *TaskHandle_t;
typedef int32_t BaseType_t;

#define pdTRUE      1
#define pdFALSE     0
#define pdMS_TO_TICKS(ms)  ((uint32_t)(ms))

/* ============================================================
 * Task Functions
 * ============================================================ */

/**
 * @brief Delay the current task (PC: SDL_Delay)
 *
 * On PC, this calls SDL_Delay which requires SDL2 to be
 * initialized. For non-SDL contexts, use usleep() instead.
 */
void vTaskDelay(uint32_t ticks);

/**
 * @brief Create a task pinned to a core (PC: no-op)
 *
 * In the simulator, tasks are not actually created —
 * the main loop drives all logic.
 */
static inline BaseType_t xTaskCreatePinnedToCore(
    void (*task_func)(void *),
    const char *name,
    uint32_t stack_depth,
    void *params,
    uint32_t priority,
    TaskHandle_t *handle,
    int core_id)
{
    (void)task_func;
    (void)name;
    (void)stack_depth;
    (void)params;
    (void)priority;
    (void)handle;
    (void)core_id;
    return pdTRUE;
}

/**
 * @brief Get the core ID (PC: always returns 0)
 */
static inline int xPortGetCoreID(void)
{
    return 0;
}

#ifdef __cplusplus
}
#endif