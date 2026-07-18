/**
 * @file ota_rollback.h
 * @brief OTA 回滚管理接口
 *
 * 提供自动回滚、健康检查和回滚计数管理功能。
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
 * @defgroup OTA_Rollback OTA 回滚管理
 * @{
 */

/* ============================================================
 * 初始化
 * ============================================================ */

/**
 * @brief 初始化回滚模块
 *
 * 加载回滚计数，检查是否需要自动回滚。
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_rollback_init(void);

/**
 * @brief 反初始化回滚模块
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_rollback_deinit(void);

/* ============================================================
 * 健康检查
 * ============================================================ */

/**
 * @brief 执行健康检查
 *
 * 检查项：
 * - WiFi 连接状态
 * - MQTT 连接状态
 * - 显示功能
 * - 传感器通信
 *
 * @return 健康检查结果
 */
health_check_result_t ota_rollback_health_check(void);

/**
 * @brief 启动健康检查定时器
 *
 * 在新固件启动后调用，启动 30 秒健康检查窗口。
 * 如果超时未通过检查，自动回滚。
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_rollback_start_health_check_timer(void);

/**
 * @brief 停止健康检查定时器
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_rollback_stop_health_check_timer(void);

/**
 * @brief 报告健康检查通过
 *
 * 由应用程序在健康检查通过后调用。
 * 标记当前分区为有效。
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_rollback_report_health_check_pass(void);

/**
 * @brief 报告健康检查失败
 *
 * @param[in] result 失败原因
 * @return ESP_OK 成功
 */
esp_err_t ota_rollback_report_health_check_fail(health_check_result_t result);

/**
 * @brief 获取健康检查截止时间
 *
 * @return 截止时间戳（毫秒），0 表示未启动
 */
uint32_t ota_rollback_get_health_check_deadline(void);

/**
 * @brief 检查健康检查是否超时
 *
 * @return true 已超时
 *         false 未超时或未启动
 */
bool ota_rollback_is_health_check_timeout(void);

/* ============================================================
 * 自动回滚检查
 * ============================================================ */

/**
 * @brief 检查是否需要自动回滚
 *
 * 检查条件：
 * - watchdog 重启次数 >= 3
 * - panic 重启次数 >= 5
 * - 健康检查超时
 *
 * @return true 需要回滚
 *         false 不需要回滚
 */
bool ota_rollback_check_needed(void);

/**
 * @brief 获取回滚原因
 *
 * @return 回滚原因
 */
rollback_reason_t ota_rollback_get_reason(void);

/* ============================================================
 * 回滚计数
 * ============================================================ */

/**
 * @brief 增加 watchdog 计数
 *
 * 在每次 watchdog 重启时调用。
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_rollback_increment_watchdog_count(void);

/**
 * @brief 增加 panic 计数
 *
 * 在每次 panic 重启时调用。
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_rollback_increment_panic_count(void);

/**
 * @brief 获取 watchdog 计数
 *
 * @return watchdog 计数
 */
uint8_t ota_rollback_get_watchdog_count(void);

/**
 * @brief 获取 panic 计数
 *
 * @return panic 计数
 */
uint8_t ota_rollback_get_panic_count(void);

/**
 * @brief 清零回滚计数
 *
 * 在健康检查通过后调用。
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_rollback_reset_counts(void);

/* ============================================================
 * 回滚操作
 * ============================================================ */

/**
 * @brief 执行自动回滚
 *
 * 回滚到上一个有效分区。
 * 如果两个 OTA 分区都无效，回退到 factory。
 *
 * @return ESP_OK 成功
 *         ESP_ERR_NO_VALID_PARTITION 无有效分区
 */
esp_err_t ota_rollback_auto(void);

/**
 * @brief 手动回滚到上一版本
 *
 * @param[in] reason 回滚原因
 * @return ESP_OK 成功
 *         ESP_ERR_ROLLBACK_FAILED 回滚失败
 */
esp_err_t ota_rollback_manual(rollback_reason_t reason);

/**
 * @brief 回退到出厂固件
 *
 * @return ESP_OK 成功
 *         ESP_FAIL 失败
 */
esp_err_t ota_rollback_to_factory(void);

/* ============================================================
 * 启动状态
 * ============================================================ */

/**
 * @brief 更新启动时间戳
 *
 * 在每次启动时调用。
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_rollback_update_boot_timestamp(void);

/**
 * @brief 获取启动状态
 *
 * @param[out] state 启动状态
 * @return ESP_OK 成功
 */
esp_err_t ota_rollback_get_boot_state(ota_boot_state_t *state);

/**
 * @brief 检查是否为新固件首次启动
 *
 * @return true 新固件首次启动
 */
bool ota_rollback_is_first_boot(void);

/**
 * @brief 标记启动验证完成
 *
 * 在健康检查通过并提交后调用。
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_rollback_mark_boot_verified(void);

/* ============================================================
 * 回调
 * ============================================================ */

/**
 * @brief 注册健康检查回调
 *
 * @param[in] callback 回调函数
 * @return ESP_OK 成功
 */
esp_err_t ota_rollback_register_health_check_callback(health_check_result_t (*callback)(void));

/** @} */

#ifdef __cplusplus
}
#endif
