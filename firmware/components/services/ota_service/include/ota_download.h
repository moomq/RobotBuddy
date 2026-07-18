/**
 * @file ota_download.h
 * @brief OTA 下载管理接口
 *
 * 提供 HTTPS 固件下载功能，支持断点续传和进度回调。
 *
 * @copyright Copyright (c) 2026 RobotBuddy
 * @author RobotBuddy Team
 * @date 2026-07-19
 */

#pragma once

#include "esp_err.h"
#include "esp_partition.h"
#include "ota_types.h"
#include "ota_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup OTA_Download OTA 下载管理
 * @{
 */

/* ============================================================
 * 配置
 * ============================================================ */

/** RSA-2048 签名长度 */
#define OTA_SIGNATURE_LEN          256

/** 最大签名文件大小 */
#define OTA_SIGNATURE_MAX_SIZE     512

/* ============================================================
 * 初始化
 * ============================================================ */

/**
 * @brief 初始化下载模块
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_download_init(void);

/**
 * @brief 反初始化下载模块
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_download_deinit(void);

/* ============================================================
 * 固件下载
 * ============================================================ */

/**
 * @brief 下载固件到指定分区
 *
 * 通过 HTTPS 下载固件并写入目标分区。
 * 支持进度回调和取消操作。
 *
 * @param[in] url 固件下载 URL
 * @param[in] target_partition 目标分区（由 ota_partition 模块提供）
 * @param[in] progress_cb 进度回调（可为 NULL）
 * @return ESP_OK 成功
 *         ESP_ERR_INVALID_ARG 参数无效
 *         ESP_ERR_HTTP_CONNECT HTTP 连接失败
 *         ESP_ERR_HTTP_OTA_DOWNLOAD 下载失败
 *         ESP_ERR_OTA_WRITE 写入失败
 */
esp_err_t ota_download_firmware(const char *url,
                                const esp_partition_t *target_partition,
                                ota_progress_callback_t progress_cb);

/**
 * @brief 断点续传下载
 *
 * 从指定偏移量继续下载固件。
 *
 * @param[in] url 固件下载 URL
 * @param[in] offset 已下载的偏移量
 * @param[in] target_partition 目标分区
 * @param[in] progress_cb 进度回调（可为 NULL）
 * @return ESP_OK 成功
 */
esp_err_t ota_download_resume(const char *url,
                              size_t offset,
                              const esp_partition_t *target_partition,
                              ota_progress_callback_t progress_cb);

/**
 * @brief 取消下载
 *
 * 设置取消标志，下载循环会检查并中止。
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_download_cancel(void);

/**
 * @brief 检查是否正在下载
 *
 * @return true 正在下载
 *         false 未下载
 */
bool ota_download_is_in_progress(void);

/**
 * @brief 获取已下载字节数
 *
 * @return 已下载字节数
 */
size_t ota_download_get_downloaded_bytes(void);

/* ============================================================
 * 签名下载
 * ============================================================ */

/**
 * @brief 下载签名文件
 *
 * @param[in] url 签名文件 URL
 * @param[out] signature 签名数据缓冲区
 * @param[in,out] sig_len 输入缓冲区大小，输出实际签名长度
 * @return ESP_OK 成功
 *         ESP_ERR_INVALID_ARG 参数无效
 *         ESP_ERR_NO_MEM 内存不足
 *         ESP_FAIL 下载失败
 */
esp_err_t ota_download_signature(const char *url,
                                  uint8_t *signature,
                                  size_t *sig_len);

/* ============================================================
 * 版本检查
 * ============================================================ */

/**
 * @brief 检查服务器是否有新版本
 *
 * 发送 HTTP GET 请求查询最新版本信息。
 *
 * @param[in] device_id 设备 ID
 * @param[in] current_version 当前固件版本
 * @param[out] info 更新信息
 * @return ESP_OK 成功
 *         ESP_ERR_INVALID_ARG 参数无效
 *         ESP_FAIL 检查失败
 */
esp_err_t ota_download_check_update(const char *device_id,
                                    const char *current_version,
                                    ota_update_info_t *info);

/* ============================================================
 * HTTP 辅助
 * ============================================================ */

/**
 * @brief 获取最后一次 HTTP 状态码
 *
 * @return HTTP 状态码
 */
int ota_download_get_last_http_status(void);

/**
 * @brief 获取最后一次 HTTP 错误描述
 *
 * @param[out] buf 缓冲区
 * @param[in] buf_size 缓冲区大小
 */
void ota_download_get_last_error_desc(char *buf, size_t buf_size);

/** @} */

#ifdef __cplusplus
}
#endif
