/**
 * @file ota_manager.c
 * @brief OTA 状态管理实现
 *
 * @copyright Copyright (c) 2026 RobotBuddy
 * @author RobotBuddy Team
 * @date 2026-07-19
 */

#include "ota_manager.h"
#include "event_bus.h"
#include "robot_events.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>
#include <stdlib.h>

/* ============================================================
 * 日志标签
 * ============================================================ */

static const char *TAG = OTA_LOG_TAG_MANAGER;

/* ============================================================
 * 模块状态
 * ============================================================ */

static bool s_initialized = false;
static SemaphoreHandle_t s_mutex = NULL;

/* 当前状态 */
static ota_state_t s_current_state = OTA_STATE_IDLE;
static ota_progress_t s_progress = {0};
static ota_error_t s_last_error = {0};
static ota_update_info_t s_update_info = {0};

/* 回调 */
#define MAX_CALLBACKS 4
static ota_state_callback_t s_state_callbacks[MAX_CALLBACKS] = {0};
static ota_progress_callback_t s_progress_callbacks[MAX_CALLBACKS] = {0};
static ota_result_callback_t s_result_callbacks[MAX_CALLBACKS] = {0};

/* ============================================================
 * 有效状态转换表
 * ============================================================ */

typedef struct {
    ota_state_t from;
    ota_state_t to;
} state_transition_t;

static const state_transition_t s_valid_transitions[] = {
    /* IDLE -> */
    {OTA_STATE_IDLE, OTA_STATE_CHECKING},
    {OTA_STATE_IDLE, OTA_STATE_ERROR},

    /* CHECKING -> */
    {OTA_STATE_CHECKING, OTA_STATE_DOWNLOADING},
    {OTA_STATE_CHECKING, OTA_STATE_IDLE},
    {OTA_STATE_CHECKING, OTA_STATE_ERROR},

    /* DOWNLOADING -> */
    {OTA_STATE_DOWNLOADING, OTA_STATE_VERIFYING},
    {OTA_STATE_DOWNLOADING, OTA_STATE_IDLE},
    {OTA_STATE_DOWNLOADING, OTA_STATE_ERROR},

    /* VERIFYING -> */
    {OTA_STATE_VERIFYING, OTA_STATE_READY},
    {OTA_STATE_VERIFYING, OTA_STATE_IDLE},
    {OTA_STATE_VERIFYING, OTA_STATE_ERROR},

    /* READY -> */
    {OTA_STATE_READY, OTA_STATE_REBOOTING},
    {OTA_STATE_READY, OTA_STATE_IDLE},

    /* REBOOTING -> */
    {OTA_STATE_REBOOTING, OTA_STATE_VERIFYING_NEW},
    {OTA_STATE_REBOOTING, OTA_STATE_ROLLING_BACK},

    /* VERIFYING_NEW -> */
    {OTA_STATE_VERIFYING_NEW, OTA_STATE_COMMITTED},
    {OTA_STATE_VERIFYING_NEW, OTA_STATE_ROLLING_BACK},

    /* COMMITTED -> */
    {OTA_STATE_COMMITTED, OTA_STATE_IDLE},

    /* ROLLING_BACK -> */
    {OTA_STATE_ROLLING_BACK, OTA_STATE_REBOOTING},
    {OTA_STATE_ROLLING_BACK, OTA_STATE_FACTORY_RESET},

    /* FACTORY_RESET -> */
    {OTA_STATE_FACTORY_RESET, OTA_STATE_REBOOTING},

    /* ERROR -> */
    {OTA_STATE_ERROR, OTA_STATE_IDLE},
};

#define NUM_VALID_TRANSITIONS (sizeof(s_valid_transitions) / sizeof(s_valid_transitions[0]))

/* ============================================================
 * 内部函数
 * ============================================================ */

/**
 * @brief 发布 OTA 状态变化事件
 */
static void publish_state_change_event(ota_state_t old_state, ota_state_t new_state)
{
    /* 使用 OTA 进度事件承载状态变化 */
    ota_progress_event_t payload = {
        .percent = (uint8_t)new_state,  /* 复用 percent 字段传递状态 */
        .downloaded = 0,
        .total = 0,
    };

    robot_event_t event = {
        .id = EVENT_OTA_PROGRESS,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };

    void *payload_copy = malloc(sizeof(payload));
    if (payload_copy) {
        memcpy(payload_copy, &payload, sizeof(payload));
        event.payload = payload_copy;
        event.payload_len = sizeof(payload);
        event_bus_publish(&event);
    }
}

/**
 * @brief 发布 OTA 进度事件
 */
static void publish_progress_event(const ota_progress_t *progress)
{
    ota_progress_event_t payload = {
        .percent = (uint8_t)progress->progress_percent,
        .downloaded = (uint32_t)progress->bytes_downloaded,
        .total = (uint32_t)progress->bytes_total,
    };

    robot_event_t event = {
        .id = EVENT_OTA_PROGRESS,
        .timestamp = progress->elapsed_ms,
        .payload = NULL,
        .payload_len = 0,
    };

    void *payload_copy = malloc(sizeof(payload));
    if (payload_copy) {
        memcpy(payload_copy, &payload, sizeof(payload));
        event.payload = payload_copy;
        event.payload_len = sizeof(payload);
        event_bus_publish(&event);
    }
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

esp_err_t ota_manager_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "OTA manager already initialized");
        return ESP_OK;
    }

    /* 创建互斥锁 */
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    /* 初始化状态 */
    s_current_state = OTA_STATE_IDLE;
    memset(&s_progress, 0, sizeof(s_progress));
    memset(&s_last_error, 0, sizeof(s_last_error));
    memset(&s_update_info, 0, sizeof(s_update_info));

    /* 清零回调 */
    memset(s_state_callbacks, 0, sizeof(s_state_callbacks));
    memset(s_progress_callbacks, 0, sizeof(s_progress_callbacks));
    memset(s_result_callbacks, 0, sizeof(s_result_callbacks));

    s_initialized = true;
    ESP_LOGI(TAG, "OTA manager initialized");

    return ESP_OK;
}

esp_err_t ota_manager_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    if (s_mutex != NULL) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
    }

    s_initialized = false;
    s_current_state = OTA_STATE_IDLE;
    memset(&s_progress, 0, sizeof(s_progress));
    memset(&s_last_error, 0, sizeof(s_last_error));
    memset(&s_update_info, 0, sizeof(s_update_info));

    if (s_mutex != NULL) {
        xSemaphoreGive(s_mutex);
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }

    ESP_LOGI(TAG, "OTA manager deinitialized");
    return ESP_OK;
}

esp_err_t ota_manager_set_state(ota_state_t new_state)
{
    if (!s_initialized || s_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    ota_state_t old_state = s_current_state;

    /* 检查转换有效性 */
    if (!ota_manager_is_valid_transition(old_state, new_state)) {
        ESP_LOGW(TAG, "Invalid state transition: %d -> %d", old_state, new_state);
        xSemaphoreGive(s_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    s_current_state = new_state;
    s_progress.state = new_state;

    ESP_LOGI(TAG, "State changed: %d -> %d", old_state, new_state);

    /* 发布事件 */
    publish_state_change_event(old_state, new_state);

    xSemaphoreGive(s_mutex);

    /* 触发回调（在锁外调用，避免死锁） */
    ota_manager_invoke_state_callbacks(old_state, new_state);

    return ESP_OK;
}

ota_state_t ota_manager_get_state(void)
{
    if (!s_initialized || s_mutex == NULL) {
        return OTA_STATE_IDLE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    ota_state_t state = s_current_state;
    xSemaphoreGive(s_mutex);

    return state;
}

bool ota_manager_is_valid_transition(ota_state_t from, ota_state_t to)
{
    /* 相同状态总是有效 */
    if (from == to) {
        return true;
    }

    /* 查找转换表 */
    for (size_t i = 0; i < NUM_VALID_TRANSITIONS; i++) {
        if (s_valid_transitions[i].from == from &&
            s_valid_transitions[i].to == to) {
            return true;
        }
    }

    return false;
}

esp_err_t ota_manager_update_progress(size_t bytes_downloaded, size_t bytes_total)
{
    if (!s_initialized || s_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    s_progress.bytes_downloaded = bytes_downloaded;
    s_progress.bytes_total = bytes_total;

    if (bytes_total > 0) {
        s_progress.progress_percent = (int)((bytes_downloaded * 100) / bytes_total);
        if (s_progress.progress_percent > 100) {
            s_progress.progress_percent = 100;
        }
    }

    /* 发布事件 */
    publish_progress_event(&s_progress);

    xSemaphoreGive(s_mutex);

    /* 触发回调 */
    ota_manager_invoke_progress_callbacks(&s_progress);

    return ESP_OK;
}

esp_err_t ota_manager_get_progress(ota_progress_t *progress)
{
    if (progress == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || s_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memcpy(progress, &s_progress, sizeof(ota_progress_t));
    xSemaphoreGive(s_mutex);

    return ESP_OK;
}

esp_err_t ota_manager_save_progress(const ota_progress_t *progress)
{
    if (progress == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(OTA_NVS_NAMESPACE_PROGRESS, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s",
                 OTA_NVS_NAMESPACE_PROGRESS, esp_err_to_name(ret));
        return ret;
    }

    nvs_set_u32(handle, OTA_NVS_KEY_RESUME_OFFSET, (uint32_t)progress->bytes_downloaded);
    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGD(TAG, "Progress saved: %lu bytes", (unsigned long)progress->bytes_downloaded);
    return ESP_OK;
}

esp_err_t ota_manager_load_progress(ota_progress_t *progress)
{
    if (progress == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(OTA_NVS_NAMESPACE_PROGRESS, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }

    uint32_t offset = 0;
    ret = nvs_get_u32(handle, OTA_NVS_KEY_RESUME_OFFSET, &offset);
    nvs_close(handle);

    if (ret != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }

    memset(progress, 0, sizeof(ota_progress_t));
    progress->bytes_downloaded = offset;

    ESP_LOGD(TAG, "Progress loaded: %lu bytes", (unsigned long)offset);
    return ESP_OK;
}

esp_err_t ota_manager_clear_progress(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(OTA_NVS_NAMESPACE_PROGRESS, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ESP_OK;  /* 不存在则直接返回 */
    }

    nvs_erase_key(handle, OTA_NVS_KEY_RESUME_OFFSET);
    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGD(TAG, "Progress cleared");
    return ESP_OK;
}

esp_err_t ota_manager_set_error(ota_error_code_t code, const char *message)
{
    if (!s_initialized || s_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    s_last_error.code = code;
    s_last_error.failed_state = s_current_state;
    s_last_error.timestamp = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    if (message != NULL) {
        strncpy(s_last_error.message, message, sizeof(s_last_error.message) - 1);
        s_last_error.message[sizeof(s_last_error.message) - 1] = '\0';
    } else {
        snprintf(s_last_error.message, sizeof(s_last_error.message),
                 "Error code: 0x%04X", code);
    }

    ESP_LOGE(TAG, "OTA error: %s", s_last_error.message);

    xSemaphoreGive(s_mutex);

    return ESP_OK;
}

esp_err_t ota_manager_get_error(ota_error_t *error)
{
    if (error == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || s_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (s_last_error.code == OTA_ERR_NONE) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    memcpy(error, &s_last_error, sizeof(ota_error_t));
    xSemaphoreGive(s_mutex);

    return ESP_OK;
}

esp_err_t ota_manager_clear_error(void)
{
    if (!s_initialized || s_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memset(&s_last_error, 0, sizeof(s_last_error));
    xSemaphoreGive(s_mutex);

    return ESP_OK;
}

esp_err_t ota_manager_save_rollback_count(uint8_t wd_count, uint8_t panic_count)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(OTA_NVS_NAMESPACE_BOOT, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s",
                 OTA_NVS_NAMESPACE_BOOT, esp_err_to_name(ret));
        return ret;
    }

    nvs_set_u8(handle, OTA_NVS_KEY_WD_COUNT, wd_count);
    nvs_set_u8(handle, OTA_NVS_KEY_PANIC_COUNT, panic_count);
    nvs_set_u32(handle, OTA_NVS_KEY_BOOT_TIME, (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS));
    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGD(TAG, "Rollback count saved: wd=%d, panic=%d", wd_count, panic_count);
    return ESP_OK;
}

esp_err_t ota_manager_load_rollback_count(uint8_t *wd_count, uint8_t *panic_count)
{
    if (wd_count == NULL || panic_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(OTA_NVS_NAMESPACE_BOOT, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        *wd_count = 0;
        *panic_count = 0;
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t ret_wd = nvs_get_u8(handle, OTA_NVS_KEY_WD_COUNT, wd_count);
    esp_err_t ret_panic = nvs_get_u8(handle, OTA_NVS_KEY_PANIC_COUNT, panic_count);

    nvs_close(handle);

    if (ret_wd != ESP_OK) *wd_count = 0;
    if (ret_panic != ESP_OK) *panic_count = 0;

    return ESP_OK;
}

esp_err_t ota_manager_reset_rollback_count(void)
{
    return ota_manager_save_rollback_count(0, 0);
}

esp_err_t ota_manager_save_boot_state(const ota_boot_state_t *state)
{
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return ota_manager_save_rollback_count(state->watchdog_count, state->panic_count);
}

esp_err_t ota_manager_load_boot_state(ota_boot_state_t *state)
{
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(state, 0, sizeof(ota_boot_state_t));

    esp_err_t ret = ota_manager_load_rollback_count(&state->watchdog_count, &state->panic_count);
    if (ret != ESP_OK) {
        return ret;
    }

    /* 加载启动时间 */
    nvs_handle_t handle;
    ret = nvs_open(OTA_NVS_NAMESPACE_BOOT, NVS_READONLY, &handle);
    if (ret == ESP_OK) {
        nvs_get_u32(handle, OTA_NVS_KEY_BOOT_TIME, &state->boot_timestamp);
        nvs_close(handle);
    }

    return ESP_OK;
}

esp_err_t ota_manager_save_update_info(const ota_update_info_t *info)
{
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || s_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memcpy(&s_update_info, info, sizeof(ota_update_info_t));
    xSemaphoreGive(s_mutex);

    /* 同时保存到 NVS */
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(OTA_NVS_NAMESPACE_CONFIG, NVS_READWRITE, &handle);
    if (ret == ESP_OK) {
        nvs_set_str(handle, OTA_NVS_KEY_VERSION, info->version);
        nvs_set_u32(handle, OTA_NVS_KEY_STATE, (uint32_t)s_current_state);
        nvs_commit(handle);
        nvs_close(handle);
    }

    return ESP_OK;
}

esp_err_t ota_manager_get_update_info(ota_update_info_t *info)
{
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || s_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (s_update_info.version[0] == '\0') {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    memcpy(info, &s_update_info, sizeof(ota_update_info_t));
    xSemaphoreGive(s_mutex);

    return ESP_OK;
}

esp_err_t ota_manager_clear_update_info(void)
{
    if (!s_initialized || s_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memset(&s_update_info, 0, sizeof(s_update_info_t));
    xSemaphoreGive(s_mutex);

    /* 清除 NVS */
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(OTA_NVS_NAMESPACE_CONFIG, NVS_READWRITE, &handle);
    if (ret == ESP_OK) {
        nvs_erase_key(handle, OTA_NVS_KEY_VERSION);
        nvs_erase_key(handle, OTA_NVS_KEY_STATE);
        nvs_commit(handle);
        nvs_close(handle);
    }

    return ESP_OK;
}

void ota_manager_invoke_state_callbacks(ota_state_t old_state, ota_state_t new_state)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (s_state_callbacks[i] != NULL) {
            s_state_callbacks[i](old_state, new_state);
        }
    }
}

void ota_manager_invoke_progress_callbacks(const ota_progress_t *progress)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (s_progress_callbacks[i] != NULL) {
            s_progress_callbacks[i](progress);
        }
    }
}

void ota_manager_invoke_result_callbacks(bool success, const char *version, const char *error)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (s_result_callbacks[i] != NULL) {
            s_result_callbacks[i](success, version, error);
        }
    }
}
