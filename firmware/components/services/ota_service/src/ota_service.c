/**
 * @file ota_service.c
 * @brief OTA 升级服务主实现
 *
 * @copyright Copyright (c) 2026 RobotBuddy
 * @author RobotBuddy Team
 * @date 2026-07-19
 */

#include "ota_service.h"
#include "ota_manager.h"
#include "ota_download.h"
#include "ota_verify.h"
#include "ota_partition.h"
#include "ota_rollback.h"
#include "ota_security.h"
#include "ota_types.h"
#include "ota_config.h"

#include "event_bus.h"
#include "robot_events.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include <string.h>
#include <stdlib.h>
#include <cJSON.h>

/* ============================================================
 * 日志标签
 * ============================================================ */

static const char *TAG = OTA_LOG_TAG_SERVICE;

/* ============================================================
 * 模块状态
 * ============================================================ */

static bool s_initialized = false;
static TaskHandle_t s_task_handle = NULL;
static QueueHandle_t s_cmd_queue = NULL;

/* 电量检查回调 */
static bool (*s_battery_check_callback)(uint8_t *percent) = NULL;

/* ============================================================
 * 内部结构
 * ============================================================ */

/**
 * @brief OTA 升级上下文
 */
typedef struct {
    ota_update_info_t update_info;
    esp_ota_handle_t ota_handle;
    const esp_partition_t *target_partition;
    uint8_t signature[OTA_SIGNATURE_LEN];
    size_t signature_len;
    uint32_t start_time;
} ota_upgrade_context_t;

static ota_upgrade_context_t s_upgrade_ctx = {0};

/* ============================================================
 * 内部函数声明
 * ============================================================ */

static void ota_service_task(void *arg);
static void process_command(ota_cmd_msg_t *cmd);
static esp_err_t execute_check_update(void);
static esp_err_t execute_upgrade(const ota_update_info_t *info);
static esp_err_t execute_rollback(void);
static esp_err_t execute_factory_reset(void);
static void handle_ota_mqtt_command(const char *payload_json);
static void on_wifi_connected(const robot_event_t *event);
static void on_mqtt_command(const robot_event_t *event);

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

esp_err_t ota_service_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "OTA service already initialized");
        return ESP_OK;
    }

    esp_err_t ret;

    /* 初始化子模块 */
    ret = ota_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init OTA manager: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = ota_partition_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init OTA partition: %s", esp_err_to_name(ret));
        ota_manager_deinit();
        return ret;
    }

    ret = ota_verify_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "OTA verify init warning: %s", esp_err_to_name(ret));
        /* 非致命错误，继续 */
    }

    ret = ota_download_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init OTA download: %s", esp_err_to_name(ret));
        ota_partition_deinit();
        ota_manager_deinit();
        return ret;
    }

    ret = ota_rollback_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "OTA rollback init warning: %s", esp_err_to_name(ret));
        /* 非致命错误，继续 */
    }

    ret = ota_security_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "OTA security init warning: %s", esp_err_to_name(ret));
        /* 非致命错误，继续 */
    }

    /* 创建命令队列 */
    s_cmd_queue = xQueueCreate(OTA_CMD_QUEUE_DEPTH, sizeof(ota_cmd_msg_t));
    if (s_cmd_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create command queue");
        ota_download_deinit();
        ota_partition_deinit();
        ota_manager_deinit();
        return ESP_ERR_NO_MEM;
    }

    /* 创建 OTA 服务任务 */
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        ota_service_task,
        OTA_SERVICE_TASK_NAME,
        OTA_SERVICE_TASK_STACK,
        NULL,
        OTA_SERVICE_TASK_PRIORITY,
        &s_task_handle,
        OTA_SERVICE_TASK_CORE
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA service task");
        vQueueDelete(s_cmd_queue);
        s_cmd_queue = NULL;
        ota_download_deinit();
        ota_partition_deinit();
        ota_manager_deinit();
        return ESP_ERR_NO_MEM;
    }

    /* 订阅事件 */
    event_bus_subscribe(EVENT_SYS_WIFI_CONNECTED, on_wifi_connected);
    event_bus_subscribe(EVENT_MQTT_MESSAGE, on_mqtt_command);

    /* 检查是否需要自动回滚 */
    if (ota_rollback_check_needed()) {
        ESP_LOGW(TAG, "Automatic rollback needed on boot");
        ota_manager_set_state(OTA_STATE_ROLLING_BACK);
        /* 回滚会在任务启动后执行 */
    }

    s_initialized = true;
    ESP_LOGI(TAG, "OTA service initialized");

    /* 打印分区信息 */
    ota_partition_print_info();

    return ESP_OK;
}

