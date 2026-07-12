/*
 * ESP-IDF Logging Compatibility — PC Simulator
 * ================================================
 * Maps ESP_LOGx macros to standard fprintf on PC.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Log Level Macros
 * ============================================================ */

#define ESP_LOGE(tag, fmt, ...) \
    fprintf(stderr, "E [%s] " fmt "\n", tag, ##__VA_ARGS__)

#define ESP_LOGW(tag, fmt, ...) \
    fprintf(stderr, "W [%s] " fmt "\n", tag, ##__VA_ARGS__)

#define ESP_LOGI(tag, fmt, ...) \
    fprintf(stdout, "I [%s] " fmt "\n", tag, ##__VA_ARGS__)

#define ESP_LOGD(tag, fmt, ...) \
    fprintf(stdout, "D [%s] " fmt "\n", tag, ##__VA_ARGS__)

#define ESP_LOGV(tag, fmt, ...) \
    ((void)0)  /* Verbose: disabled on PC */

#ifdef __cplusplus
}
#endif