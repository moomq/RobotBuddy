/**
 * @file ota_partition.c
 * @brief OTA 分区管理实现
 *
 * @copyright Copyright (c) 2026 RobotBuddy
 * @author RobotBuddy Team
 * @date 2026-07-19
 */

#include "ota_partition.h"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"

#include <string.h>

/* ============================================================
 * 日志标签
 * ============================================================ */

static const char *TAG = OTA_LOG_TAG_PARTITION;

/* ============================================================
 * 模块状态
 * ============================================================ */

static bool s_initialized = false;

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

esp_err_t ota_partition_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    /* 验证 OTA 分区存在 */
    const esp_partition_t *ota_0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                                            ESP_PARTITION_SUBTYPE_APP_OTA_0,
                                                            NULL);
    const esp_partition_t *ota_1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                                            ESP_PARTITION_SUBTYPE_APP_OTA_1,
                                                            NULL);
    const esp_partition_t *factory = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                                              ESP_PARTITION_SUBTYPE_APP_FACTORY,
                                                              NULL);

    if (ota_0 == NULL && ota_1 == NULL) {
        ESP_LOGE(TAG, "No OTA partitions found!");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "OTA partitions found:");
    if (ota_0) {
        ESP_LOGI(TAG, "  ota_0: 0x%lx (%lu KB)",
                 (unsigned long)ota_0->address,
                 (unsigned long)(ota_0->size / 1024));
    }
    if (ota_1) {
        ESP_LOGI(TAG, "  ota_1: 0x%lx (%lu KB)",
                 (unsigned long)ota_1->address,
                 (unsigned long)(ota_1->size / 1024));
    }
    if (factory) {
        ESP_LOGI(TAG, "  factory: 0x%lx (%lu KB)",
                 (unsigned long)factory->address,
                 (unsigned long)(factory->size / 1024));
    }

    s_initialized = true;
    return ESP_OK;
}

esp_err_t ota_partition_deinit(void)
{
    s_initialized = false;
    return ESP_OK;
}

const esp_partition_t* ota_partition_get_idle(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL) {
        ESP_LOGE(TAG, "Failed to get running partition");
        return NULL;
    }

    const esp_partition_t *ota_0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                                            ESP_PARTITION_SUBTYPE_APP_OTA_0,
                                                            NULL);
    const esp_partition_t *ota_1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                                            ESP_PARTITION_SUBTYPE_APP_OTA_1,
                                                            NULL);

    /* 返回非当前运行分区 */
    if (ota_0 != NULL && running != ota_0) {
        ESP_LOGD(TAG, "Idle partition: ota_0");
        return ota_0;
    }

    if (ota_1 != NULL && running != ota_1) {
        ESP_LOGD(TAG, "Idle partition: ota_1");
        return ota_1;
    }

    /* 如果都在运行（不太可能），返回 ota_1 */
    if (ota_1 != NULL) {
        ESP_LOGW(TAG, "Returning ota_1 as idle (may be same as running)");
        return ota_1;
    }

    ESP_LOGW(TAG, "No idle OTA partition available");
    return NULL;
}

const esp_partition_t* ota_partition_get_running(void)
{
    return esp_ota_get_running_partition();
}

const esp_partition_t* ota_partition_get_by_label(const char *label)
{
    if (label == NULL) {
        return NULL;
    }

    if (strcmp(label, OTA_PARTITION_FACTORY) == 0) {
        return esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                        ESP_PARTITION_SUBTYPE_APP_FACTORY,
                                        NULL);
    } else if (strcmp(label, OTA_PARTITION_OTA_0) == 0) {
        return esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                        ESP_PARTITION_SUBTYPE_APP_OTA_0,
                                        NULL);
    } else if (strcmp(label, OTA_PARTITION_OTA_1) == 0) {
        return esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                        ESP_PARTITION_SUBTYPE_APP_OTA_1,
                                        NULL);
    }

    /* 尝试通用查找 */
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                    ESP_PARTITION_SUBTYPE_ANY,
                                    label);
}

const esp_partition_t* ota_partition_get_factory(void)
{
    return esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                    ESP_PARTITION_SUBTYPE_APP_FACTORY,
                                    NULL);
}

const esp_partition_t* ota_partition_get_next(const esp_partition_t *current)
{
    return esp_ota_get_next_update_partition(current);
}

int ota_partition_get_valid_count(void)
{
    int count = 0;

    /* 检查 factory */
    const esp_partition_t *factory = ota_partition_get_factory();
    if (factory != NULL && ota_partition_is_valid(factory)) {
        count++;
    }

    /* 检查 ota_0 */
    const esp_partition_t *ota_0 = ota_partition_get_by_label(OTA_PARTITION_OTA_0);
    if (ota_0 != NULL && ota_partition_is_valid(ota_0)) {
        count++;
    }

    /* 检查 ota_1 */
    const esp_partition_t *ota_1 = ota_partition_get_by_label(OTA_PARTITION_OTA_1);
    if (ota_1 != NULL && ota_partition_is_valid(ota_1)) {
        count++;
    }

    return count;
}

bool ota_partition_is_valid(const esp_partition_t *partition)
{
    if (partition == NULL) {
        return false;
    }

    /* 检查分区状态 */
    esp_ota_img_states_t state = ota_partition_get_state(partition);

    switch (state) {
        case ESP_OTA_IMG_VALID:
        case ESP_OTA_IMG_UNDEFINED:
            /* 未定义状态也可能是有效的（factory 分区通常是这个状态） */
            return true;
        case ESP_OTA_IMG_NEW:
            /* 新镜像，尚未验证 */
            return true;
        default:
            /* INVALID, ABORTED 等 */
            return false;
    }
}

