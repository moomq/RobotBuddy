/**
 * @file ota_config.h
 * @brief OTA 升级功能配置常量
 *
 * @copyright Copyright (c) 2026 RobotBuddy
 * @author RobotBuddy Team
 * @date 2026-07-19
 */

#pragma once

#include <stdint.h>

/**
 * @defgroup OTA_Config OTA 配置常量
 * @{
 */

/* ===== 健康检查配置 ===== */

/** 健康检查超时时间 (ms) */
#define OTA_HEALTH_CHECK_TIMEOUT_MS       30000

/** Watchdog 重启阈值（达到此值触发自动回滚） */
#define OTA_WATCHDOG_THRESHOLD            3

/** Panic 重启阈值 */
#define OTA_PANIC_THRESHOLD               5

/* ===== 下载配置 ===== */

/** 最大重试次数 */
#define OTA_MAX_RETRIES                   3

/** 重试延迟 (ms) */
#define OTA_RETRY_DELAY_MS_1              5000
#define OTA_RETRY_DELAY_MS_2              15000
#define OTA_RETRY_DELAY_MS_3              30000

/** 断点续传块大小 (bytes) */
#define OTA_RESUME_CHUNK_SIZE             10240

/** HTTP 下载缓冲区大小 */
#define OTA_DOWNLOAD_BUFFER_SIZE          4096

/** HTTP 超时时间 (ms) */
#define OTA_HTTP_TIMEOUT_MS               30000

/* ===== 进度回调配置 ===== */

/** 进度回调间隔 (ms) */
#define OTA_PROGRESS_INTERVAL_MS          500

/* ===== 电量检查配置 ===== */

/** 最低电量百分比（低于此值拒绝 OTA） */
#define OTA_MIN_BATTERY_PERCENT            20

/* ===== 任务配置 ===== */

/** OTA 服务任务名称 */
#define OTA_SERVICE_TASK_NAME             "ota_svc"

/** OTA 服务任务栈大小 (bytes) */
#define OTA_SERVICE_TASK_STACK            8192

/** OTA 服务任务优先级 */
#define OTA_SERVICE_TASK_PRIORITY         1

/** OTA 服务任务核心 (0 = PRO_CPU, 1 = APP_CPU) */
#define OTA_SERVICE_TASK_CORE             0

/** OTA 命令队列深度 */
#define OTA_CMD_QUEUE_DEPTH               5

/* ===== 分区配置 ===== */

/** Factory 分区标签 */
#define OTA_PARTITION_FACTORY             "factory"

/** OTA_0 分区标签 */
#define OTA_PARTITION_OTA_0               "ota_0"

/** OTA_1 分区标签 */
#define OTA_PARTITION_OTA_1               "ota_1"

/* ===== NVS 命名空间 ===== */

/** OTA 启动状态 NVS 命名空间 */
#define OTA_NVS_NAMESPACE_BOOT            "ota_boot"

/** OTA 进度 NVS 命名空间 */
#define OTA_NVS_NAMESPACE_PROGRESS        "ota_prog"

/** OTA 配置 NVS 命名空间 */
#define OTA_NVS_NAMESPACE_CONFIG          "ota_cfg"

/* ===== NVS 键名 ===== */

/** Watchdog 计数键 */
#define OTA_NVS_KEY_WD_COUNT              "wd_count"

/** Panic 计数键 */
#define OTA_NVS_KEY_PANIC_COUNT           "panic_count"

/** 启动时间键 */
#define OTA_NVS_KEY_BOOT_TIME             "boot_time"

/** 断点续传偏移量键 */
#define OTA_NVS_KEY_RESUME_OFFSET         "resume_off"

/** OTA 状态键 */
#define OTA_NVS_KEY_STATE                 "state"

/** 固件版本键 */
#define OTA_NVS_KEY_VERSION               "version"

/* ===== 日志标签 ===== */

/** OTA 服务日志标签 */
#define OTA_LOG_TAG_SERVICE               "ota_svc"

/** OTA 管理器日志标签 */
#define OTA_LOG_TAG_MANAGER               "ota_mgr"

/** OTA 下载日志标签 */
#define OTA_LOG_TAG_DOWNLOAD              "ota_dl"

/** OTA 验证日志标签 */
#define OTA_LOG_TAG_VERIFY                "ota_vrf"

/** OTA 分区日志标签 */
#define OTA_LOG_TAG_PARTITION             "ota_part"

/** OTA 回滚日志标签 */
#define OTA_LOG_TAG_ROLLBACK              "ota_rb"

/** OTA 安全日志标签 */
#define OTA_LOG_TAG_SECURITY              "ota_sec"

/** @} */ // end of OTA_Config group
