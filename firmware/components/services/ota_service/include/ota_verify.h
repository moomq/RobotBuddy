/**
 * @file ota_verify.h
 * @brief OTA 签名验证接口
 *
 * 提供 RSA-2048 签名验证和 SHA256 校验功能。
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
 * @defgroup OTA_Verify OTA 签名验证
 * @{
 */

/* ============================================================
 * 初始化
 * ============================================================ */

/**
 * @brief 初始化验证模块
 *
 * 加载嵌入式 RSA 公钥。
 *
 * @return ESP_OK 成功
 *         ESP_FAIL 公钥加载失败
 */
esp_err_t ota_verify_init(void);

/**
 * @brief 反初始化验证模块
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_verify_deinit(void);

/* ============================================================
 * RSA-2048 签名验证
 * ============================================================ */

/**
 * @brief 验证固件 RSA-2048 签名
 *
 * 使用嵌入式公钥验证固件签名。
 *
 * @param[in] partition 固件所在分区
 * @param[in] firmware_size 固件大小
 * @param[in] signature 签名数据（256 字节）
 * @param[in] sig_len 签名长度
 * @return ESP_OK 验证通过
 *         ESP_ERR_INVALID_ARG 参数无效
 *         ESP_ERR_RSA_VERIFY_FAILED 验证失败
 *         ESP_FAIL 其他错误
 */
esp_err_t ota_verify_rsa_signature(const esp_partition_t *partition,
                                    size_t firmware_size,
                                    const uint8_t *signature,
                                    size_t sig_len);

/**
 * @brief 验证数据 RSA-2048 签名
 *
 * 直接验证内存中数据的签名。
 *
 * @param[in] data 数据指针
 * @param[in] data_len 数据长度
 * @param[in] signature 签名数据
 * @param[in] sig_len 签名长度
 * @return ESP_OK 验证通过
 */
esp_err_t ota_verify_rsa_signature_data(const uint8_t *data,
                                         size_t data_len,
                                         const uint8_t *signature,
                                         size_t sig_len);

/* ============================================================
 * SHA256 校验
 * ============================================================ */

/**
 * @brief 计算分区 SHA256 哈希
 *
 * @param[in] partition 目标分区
 * @param[in] size 数据大小（0 = 整个分区）
 * @param[out] hash_out 32 字节哈希输出
 * @return ESP_OK 成功
 */
esp_err_t ota_verify_compute_sha256(const esp_partition_t *partition,
                                     size_t size,
                                     uint8_t *hash_out);

/**
 * @brief 验证分区 SHA256 哈希
 *
 * @param[in] partition 目标分区
 * @param[in] size 数据大小
 * @param[in] expected_sha256_hex 预期的 SHA256 十六进制字符串
 * @return ESP_OK 验证通过
 *         ESP_ERR_SHA256_MISMATCH 不匹配
 */
esp_err_t ota_verify_sha256(const esp_partition_t *partition,
                             size_t size,
                             const char *expected_sha256_hex);

/**
 * @brief 计算数据 SHA256 哈希
 *
 * @param[in] data 数据指针
 * @param[in] data_len 数据长度
 * @param[out] hash_out 32 字节哈希输出
 * @return ESP_OK 成功
 */
esp_err_t ota_verify_compute_sha256_data(const uint8_t *data,
                                          size_t data_len,
                                          uint8_t *hash_out);

/**
 * @brief 将哈希转换为十六进制字符串
 *
 * @param[in] hash 32 字节哈希
 * @param[out] hex_out 65 字节输出缓冲区（64 字符 + null）
 */
void ota_verify_hash_to_hex(const uint8_t *hash, char *hex_out);

/* ============================================================
 * 综合验证
 * ============================================================ */

/**
 * @brief 综合验证（签名 + SHA256）
 *
 * 执行完整的固件验证流程：
 * 1. 验证 SHA256 完整性
 * 2. 验证 RSA 签名
 *
 * @param[in] partition 固件所在分区
 * @param[in] firmware_size 固件大小
 * @param[in] signature 签名数据
 * @param[in] sig_len 签名长度
 * @param[in] expected_sha256_hex 预期的 SHA256（可为 NULL 跳过）
 * @return ESP_OK 验证通过
 */
esp_err_t ota_verify_full(const esp_partition_t *partition,
                           size_t firmware_size,
                           const uint8_t *signature,
                           size_t sig_len,
                           const char *expected_sha256_hex);

/* ============================================================
 * 固件头验证
 * ============================================================ */

/**
 * @brief 验证固件镜像头
 *
 * 检查 ESP32 固件魔数（0xE9）。
 *
 * @param[in] header_data 头部数据（至少 1 字节）
 * @param[in] len 数据长度
 * @return ESP_OK 验证通过
 *         ESP_ERR_INVALID_IMAGE 头部无效
 */
esp_err_t ota_verify_firmware_header(const uint8_t *header_data, size_t len);

/* ============================================================
 * 公钥管理
 * ============================================================ */

/**
 * @brief 获取嵌入式公钥
 *
 * @param[out] key_out 公钥数据指针
 * @param[out] key_len 公钥长度
 * @return ESP_OK 成功
 */
esp_err_t ota_verify_get_public_key(const uint8_t **key_out, size_t *key_len);

/**
 * @brief 检查公钥是否已加载
 *
 * @return true 已加载
 */
bool ota_verify_is_public_key_loaded(void);

/** @} */

#ifdef __cplusplus
}
#endif
