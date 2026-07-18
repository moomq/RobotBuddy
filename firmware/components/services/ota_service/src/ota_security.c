/**
 * @file ota_security.c
 * @brief OTA 安全管理实现
 *
 * @copyright Copyright (c) 2026 RobotBuddy
 * @author RobotBuddy Team
 * @date 2026-07-19
 */

#include "ota_security.h"

#include "esp_log.h"
#include "esp_efuse.h"
#include "esp_secure_boot.h"

#include "mbedtls/sha256.h"

#include <string.h>

/* ============================================================
 * 日志标签
 * ============================================================ */

static const char *TAG = OTA_LOG_TAG_SECURITY;

/* ============================================================
 * 模块状态
 * ============================================================ */

static bool s_initialized = false;

/**
 * @brief OTA 服务器证书 SHA256 指纹
 *
 * 这是示例指纹，实际部署时应替换为真实 OTA 服务器的证书指纹。
 * 使用以下命令获取证书指纹:
 *   openssl x509 -in server_cert.pem -outform DER | openssl sha256
 */
static const uint8_t s_server_cert_fingerprint[OTA_CERT_FINGERPRINT_LEN] = {
    0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x78, 0x90,
    0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x90,
    0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x90,
    0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x90,
};

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

esp_err_t ota_security_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    /* 检查安全启动状态 */
    bool secure_boot_enabled = false;
    ota_security_check_secure_boot_status(&secure_boot_enabled);

    if (secure_boot_enabled) {
        ESP_LOGI(TAG, "Secure Boot V2 is enabled");
    } else {
        ESP_LOGW(TAG, "Secure Boot is NOT enabled - OTA less secure");
    }

    /* 检查 Flash 加密状态 */
    bool flash_enc_enabled = ota_security_is_flash_encryption_enabled();
    if (flash_enc_enabled) {
        ESP_LOGI(TAG, "Flash encryption is enabled");
    }

    s_initialized = true;
    return ESP_OK;
}

esp_err_t ota_security_deinit(void)
{
    s_initialized = false;
    return ESP_OK;
}

