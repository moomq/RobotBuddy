/**
 * @file ota_rollback.c
 * @brief OTA 回滚管理实现
 *
 * @copyright Copyright (c) 2026 RobotBuddy
 * @author RobotBuddy Team
 * @date 2026-07-19
 */

#include "ota_rollback.h"
#include "ota_manager.h"
#include "ota_partition.h"

#include "event_bus.h"
#include "robot_events.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include <string.h>

/* ============================================================
 * 日志标签
 * ============================================================ */

static const char *TAG = OTA_LOG_TAG_ROLLBACK;

/* ============================================================
 * 模块状态
 * ============================================================ */

static bool s_initialized = false;

/* 回滚计数 */
static uint8_t s_watchdog_count = 0;
static uint8_t s_panic_count = 0;

/* 健康检查 */
static TimerHandle_t s_health_check_timer = NULL;
static uint32_t s_health_check_deadline = 0;
static bool s_health_check_passed = false;
static bool s_health_check_active = false;

/* 回滚原因 */
static rollback_reason_t s_rollback_reason = ROLLBACK_REASON_NONE;

/* 健康检查回调 */
static health_check_result_t (*s_health_check_callback)(void) = NULL;

/* ============================================================
 * 内部函数
 * ============================================================ */

/**
 * @brief 健康检查定时器回调
 */
static void health_check_timer_callback(TimerHandle_t timer)
{
    ESP_LOGW(TAG, "Health check timeout!");

    s_rollback_reason = ROLLBACK_REASON_HEALTH_FAIL;

    /* 发布事件 */
    robot_event_t event = {
        .id = EVENT_OTA_ERROR,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };
    event_bus_publish(&event);

    /* 执行自动回滚 */
    ota_rollback_auto();
}

/**
 * @brief 保存计数到 NVS
 */
static void save_counts_to_nvs(void)
{
    ota_manager_save_rollback_count(s_watchdog_count, s_panic_count);
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

esp_err_t ota_rollback_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    /* 从 NVS 加载计数 */
    ota_manager_load_rollback_count(&s_watchdog_count, &s_panic_count);

    ESP_LOGI(TAG, "Rollback counts loaded: watchdog=%d, panic=%d",
             s_watchdog_count, s_panic_count);

    /* 创建健康检查定时器 */
    s_health_check_timer = xTimerCreate("health_chk",
                                         pdMS_TO_TICKS(OTA_HEALTH_CHECK_TIMEOUT_MS),
                                         pdFALSE,  /* 单次触发 */
                                         NULL,
                                         health_check_timer_callback);

    if (s_health_check_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create health check timer");
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    return ESP_OK;
}

esp_err_t ota_rollback_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    /* 停止并删除定时器 */
    if (s_health_check_timer != NULL) {
        xTimerStop(s_health_check_timer, 0);
        xTimerDelete(s_health_check_timer, 0);
        s_health_check_timer = NULL;
    }

    s_initialized = false;
    return ESP_OK;
}

health_check_result_t ota_rollback_health_check(void)
{
    ESP_LOGI(TAG, "Running health check...");

    health_check_result_t result = HEALTH_CHECK_PASS;

    /* 1. 检查 WiFi 连接状态 */
    /* TODO: 实现 WiFi 状态检查 */
    ESP_LOGD(TAG, "Checking WiFi status...");

    /* 2. 检查 MQTT 连接状态 */
    /* TODO: 实现 MQTT 状态检查 */
    ESP_LOGD(TAG, "Checking MQTT status...");

    /* 3. 检查显示功能 */
    /* TODO: 实现显示检查 */
    ESP_LOGD(TAG, "Checking display...");

    /* 4. 检查传感器 */
    /* TODO: 实现传感器检查 */
    ESP_LOGD(TAG, "Checking sensors...");

    /* 调用自定义回调 */
    if (s_health_check_callback != NULL) {
        result = s_health_check_callback();
    }

    if (result == HEALTH_CHECK_PASS) {
        ESP_LOGI(TAG, "Health check passed");
    } else {
        ESP_LOGW(TAG, "Health check failed: %d", result);
    }

    return result;
}

