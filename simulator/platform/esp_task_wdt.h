/*
 * ESP-IDF Task Watchdog Compatibility — PC Simulator
 * ======================================================
 * No-op stubs for task watchdog API.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reset the task watchdog (PC: no-op)
 */
static inline esp_err_t esp_task_wdt_reset(void)
{
    return ESP_OK;
}

/**
 * @brief Add current task to watchdog (PC: no-op)
 */
static inline esp_err_t esp_task_wdt_add(void *unused)
{
    (void)unused;
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif