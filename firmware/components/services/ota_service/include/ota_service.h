/**
 * @file ota_service.h
 * @brief OTA 升级服务主接口
 *
 * 提供完整的 OTA 升级功能，包括版本检查、固件下载、签名验证、
 * 分区管理和自动回滚。
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
 * @defgroup OTA_Service OTA 服务接口
 * @{
 */

/* ============================================================
 * 初始化与反初始化
 * ============================================================ */

/**
 * @brief 初始化 OTA 服务
 *
 * 初始化 OTA 管理器、分区管理器和安全模块。
 * 创建 OTA 服务任务，准备接收升级命令。
 *
 * @return ESP_OK 成功
 *         ESP_ERR_NO_MEM 内存不足
 *         ESP_FAIL 初始化失败
 */
esp_err_t ota_service_init(void);

/**
 * @brief 反初始化 OTA 服务
 *
 * 停止 OTA 服务任务，释放资源。
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_service_deinit(void);

/**
 * @brief 检查 OTA 服务是否已初始化
 *
 * @return true 已初始化
 *         false 未初始化
 */
bool ota_service_is_initialized(void);

/* ============================================================
 * 版本检查
 * ============================================================ */

/**
 * @brief 检查是否有新版本
 *
 * 向服务器查询是否有新固件版本可用。
 *
 * @param[out] info 更新信息（如果有新版本）
 * @return ESP_OK 检查成功
 *         ESP_ERR_INVALID_ARG 参数无效
 *         ESP_ERR_WIFI_DISCONNECT WiFi 未连接
 *         ESP_FAIL 检查失败
 */
esp_err_t ota_service_check_update(ota_update_info_t *info);

/* ============================================================
 * 升级操作
 * ============================================================ */

/**
 * @brief 开始 OTA 升级
 *
 * 下载固件、验证签名、刷写到备用分区。
 * 支持断点续传。
 *
 * @param[in] info 更新信息
 * @return ESP_OK 开始成功
 *         ESP_ERR_INVALID_ARG 参数无效
 *         ESP_ERR_INVALID_STATE OTA 已在进行中
 *         ESP_ERR_LOW_BATTERY 电量不足
 *         ESP_FAIL 启动失败
 */
esp_err_t ota_service_start_upgrade(const ota_update_info_t *info);

/**
 * @brief 取消 OTA 升级
 *
 * 取消正在进行的升级操作。
 *
 * @return ESP_OK 取消成功
 *         ESP_ERR_INVALID_STATE OTA 未在进行中
 */
esp_err_t ota_service_cancel(void);

/**
 * @brief 应用升级
 *
 * 标记新分区为启动分区，准备重启。
 *
 * @return ESP_OK 成功
 *         ESP_FAIL 失败
 */
esp_err_t ota_service_apply(void);

/**
 * @brief 确认升级成功
 *
 * 新固件启动后健康检查通过，确认升级。
 * 标记新分区为有效，防止回滚。
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_service_commit(void);

/* ============================================================
 * 状态查询
 * ============================================================ */

/**
 * @brief 获取 OTA 当前状态
 *
 * @return OTA 状态
 */
ota_state_t ota_service_get_state(void);

/**
 * @brief 获取 OTA 进度
 *
 * @param[out] progress 进度信息
 * @return ESP_OK 成功
 *         ESP_ERR_INVALID_ARG 参数无效
 */
esp_err_t ota_service_get_progress(ota_progress_t *progress);

/**
 * @brief 获取 OTA 错误信息
 *
 * @param[out] error 错误信息
 * @return ESP_OK 成功
 *         ESP_ERR_NOT_FOUND 无错误
 */
esp_err_t ota_service_get_error(ota_error_t *error);

/**
 * @brief 获取当前运行分区信息
 *
 * @param[out] label 分区标签（如 "ota_0"）
 * @param[in] label_size 标签缓冲区大小
 * @return ESP_OK 成功
 */
esp_err_t ota_service_get_running_partition(char *label, size_t label_size);

/* ============================================================
 * 回滚操作
 * ============================================================ */

/**
 * @brief 手动触发回滚
 *
 * 回滚到上一个正常固件版本。
 *
 * @return ESP_OK 成功
 *         ESP_FAIL 失败
 */
esp_err_t ota_service_rollback(void);

/**
 * @brief 回退到出厂固件
 *
 * 回退到 factory 分区的出厂固件。
 *
 * @return ESP_OK 成功
 *         ESP_FAIL 失败
 */
esp_err_t ota_service_factory_reset(void);

/* ============================================================
 * 回调注册
 * ============================================================ */

/**
 * @brief 注册状态变化回调
 *
 * @param[in] cb 回调函数
 * @return ESP_OK 成功
 *         ESP_ERR_NO_MEM 已达最大回调数
 */
esp_err_t ota_service_register_state_callback(ota_state_callback_t cb);

/**
 * @brief 注册进度回调
 *
 * @param[in] cb 回调函数
 * @return ESP_OK 成功
 *         ESP_ERR_NO_MEM 已达最大回调数
 */
esp_err_t ota_service_register_progress_callback(ota_progress_callback_t cb);

/**
 * @brief 注册结果回调
 *
 * @param[in] cb 回调函数
 * @return ESP_OK 成功
 *         ESP_ERR_NO_MEM 已达最大回调数
 */
esp_err_t ota_service_register_result_callback(ota_result_callback_t cb);

/* ============================================================
 * MQTT 命令处理
 * ============================================================ */

/**
 * @brief 处理 MQTT OTA 命令
 *
 * 解析并执行来自 MQTT 的 OTA 命令。
 *
 * @param[in] payload_json JSON 格式的命令载荷
 *
 * 命令格式:
 * {"cmd":"check"} - 检查更新
 * {"cmd":"upgrade","url":"...","version":"...","sha256":"..."} - 开始升级
 * {"cmd":"rollback"} - 回滚
 * {"cmd":"factory_reset"} - 回退出厂固件
 */
void ota_service_handle_mqtt_command(const char *payload_json);

/** @} */

#ifdef __cplusplus
}
#endif