esp_err_t ota_service_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    /* 停止任务 */
    if (s_task_handle != NULL) {
        vTaskDelete(s_task_handle);
        s_task_handle = NULL;
    }

    /* 删除队列 */
    if (s_cmd_queue != NULL) {
        vQueueDelete(s_cmd_queue);
        s_cmd_queue = NULL;
    }

    /* 反初始化子模块 */
    ota_rollback_deinit();
    ota_download_deinit();
    ota_verify_deinit();
    ota_partition_deinit();
    ota_manager_deinit();
    ota_security_deinit();

    s_initialized = false;
    ESP_LOGI(TAG, "OTA service deinitialized");
    return ESP_OK;
}

bool ota_service_is_initialized(void)
{
    return s_initialized;
}

esp_err_t ota_service_check_update(ota_update_info_t *info)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 发送检查命令到队列 */
    ota_cmd_msg_t cmd = {
        .type = OTA_CMD_CHECK,
        .info = {0},
    };

    if (xQueueSend(s_cmd_queue, &cmd, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGW(TAG, "Failed to send check command");
        return ESP_FAIL;
    }

    /* 等待结果（简化：返回 OK，实际应等待） */
    return ESP_OK;
}

esp_err_t ota_service_start_upgrade(const ota_update_info_t *info)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 检查当前状态 */
    ota_state_t state = ota_manager_get_state();
    if (state != OTA_STATE_IDLE) {
        ESP_LOGW(TAG, "OTA already in progress (state=%d)", state);
        return ESP_ERR_ALREADY_UPDATING;
    }

    /* 检查电量 */
    if (s_battery_check_callback != NULL) {
        uint8_t battery_percent = 0;
        if (s_battery_check_callback(&battery_percent)) {
            if (battery_percent < OTA_MIN_BATTERY_PERCENT) {
                ESP_LOGW(TAG, "Battery too low: %d%% < %d%%",
                         battery_percent, OTA_MIN_BATTERY_PERCENT);
                ota_manager_set_error(OTA_ERR_LOW_BATTERY, "Battery too low");
                return ESP_ERR_LOW_BATTERY;
            }
        }
    }

    /* 发送升级命令 */
    ota_cmd_msg_t cmd = {
        .type = OTA_CMD_UPGRADE,
    };
    memcpy(&cmd.info, info, sizeof(ota_update_info_t));

    if (xQueueSend(s_cmd_queue, &cmd, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGW(TAG, "Failed to send upgrade command");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ota_service_cancel(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ota_state_t state = ota_manager_get_state();
    if (state != OTA_STATE_DOWNLOADING) {
        ESP_LOGW(TAG, "Cannot cancel - not downloading (state=%d)", state);
        return ESP_ERR_INVALID_STATE;
    }

    /* 取消下载 */
    esp_err_t ret = ota_download_cancel();
    if (ret == ESP_OK) {
        ota_manager_set_state(OTA_STATE_IDLE);
    }

    return ret;
}

esp_err_t ota_service_apply(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ota_state_t state = ota_manager_get_state();
    if (state != OTA_STATE_READY) {
        ESP_LOGW(TAG, "Cannot apply - not ready (state=%d)", state);
        return ESP_ERR_INVALID_STATE;
    }

    /* 设置启动分区 */
    if (s_upgrade_ctx.target_partition != NULL) {
        ota_manager_set_state(OTA_STATE_REBOOTING);

        ESP_LOGI(TAG, "Setting boot partition to %s", s_upgrade_ctx.target_partition->label);
        esp_err_t ret = ota_partition_mark_bootable(s_upgrade_ctx.target_partition);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set boot partition: %s", esp_err_to_name(ret));
            ota_manager_set_state(OTA_STATE_ERROR);
            return ret;
        }

        /* 重启 */
        ESP_LOGI(TAG, "Rebooting to apply update...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }

    return ESP_FAIL;
}

esp_err_t ota_service_commit(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ota_state_t state = ota_manager_get_state();
    if (state != OTA_STATE_VERIFYING_NEW) {
        ESP_LOGW(TAG, "Cannot commit - not in verifying state (state=%d)", state);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Committing OTA update");

    /* 报告健康检查通过 */
    esp_err_t ret = ota_rollback_report_health_check_pass();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 更新状态 */
    ota_manager_set_state(OTA_STATE_COMMITTED);
    ota_manager_set_state(OTA_STATE_IDLE);

    /* 触发结果回调 */
    char version[32] = {0};
    ota_manager_get_update_info(&s_upgrade_ctx.update_info);
    ota_manager_invoke_result_callbacks(true, version, NULL);

    return ESP_OK;
}

ota_state_t ota_service_get_state(void)
{
    return ota_manager_get_state();
}

esp_err_t ota_service_get_progress(ota_progress_t *progress)
{
    return ota_manager_get_progress(progress);
}

esp_err_t ota_service_get_error(ota_error_t *error)
{
    return ota_manager_get_error(error);
}

esp_err_t ota_service_get_running_partition(char *label, size_t label_size)
{
    if (label == NULL || label_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = ota_partition_get_running();
    if (running == NULL) {
        return ESP_FAIL;
    }

    strncpy(label, running->label, label_size - 1);
    label[label_size - 1] = '\0';

    return ESP_OK;
}

esp_err_t ota_service_rollback(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ota_state_t state = ota_manager_get_state();
    if (state != OTA_STATE_IDLE && state != OTA_STATE_ERROR &&
        state != OTA_STATE_VERIFYING_NEW) {
        ESP_LOGW(TAG, "Cannot rollback - invalid state (state=%d)", state);
        return ESP_ERR_INVALID_STATE;
    }

    return execute_rollback();
}

esp_err_t ota_service_factory_reset(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    return execute_factory_reset();
}

esp_err_t ota_service_register_state_callback(ota_state_callback_t cb)
{
    /* 简化：通过 ota_manager 注册 */
    /* 实际实现需要维护回调列表 */
    return ESP_OK;
}

esp_err_t ota_service_register_progress_callback(ota_progress_callback_t cb)
{
    return ESP_OK;
}

esp_err_t ota_service_register_result_callback(ota_result_callback_t cb)
{
    return ESP_OK;
}

void ota_service_handle_mqtt_command(const char *payload_json)
{
    if (payload_json == NULL || !s_initialized) {
        return;
    }

    handle_ota_mqtt_command(payload_json);
}

/* ============================================================
 * 任务实现
 * ============================================================ */

static void ota_service_task(void *arg)
{
    ESP_LOGI(TAG, "OTA service task started");

    /* 首次启动检查 */
    if (ota_rollback_is_first_boot()) {
        ESP_LOGI(TAG, "First boot after OTA - starting health check timer");
        ota_rollback_start_health_check_timer();
        ota_manager_set_state(OTA_STATE_VERIFYING_NEW);
    }

    ota_cmd_msg_t cmd;

    while (1) {
        /* 等待命令 */
        if (xQueueReceive(s_cmd_queue, &cmd, pdMS_TO_TICKS(1000)) == pdPASS) {
            process_command(&cmd);
        }

        /* 检查健康检查超时 */
        if (ota_rollback_is_health_check_timeout()) {
            ESP_LOGW(TAG, "Health check timeout!");
            ota_rollback_auto();
        }

        /* 喂看门狗 */
        /* esp_task_wdt_reset(); */
    }
}

static void process_command(ota_cmd_msg_t *cmd)
{
    if (cmd == NULL) {
        return;
    }

    ESP_LOGI(TAG, "Processing command: %d", cmd->type);

    switch (cmd->type) {
        case OTA_CMD_CHECK:
            execute_check_update();
            break;

        case OTA_CMD_UPGRADE:
            execute_upgrade(&cmd->info);
            break;

        case OTA_CMD_ROLLBACK:
            execute_rollback();
            break;

        case OTA_CMD_FACTORY_RESET:
            execute_factory_reset();
            break;

        case OTA_CMD_CANCEL:
            ota_service_cancel();
            break;

        default:
            ESP_LOGW(TAG, "Unknown command: %d", cmd->type);
            break;
    }
}

static esp_err_t execute_check_update(void)
{
    ESP_LOGI(TAG, "Checking for updates...");

    ota_manager_set_state(OTA_STATE_CHECKING);

    /* 获取设备 ID 和版本 */
    char device_id[32] = "unknown";
    char version[32] = "1.0.0";

#ifdef CONFIG_APP_PROJECT_VER
    strncpy(version, CONFIG_APP_PROJECT_VER, sizeof(version) - 1);
#endif

    /* 查询更新 */
    ota_update_info_t info;
    esp_err_t ret = ota_download_check_update(device_id, version, &info);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Update check failed: %s", esp_err_to_name(ret));
        ota_manager_set_state(OTA_STATE_IDLE);
        return ret;
    }

    if (info.level == OTA_UPDATE_NONE) {
        ESP_LOGI(TAG, "No updates available");
        ota_manager_set_state(OTA_STATE_IDLE);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Update available: %s (level: %d)",
             info.version, info.level);

    /* 保存更新信息 */
    ota_manager_save_update_info(&info);

    /* 发布事件 */
    robot_event_t event = {
        .id = EVENT_OTA_PROGRESS,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };
    event_bus_publish(&event);

    ota_manager_set_state(OTA_STATE_IDLE);
    return ESP_OK;
}

static esp_err_t execute_upgrade(const ota_update_info_t *info)
{
    ESP_LOGI(TAG, "Starting upgrade to version %s", info->version);

    /* 检查电量 */
    if (s_battery_check_callback != NULL) {
        uint8_t battery_percent = 0;
        if (s_battery_check_callback(&battery_percent)) {
            if (battery_percent < OTA_MIN_BATTERY_PERCENT) {
                ESP_LOGW(TAG, "Battery too low: %d%%", battery_percent);
                ota_manager_set_error(OTA_ERR_LOW_BATTERY, "Battery too low for OTA");
                return ESP_ERR_LOW_BATTERY;
            }
        }
    }

    /* 保存更新信息 */
    ota_manager_save_update_info(info);

    /* 获取空闲分区 */
    const esp_partition_t *target = ota_partition_get_idle();
    if (target == NULL) {
        ESP_LOGE(TAG, "No idle OTA partition available");
        ota_manager_set_error(OTA_ERR_NO_IDLE_PARTITION, "No idle partition");
        ota_manager_set_state(OTA_STATE_ERROR);
        return ESP_ERR_NO_IDLE_PARTITION;
    }

    ESP_LOGI(TAG, "Target partition: %s (0x%lx)",
             target->label, (unsigned long)target->address);

    /* 保存目标分区 */
    s_upgrade_ctx.target_partition = target;
    s_upgrade_ctx.start_time = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    /* 下载固件 */
    ota_manager_set_state(OTA_STATE_DOWNLOADING);

    esp_err_t ret = ota_download_firmware(info->firmware_url, target, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Download failed: %s", esp_err_to_name(ret));
        ota_manager_set_error(OTA_ERR_DOWNLOAD_INCOMPLETE, "Download failed");
        ota_manager_set_state(OTA_STATE_ERROR);
        return ret;
    }

    /* 下载签名文件 */
    if (info->signature_url[0] != '\0') {
        s_upgrade_ctx.signature_len = sizeof(s_upgrade_ctx.signature);
        ret = ota_download_signature(info->signature_url,
                                     s_upgrade_ctx.signature,
                                     &s_upgrade_ctx.signature_len);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Signature download failed: %s", esp_err_to_name(ret));
            /* 非致命错误，继续 */
        }
    }

    /* 验证 */
    ota_manager_set_state(OTA_STATE_VERIFYING);

    /* 验证 SHA256 */
    if (info->sha256[0] != '\0') {
        ESP_LOGI(TAG, "Verifying SHA256...");
        ret = ota_verify_sha256(target, info->firmware_size, info->sha256);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SHA256 verification failed");
            ota_manager_set_error(OTA_ERR_SHA256_MISMATCH, "SHA256 mismatch");
            ota_manager_set_state(OTA_STATE_ERROR);
            return ret;
        }
        ESP_LOGI(TAG, "SHA256 verification passed");
    }

    /* 验证 RSA 签名 */
    if (s_upgrade_ctx.signature_len > 0) {
        ESP_LOGI(TAG, "Verifying RSA signature...");
        ret = ota_verify_rsa_signature(target, info->firmware_size,
                                       s_upgrade_ctx.signature,
                                       s_upgrade_ctx.signature_len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "RSA signature verification failed");
            ota_manager_set_error(OTA_ERR_RSA_VERIFY_FAILED, "Signature verification failed");
            ota_manager_set_state(OTA_STATE_ERROR);
            return ret;
        }
        ESP_LOGI(TAG, "RSA signature verification passed");
    }

    /* 验证完成，准备应用 */
    ota_manager_set_state(OTA_STATE_READY);

    ESP_LOGI(TAG, "Upgrade ready. Call ota_service_apply() to apply.");

    /* 自动应用（如果配置） */
    /* ota_service_apply(); */

    return ESP_OK;
}

static esp_err_t execute_rollback(void)
{
    ESP_LOGI(TAG, "Executing rollback...");

    ota_manager_set_state(OTA_STATE_ROLLING_BACK);

    esp_err_t ret = ota_rollback_manual(ROLLBACK_REASON_USER_REQUEST);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Rollback failed: %s", esp_err_to_name(ret));
        ota_manager_set_state(OTA_STATE_ERROR);
        return ret;
    }

    return ESP_OK;
}

