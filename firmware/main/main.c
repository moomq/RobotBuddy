/*
 * RobotBuddy — Desktop AI Coding Assistant Robot
 * ================================================
 * ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM)
 *
 * Entry point: app_main()
 * Initializes board, starts FreeRTOS tasks
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "nvs_flash.h"

#include "bsp_board.h"

static const char *TAG = "RobotBuddy";

/* Firmware version — updated by /release command */
#define FIRMWARE_VERSION "0.1.0-dev"
#define HARDWARE_VERSION "V1.0"

/**
 * @brief Print system information on boot
 */
static void print_system_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  RobotBuddy — Desktop AI Coding Buddy");
    ESP_LOGI(TAG, "  Firmware: %s  Hardware: %s", FIRMWARE_VERSION, HARDWARE_VERSION);
    ESP_LOGI(TAG, "========================================");

    ESP_LOGI(TAG, "Chip: %s with %d CPU core(s), WiFi%s%s",
             CONFIG_IDF_TARGET,
             chip_info.cores,
             (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "" : "",
             (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "");

    uint32_t flash_size;
    esp_flash_get_size(NULL, &flash_size);
    ESP_LOGI(TAG, "Flash: %luMB %s", flash_size / (1024 * 1024),
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    ESP_LOGI(TAG, "PSRAM: %s", CONFIG_SPIRAM_MODE_OCT ? "8MB OCTAL" : "disabled");
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "========================================");
}

/**
 * @brief Application entry point
 *
 * Called by FreeRTOS after kernel initialization.
 * Initializes all subsystems and starts the main task loop.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Starting RobotBuddy firmware v%s", FIRMWARE_VERSION);

    /* Step 1: Print system information */
    print_system_info();

    /* Step 2: Initialize NVS (required for WiFi, OTA, calibration data) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_KEY_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");

    /* Step 3: Initialize board support package (GPIO, SPI, I2C, I2S buses) */
    ESP_ERROR_CHECK(bsp_board_init());
    ESP_LOGI(TAG, "BSP initialized");

    /* Step 4: Print free heap after initialization */
    ESP_LOGI(TAG, "Free heap after init: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Free PSRAM after init: %lu bytes",
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    /* Step 5: TODO — Initialize subsystem tasks
     *
     * The following tasks will be added as components are developed:
     *
     * - audio_capture_task  (Priority 8, Core 0) — I2S microphone capture
     * - audio_playback_task  (Priority 7, Core 0) — I2S speaker playback
     * - display_task         (Priority 6, Core 1) — SPI screen refresh (30FPS)
     * - emotion_task         (Priority 5, Core 1) — Emotion state machine
     * - cloud_task           (Priority 4, Core 0) — WiFi/HTTP/WS/MQTT
     * - motion_task          (Priority 3, Core 1) — Motor PID control (100Hz)
     * - sensor_task          (Priority 2, Core 1) — IMU/IR/TOF polling
     * - behavior_task        (Priority 1, Core 1) — Behavior orchestration
     * - monitor_task         (Priority 0, Core 1) — System health monitor
     */

    ESP_LOGI(TAG, "RobotBuddy is running! Waiting for subsystem initialization...");
    ESP_LOGI(TAG, "Use /firmware or /feature commands to add task modules.");

    /* Main loop — currently idle, will be replaced by behavior manager */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}