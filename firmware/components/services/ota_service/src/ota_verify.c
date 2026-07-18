/**
 * @file ota_verify.c
 * @brief OTA 签名验证实现
 *
 * @copyright Copyright (c) 2026 RobotBuddy
 * @author RobotBuddy Team
 * @date 2026-07-19
 */

#include "ota_verify.h"

#include "esp_log.h"
#include "esp_partition.h"

#include "mbedtls/sha256.h"
#include "mbedtls/pk.h"
#include "mbedtls/rsa.h"
#include "mbedtls/md.h"

#include <string.h>

/* ============================================================
 * 日志标签
 * ============================================================ */

static const char *TAG = OTA_LOG_TAG_VERIFY;

/* ============================================================
 * ESP32 固件魔数
 * ============================================================ */

#define ESP_IMAGE_HEADER_MAGIC  0xE9

/* ============================================================
 * 模块状态
 * ============================================================ */

static bool s_initialized = false;

/**
 * @brief RSA-2048 公钥（嵌入式，编译时嵌入）
 *
 * 这是示例公钥，实际部署时应替换为真实的公钥。
 * 格式：PEM 格式的 RSA 公钥
 */
static const char s_rsa_public_key_pem[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAz7p2GjLnYJHjz5F1\n"
    "6OuKIjQKz3J3B6tX8aLKQcFdPxJLhP+QKqKjN8m1JLz9lVZxJ+QKjB5yJLz9\n"
    "vHqJ5B6tX8aLKQcFdPxJLhP+QKqKjN8m1JLz9lVZxJ+QKjB5yJLz9vHqJ5B6\n"
    "tX8aLKQcFdPxJLhP+QKqKjN8m1JLz9lVZxJ+QKjB5yJLz9vHqJ5B6tX8aLK\n"
    "QcFdPxJLhP+QKqKjN8m1JLz9lVZxJ+QKjB5yJLz9vHqJ5B6tX8aLKQcFdPx\n"
    "JLhP+QKqKjN8m1JLz9lVZxJ+QKjB5yJLz9vHqJ5B6tX8aLKQcFdPxJLhP+Q\n"
    "KqKjN8m1JLz9lVZxJ+QKjB5yJLz9vHqJ5B6tX8aLKQcFdPxJLhP+QKqKjN8\n"
    "m1JLz9lVZxJ+QKjB5yJLz9vHqJ5B6tX8aLKQcFdPxJLhP+QKqKjN8m1JLz9\n"
    "lVZxJ+QKjB5yJLz9vHqJ5B6tX8aLKQcFdPxJLhP+QKqKjN8m1JLz9lVZxJ+\n"
    "QKjB5yJLz9vHqJ5B6tX8aLKQcFdPxJLhP+QKqKjN8m1JLz9lVZxJ+QKjB5y\n"
    "JLz9vHqJ5B6tX8aLKQcFdPxJLhP+QKqKjN8m1JLz9lVZxJ+QKjB5yJLz9vHq\n"
    "J5B6tX8aLKQcFdPxJLhP+QKqKjN8m1JLz9lVZxJ+QKjB5yJLz9vHqJ5B6tX\n"
    "8QIDAQAB\n"
    "-----END PUBLIC KEY-----\n";

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

esp_err_t ota_verify_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    /* 验证公钥是否可用 */
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    int ret = mbedtls_pk_parse_public_key(&pk,
                                          (const unsigned char *)s_rsa_public_key_pem,
                                          sizeof(s_rsa_public_key_pem));
    if (ret != 0) {
        ESP_LOGW(TAG, "Failed to parse embedded public key (error: -0x%04X)", -ret);
        ESP_LOGW(TAG, "Using key anyway, verification may fail at runtime");
    } else {
        ESP_LOGI(TAG, "RSA public key loaded successfully");
    }

    mbedtls_pk_free(&pk);
    s_initialized = true;

    return ESP_OK;
}

esp_err_t ota_verify_deinit(void)
{
    s_initialized = false;
    return ESP_OK;
}

esp_err_t ota_verify_rsa_signature(const esp_partition_t *partition,
                                    size_t firmware_size,
                                    const uint8_t *signature,
                                    size_t sig_len)
{
    if (partition == NULL || signature == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (sig_len != OTA_SIGNATURE_LEN) {
        ESP_LOGE(TAG, "Invalid signature length: %zu (expected %d)", sig_len, OTA_SIGNATURE_LEN);
        return ESP_ERR_INVALID_ARG;
    }

    /* 计算固件 SHA256 哈希 */
    ESP_LOGI(TAG, "Computing firmware SHA256 hash...");

    uint8_t firmware_hash[32];
    esp_err_t ret = ota_verify_compute_sha256(partition, firmware_size, firmware_hash);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to compute firmware hash");
        return ret;
    }

    /* 使用内存中数据验证签名 */
    return ota_verify_rsa_signature_data(firmware_hash, 32, signature, sig_len);
}

esp_err_t ota_verify_rsa_signature_data(const uint8_t *data,
                                         size_t data_len,
                                         const uint8_t *signature,
                                         size_t sig_len)
{
    if (data == NULL || signature == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 初始化 RSA 上下文 */
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    /* 解析公钥 */
    int ret = mbedtls_pk_parse_public_key(&pk,
                                          (const unsigned char *)s_rsa_public_key_pem,
                                          sizeof(s_rsa_public_key_pem));
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to parse public key: -0x%04X", -ret);
        mbedtls_pk_free(&pk);
        return ESP_FAIL;
    }

    /* 验证签名 */
    ESP_LOGI(TAG, "Verifying RSA signature...");

    ret = mbedtls_pk_verify(&pk,
                            MBEDTLS_MD_SHA256,
                            data,
                            data_len,
                            signature,
                            sig_len);

    mbedtls_pk_free(&pk);

    if (ret != 0) {
        ESP_LOGE(TAG, "RSA signature verification failed: -0x%04X", -ret);
        return ESP_ERR_RSA_VERIFY_FAILED;
    }

    ESP_LOGI(TAG, "RSA signature verification passed");
    return ESP_OK;
}

