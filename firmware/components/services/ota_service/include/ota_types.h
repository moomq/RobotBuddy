/**
 * @file ota_types.h
 * @brief OTA 升级功能类型定义
 *
 * @copyright Copyright (c) 2026 RobotBuddy
 * @author RobotBuddy Team
 * @date 2026-07-19
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OTA 状态枚举
 */
typedef enum {
    OTA_STATE_IDLE,             ///< 空闲，等待升级命令
    OTA_STATE_CHECKING,         ///< 检查服务器是否有新版本
    OTA_STATE_DOWNLOADING,      ///< 下载固件包
    OTA_STATE_VERIFYING,        ///< RSA-2048 签名校验 + SHA256 校验
    OTA_STATE_READY,            ///< 校验通过，等待应用
    OTA_STATE_REBOOTING,        ///< 设置启动分区，准备重启
    OTA_STATE_VERIFYING_NEW,    ///< 重启后：新固件运行中，健康检查中
    OTA_STATE_COMMITTED,        ///< 健康检查通过 → 确认生效，标记为稳定
    OTA_STATE_ROLLING_BACK,     ///< 健康检查失败 → 回滚到上一个正常分区
    OTA_STATE_FACTORY_RESET,    ///< 回滚失败 / 所有分区损坏 → 回退 factory
    OTA_STATE_ERROR,            ///< 不可恢复错误
} ota_state_t;

/**
 * @brief OTA 更新级别
 */
typedef enum {
    OTA_UPDATE_NONE,            ///< 无新版本
    OTA_UPDATE_AVAILABLE,       ///< 有新版本可选升级
    OTA_UPDATE_MANDATORY,       ///< 强制升级（严重安全漏洞）
} ota_update_level_t;

/**
 * @brief OTA 命令类型
 */
typedef enum {
    OTA_CMD_CHECK,              ///< 检查更新
    OTA_CMD_UPGRADE,            ///< 立即升级
    OTA_CMD_ROLLBACK,           ///< 回滚到上一版本
    OTA_CMD_FACTORY_RESET,      ///< 回退到出厂固件
    OTA_CMD_CANCEL,             ///< 取消升级
} ota_cmd_type_t;

/**
 * @brief OTA 错误码
 */
typedef enum {
    OTA_ERR_NONE                  = 0x0000,  ///< 无错误

    // 网络错误 (0x0100-0x01FF)
    OTA_ERR_WIFI_DISCONNECT       = 0x0100,  ///< WiFi 断开
    OTA_ERR_HTTP_TIMEOUT          = 0x0101,  ///< HTTP 超时
    OTA_ERR_HTTP_ERROR            = 0x0102,  ///< HTTP 错误 (4xx/5xx)
    OTA_ERR_DNS_FAILED            = 0x0103,  ///< DNS 解析失败
    OTA_ERR_CONNECTION_REFUSED    = 0x0104,  ///< 连接被拒绝

    // 下载错误 (0x0200-0x02FF)
    OTA_ERR_DOWNLOAD_INCOMPLETE   = 0x0200,  ///< 下载不完整
    OTA_ERR_SIZE_MISMATCH         = 0x0201,  ///< 大小不匹配
    OTA_ERR_WRITE_FAILED          = 0x0202,  ///< Flash 写入失败
    OTA_ERR_NO_SPACE              = 0x0203,  ///< 分区空间不足

    // 验证错误 (0x0300-0x03FF)
    OTA_ERR_RSA_VERIFY_FAILED     = 0x0300,  ///< RSA 签名验证失败
    OTA_ERR_SHA256_MISMATCH       = 0x0301,  ///< SHA256 不匹配
    OTA_ERR_INVALID_SIGNATURE     = 0x0302,  ///< 无效签名

    // 分区错误 (0x0400-0x04FF)
    OTA_ERR_NO_IDLE_PARTITION     = 0x0400,  ///< 无空闲分区
    OTA_ERR_PARTITION_CORRUPT     = 0x0401,  ///< 分区损坏
    OTA_ERR_BOOT_SET_FAILED       = 0x0402,  ///< 启动分区设置失败

    // 回滚错误 (0x0500-0x05FF)
    OTA_ERR_ROLLBACK_FAILED       = 0x0500,  ///< 回滚失败
    OTA_ERR_NO_VALID_PARTITION    = 0x0501,  ///< 无有效分区

    // 系统错误 (0x0600-0x06FF)
    OTA_ERR_LOW_BATTERY           = 0x0600,  ///< 电量不足
    OTA_ERR_ALREADY_UPDATING      = 0x0601,  ///< 已在升级中
    OTA_ERR_USER_CANCEL           = 0x0602,  ///< 用户取消
    OTA_ERR_INTERNAL              = 0x0603,  ///< 内部错误

} ota_error_code_t;

