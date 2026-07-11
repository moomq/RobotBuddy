/*
 * RobotBuddy — System Monitor
 * ============================
 * Runtime health monitoring: stack watermarks, heap usage, CPU usage.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Types
 * ============================================================ */

#define SYSMON_MAX_TASKS 10    /**< Max tracked tasks */

typedef struct {
    char name[16];                   /**< Task name */
    uint32_t stack_free;             /**< Free stack bytes (watermark) */
    uint32_t stack_total;            /**< Total stack bytes */
    float cpu_usage_percent;         /**< CPU usage percentage */
    uint32_t runtime_ms;            /**< Total runtime ms */
} sysmon_task_info_t;

typedef struct {
    uint32_t heap_free_dram;                 /**< Free DRAM bytes */
    uint32_t heap_free_psram;                /**< Free PSRAM bytes */
    uint32_t heap_largest_free_block;        /**< Largest free block */
    uint32_t heap_min_free_dram;             /**< Minimum free DRAM ever */
    sysmon_task_info_t tasks[SYSMON_MAX_TASKS]; /**< Per-task info */
    uint8_t task_count;                       /**< Number of tracked tasks */
    uint32_t uptime_seconds;                  /**< System uptime in seconds */
    uint32_t watchdog_reset_count;           /**< WDT reset count */
} sysmon_report_t;

/* ============================================================
 * Public API
 * ============================================================ */

/**
 * @brief Initialize system monitor
 *
 * Starts the monitor task (1Hz) that collects health data.
 *
 * @return ESP_OK on success
 */
esp_err_t sysmon_init(void);

/**
 * @brief Deinitialize system monitor
 * @return ESP_OK on success
 */
esp_err_t sysmon_deinit(void);

/**
 * @brief Register a task for monitoring
 *
 * @param name Human-readable task name (max 15 chars)
 * @param handle FreeRTOS task handle (NULL for current task)
 * @return ESP_OK on success, ESP_ERR_NO_MEM if max tasks reached
 */
esp_err_t sysmon_register_task(const char *name, TaskHandle_t handle);

/**
 * @brief Get current system health report
 *
 * @param[out] report Pointer to report structure to fill
 * @return ESP_OK on success
 */
esp_err_t sysmon_get_report(sysmon_report_t *report);

/**
 * @brief Print formatted report to log output
 *
 * @return ESP_OK on success
 */
esp_err_t sysmon_print_report(void);

#ifdef __cplusplus
}
#endif