esp_ota_img_states_t ota_partition_get_state(const esp_partition_t *partition)
{
    if (partition == NULL) {
        return ESP_OTA_IMG_INVALID;
    }

    /* 获取引导状态 */
    esp_ota_img_states_t state = ESP_OTA_IMG_INVALID;
    const esp_partition_t *running = esp_ota_get_running_partition();

    /* 对于运行分区，直接获取状态 */
    if (running != NULL && partition->address == running->address) {
        esp_err_t ret = esp_ota_get_state_partition(&state);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to get partition state: %s", esp_err_to_name(ret));
            return ESP_OTA_IMG_UNDEFINED;
        }
        return state;
    }

    /* 对于其他分区，假设是有效的（需要更复杂的检查逻辑） */
    return ESP_OTA_IMG_UNDEFINED;
}

bool ota_partition_is_running_ota(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL) {
        return false;
    }

    /* 检查是否为 OTA 分区 */
    return (running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 ||
            running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1);
}

bool ota_partition_is_running_factory(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL) {
        return false;
    }

    return (running->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY);
}

esp_err_t ota_partition_mark_bootable(const esp_partition_t *partition)
{
    if (partition == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_ota_set_boot_partition(partition);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set boot partition: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Boot partition set to %s", partition->label);
    return ESP_OK;
}

esp_err_t ota_partition_switch_and_reboot(const esp_partition_t *partition)
{
    if (partition == NULL) {
        ESP_LOGE(TAG, "Invalid partition");
        /* 不会真正返回，但需要返回值 */
        esp_restart();
    }

    esp_err_t ret = ota_partition_mark_bootable(partition);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mark bootable partition");
        esp_restart();
    }

    ESP_LOGI(TAG, "Switching to partition %s and rebooting...", partition->label);
    esp_restart();

    /* 永不返回 */
}

esp_err_t ota_partition_mark_valid(void)
{
    esp_err_t ret = esp_ota_mark_app_valid_cancel_rollback();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mark app valid: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Current app marked as valid");
    return ESP_OK;
}

esp_err_t ota_partition_mark_invalid_and_rollback(void)
{
    ESP_LOGW(TAG, "Marking app invalid and rolling back...");

    /* 此函数不会返回 */
    esp_ota_mark_app_invalid_rollback_and_reboot();

    /* 永不返回 */
}

esp_err_t ota_partition_begin_write(const esp_partition_t *partition,
                                     size_t image_size,
                                     esp_ota_handle_t *out_handle)
{
    if (partition == NULL || out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t ota_size = (image_size > 0) ? image_size : OTA_SIZE_UNKNOWN;

    esp_err_t ret = esp_ota_begin(partition, ota_size, out_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "OTA write started (partition: %s)", partition->label);
    return ESP_OK;
}

esp_err_t ota_partition_write(esp_ota_handle_t handle,
                               const void *data,
                               size_t size)
{
    if (data == NULL || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_ota_write(handle, data, size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

esp_err_t ota_partition_end_write(esp_ota_handle_t handle)
{
    esp_err_t ret = esp_ota_end(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(ret));
        if (ret == ESP_ERR_OTA_VALIDATE) {
            ESP_LOGE(TAG, "OTA image validation failed - image may be corrupted");
        }
        return ret;
    }

    ESP_LOGI(TAG, "OTA write completed successfully");
    return ESP_OK;
}

esp_err_t ota_partition_abort_write(esp_ota_handle_t handle)
{
    esp_err_t ret = esp_ota_abort(handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "esp_ota_abort failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "OTA write aborted");
    return ESP_OK;
}

const char* ota_partition_get_label(const esp_partition_t *partition)
{
    if (partition == NULL) {
        return NULL;
    }
    return partition->label;
}

size_t ota_partition_get_size(const esp_partition_t *partition)
{
    if (partition == NULL) {
        return 0;
    }
    return partition->size;
}

size_t ota_partition_get_address(const esp_partition_t *partition)
{
    if (partition == NULL) {
        return 0;
    }
    return partition->address;
}

void ota_partition_print_info(void)
{
    ESP_LOGI(TAG, "=== Partition Information ===");

    /* 打印运行分区 */
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running != NULL) {
        ESP_LOGI(TAG, "Running: %s at 0x%lx (%lu KB)",
                 running->label,
                 (unsigned long)running->address,
                 (unsigned long)(running->size / 1024));
    }

    /* 打印 OTA 分区 */
    const esp_partition_t *ota_0 = ota_partition_get_by_label(OTA_PARTITION_OTA_0);
    if (ota_0 != NULL) {
        const char *status = ota_partition_is_valid(ota_0) ? "valid" : "invalid";
        ESP_LOGI(TAG, "ota_0: 0x%lx (%lu KB) [%s]",
                 (unsigned long)ota_0->address,
                 (unsigned long)(ota_0->size / 1024),
                 status);
    }

    const esp_partition_t *ota_1 = ota_partition_get_by_label(OTA_PARTITION_OTA_1);
    if (ota_1 != NULL) {
        const char *status = ota_partition_is_valid(ota_1) ? "valid" : "invalid";
        ESP_LOGI(TAG, "ota_1: 0x%lx (%lu KB) [%s]",
                 (unsigned long)ota_1->address,
                 (unsigned long)(ota_1->size / 1024),
                 status);
    }

    /* 打印 factory 分区 */
    const esp_partition_t *factory = ota_partition_get_factory();
    if (factory != NULL) {
        ESP_LOGI(TAG, "factory: 0x%lx (%lu KB)",
                 (unsigned long)factory->address,
                 (unsigned long)(factory->size / 1024));
    }

    ESP_LOGI(TAG, "Valid partitions: %d", ota_partition_get_valid_count());
    ESP_LOGI(TAG, "=============================");
}
