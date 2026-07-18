/**
 * @file ota_manager.h
 * @brief OTA 状态管理接口
 *
 * 管理 OTA 状态机、进度追踪和 NVS 持久化。
 *
 * @copyright Copyright (c) 2026 RobotBuddy
 * @author RobotBuddy Team
 * @date 2026-07-19
 */

#pragma once

#include "esp_err.h"
#include "ota_types.h"
#include "ota_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup OTA_Manager OTA 状态管理
 * @{
 */

/* ============================================================
 * 初始化与反初始化
 * ============================================================ */

/**
 * @brief 初始化 OTA 管理器
 *
 * 创建互斥锁，从 NVS 加载保存的状态。
 *
 * @return ESP_OK 成功
 *         ESP_ERR_NO_MEM 内存不足
 */
esp_err_t ota_manager_init(void);

/**
 * @brief 反初始化 OTA 管理器
 *
 * 释放互斥锁，清理资源。
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_manager_deinit(void);

/* ============================================================
 * 状态管理
 * ============================================================ */

/**
 * @brief 设置 OTA 状态
 *
 * 线程安全地设置新状态，发布状态变化事件。
 *
 * @param[in] new_state 新状态
 * @return ESP_OK 成功
 */
esp_err_t ota_manager_set_state(ota_state_t new_state);

/**
 * @brief 获取 OTA 状态
 *
 * 线程安全地获取当前状态。
 *
 * @return 当前状态
 */
ota_state_t ota_manager_get_state(void);

/**
 * @brief 检查状态转换是否有效
 *
 * @param[in] from 当前状态
 * @param[in] to 目标状态
 * @return true 有效转换
 *         false 无效转换
 */
bool ota_manager_is_valid_transition(ota_state_t from, ota_state_t to);

/* ============================================================
 * 进度管理
 * ============================================================ */

/**
 * @brief 更新下载进度
 *
 * 线程安全地更新进度，触发进度回调。
 *
 * @param[in] bytes_downloaded 已下载字节数
 * @param[in] bytes_total 总字节数
 * @return ESP_OK 成功
 */
esp_err_t ota_manager_update_progress(size_t bytes_downloaded, size_t bytes_total);

/**
 * @brief 获取当前进度
 *
 * @param[out] progress 进度信息
 * @return ESP_OK 成功
 *         ESP_ERR_INVALID_ARG 参数无效
 */
esp_err_t ota_manager_get_progress(ota_progress_t *progress);

/**
 * @brief 保存进度到 NVS
 *
 * 用于断点续传，保存当前下载偏移量。
 *
 * @param[in] progress 进度信息
 * @return ESP_OK 成功
 */
esp_err_t ota_manager_save_progress(const ota_progress_t *progress);

/**
 * @brief 从 NVS 加载进度
 *
 * @param[out] progress 进度信息
 * @return ESP_OK 成功
 *         ESP_ERR_NOT_FOUND 无保存的进度
 */
esp_err_t ota_manager_load_progress(ota_progress_t *progress);

/**
 * @brief 清除保存的进度
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_manager_clear_progress(void);

/* ============================================================
 * 错误管理
 * ============================================================ */

/**
 * @brief 设置错误信息
 *
 * @param[in] code 错误码
 * @param[in] message 错误描述
 * @return ESP_OK 成功
 */
esp_err_t ota_manager_set_error(ota_error_code_t code, const char *message);

/**
 * @brief 获取错误信息
 *
 * @param[out] error 错误信息
 * @return ESP_OK 成功
 *         ESP_ERR_NOT_FOUND 无错误
 */
esp_err_t ota_manager_get_error(ota_error_t *error);

/**
 * @brief 清除错误信息
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_manager_clear_error(void);

/* ============================================================
 * 回滚计数管理
 * ============================================================ */

/**
 * @brief 保存回滚计数到 NVS
 *
 * @param[in] wd_count watchdog 计数
 * @param[in] panic_count panic 计数
 * @return ESP_OK 成功
 */
esp_err_t ota_manager_save_rollback_count(uint8_t wd_count, uint8_t panic_count);

/**
 * @brief 从 NVS 加载回滚计数
 *
 * @param[out] wd_count watchdog 计数
 * @param[out] panic_count panic 计数
 * @return ESP_OK 成功
 */
esp_err_t ota_manager_load_rollback_count(uint8_t *wd_count, uint8_t *panic_count);

/**
 * @brief 清零回滚计数
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_manager_reset_rollback_count(void);

/* ============================================================
 * 启动状态管理
 * ============================================================ */

/**
 * @brief 保存启动状态到 NVS
 *
 * @param[in] state 启动状态
 * @return ESP_OK 成功
 */
esp_err_t ota_manager_save_boot_state(const ota_boot_state_t *state);

/**
 * @brief 从 NVS 加载启动状态
 *
 * @param[out] state 启动状态
 * @return ESP_OK 成功
 *         ESP_ERR_NOT_FOUND 无保存的状态
 */
esp_err_t ota_manager_load_boot_state(ota_boot_state_t *state);

/* ============================================================
 * 更新信息管理
 * ============================================================ */

/**
 * @brief 保存更新信息
 *
 * 用于在升级过程中保存更新信息。
 *
 * @param[in] info 更新信息
 * @return ESP_OK 成功
 */
esp_err_t ota_manager_save_update_info(const ota_update_info_t *info);

/**
 * @brief 获取保存的更新信息
 *
 * @param[out] info 更新信息
 * @return ESP_OK 成功
 *         ESP_ERR_NOT_FOUND 无保存的信息
 */
esp_err_t ota_manager_get_update_info(ota_update_info_t *info);

/**
 * @brief 清除更新信息
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_manager_clear_update_info(void);

/* ============================================================
 * 回调管理
 * ============================================================ */

/**
 * @brief 触发状态回调
 *
 * 通知所有注册的状态回调函数。
 *
 * @param[in] old_state 旧状态
 * @param[in] new_state 新状态
 */
void ota_manager_invoke_state_callbacks(ota_state_t old_state, ota_state_t new_state);

/**
 * @brief 触发进度回调
 *
 * 通知所有注册的进度回调函数。
 *
 * @param[in] progress 进度信息
 */
void ota_manager_invoke_progress_callbacks(const ota_progress_t *progress);

/**
 * @brief 触发结果回调
 *
 * 通知所有注册的结果回调函数。
 *
 * @param[in] success 是否成功
 * @param[in] version 固件版本
 * @param[in] error 错误描述
 */
void ota_manager_invoke_result_callbacks(bool success, const char *version, const char *error);

/** @} */

#ifdef __cplusplus
}
#endif
