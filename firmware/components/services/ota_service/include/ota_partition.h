/**
 * @file ota_partition.h
 * @brief OTA 分区管理接口
 *
 * 提供分区查询、切换和标记功能。
 *
 * @copyright Copyright (c) 2026 RobotBuddy
 * @author RobotBuddy Team
 * @date 2026-07-19
 */

#pragma once

#include "esp_err.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "ota_types.h"
#include "ota_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup OTA_Partition OTA 分区管理
 * @{
 */

/* ============================================================
 * 初始化
 * ============================================================ */

/**
 * @brief 初始化分区管理模块
 *
 * 检查分区表完整性，验证 OTA 分区存在。
 *
 * @return ESP_OK 成功
 *         ESP_ERR_NOT_FOUND 分区不存在
 */
esp_err_t ota_partition_init(void);

/**
 * @brief 反初始化分区管理模块
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_partition_deinit(void);

/* ============================================================
 * 分区查询
 * ============================================================ */

/**
 * @brief 获取空闲的 OTA 分区
 *
 * 返回非当前运行分区的 OTA 分区。
 *
 * @return 空闲分区指针（ota_0 或 ota_1），NULL 表示无空闲分区
 */
const esp_partition_t* ota_partition_get_idle(void);

/**
 * @brief 获取当前运行的分区
 *
 * @return 当前分区指针，NULL 表示获取失败
 */
const esp_partition_t* ota_partition_get_running(void);

/**
 * @brief 获取指定标签的分区
 *
 * @param[in] label 分区标签（如 "ota_0", "ota_1", "factory"）
 * @return 分区指针，NULL 表示不存在
 */
const esp_partition_t* ota_partition_get_by_label(const char *label);

/**
 * @brief 获取 factory 分区
 *
 * @return factory 分区指针，NULL 表示不存在
 */
const esp_partition_t* ota_partition_get_factory(void);

/**
 * @brief 获取下一个 OTA 分区
 *
 * 按 ota_0 -> ota_1 顺序获取下一个 OTA 分区。
 *
 * @param[in] current 当前分区（NULL 表示从第一个开始）
 * @return 下一个 OTA 分区，NULL 表示无更多分区
 */
const esp_partition_t* ota_partition_get_next(const esp_partition_t *current);

/* ============================================================
 * 分区状态
 * ============================================================ */

/**
 * @brief 获取所有有效分区数量
 *
 * @return 有效分区数量
 */
int ota_partition_get_valid_count(void);

/**
 * @brief 检查分区是否有效
 *
 * @param[in] partition 目标分区
 * @return true 有效
 *         false 无效或损坏
 */
bool ota_partition_is_valid(const esp_partition_t *partition);

/**
 * @brief 获取分区状态
 *
 * @param[in] partition 目标分区
 * @return 分区状态
 */
esp_ota_img_states_t ota_partition_get_state(const esp_partition_t *partition);

/**
 * @brief 检查是否运行在 OTA 分区
 *
 * @return true 运行在 OTA 分区
 *         false 运行在 factory 分区
 */
bool ota_partition_is_running_ota(void);

/**
 * @brief 检查是否运行在 factory 分区
 *
 * @return true 运行在 factory 分区
 */
bool ota_partition_is_running_factory(void);

/* ============================================================
 * 分区操作
 * ============================================================ */

/**
 * @brief 标记启动分区
 *
 * 设置指定分区为下次启动分区。
 *
 * @param[in] partition 目标分区
 * @return ESP_OK 成功
 *         ESP_ERR_INVALID_ARG 参数无效
 *         ESP_ERR_OTA_VALIDATE 分区无效
 */
esp_err_t ota_partition_mark_bootable(const esp_partition_t *partition);

/**
 * @brief 切换到指定分区并重启
 *
 * 设置启动分区并立即重启。
 *
 * @param[in] partition 目标分区
 * @return ESP_OK 成功（不会返回）
 *         ESP_FAIL 失败
 */
esp_err_t ota_partition_switch_and_reboot(const esp_partition_t *partition) __attribute__((noreturn));

/**
 * @brief 标记当前分区为有效
 *
 * 取消回滚保护，确认当前分区运行正常。
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_partition_mark_valid(void);

/**
 * @brief 标记当前分区为无效并回滚
 *
 * 标记当前分区无效，重启到上一个有效分区。
 *
 * @return ESP_OK 成功（不会返回）
 */
esp_err_t ota_partition_mark_invalid_and_rollback(void) __attribute__((noreturn));

/* ============================================================
 * 分区写入
 * ============================================================ */

/**
 * @brief 开始 OTA 写入
 *
 * @param[in] partition 目标分区
 * @param[in] image_size 镜像大小（OTA_SIZE_UNKNOWN 表示未知）
 * @param[out] out_handle OTA 句柄
 * @return ESP_OK 成功
 */
esp_err_t ota_partition_begin_write(const esp_partition_t *partition,
                                     size_t image_size,
                                     esp_ota_handle_t *out_handle);

/**
 * @brief 写入数据到 OTA 分区
 *
 * @param[in] handle OTA 句柄
 * @param[in] data 数据指针
 * @param[in] size 数据大小
 * @return ESP_OK 成功
 */
esp_err_t ota_partition_write(esp_ota_handle_t handle,
                               const void *data,
                               size_t size);

/**
 * @brief 结束 OTA 写入
 *
 * @param[in] handle OTA 句柄
 * @return ESP_OK 成功
 */
esp_err_t ota_partition_end_write(esp_ota_handle_t handle);

/**
 * @brief 中止 OTA 写入
 *
 * @param[in] handle OTA 句柄
 * @return ESP_OK 成功
 */
esp_err_t ota_partition_abort_write(esp_ota_handle_t handle);

/* ============================================================
 * 分区信息
 * ============================================================ */

/**
 * @brief 获取分区标签
 *
 * @param[in] partition 目标分区
 * @return 分区标签字符串，NULL 表示无效
 */
const char* ota_partition_get_label(const esp_partition_t *partition);

/**
 * @brief 获取分区大小
 *
 * @param[in] partition 目标分区
 * @return 分区大小（字节）
 */
size_t ota_partition_get_size(const esp_partition_t *partition);

/**
 * @brief 获取分区地址
 *
 * @param[in] partition 目标分区
 * @return 分区起始地址
 */
size_t ota_partition_get_address(const esp_partition_t *partition);

/**
 * @brief 打印分区信息（调试用）
 *
 * 打印所有 OTA 相关分区的信息。
 */
void ota_partition_print_info(void);

/** @} */

#ifdef __cplusplus
}
#endif
