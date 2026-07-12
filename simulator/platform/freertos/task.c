/*
 * FreeRTOS Task Implementation — PC Simulator
 * ===============================================
 * Uses platform-native sleep for task delay simulation.
 *
 * Note: This implementation does NOT depend on SDL2,
 * so it can be reused in headless/unit-test contexts.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "freertos/task.h"
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

void vTaskDelay(uint32_t ticks)
{
    /* On PC, 1 tick = 1ms (simplified from FreeRTOS tick model) */
    uint32_t ms = ticks;
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}