esp_err_t ota_security_verify_cert_fingerprint(const char *cert_pem)
{
    if (cert_pem == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 计算 PEM 证书的 SHA256 */
    uint8_t computed_hash[32];
    mbedtls_sha256((const unsigned char *)cert_pem, strlen(cert_pem),
                   computed_hash, 0);

    /* 比较指纹 */
    if (memcmp(computed_hash, s_server_cert_fingerprint, OTA_CERT_FINGERPRINT_LEN) != 0) {
        ESP_LOGE(TAG, "Server certificate fingerprint mismatch!");

        /* 打印计算的和预期的指纹 */
        char computed_hex[65], expected_hex[65];
        for (int i = 0; i < 32; i++) {
            snprintf(&computed_hex[i * 2], 3, "%02X", computed_hash[i]);
            snprintf(&expected_hex[i * 2], 3, "%02X", s_server_cert_fingerprint[i]);
        }
        computed_hex[64] = '\0';
        expected_hex[64] = '\0';

        ESP_LOGE(TAG, "  Expected: %s", expected_hex);
        ESP_LOGE(TAG, "  Computed: %s", computed_hex);

        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Server certificate fingerprint verified");
    return ESP_OK;
}

esp_err_t ota_security_verify_cert_fingerprint_der(const uint8_t *cert_der, size_t cert_len)
{
    if (cert_der == NULL || cert_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 计算 DER 证书的 SHA256 */
    uint8_t computed_hash[32];
    mbedtls_sha256(cert_der, cert_len, computed_hash, 0);

    /* 比较指纹 */
    if (memcmp(computed_hash, s_server_cert_fingerprint, OTA_CERT_FINGERPRINT_LEN) != 0) {
        ESP_LOGE(TAG, "Server certificate fingerprint mismatch (DER)!");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Server certificate fingerprint verified (DER)");
    return ESP_OK;
}

esp_err_t ota_security_get_cert_fingerprint(uint8_t *fingerprint, char *hex_str)
{
    if (fingerprint == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(fingerprint, s_server_cert_fingerprint, OTA_CERT_FINGERPRINT_LEN);

    if (hex_str != NULL) {
        for (int i = 0; i < 32; i++) {
            snprintf(&hex_str[i * 2], 3, "%02X", s_server_cert_fingerprint[i]);
        }
        hex_str[64] = '\0';
    }

    return ESP_OK;
}

bool ota_security_is_cert_pinning_enabled(void)
{
    /* 证书锁定总是启用的（如果模块已初始化） */
    return s_initialized;
}

esp_err_t ota_security_parse_signed_url(const char *raw_url, ota_signed_url_t *signed_url)
{
    if (raw_url == NULL || signed_url == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 初始化 */
    memset(signed_url, 0, sizeof(ota_signed_url_t));

    /* 简单解析 URL 参数 */
    /* 格式: https://server/path?token=xxx&expires=xxx&device_id=xxx */

    const char *url_start = raw_url;
    const char *query_start = strchr(raw_url, '?');
    if (query_start == NULL) {
        /* 无查询参数，直接复制 URL */
        strncpy(signed_url->url, raw_url, OTA_SIGNED_URL_MAX_LEN - 1);
        return ESP_OK;
    }

    /* 复制基础 URL */
    size_t base_len = query_start - url_start;
    strncpy(signed_url->url, raw_url, base_len);
    signed_url->url[base_len] = '\0';

    /* 解析查询参数 */
    const char *token_start = strstr(query_start, "token=");
    if (token_start != NULL) {
        token_start += 6;  /* 跳过 "token=" */
        const char *token_end = strchr(token_start, '&');
        size_t token_len = (token_end != NULL) ?
                           (token_end - token_start) :
                           strlen(token_start);
        if (token_len > sizeof(signed_url->token) - 1) {
            token_len = sizeof(signed_url->token) - 1;
        }
        strncpy(signed_url->token, token_start, token_len);
    }

    const char *expires_start = strstr(query_start, "expires=");
    if (expires_start != NULL) {
        expires_start += 8;  /* 跳过 "expires=" */
        signed_url->expires_at = strtoull(expires_start, NULL, 10);
    }

    const char *device_start = strstr(query_start, "device_id=");
    if (device_start != NULL) {
        device_start += 10;  /* 跳过 "device_id=" */
        const char *device_end = strchr(device_start, '&');
        size_t device_len = (device_end != NULL) ?
                            (device_end - device_start) :
                            strlen(device_start);
        if (device_len > sizeof(signed_url->device_id) - 1) {
            device_len = sizeof(signed_url->device_id) - 1;
        }
        strncpy(signed_url->device_id, device_start, device_len);
    }

    /* 重建完整 URL */
    strncpy(signed_url->url, raw_url, OTA_SIGNED_URL_MAX_LEN - 1);

    return ESP_OK;
}

bool ota_security_is_url_expired(const ota_signed_url_t *signed_url)
{
    if (signed_url == NULL || signed_url->expires_at == 0) {
        return false;
    }

    /* 获取当前时间 */
    /* TODO: 使用系统时间或 NTP 时间 */
    uint64_t current_time = 0;

#ifdef CONFIG_FREERTOS_USE_TICKLESS_IDLE
    /* 如果使用了 Tickless Idle，可以使用 FreeRTOS ticks */
    current_time = (uint64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS) / 1000;
#endif

    return (current_time >= signed_url->expires_at);
}

esp_err_t ota_security_verify_url_token(const ota_signed_url_t *signed_url,
                                         const char *server_secret)
{
    if (signed_url == NULL || server_secret == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* TODO: 实现 HMAC-SHA256 验证 */
    /* 签名 = HMAC_SHA256(device_id + version + timestamp, server_secret) */

    ESP_LOGW(TAG, "URL token verification not fully implemented");

    /* 暂时返回成功 */
    return ESP_OK;
}

esp_err_t ota_security_build_signed_url(const char *base_url,
                                         const char *device_id,
                                         const char *version,
                                         const char *server_secret,
                                         uint32_t expires_in,
                                         ota_signed_url_t *signed_url)
{
    if (base_url == NULL || signed_url == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(signed_url, 0, sizeof(ota_signed_url_t));

    /* TODO: 实现 URL 签名 */

    /* 构建基础 URL */
    snprintf(signed_url->url, OTA_SIGNED_URL_MAX_LEN, "%s", base_url);

    if (device_id != NULL) {
        strncpy(signed_url->device_id, device_id, sizeof(signed_url->device_id) - 1);
    }

    /* 计算过期时间 */
    /* TODO: 使用当前时间 + expires_in */
    signed_url->expires_at = 0;

    ESP_LOGW(TAG, "Signed URL building not fully implemented");

    return ESP_OK;
}

esp_err_t ota_security_check_secure_boot_status(bool *enabled)
{
    if (enabled == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#ifdef CONFIG_SECURE_BOOT
    *enabled = true;
#else
    *enabled = false;

    /* 尝试从 eFuse 读取安全启动状态 */
    /* ESP32-S3 的安全启动状态存储在 eFuse 中 */
#endif

    return ESP_OK;
}

bool ota_security_is_secure_boot_enabled(void)
{
    bool enabled = false;
    ota_security_check_secure_boot_status(&enabled);
    return enabled;
}

bool ota_security_is_flash_encryption_enabled(void)
{
#ifdef CONFIG_SECURE_FLASH_ENC_ENABLED
    return true;
#else
    return false;
#endif
}

uint8_t ota_security_get_level(void)
{
    uint8_t level = 0;

    /* 基础层：TLS + 签名验证 */
    level = 1;

    /* 第二层：证书锁定 */
    if (s_initialized) {
        level = 2;
    }

    /* 第三层：安全启动 */
    if (ota_security_is_secure_boot_enabled()) {
        level = 3;
    }

    return level;
}

esp_err_t ota_security_check_ota_allowed(void)
{
    /* 检查是否允许 OTA */

    /* 1. 检查是否已初始化 */
    if (!s_initialized) {
        ESP_LOGW(TAG, "Security module not initialized");
        /* 允许继续，但不安全 */
    }

    /* 2. 检查证书锁定 */
    if (!ota_security_is_cert_pinning_enabled()) {
        ESP_LOGW(TAG, "Certificate pinning not enabled");
    }

    /* 3. 如果启用了安全启动，需要确保签名正确 */
    if (ota_security_is_secure_boot_enabled()) {
        ESP_LOGI(TAG, "Secure boot enabled - OTA must be signed correctly");
    }

    return ESP_OK;
}

esp_err_t ota_security_get_public_key(const uint8_t **key, size_t *key_len)
{
    /* 返回嵌入式 RSA 公钥 */
    if (key == NULL || key_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 公钥由 ota_verify 模块管理 */
    extern esp_err_t ota_verify_get_public_key(const uint8_t **key_out, size_t *key_len);

    return ota_verify_get_public_key(key, key_len);
}

esp_err_t ota_security_get_public_key_fingerprint(uint8_t *fingerprint)
{
    if (fingerprint == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 获取公钥并计算指纹 */
    const uint8_t *key = NULL;
    size_t key_len = 0;

    esp_err_t ret = ota_security_get_public_key(&key, &key_len);
    if (ret != ESP_OK) {
        return ret;
    }

    /* 计算公钥 SHA256 指纹 */
    mbedtls_sha256(key, key_len, fingerprint, 0);

    return ESP_OK;
}

const char* ota_security_get_ca_cert(void)
{
    /* 返回 NULL 使用默认 CA 证书 */
    /* 如果需要自定义 CA 证书，可以在这里返回 */
    return NULL;
}

bool ota_security_skip_cn_check(void)
{
#ifdef CONFIG_OTA_SKIP_CERT_COMMON_NAME_CHECK
    return true;
#else
    return false;
#endif
}
