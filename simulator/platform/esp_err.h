/*
 * ESP-IDF Error Code Compatibility — PC Simulator
 * =================================================
 * Provides esp_err_t and error code definitions for
 * compiling shared firmware code on PC without ESP-IDF.
 *
 * Error code values are aligned with ESP-IDF v5.x definitions
 * to ensure behavioral consistency across platforms.
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
 * Error Code Type and Constants
 * ============================================================
 * Values match ESP-IDF v5.x definitions exactly.
 * Reference: esp-idf/components/esp_common/include/esp_err.h
 */

typedef int32_t esp_err_t;

#define ESP_OK                      0
#define ESP_FAIL                    (-1)

/* ESP-IDF v5.x error code values */
#define ESP_ERR_INVALID_ARG         0x102
#define ESP_ERR_INVALID_STATE       0x103
#define ESP_ERR_INVALID_SIZE        0x104
#define ESP_ERR_NOT_FOUND           0x105
#define ESP_ERR_NOT_SUPPORTED       0x106
#define ESP_ERR_INVALID_RESPONSE    0x107
#define ESP_ERR_INVALID_CRC         0x108
#define ESP_ERR_INVALID_VERSION     0x109
#define ESP_ERR_INVALID_MAC         0x10A

#define ESP_ERR_NO_MEM              0x101

/* Timeout errors */
#define ESP_ERR_TIMEOUT             0x10D
#define ESP_ERR_NOT_FINISHED        0x10E

/* ============================================================
 * Error Name Lookup
 * ============================================================ */

static inline const char *esp_err_to_name(esp_err_t err)
{
    switch (err) {
        case ESP_OK:                  return "OK";
        case ESP_FAIL:               return "FAIL";
        case ESP_ERR_NO_MEM:          return "NO_MEM";
        case ESP_ERR_INVALID_ARG:     return "INVALID_ARG";
        case ESP_ERR_INVALID_STATE:   return "INVALID_STATE";
        case ESP_ERR_INVALID_SIZE:    return "INVALID_SIZE";
        case ESP_ERR_NOT_FOUND:       return "NOT_FOUND";
        case ESP_ERR_NOT_SUPPORTED:   return "NOT_SUPPORTED";
        case ESP_ERR_TIMEOUT:         return "TIMEOUT";
        case ESP_ERR_INVALID_RESPONSE: return "INVALID_RESPONSE";
        default:                      return "UNKNOWN";
    }
}

#ifdef __cplusplus
}
#endif