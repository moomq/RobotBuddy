/*
 * RobotBuddy — Motion Manager Implementation
 * ==============================================
 * Differential drive motor control with command queue.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "motion_manager.h"
#include "drv8833.h"
#include "bsp_pinmap.h"
#include "robot_events.h"
#include "event_bus.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "motion_mgr";

/* ============================================================
 * Internal command structure
 * ============================================================ */

typedef struct {
    motion_cmd_t cmd;
    int16_t speed;
    int16_t angle;
    uint16_t duration_ms;
} motion_command_t;

/* ============================================================
 * Module State
 * ============================================================ */

static TaskHandle_t s_task_handle = NULL;
static QueueHandle_t s_cmd_queue = NULL;
static bool s_initialized = false;
static bool s_is_moving = false;
static uint16_t s_max_speed = MOTION_MAX_SPEED;
static uint16_t s_default_speed = MOTION_DEFAULT_SPEED;

/* Current motor speeds for differential drive */
static int16_t s_left_speed = 0;
static int16_t s_right_speed = 0;

/* Non-blocking motion duration timer */
static int64_t s_motion_end_time_ms = 0;  /* 0 = no duration active */

/* ============================================================
 * Internal: Apply motor speeds
 * ============================================================ */

static void apply_speeds(int16_t left, int16_t right)
{
    /* Clamp to max speed */
    if (left > (int16_t)s_max_speed) left = (int16_t)s_max_speed;
    if (left < -(int16_t)s_max_speed) left = -(int16_t)s_max_speed;
    if (right > (int16_t)s_max_speed) right = (int16_t)s_max_speed;
    if (right < -(int16_t)s_max_speed) right = -(int16_t)s_max_speed;

    drv8833_set_speed(DRV8833_MOTOR_LEFT, left);
    drv8833_set_speed(DRV8833_MOTOR_RIGHT, right);

    s_left_speed = left;
    s_right_speed = right;
    s_is_moving = (left != 0 || right != 0);
}

/* ============================================================
 * Internal: Execute motion command
 * ============================================================ */

static void execute_command(const motion_command_t *cmd)
{
    int16_t speed = cmd->speed;
    if (speed == 0) {
        speed = (int16_t)s_default_speed;
    }

    ESP_LOGI(TAG, "Execute: cmd=%d speed=%d angle=%d dur=%d",
             cmd->cmd, speed, cmd->angle, cmd->duration_ms);

    /* Clear any pending duration timer from previous command */
    s_motion_end_time_ms = 0;

    switch (cmd->cmd) {
        case MOTION_CMD_STOP:
            apply_speeds(0, 0);
            break;

        case MOTION_CMD_FORWARD:
            apply_speeds(speed, speed);
            break;

        case MOTION_CMD_BACKWARD:
            apply_speeds(-speed, -speed);
            break;

        case MOTION_CMD_TURN_LEFT:
            /* Left motor slower, right motor faster */
            apply_speeds(speed / 2, speed);
            break;

        case MOTION_CMD_TURN_RIGHT:
            /* Left motor faster, right motor slower */
            apply_speeds(speed, speed / 2);
            break;

        case MOTION_CMD_ROTATE:
            /* Spin in place */
            if (cmd->angle > 0) {
                /* Clockwise: left forward, right backward */
                apply_speeds(speed, -speed);
            } else {
                /* Counter-clockwise: left backward, right forward */
                apply_speeds(-speed, speed);
            }
            break;

        default:
            ESP_LOGW(TAG, "Unknown motion command: %d", cmd->cmd);
            break;
    }

    /* Set non-blocking duration timer if duration_ms > 0 */
    if (cmd->duration_ms > 0) {
        int64_t now_ms = esp_timer_get_time() / 1000;
        s_motion_end_time_ms = now_ms + cmd->duration_ms;
    }
}

/* ============================================================
 * Motion Task
 * ============================================================ */