esp_err_t ota_verify_compute_sha256(const esp_partition_t *partition,
                                     size_t size,
                                     uint8_t *hash_out)
{
    if (partition == NULL || hash_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 确定要读取的大小 */
    size_t read_size = (size > 0) ? size : partition->size;

    /* 初始化 SHA256 上下文 */
    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0);

    /* 分块读取分区数据并计算哈希 */
    uint8_t *buffer = (uint8_t *)malloc(OTA_DOWNLOAD_BUFFER_SIZE);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate SHA256 buffer");
        mbedtls_sha256_free(&sha256_ctx);
        return ESP_ERR_NO_MEM;
    }

    size_t offset = 0;
    size_t remaining = read_size;

    while (remaining > 0) {
        size_t chunk_size = (remaining > OTA_DOWNLOAD_BUFFER_SIZE) ?
                            OTA_DOWNLOAD_BUFFER_SIZE : remaining;

        esp_err_t ret = esp_partition_read(partition, offset, buffer, chunk_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read partition at offset %zu: %s",
                     offset, esp_err_to_name(ret));
            free(buffer);
            mbedtls_sha256_free(&sha256_ctx);
            return ret;
        }

        mbedtls_sha256_update(&sha256_ctx, buffer, chunk_size);

        offset += chunk_size;
        remaining -= chunk_size;
    }

    /* 完成哈希计算 */
    mbedtls_sha256_finish(&sha256_ctx, hash_out);
    mbedtls_sha256_free(&sha256_ctx);
    free(buffer);

    /* 打印哈希值 */
    char hash_hex[65];
    ota_verify_hash_to_hex(hash_out, hash_hex);
    ESP_LOGI(TAG, "Computed SHA256: %s", hash_hex);

    return ESP_OK;
}

esp_err_t ota_verify_sha256(const esp_partition_t *partition,
                             size_t size,
                             const char *expected_sha256_hex)
{
    if (partition == NULL || expected_sha256_hex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 计算实际哈希 */
    uint8_t actual_hash[32];
    esp_err_t ret = ota_verify_compute_sha256(partition, size, actual_hash);
    if (ret != ESP_OK) {
        return ret;
    }

    /* 转换为十六进制字符串 */
    char actual_hex[65];
    ota_verify_hash_to_hex(actual_hash, actual_hex);

    /* 比较（不区分大小写） */
    if (strcasecmp(actual_hex, expected_sha256_hex) != 0) {
        ESP_LOGE(TAG, "SHA256 mismatch!");
        ESP_LOGE(TAG, "  Expected: %s", expected_sha256_hex);
        ESP_LOGE(TAG, "  Actual:   %s", actual_hex);
        return ESP_ERR_SHA256_MISMATCH;
    }

    ESP_LOGI(TAG, "SHA256 verification passed");
    return ESP_OK;
}

esp_err_t ota_verify_compute_sha256_data(const uint8_t *data,
                                          size_t data_len,
                                          uint8_t *hash_out)
{
    if (data == NULL || hash_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, data, data_len);
    mbedtls_sha256_finish(&ctx, hash_out);
    mbedtls_sha256_free(&ctx);

    return ESP_OK;
}

void ota_verify_hash_to_hex(const uint8_t *hash, char *hex_out)
{
    static const char hex_chars[] = "0123456789abcdef";

    for (int i = 0; i < 32; i++) {
        hex_out[i * 2]     = hex_chars[(hash[i] >> 4) & 0x0F];
        hex_out[i * 2 + 1] = hex_chars[hash[i] & 0x0F];
    }
    hex_out[64] = '\0';
}

esp_err_t ota_verify_full(const esp_partition_t *partition,
                           size_t firmware_size,
                           const uint8_t *signature,
                           size_t sig_len,
                           const char *expected_sha256_hex)
{
    if (partition == NULL || signature == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 1. 验证 SHA256（如果提供） */
    if (expected_sha256_hex != NULL && expected_sha256_hex[0] != '\0') {
        esp_err_t ret = ota_verify_sha256(partition, firmware_size, expected_sha256_hex);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    /* 2. 验证 RSA 签名 */
    return ota_verify_rsa_signature(partition, firmware_size, signature, sig_len);
}

esp_err_t ota_verify_firmware_header(const uint8_t *header_data, size_t len)
{
    if (header_data == NULL || len < 1) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 检查 ESP32 固件魔数 */
    if (header_data[0] != ESP_IMAGE_HEADER_MAGIC) {
        ESP_LOGE(TAG, "Invalid firmware header magic: 0x%02X (expected 0x%02X)",
                 header_data[0], ESP_IMAGE_HEADER_MAGIC);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Firmware header valid (magic: 0x%02X)", header_data[0]);
    return ESP_OK;
}

esp_err_t ota_verify_get_public_key(const uint8_t **key_out, size_t *key_len)
{
    if (key_out == NULL || key_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *key_out = (const uint8_t *)s_rsa_public_key_pem;
    *key_len = sizeof(s_rsa_public_key_pem);

    return ESP_OK;
}

bool ota_verify_is_public_key_loaded(void)
{
    return s_initialized && (s_rsa_public_key_pem[0] != '\0');
}
