/*
 * RobotBuddy — System Monitor Implementation
 * ============================================
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "sysmon.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "sysmon";

#define SYSMON_TASK_STACK_SIZE   2048
#define SYSMON_TASK_PRIORITY     0   /* Lowest priority */
#define SYSMON_TASK_CORE_ID      1   /* Core 1 */
#define SYSMON_REPORT_INTERVAL_S 5   /* Report every 5 seconds */

/* ============================================================
 * Tracked task entry
 * ============================================================ */

typedef struct {
    char name[16];
    TaskHandle_t handle;
} sysmon_tracked_task_t;

/* ============================================================
 * Module state
 * ============================================================ */

static sysmon_tracked_task_t s_tracked_tasks[SYSMON_MAX_TASKS];
static uint8_t s_tracked_count = 0;
static TaskHandle_t s_sysmon_task = NULL;
static SemaphoreHandle_t s_mutex = NULL;
static bool s_initialized = false;
static volatile bool s_running = false;
static uint32_t s_uptime_seconds = 0;

/* ============================================================
 * Monitor task
 * ============================================================ */

static void sysmon_task_fn(void *arg)
{
    ESP_LOGI(TAG, "System monitor task started");

    while (s_running) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        s_uptime_seconds++;

        /* Print report every SYSMON_REPORT_INTERVAL_S seconds */
        if (s_uptime_seconds % SYSMON_REPORT_INTERVAL_S == 0) {
            sysmon_print_report();
        }
    }
}

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t sysmon_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    memset(s_tracked_tasks, 0, sizeof(s_tracked_tasks));
    s_tracked_count = 0;
    s_uptime_seconds = 0;

    BaseType_t ret = xTaskCreatePinnedToCore(
        sysmon_task_fn,
        "sysmon",
        SYSMON_TASK_STACK_SIZE,
        NULL,
        SYSMON_TASK_PRIORITY,
        &s_sysmon_task,
        SYSMON_TASK_CORE_ID
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sysmon task");
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_running = true;
    s_initialized = true;
    ESP_LOGI(TAG, "System monitor initialized");
    return ESP_OK;
}

esp_err_t sysmon_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    s_running = false;
    if (s_sysmon_task != NULL) {
        vTaskDelay(pdMS_TO_TICKS(200));
        vTaskDelete(s_sysmon_task);
        s_sysmon_task = NULL;
    }

    if (s_mutex != NULL) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }

    s_initialized = false;
    return ESP_OK;
}

esp_err_t sysmon_register_task(const char *name, TaskHandle_t handle)
{
    if (name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (s_tracked_count >= SYSMON_MAX_TASKS) {
        xSemaphoreGive(s_mutex);
        ESP_LOGE(TAG, "Max tracked tasks reached (%d)", SYSMON_MAX_TASKS);
        return ESP_ERR_NO_MEM;
    }

    sysmon_tracked_task_t *entry = &s_tracked_tasks[s_tracked_count++];
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->handle = (handle != NULL) ? handle : xTaskGetCurrentTaskHandle();

    xSemaphoreGive(s_mutex);
    ESP_LOGD(TAG, "Registered task '%s'", name);
    return ESP_OK;
}

esp_err_t sysmon_get_report(sysmon_report_t *report)
{
    if (report == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(report, 0, sizeof(sysmon_report_t));

    /* Heap info */
    report->heap_free_dram = esp_get_free_heap_size();
    report->heap_free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    report->heap_largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_32BIT | MALLOC_CAP_8BIT);
    report->heap_min_free_dram = esp_get_minimum_free_heap_size();

    /* Task info */
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    report->task_count = s_tracked_count;

    for (uint8_t i = 0; i < s_tracked_count; i++) {
        strncpy(report->tasks[i].name, s_tracked_tasks[i].name, sizeof(report->tasks[i].name) - 1);
        if (s_tracked_tasks[i].handle != NULL) {
            report->tasks[i].stack_free = uxTaskGetStackHighWaterMark(s_tracked_tasks[i].handle) * sizeof(StackType_t);
        }
    }

    xSemaphoreGive(s_mutex);

    report->uptime_seconds = s_uptime_seconds;

    return ESP_OK;
}

esp_err_t sysmon_print_report(void)
{
    sysmon_report_t report;
    sysmon_get_report(&report);

    ESP_LOGI(TAG, "=== System Health Report ===");
    ESP_LOGI(TAG, "  Uptime: %lu seconds", report.uptime_seconds);
    ESP_LOGI(TAG, "  Heap free: %lu bytes DRAM, %lu bytes PSRAM",
             report.heap_free_dram, report.heap_free_psram);
    ESP_LOGI(TAG, "  Heap min free: %lu bytes DRAM", report.heap_min_free_dram);
    ESP_LOGI(TAG, "  Largest free block: %lu bytes", report.heap_largest_free_block);

    for (uint8_t i = 0; i < report.task_count; i++) {
        ESP_LOGI(TAG, "  Task %-14s: stack free %lu bytes",
                 report.tasks[i].name, report.tasks[i].stack_free);
    }

    ESP_LOGI(TAG, "============================");
    return ESP_OK;
}