esp_err_t ota_rollback_start_health_check_timer(void)
{
    if (!s_initialized || s_health_check_timer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_health_check_deadline = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS) +
                              OTA_HEALTH_CHECK_TIMEOUT_MS;
    s_health_check_passed = false;
    s_health_check_active = true;

    /* 启动定时器 */
    BaseType_t ret = xTimerStart(s_health_check_timer, pdMS_TO_TICKS(100));
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to start health check timer");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Health check timer started (%d ms)", OTA_HEALTH_CHECK_TIMEOUT_MS);
    return ESP_OK;
}

esp_err_t ota_rollback_stop_health_check_timer(void)
{
    if (!s_initialized || s_health_check_timer == NULL) {
        return ESP_OK;
    }

    xTimerStop(s_health_check_timer, 0);
    s_health_check_active = false;

    ESP_LOGD(TAG, "Health check timer stopped");
    return ESP_OK;
}

esp_err_t ota_rollback_report_health_check_pass(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Health check passed!");

    s_health_check_passed = true;
    s_health_check_active = false;

    /* 停止定时器 */
    ota_rollback_stop_health_check_timer();

    /* 清零回滚计数 */
    ota_rollback_reset_counts();

    /* 标记分区有效 */
    return ota_partition_mark_valid();
}

esp_err_t ota_rollback_report_health_check_fail(health_check_result_t result)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGW(TAG, "Health check failed: %d", result);

    s_health_check_passed = false;
    s_health_check_active = false;
    s_rollback_reason = ROLLBACK_REASON_HEALTH_FAIL;

    /* 停止定时器 */
    ota_rollback_stop_health_check_timer();

    /* 执行回滚 */
    return ota_rollback_auto();
}

uint32_t ota_rollback_get_health_check_deadline(void)
{
    return s_health_check_deadline;
}

bool ota_rollback_is_health_check_timeout(void)
{
    if (!s_health_check_active) {
        return false;
    }

    uint32_t now = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    return (now >= s_health_check_deadline);
}

bool ota_rollback_check_needed(void)
{
    /* 检查 watchdog 计数 */
    if (s_watchdog_count >= OTA_WATCHDOG_THRESHOLD) {
        s_rollback_reason = ROLLBACK_REASON_WATCHDOG;
        ESP_LOGW(TAG, "Rollback needed: watchdog count %d >= %d",
                 s_watchdog_count, OTA_WATCHDOG_THRESHOLD);
        return true;
    }

    /* 检查 panic 计数 */
    if (s_panic_count >= OTA_PANIC_THRESHOLD) {
        s_rollback_reason = ROLLBACK_REASON_PANIC;
        ESP_LOGW(TAG, "Rollback needed: panic count %d >= %d",
                 s_panic_count, OTA_PANIC_THRESHOLD);
        return true;
    }

    /* 检查健康检查超时 */
    if (ota_rollback_is_health_check_timeout()) {
        s_rollback_reason = ROLLBACK_REASON_HEALTH_FAIL;
        ESP_LOGW(TAG, "Rollback needed: health check timeout");
        return true;
    }

    return false;
}

rollback_reason_t ota_rollback_get_reason(void)
{
    return s_rollback_reason;
}

esp_err_t ota_rollback_increment_watchdog_count(void)
{
    s_watchdog_count++;
    save_counts_to_nvs();

    ESP_LOGW(TAG, "Watchdog count incremented to %d", s_watchdog_count);

    /* 检查是否需要回滚 */
    if (s_watchdog_count >= OTA_WATCHDOG_THRESHOLD) {
        s_rollback_reason = ROLLBACK_REASON_WATCHDOG;
        ESP_LOGW(TAG, "Watchdog threshold reached, rollback needed");
    }

    return ESP_OK;
}

esp_err_t ota_rollback_increment_panic_count(void)
{
    s_panic_count++;
    save_counts_to_nvs();

    ESP_LOGW(TAG, "Panic count incremented to %d", s_panic_count);

    /* 检查是否需要回滚 */
    if (s_panic_count >= OTA_PANIC_THRESHOLD) {
        s_rollback_reason = ROLLBACK_REASON_PANIC;
        ESP_LOGW(TAG, "Panic threshold reached, rollback needed");
    }

    return ESP_OK;
}