static esp_err_t execute_factory_reset(void)
{
    ESP_LOGI(TAG, "Executing factory reset...");

    ota_manager_set_state(OTA_STATE_FACTORY_RESET);

    esp_err_t ret = ota_rollback_to_factory();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Factory reset failed: %s", esp_err_to_name(ret));
        ota_manager_set_state(OTA_STATE_ERROR);
        return ret;
    }

    return ESP_OK;
}

static void handle_ota_mqtt_command(const char *payload_json)
{
    ESP_LOGI(TAG, "Parsing MQTT OTA command: %s", payload_json);

    /* 解析 JSON */
    cJSON *root = cJSON_Parse(payload_json);
    if (root == NULL) {
        ESP_LOGE(TAG, "Invalid JSON");
        return;
    }

    /* 获取命令类型 */
    cJSON *cmd_item = cJSON_GetObjectItem(root, "cmd");
    if (cmd_item == NULL || !cJSON_IsString(cmd_item)) {
        ESP_LOGE(TAG, "Missing 'cmd' field");
        cJSON_Delete(root);
        return;
    }

    const char *cmd_str = cmd_item->valuestring;
    ESP_LOGI(TAG, "OTA command: %s", cmd_str);

    ota_cmd_msg_t cmd = {0};

    if (strcmp(cmd_str, "check") == 0) {
        cmd.type = OTA_CMD_CHECK;
    } else if (strcmp(cmd_str, "upgrade") == 0) {
        cmd.type = OTA_CMD_UPGRADE;

        /* 解析升级参数 */
        cJSON *url_item = cJSON_GetObjectItem(root, "url");
        if (url_item && cJSON_IsString(url_item)) {
            strncpy(cmd.info.firmware_url, url_item->valuestring,
                    sizeof(cmd.info.firmware_url) - 1);
        }

        cJSON *sig_url_item = cJSON_GetObjectItem(root, "signature_url");
        if (sig_url_item && cJSON_IsString(sig_url_item)) {
            strncpy(cmd.info.signature_url, sig_url_item->valuestring,
                    sizeof(cmd.info.signature_url) - 1);
        }

        cJSON *ver_item = cJSON_GetObjectItem(root, "version");
        if (ver_item && cJSON_IsString(ver_item)) {
            strncpy(cmd.info.version, ver_item->valuestring,
                    sizeof(cmd.info.version) - 1);
        }

        cJSON *sha_item = cJSON_GetObjectItem(root, "sha256");
        if (sha_item && cJSON_IsString(sha_item)) {
            strncpy(cmd.info.sha256, sha_item->valuestring,
                    sizeof(cmd.info.sha256) - 1);
        }

        cJSON *size_item = cJSON_GetObjectItem(root, "size");
        if (size_item && cJSON_IsNumber(size_item)) {
            cmd.info.firmware_size = (size_t)size_item->valueint;
        }
    } else if (strcmp(cmd_str, "rollback") == 0) {
        cmd.type = OTA_CMD_ROLLBACK;
    } else if (strcmp(cmd_str, "factory_reset") == 0) {
        cmd.type = OTA_CMD_FACTORY_RESET;
    } else {
        ESP_LOGW(TAG, "Unknown OTA command: %s", cmd_str);
        cJSON_Delete(root);
        return;
    }

    cJSON_Delete(root);

    /* 发送命令到队列 */
    if (xQueueSend(s_cmd_queue, &cmd, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGW(TAG, "Failed to queue OTA command");
    }
}

/* ============================================================
 * 事件处理器
 * ============================================================ */

static void on_wifi_connected(const robot_event_t *event)
{
    ESP_LOGI(TAG, "WiFi connected - OTA service ready");
}

static void on_mqtt_command(const robot_event_t *event)
{
    /* 检查是否是 OTA 命令 */
    if (event->payload != NULL && event->payload_len > 0) {
        mqtt_message_event_t *msg = (mqtt_message_event_t *)event->payload;

        /* 检查 topic 是否包含 "ota" */
        if (strstr(msg->topic, "ota") != NULL) {
            handle_ota_mqtt_command(msg->payload);
        }
    }
}
