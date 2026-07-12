/*
 * ESP-IDF Timer Compatibility — PC Simulator
 * =============================================
 * Maps esp_timer_get_time() to clock_gettime on PC.
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
 * Timer Functions
 * ============================================================ */

/**
 * @brief Get current time in microseconds (PC: clock_gettime)
 * @return Time in microseconds since some arbitrary point
 */
int64_t esp_timer_get_time(void);

#ifdef __cplusplus
}
#endif