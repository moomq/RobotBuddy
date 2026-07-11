/*
 * RobotBuddy — Sensor Manager Implementation
 * ==============================================
 * Periodically polls MPU6050, IR sensors, and battery voltage.
 * Publishes events via event bus.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "sensor_manager.h"
#include "mpu6050.h"
#include "ir_sensor.h"
#include "battery.h"
#include "bsp_pinmap.h"
#include "event_bus.h"

#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "sensor_mgr";

/* ============================================================
 * Module State
 * ============================================================ */

static TaskHandle_t s_task_handle = NULL;
static bool s_initialized = false;

/* Latest sensor data (protected by mutex) */
static sensor_data_t s_latest_data;
static SemaphoreHandle_t s_data_mutex = NULL;
static volatile bool s_data_ready = false;

/* Sensor handles */
static bool s_imu_ok = false;
static bool s_ir_ok = false;
static bool s_battery_ok = false;

/* Callbacks */
#define MAX_SENSOR_CALLBACKS 4
static sensor_event_cb_t s_callbacks[MAX_SENSOR_CALLBACKS];
static uint8_t s_callback_count = 0;

/* ============================================================
 * Internal: Notify callbacks
 * ============================================================ */

static void notify_callbacks(robot_event_id_t event, const sensor_data_t *data)
{
    for (uint8_t i = 0; i < s_callback_count; i++) {
        if (s_callbacks[i] != NULL) {
            s_callbacks[i](event, data);
        }
    }
}

/* ============================================================
 * Sensor Task
 * ============================================================ */

