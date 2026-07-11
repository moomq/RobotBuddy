/*
 * RobotBuddy — Battery Monitor Implementation
 * ==============================================
 * Integrates battery driver with event bus for power management.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "battery_monitor.h"
#include "battery.h"
#include "bsp_pinmap.h"
#include "event_bus.h"
#include "robot_events.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "bat_mon";

/* ============================================================
 * Module State
 * ============================================================ */

static bool s_initialized = false;
static float s_voltage = 3.7f;       /* Default safe value */
static uint8_t s_percentage = 50;
static bool s_is_charging = false;
static bool s_is_full = false;
static power_mode_t s_power_mode = POWER_MODE_ACTIVE;

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t battery_monitor_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Battery monitor already initialized");
        return ESP_OK;
    }

    /* Initialize battery driver */
    battery_config_t config = {
        .pin_adc = BSP_PIN_VBAT_ADC,
        .adc_unit = BSP_ADC_UNIT,
        .adc_channel = BSP_ADC_CHANNEL,
        .divider_ratio = BSP_BATTERY_DIVIDER_RATIO,
    };

    esp_err_t ret = battery_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Battery driver init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Initial reading */
    battery_data_t data;
    ret = battery_read(&data);
    if (ret == ESP_OK) {
        s_voltage = data.voltage;
        s_percentage = data.percentage;
        s_is_charging = data.is_charging;
        s_is_full = data.is_full;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Battery monitor initialized (%.2fV, %d%%, charging=%d)",
             s_voltage, s_percentage, s_is_charging);
    return ESP_OK;
}

esp_err_t battery_monitor_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    battery_deinit();
    s_initialized = false;
    return ESP_OK;
}

float battery_monitor_get_voltage(void)
{
    if (!s_initialized) return 0.0f;

    battery_data_t data;
    if (battery_read(&data) == ESP_OK) {
        s_voltage = data.voltage;
        s_percentage = data.percentage;
        s_is_charging = data.is_charging;
        s_is_full = data.is_full;
    }

    return s_voltage;
}

uint8_t battery_monitor_get_percentage(void)
{
    battery_monitor_get_voltage();  /* Updates percentage */
    return s_percentage;
}

bool battery_monitor_is_charging(void)
{
    if (!s_initialized) return false;

    battery_data_t data;
    if (battery_read(&data) == ESP_OK) {
        s_is_charging = data.is_charging;
        s_is_full = data.is_full;
    }

    return s_is_charging;
}

power_mode_t battery_monitor_get_power_mode(void)
{
    float voltage = battery_monitor_get_voltage();

    if (voltage <= BATTERY_CRITICAL_VOLTAGE && !s_is_charging) {
        s_power_mode = POWER_MODE_DEEP_SLEEP;
    } else if (voltage <= BATTERY_LOW_VOLTAGE && !s_is_charging) {
        s_power_mode = POWER_MODE_LIGHT_SLEEP;
    } else {
        s_power_mode = POWER_MODE_ACTIVE;
    }

    return s_power_mode;
}