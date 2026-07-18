/**
 * @file ota_security.h
 * @brief OTA 安全管理接口
 *
 * 提供 TLS 证书锁定、签名 URL 验证和安全启动状态检查。
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
 * @defgroup OTA_Security OTA 安全管理
 * @{
 */

/* ============================================================
 * 配置
 * ============================================================ */

/** 证书指纹长度（SHA256 = 32 字节） */
#define OTA_CERT_FINGERPRINT_LEN    32

/** 签名 URL 最大长度 */
#define OTA_SIGNED_URL_MAX_LEN      320

/* ============================================================
 * 初始化
 * ============================================================ */

/**
 * @brief 初始化安全模块
 *
 * 加载证书指纹，检查安全启动状态。
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_security_init(void);

/**
 * @brief 反初始化安全模块
 *
 * @return ESP_OK 成功
 */
esp_err_t ota_security_deinit(void);

/* ============================================================
 * 证书锁定
 * ============================================================ */

/**
 * @brief 验证服务器证书指纹
 *
 * 对比服务器证书的 SHA256 指纹与嵌入式指纹。
 *
 * @param[in] cert_pem PEM 格式证书
 * @return ESP_OK 验证通过
 *         ESP_ERR_INVALID_STATE 指纹不匹配
 */
esp_err_t ota_security_verify_cert_fingerprint(const char *cert_pem);

/**
 * @brief 验证服务器证书指纹（原始字节）
 *
 * @param[in] cert_der DER 格式证书
 * @param[in] cert_len 证书长度
 * @return ESP_OK 验证通过
 */
esp_err_t ota_security_verify_cert_fingerprint_der(const uint8_t *cert_der, size_t cert_len);

/**
 * @brief 获取嵌入式证书指纹
 *
 * @param[out] fingerprint 32 字节指纹输出
 * @param[out] hex_str 65 字节十六进制字符串输出（可为 NULL）
 * @return ESP_OK 成功
 */
esp_err_t ota_security_get_cert_fingerprint(uint8_t *fingerprint, char *hex_str);

/**
 * @brief 检查证书锁定是否启用
 *
 * @return true 启用
 *         false 禁用
 */
bool ota_security_is_cert_pinning_enabled(void);

/* ============================================================
 * 签名 URL
 * ============================================================ */

/**
 * @brief 签名 URL 结构
 */
typedef struct {
    char url[OTA_SIGNED_URL_MAX_LEN];   ///< 完整 URL
    char token[128];                     ///< 签名令牌
    uint64_t expires_at;                 ///< 过期时间戳
    char device_id[32];                  ///< 设备 ID
} ota_signed_url_t;

/**
 * @brief 解析签名 URL
 *
 * 从 URL 中提取 token、过期时间等参数。
 *
 * @param[in] raw_url 原始 URL
 * @param[out] signed_url 解析结果
 * @return ESP_OK 成功
 *         ESP_ERR_INVALID_ARG URL 格式无效
 */
esp_err_t ota_security_parse_signed_url(const char *raw_url, ota_signed_url_t *signed_url);

/**
 * @brief 检查签名 URL 是否过期
 *
 * @param[in] signed_url 签名 URL
 * @return true 已过期
 *         false 未过期
 */
bool ota_security_is_url_expired(const ota_signed_url_t *signed_url);

/**
 * @brief 验证签名 URL 令牌
 *
 * @param[in] signed_url 签名 URL
 * @param[in] server_secret 服务器密钥
 * @return ESP_OK 验证通过
 *         ESP_ERR_INVALID_STATE 验证失败
 */
esp_err_t ota_security_verify_url_token(const ota_signed_url_t *signed_url,
                                         const char *server_secret);

/**
 * @brief 构建签名 URL
 *
 * @param[in] base_url 基础 URL
 * @param[in] device_id 设备 ID
 * @param[in] version 固件版本
 * @param[in] server_secret 服务器密钥
 * @param[in] expires_in 有效期（秒）
 * @param[out] signed_url 输出签名 URL
 * @return ESP_OK 成功
 */
esp_err_t ota_security_build_signed_url(const char *base_url,
                                         const char *device_id,
                                         const char *version,
                                         const char *server_secret,
                                         uint32_t expires_in,
                                         ota_signed_url_t *signed_url);

/* ============================================================
 * 安全启动
 * ============================================================ */

/**
 * @brief 检查安全启动状态
 *
 * @param[out] enabled 是否启用安全启动
 * @return ESP_OK 成功
 */
esp_err_t ota_security_check_secure_boot_status(bool *enabled);

/**
 * @brief 检查是否启用安全启动
 *
 * @return true 启用安全启动
 *         false 未启用
 */
bool ota_security_is_secure_boot_enabled(void);

/**
 * @brief 检查是否启用 Flash 加密
 *
 * @return true 启用 Flash 加密
 *         false 未启用
 */
bool ota_security_is_flash_encryption_enabled(void);

/* ============================================================
 * 安全配置
 * ============================================================ */

/**
 * @brief 获取安全等级
 *
 * 安全等级定义：
 * - 0: 无安全措施
 * - 1: TLS + 签名验证
 * - 2: TLS + 签名验证 + 证书锁定
 * - 3: TLS + 签名验证 + 证书锁定 + 安全启动
 *
 * @return 安全等级（0-3）
 */
uint8_t ota_security_get_level(void);

/**
 * @brief 检查是否允许 OTA
 *
 * 检查安全条件是否满足 OTA 要求。
 *
 * @return ESP_OK 允许 OTA
 *         ESP_ERR_INVALID_STATE 不允许
 */
esp_err_t ota_security_check_ota_allowed(void);

/* ============================================================
 * 公钥管理
 * ============================================================ */

/**
 * @brief 获取 RSA 公钥
 *
 * @param[out] key 公钥数据
 * @param[out] key_len 公钥长度
 * @return ESP_OK 成功
 */
esp_err_t ota_security_get_public_key(const uint8_t **key, size_t *key_len);

/**
 * @brief 获取公钥指纹
 *
 * @param[out] fingerprint 32 字节指纹输出
 * @return ESP_OK 成功
 */
esp_err_t ota_security_get_public_key_fingerprint(uint8_t *fingerprint);

/* ============================================================
 * HTTP 客户端安全配置
 * ============================================================ */

/**
 * @brief 获取 TLS 配置
 *
 * 返回用于 OTA 下载的 TLS 配置字符串。
 *
 * @return PEM 格式的 CA 证书，或 NULL 使用默认
 */
const char* ota_security_get_ca_cert(void);

/**
 * @brief 获取跳过 CN 检查标志
 *
 * @return true 跳过
 *         false 检查
 */
bool ota_security_skip_cn_check(void);

/** @} */

#ifdef __cplusplus
}
#endif