static void sensor_task(void *arg)
{
    ESP_LOGI(TAG, "Sensor task started (20Hz)");

    uint32_t cycle = 0;

    while (1) {
        /* Build sensor data locally, then copy under mutex at end of cycle.
         * This avoids holding the mutex during slow I2C/ADC reads. */
        sensor_data_t new_data;
        xSemaphoreTake(s_data_mutex, portMAX_DELAY);
        new_data = s_latest_data;
        xSemaphoreGive(s_data_mutex);

        /* Read IMU */
        if (s_imu_ok) {
            mpu6050_data_t imu_data;
            esp_err_t ret = mpu6050_read(&imu_data);
            if (ret == ESP_OK) {
                new_data.accel_x = imu_data.accel_x;
                new_data.accel_y = imu_data.accel_y;
                new_data.accel_z = imu_data.accel_z;
                new_data.gyro_x = imu_data.gyro_x;
                new_data.gyro_y = imu_data.gyro_y;
                new_data.gyro_z = imu_data.gyro_z;
                new_data.temperature = imu_data.temperature;
            } else {
                ESP_LOGW(TAG, "MPU6050 read failed: %s", esp_err_to_name(ret));
            }
        }

        /* Read IR sensors */
        if (s_ir_ok) {
            ir_sensor_data_t ir_data = ir_sensor_read();
            bool prev_obstacle_left = new_data.obstacle_left;
            bool prev_obstacle_right = new_data.obstacle_right;
            bool prev_edge_left = new_data.edge_left;
            bool prev_edge_right = new_data.edge_right;

            new_data.obstacle_left = ir_data.obstacle_left;
            new_data.obstacle_right = ir_data.obstacle_right;
            new_data.edge_left = ir_data.edge_left;
            new_data.edge_right = ir_data.edge_right;

            /* Publish events on edge detection (rising edge) */
            if (!prev_edge_left && ir_data.edge_left) {
                robot_event_t event = { .id = EVENT_SENSOR_EDGE, .payload = NULL, .payload_len = 0 };
                event_bus_publish(&event);
            }
            if (!prev_edge_right && ir_data.edge_right) {
                robot_event_t event = { .id = EVENT_SENSOR_EDGE, .payload = NULL, .payload_len = 0 };
                event_bus_publish(&event);
            }

            /* Publish obstacle events */
            if ((!prev_obstacle_left && ir_data.obstacle_left) ||
                (!prev_obstacle_right && ir_data.obstacle_right)) {
                robot_event_t event = { .id = EVENT_SENSOR_OBSTACLE, .payload = NULL, .payload_len = 0 };
                event_bus_publish(&event);
            }
        }

        /* Read battery (every BATTERY_POLL_DIVIDER cycles = 1 Hz) */
        if (s_battery_ok && (cycle % BATTERY_POLL_DIVIDER == 0)) {
            battery_data_t bat_data;
            esp_err_t ret = battery_read(&bat_data);
            if (ret == ESP_OK) {
                new_data.battery_voltage = bat_data.voltage;
                new_data.battery_percent = bat_data.percentage;

                /* Publish battery events */
                if (bat_data.percentage <= 10) {
                    robot_event_t event = { .id = EVENT_SYS_CRITICAL_BATTERY, .payload = NULL, .payload_len = 0 };
                    event_bus_publish(&event);
                } else if (bat_data.percentage <= 20) {
                    robot_event_t event = { .id = EVENT_SYS_LOW_BATTERY, .payload = NULL, .payload_len = 0 };
                    event_bus_publish(&event);
                }
            } else {
                ESP_LOGW(TAG, "Battery read failed: %s", esp_err_to_name(ret));
            }
        }

        /* Atomically publish updated data */
        xSemaphoreTake(s_data_mutex, portMAX_DELAY);
        s_latest_data = new_data;
        xSemaphoreGive(s_data_mutex);

        /* Publish IMU data event (use deep copy via event_bus) */
        if (s_imu_ok) {
            sensor_data_t data_copy;
            xSemaphoreTake(s_data_mutex, portMAX_DELAY);
            data_copy = s_latest_data;
            xSemaphoreGive(s_data_mutex);

            robot_event_t event = {
                .id = EVENT_SENSOR_IMU_DATA,
                .timestamp = 0,
                .payload = &data_copy,
                .payload_len = sizeof(sensor_data_t),
            };
            event_bus_publish(&event);
        }

        /* Notify registered callbacks with latest data (under mutex) */
        xSemaphoreTake(s_data_mutex, portMAX_DELAY);
        notify_callbacks(EVENT_SENSOR_IMU_DATA, &s_latest_data);
        xSemaphoreGive(s_data_mutex);

        s_data_ready = true;
        cycle++;

        /* Feed watchdog and delay */
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(SENSOR_POLL_INTERVAL_MS));
    }
}

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t sensor_manager_init(const sensor_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Sensor manager already initialized");
        return ESP_OK;
    }

    /* Initialize MPU6050 */
    mpu6050_config_t imu_config = {
        .i2c_port = BSP_I2C_NUM,
        .addr = BSP_I2C_ADDR_MPU6050,
        .accel_range = MPU6050_ACCEL_RANGE_2G,
        .gyro_range = MPU6050_GYRO_RANGE_250,
    };

    if (config != NULL) {
        imu_config.i2c_port = config->i2c_port;
        imu_config.addr = config->mpu6050_addr;
    }

    if (mpu6050_init(&imu_config) == ESP_OK) {
        s_imu_ok = true;
        ESP_LOGI(TAG, "MPU6050 initialized");
    } else {
        ESP_LOGW(TAG, "MPU6050 not found, continuing without IMU");
        s_imu_ok = false;
    }

    /* Initialize IR sensors */
    ir_sensor_config_t ir_config = {
        .pin_obstacle_left = BSP_PIN_IR_OBSTACLE_L,
        .pin_obstacle_right = BSP_PIN_IR_OBSTACLE_R,
        .pin_edge_left = BSP_PIN_IR_EDGE_L,
        .pin_edge_right = BSP_PIN_IR_EDGE_R,
    };

    if (config != NULL) {
        ir_config.pin_obstacle_left = config->pin_obstacle_left;
        ir_config.pin_obstacle_right = config->pin_obstacle_right;
        ir_config.pin_edge_left = config->pin_edge_left;
        ir_config.pin_edge_right = config->pin_edge_right;
    }

    if (ir_sensor_init(&ir_config) == ESP_OK) {
        s_ir_ok = true;
        ESP_LOGI(TAG, "IR sensors initialized");
    } else {
        ESP_LOGW(TAG, "IR sensors init failed");
        s_ir_ok = false;
    }

    /* Initialize battery monitor */
    battery_config_t bat_config = {
        .pin_adc = BSP_PIN_VBAT_ADC,
        .adc_unit = BSP_ADC_UNIT,
        .adc_channel = BSP_ADC_CHANNEL,
        .divider_ratio = BSP_BATTERY_DIVIDER_RATIO,
    };

    if (config != NULL) {
        bat_config.pin_adc = config->adc_pin;
        bat_config.divider_ratio = config->battery_divider_ratio;
    }

    if (battery_init(&bat_config) == ESP_OK) {
        s_battery_ok = true;
        ESP_LOGI(TAG, "Battery monitor initialized");
    } else {
        ESP_LOGW(TAG, "Battery monitor init failed");
        s_battery_ok = false;
    }

    /* Create data mutex for thread-safe access to s_latest_data */
    s_data_mutex = xSemaphoreCreateMutex();
    if (s_data_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create data mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Initialize sensor data */
    memset(&s_latest_data, 0, sizeof(s_latest_data));
    s_latest_data.battery_voltage = 3.7f;  /* Default safe value */
    s_latest_data.battery_percent = 50;

    /* Create sensor task */
    BaseType_t ret = xTaskCreatePinnedToCore(
        sensor_task,
        "sensor",
        SENSOR_TASK_STACK_SIZE,
        NULL,
        SENSOR_TASK_PRIORITY,
        &s_task_handle,
        SENSOR_TASK_CORE_ID
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sensor task");
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Sensor manager initialized (IMU=%d, IR=%d, BAT=%d)", s_imu_ok, s_ir_ok, s_battery_ok);
    return ESP_OK;
}

esp_err_t sensor_manager_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    if (s_task_handle != NULL) {
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
    }

    if (s_imu_ok) mpu6050_deinit();
    if (s_ir_ok) ir_sensor_deinit();
    if (s_battery_ok) battery_deinit();

    if (s_data_mutex != NULL) {
        vSemaphoreDelete(s_data_mutex);
        s_data_mutex = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "Sensor manager deinitialized");
    return ESP_OK;
}

const sensor_data_t *sensor_get_data(void)
{
    static sensor_data_t s_snapshot;

    if (!s_data_ready || s_data_mutex == NULL) {
        return NULL;
    }

    xSemaphoreTake(s_data_mutex, portMAX_DELAY);
    s_snapshot = s_latest_data;
    xSemaphoreGive(s_data_mutex);

    return &s_snapshot;
}

esp_err_t sensor_register_callback(sensor_event_cb_t cb)
{
    if (cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_callback_count >= MAX_SENSOR_CALLBACKS) {
        return ESP_ERR_NO_MEM;
    }
    s_callbacks[s_callback_count++] = cb;
    return ESP_OK;
}