uint8_t ota_rollback_get_watchdog_count(void)
{
    return s_watchdog_count;
}

uint8_t ota_rollback_get_panic_count(void)
{
    return s_panic_count;
}

esp_err_t ota_rollback_reset_counts(void)
{
    s_watchdog_count = 0;
    s_panic_count = 0;
    s_rollback_reason = ROLLBACK_REASON_NONE;

    save_counts_to_nvs();

    ESP_LOGI(TAG, "Rollback counts reset");
    return ESP_OK;
}

esp_err_t ota_rollback_auto(void)
{
    ESP_LOGW(TAG, "Auto rollback triggered (reason: %d)", s_rollback_reason);

    /* 设置状态 */
    ota_manager_set_state(OTA_STATE_ROLLING_BACK);

    /* 获取上一个有效分区 */
    const esp_partition_t *running = ota_partition_get_running();
    const esp_partition_t *idle = ota_partition_get_idle();

    /* 检查是否有有效分区可回滚 */
    if (idle != NULL && ota_partition_is_valid(idle)) {
        ESP_LOGI(TAG, "Rolling back to partition: %s", idle->label);

        /* 清零计数 */
        ota_rollback_reset_counts();

        /* 切换分区并重启 */
        ota_partition_mark_invalid_and_rollback();
    }

    /* 没有有效的 OTA 分区，回退到 factory */
    ESP_LOGW(TAG, "No valid OTA partition, falling back to factory");

    const esp_partition_t *factory = ota_partition_get_factory();
    if (factory != NULL) {
        ota_manager_set_state(OTA_STATE_FACTORY_RESET);
        ota_partition_switch_and_reboot(factory);
    }

    /* 完全失败 */
    ESP_LOGE(TAG, "No valid partition available!");
    return ESP_ERR_NO_VALID_PARTITION;
}

esp_err_t ota_rollback_manual(rollback_reason_t reason)
{
    ESP_LOGI(TAG, "Manual rollback requested (reason: %d)", reason);

    s_rollback_reason = reason;
    return ota_rollback_auto();
}

esp_err_t ota_rollback_to_factory(void)
{
    ESP_LOGI(TAG, "Rollback to factory requested");

    const esp_partition_t *factory = ota_partition_get_factory();
    if (factory == NULL) {
        ESP_LOGE(TAG, "Factory partition not found");
        return ESP_FAIL;
    }

    ota_manager_set_state(OTA_STATE_FACTORY_RESET);

    /* 清除 OTA 状态 */
    ota_rollback_reset_counts();

    /* 切换到 factory 分区 */
    ESP_LOGI(TAG, "Switching to factory partition...");
    ota_partition_switch_and_reboot(factory);

    /* 不返回 */
}

esp_err_t ota_rollback_update_boot_timestamp(void)
{
    uint32_t boot_time = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(OTA_NVS_NAMESPACE_BOOT, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    nvs_set_u32(handle, OTA_NVS_KEY_BOOT_TIME, boot_time);
    nvs_commit(handle);
    nvs_close(handle);

    return ESP_OK;
}

esp_err_t ota_rollback_get_boot_state(ota_boot_state_t *state)
{
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return ota_manager_load_boot_state(state);
}

bool ota_rollback_is_first_boot(void)
{
    /* 检查 OTA 状态 */
    ota_state_t state = ota_manager_get_state();
    return (state == OTA_STATE_VERIFYING_NEW);
}

esp_err_t ota_rollback_mark_boot_verified(void)
{
    ESP_LOGI(TAG, "Boot verified, committing OTA");

    /* 清零回滚计数 */
    ota_rollback_reset_counts();

    /* 标记分区有效 */
    esp_err_t ret = ota_partition_mark_valid();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mark partition valid: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 更新状态 */
    ota_manager_set_state(OTA_STATE_COMMITTED);

    return ESP_OK;
}

esp_err_t ota_rollback_register_health_check_callback(health_check_result_t (*callback)(void))
{
    s_health_check_callback = callback;
    return ESP_OK;
}