static void motion_task(void *arg)
{
    ESP_LOGI(TAG, "Motion task started (100Hz)");

    motion_command_t cmd;

    while (1) {
        /* Wait for command with timeout (control loop) */
        if (xQueueReceive(s_cmd_queue, &cmd, pdMS_TO_TICKS(MOTION_TASK_PERIOD_MS)) == pdTRUE) {
            execute_command(&cmd);
        }

        /* Check non-blocking duration timer */
        if (s_motion_end_time_ms > 0) {
            int64_t now_ms = esp_timer_get_time() / 1000;
            if (now_ms >= s_motion_end_time_ms) {
                /* Duration expired — stop motors */
                apply_speeds(0, 0);
                s_motion_end_time_ms = 0;

                robot_event_t event = {
                    .id = EVENT_MOTION_COMPLETE,
                    .timestamp = 0,
                    .payload = NULL,
                    .payload_len = 0,
                };
                event_bus_publish(&event);
            }
        }

        /* Feed watchdog */
        esp_task_wdt_reset();
    }
}

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t motion_manager_init(const motion_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Motion manager already initialized");
        return ESP_OK;
    }

    /* Initialize DRV8833 driver */
    drv8833_config_t drv_config = {
        .pin_ain1 = BSP_PIN_MOTOR_AIN1,
        .pin_ain2 = BSP_PIN_MOTOR_AIN2,
        .pin_bin1 = BSP_PIN_MOTOR_BIN1,
        .pin_bin2 = BSP_PIN_MOTOR_BIN2,
        .pwm_freq = BSP_MOTOR_PWM_FREQ_HZ,
        .pwm_timer = BSP_MOTOR_PWM_TIMER,
    };

    if (config != NULL) {
        drv_config.pin_ain1 = config->pin_ain1;
        drv_config.pin_ain2 = config->pin_ain2;
        drv_config.pin_bin1 = config->pin_bin1;
        drv_config.pin_bin2 = config->pin_bin2;
        drv_config.pwm_freq = config->pwm_freq;
        s_max_speed = config->max_speed;
        s_default_speed = config->default_speed;
    }

    esp_err_t ret = drv8833_init(&drv_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "DRV8833 init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Create command queue */
    s_cmd_queue = xQueueCreate(MOTION_QUEUE_DEPTH, sizeof(motion_command_t));
    if (s_cmd_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create command queue");
        drv8833_deinit();
        return ESP_ERR_NO_MEM;
    }

    /* Create motion task */
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        motion_task,
        "motion",
        MOTION_TASK_STACK_SIZE,
        NULL,
        MOTION_TASK_PRIORITY,
        &s_task_handle,
        MOTION_TASK_CORE_ID
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create motion task");
        vQueueDelete(s_cmd_queue);
        s_cmd_queue = NULL;
        drv8833_deinit();
        return ESP_ERR_NO_MEM;
    }

    /* Register task with system monitor */
    s_initialized = true;

    ESP_LOGI(TAG, "Motion manager initialized");
    return ESP_OK;
}

esp_err_t motion_manager_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    /* Stop motors */
    drv8833_stop_all();

    /* Delete task */
    if (s_task_handle != NULL) {
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
    }

    /* Delete queue */
    if (s_cmd_queue != NULL) {
        vQueueDelete(s_cmd_queue);
        s_cmd_queue = NULL;
    }

    /* Deinitialize DRV8833 */
    drv8833_deinit();

    s_initialized = false;
    ESP_LOGI(TAG, "Motion manager deinitialized");
    return ESP_OK;
}

esp_err_t motion_execute(motion_cmd_t cmd, int16_t speed, int16_t angle, uint16_t duration_ms)
{
    if (!s_initialized || s_cmd_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    motion_command_t command = {
        .cmd = cmd,
        .speed = speed,
        .angle = angle,
        .duration_ms = duration_ms,
    };

    BaseType_t ret = xQueueSend(s_cmd_queue, &command, pdMS_TO_TICKS(10));
    if (ret != pdTRUE) {
        ESP_LOGW(TAG, "Motion command queue full");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t motion_emergency_stop(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Immediately stop motors (bypasses queue) */
    drv8833_stop_all();
    s_is_moving = false;
    s_left_speed = 0;
    s_right_speed = 0;
    s_motion_end_time_ms = 0;  /* Cancel any pending duration timer */

    /* Flush command queue */
    if (s_cmd_queue != NULL) {
        xQueueReset(s_cmd_queue);
    }

    ESP_LOGW(TAG, "EMERGENCY STOP");
    return ESP_OK;
}

bool motion_is_moving(void)
{
    return s_is_moving;
}