/**
 * @brief 健康检查结果
 */
typedef enum {
    HEALTH_CHECK_PASS,          ///< 所有检查通过
    HEALTH_CHECK_WIFI_FAIL,     ///< WiFi 连接失败
    HEALTH_CHECK_MQTT_FAIL,     ///< MQTT 连接失败
    HEALTH_CHECK_DISPLAY_FAIL,  ///< 显示异常
    HEALTH_CHECK_SENSOR_FAIL,   ///< 传感器初始化失败
    HEALTH_CHECK_WATCHDOG,      ///< watchdog 触发
    HEALTH_CHECK_PANIC,         ///< panic 触发
} health_check_result_t;

/**
 * @brief 回滚原因
 */
typedef enum {
    ROLLBACK_REASON_NONE,           ///< 无回滚
    ROLLBACK_REASON_WATCHDOG,       ///< watchdog 重启超限
    ROLLBACK_REASON_PANIC,          ///< panic 重启超限
    ROLLBACK_REASON_HEALTH_FAIL,    ///< 健康检查失败
    ROLLBACK_REASON_USER_REQUEST,   ///< 用户手动回滚
    ROLLBACK_REASON_SERVER_COMMAND, ///< 服务器下发回滚命令
} rollback_reason_t;

/**
 * @brief OTA 更新信息
 */
typedef struct {
    char firmware_url[256];         ///< 固件下载 URL
    char signature_url[256];        ///< 签名文件 URL
    char version[32];               ///< 固件版本号 x.y.z
    size_t firmware_size;           ///< 固件大小 (bytes)
    char sha256[65];                ///< SHA256 校验和 (hex string, 64 chars + null)
    ota_update_level_t level;       ///< 升级级别
    uint32_t release_timestamp;     ///< 发布时间戳
    char changelog[512];            ///< 更新日志
} ota_update_info_t;

/**
 * @brief OTA 进度信息
 */
typedef struct {
    ota_state_t state;              ///< 当前状态
    int progress_percent;           ///< 进度百分比 (0-100)
    size_t bytes_downloaded;        ///< 已下载字节数
    size_t bytes_total;             ///< 总字节数
    uint32_t elapsed_ms;            ///< 已耗时
    uint32_t estimated_remaining_ms;///< 预估剩余时间
} ota_progress_t;

/**
 * @brief OTA 错误信息
 */
typedef struct {
    ota_error_code_t code;          ///< 错误码
    char message[256];              ///< 错误描述
    ota_state_t failed_state;       ///< 失败时的状态
    uint32_t timestamp;             ///< 错误时间戳
} ota_error_t;

/**
 * @brief OTA 命令消息
 */
typedef struct {
    ota_cmd_type_t type;            ///< 命令类型
    ota_update_info_t info;         ///< 更新信息（可选）
} ota_cmd_msg_t;

/**
 * @brief OTA 启动状态（NVS 持久化）
 */
typedef struct {
    uint8_t watchdog_count;         ///< watchdog 重启计数
    uint8_t panic_count;            ///< panic 计数
    uint32_t boot_timestamp;        ///< 本次启动时间戳
    uint32_t health_check_deadline; ///< 健康检查截止时间
    bool health_check_passed;       ///< 健康检查是否通过
} ota_boot_state_t;

/**
 * @brief 进度回调函数类型
 *
 * @param progress 进度信息
 */
typedef void (*ota_progress_callback_t)(const ota_progress_t *progress);

/**
 * @brief 状态回调函数类型
 *
 * @param old_state 旧状态
 * @param new_state 新状态
 */
typedef void (*ota_state_callback_t)(ota_state_t old_state, ota_state_t new_state);

/**
 * @brief 结果回调函数类型
 *
 * @param success 是否成功
 * @param version 固件版本
 * @param error 错误描述（失败时）
 */
typedef void (*ota_result_callback_t)(bool success, const char *version, const char *error);

#ifdef __cplusplus
}
#endif
