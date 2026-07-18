/**
 * @file ota_download.c
 * @brief OTA 下载管理实现
 *
 * @copyright Copyright (c) 2026 RobotBuddy
 * @author RobotBuddy Team
 * @date 2026-07-19
 */

#include "ota_download.h"
#include "ota_manager.h"
#include "ota_partition.h"
#include "ota_verify.h"
#include "ota_security.h"

#include "event_bus.h"
#include "robot_events.h"

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_heap_caps.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdlib.h>

/* ============================================================
 * 日志标签
 * ============================================================ */

static const char *TAG = OTA_LOG_TAG_DOWNLOAD;

/* ============================================================
 * 模块状态
 * ============================================================ */

static bool s_initialized = false;
static volatile bool s_cancel_flag = false;
static volatile bool s_download_in_progress = false;
static size_t s_downloaded_bytes = 0;
static int s_last_http_status = 0;
static char s_last_error_desc[128] = {0};

/* ============================================================
 * 内部函数
 * ============================================================ */

/**
 * @brief HTTP 事件处理器
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP event: ERROR");
            strncpy(s_last_error_desc, "HTTP error", sizeof(s_last_error_desc) - 1);
            break;

        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP event: CONNECTED");
            break;

        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP event: HEADER_SENT");
            break;

        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP event: ON_HEADER: %s=%s",
                     evt->header_key, evt->header_value);
            break;

        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP event: ON_DATA (%d bytes)", evt->data_len);
            break;

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP event: ON_FINISH");
            break;

        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP event: DISCONNECTED");
            strncpy(s_last_error_desc, "Disconnected", sizeof(s_last_error_desc) - 1);
            break;

        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP event: REDIRECT");
            break;
    }

    return ESP_OK;
}

/**
 * @brief 发布下载进度事件
 */
static void publish_progress_event(size_t downloaded, size_t total)
{
    ota_progress_event_t payload = {
        .percent = (total > 0) ? (uint8_t)((downloaded * 100) / total) : 0,
        .downloaded = (uint32_t)downloaded,
        .total = (uint32_t)total,
    };

    robot_event_t event = {
        .id = EVENT_OTA_PROGRESS,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };

    void *payload_copy = malloc(sizeof(payload));
    if (payload_copy) {
        memcpy(payload_copy, &payload, sizeof(payload));
        event.payload = payload_copy;
        event.payload_len = sizeof(payload);
        event_bus_publish(&event);
    }
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

esp_err_t ota_download_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_cancel_flag = false;
    s_download_in_progress = false;
    s_downloaded_bytes = 0;
    s_last_http_status = 0;
    memset(s_last_error_desc, 0, sizeof(s_last_error_desc));

    s_initialized = true;
    ESP_LOGI(TAG, "OTA download module initialized");
    return ESP_OK;
}

esp_err_t ota_download_deinit(void)
{
    s_initialized = false;
    return ESP_OK;
}

esp_err_t ota_download_firmware(const char *url,
                                const esp_partition_t *target_partition,
                                ota_progress_callback_t progress_cb)
{
    if (url == NULL || target_partition == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    /* 重置状态 */
    s_cancel_flag = false;
    s_download_in_progress = true;
    s_downloaded_bytes = 0;
    s_last_http_status = 0;

    esp_err_t ret = ESP_FAIL;
    esp_ota_handle_t ota_handle = 0;
    bool ota_begun = false;
    bool header_validated = false;
    uint8_t *buffer = NULL;

    /* 分配下载缓冲区（优先使用 PSRAM） */
    buffer = (uint8_t *)heap_caps_malloc(OTA_DOWNLOAD_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    if (buffer == NULL) {
        buffer = (uint8_t *)malloc(OTA_DOWNLOAD_BUFFER_SIZE);
    }
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate download buffer");
        s_download_in_progress = false;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Download buffer: %d bytes (%s)",
             OTA_DOWNLOAD_BUFFER_SIZE,
             heap_caps_get_allocated_size(buffer) ? "PSRAM" : "internal");

    /* 配置 HTTP 客户端 */
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = OTA_HTTP_TIMEOUT_MS,
        .event_handler = http_event_handler,
        .buffer_size = OTA_DOWNLOAD_BUFFER_SIZE,
        .is_async = false,
        .skip_cert_common_name_check = ota_security_skip_cn_check(),
    };

    /* 创建 HTTP 客户端 */
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to create HTTP client");
        free(buffer);
        s_download_in_progress = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Connecting to: %s", url);

    /* 打开连接 */
    ret = esp_http_client_open(client, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(ret));
        snprintf(s_last_error_desc, sizeof(s_last_error_desc),
                 "Connection failed: %s", esp_err_to_name(ret));
        esp_http_client_cleanup(client);
        free(buffer);
        s_download_in_progress = false;
        return ret;
    }

    /* 获取内容长度 */
    int content_length = esp_http_client_fetch_headers(client);
    s_last_http_status = esp_http_client_get_status_code(client);

    if (s_last_http_status != 200) {
        ESP_LOGE(TAG, "HTTP status %d (expected 200)", s_last_http_status);
        snprintf(s_last_error_desc, sizeof(s_last_error_desc),
                 "HTTP status %d", s_last_http_status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        free(buffer);
        s_download_in_progress = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Content length: %d bytes", content_length);

    /* 发布开始事件 */
    robot_event_t start_event = {
        .id = EVENT_OTA_START,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };
    event_bus_publish(&start_event);

    /* 更新管理器状态 */
    ota_manager_update_progress(0, content_length);

    /* 下载循环 */
    int data_read = 0;
    size_t total_downloaded = 0;
    size_t last_reported = 0;

    while ((data_read = esp_http_client_read(client, (char *)buffer,
                                             OTA_DOWNLOAD_BUFFER_SIZE)) > 0) {
        /* 检查取消标志 */
        if (s_cancel_flag) {
            ESP_LOGW(TAG, "Download cancelled by user");
            if (ota_begun) {
                ota_partition_abort_write(ota_handle);
            }
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            free(buffer);
            s_cancel_flag = false;
            s_download_in_progress = false;
            return ESP_ERR_USER_CANCEL;
        }

        /* 验证固件头（仅第一次） */
        if (!header_validated) {
            ret = ota_verify_firmware_header(buffer, data_read);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Invalid firmware header");
                snprintf(s_last_error_desc, sizeof(s_last_error_desc),
                         "Invalid firmware header");
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                free(buffer);
                s_download_in_progress = false;
                return ret;
            }
            header_validated = true;

            /* 开始 OTA 写入 */
            ret = ota_partition_begin_write(target_partition, content_length, &ota_handle);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to begin OTA write: %s", esp_err_to_name(ret));
                snprintf(s_last_error_desc, sizeof(s_last_error_desc),
                         "OTA begin failed: %s", esp_err_to_name(ret));
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                free(buffer);
                s_download_in_progress = false;
                return ret;
            }
            ota_begun = true;
            ESP_LOGI(TAG, "OTA write started (partition: %s)", target_partition->label);
        }

        /* 写入分区 */
        ret = ota_partition_write(ota_handle, buffer, data_read);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write to partition: %s", esp_err_to_name(ret));
            snprintf(s_last_error_desc, sizeof(s_last_error_desc),
                     "Write failed: %s", esp_err_to_name(ret));
            ota_partition_abort_write(ota_handle);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            free(buffer);
            s_download_in_progress = false;
            return ret;
        }

        total_downloaded += data_read;
        s_downloaded_bytes = total_downloaded;

        /* 更新进度（每 10% 或 100KB 报告一次） */
        if (content_length > 0) {
            size_t threshold = (content_length / 10);
            if (threshold < OTA_RESUME_CHUNK_SIZE) {
                threshold = OTA_RESUME_CHUNK_SIZE;
            }
            if (total_downloaded - last_reported >= threshold ||
                total_downloaded >= (size_t)content_length) {
                last_reported = total_downloaded;

                /* 更新管理器 */
                ota_manager_update_progress(total_downloaded, content_length);

                /* 发布事件 */
                publish_progress_event(total_downloaded, content_length);

                /* 调用回调 */
                if (progress_cb != NULL) {
                    ota_progress_t prog = {
                        .state = OTA_STATE_DOWNLOADING,
                        .progress_percent = (int)((total_downloaded * 100) / content_length),
                        .bytes_downloaded = total_downloaded,
                        .bytes_total = content_length,
                    };
                    progress_cb(&prog);
                }

                ESP_LOGD(TAG, "Downloaded: %zu / %d bytes (%d%%)",
                         total_downloaded, content_length,
                         (int)((total_downloaded * 100) / content_length));
            }
        }
    }

    /* 检查读取错误 */
    if (data_read < 0) {
        ESP_LOGE(TAG, "HTTP read error: %d", data_read);
        snprintf(s_last_error_desc, sizeof(s_last_error_desc), "Read error");
        if (ota_begun) {
            ota_partition_abort_write(ota_handle);
        }
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        free(buffer);
        s_download_in_progress = false;
        return ESP_FAIL;
    }

    /* 关闭 HTTP 连接 */
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    /* 检查下载完整性 */
    if (content_length > 0 && total_downloaded != (size_t)content_length) {
        ESP_LOGE(TAG, "Download incomplete: %zu / %d bytes",
                 total_downloaded, content_length);
        snprintf(s_last_error_desc, sizeof(s_last_error_desc),
                 "Incomplete download: %zu/%d", total_downloaded, content_length);
        if (ota_begun) {
            ota_partition_abort_write(ota_handle);
        }
        free(buffer);
        s_download_in_progress = false;
        return ESP_ERR_DOWNLOAD_INCOMPLETE;
    }

    /* 结束 OTA 写入 */
    if (ota_begun) {
        ret = ota_partition_end_write(ota_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "OTA end failed: %s", esp_err_to_name(ret));
            snprintf(s_last_error_desc, sizeof(s_last_error_desc),
                     "OTA end failed: %s", esp_err_to_name(ret));
            free(buffer);
            s_download_in_progress = false;
            return ret;
        }
    }

    /* 清理 */
    free(buffer);
    s_download_in_progress = false;

    ESP_LOGI(TAG, "Download completed: %zu bytes", total_downloaded);

    /* 发布完成事件 */
    robot_event_t complete_event = {
        .id = EVENT_OTA_COMPLETE,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };
    event_bus_publish(&complete_event);

    return ESP_OK;
}

esp_err_t ota_download_resume(const char *url,
                              size_t offset,
                              const esp_partition_t *target_partition,
                              ota_progress_callback_t progress_cb)
{
    if (url == NULL || target_partition == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Resuming download from offset %zu", offset);

    /* 构建带 Range 头的 URL 或使用 HTTP Range 选项 */
    /* 注意：ESP-IDF 的 esp_http_client 支持 Range 请求 */

    /* 对于断点续传，我们需要：
     * 1. 在目标分区跳过已写入的部分
     * 2. 使用 HTTP Range 头请求剩余数据
     */

    /* 目前简化处理：重新开始下载 */
    /* TODO: 实现真正的断点续传 */

    return ota_download_firmware(url, target_partition, progress_cb);
}

esp_err_t ota_download_cancel(void)
{
    if (!s_download_in_progress) {
        ESP_LOGW(TAG, "No download in progress to cancel");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Cancel requested");
    s_cancel_flag = true;

    return ESP_OK;
}

bool ota_download_is_in_progress(void)
{
    return s_download_in_progress;
}

size_t ota_download_get_downloaded_bytes(void)
{
    return s_downloaded_bytes;
}

esp_err_t ota_download_signature(const char *url,
                                  uint8_t *signature,
                                  size_t *sig_len)
{
    if (url == NULL || signature == NULL || sig_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_FAIL;
    uint8_t *buffer = (uint8_t *)malloc(OTA_SIGNATURE_MAX_SIZE);
    if (buffer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    /* 配置 HTTP 客户端 */
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = OTA_HTTP_TIMEOUT_MS,
        .event_handler = http_event_handler,
        .is_async = false,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        free(buffer);
        return ESP_FAIL;
    }

    /* 打开连接 */
    ret = esp_http_client_open(client, 0);
    if (ret != ESP_OK) {
        esp_http_client_cleanup(client);
        free(buffer);
        return ret;
    }

    /* 获取内容长度 */
    int content_length = esp_http_client_fetch_headers(client);
    int http_status = esp_http_client_get_status_code(client);

    if (http_status != 200) {
        ESP_LOGE(TAG, "Signature download failed: HTTP %d", http_status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        free(buffer);
        return ESP_FAIL;
    }

    /* 读取签名数据 */
    int data_read = esp_http_client_read(client, (char *)buffer, OTA_SIGNATURE_MAX_SIZE);
    if (data_read <= 0) {
        ESP_LOGE(TAG, "Failed to read signature data");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        free(buffer);
        return ESP_FAIL;
    }

    /* 复制签名 */
    if ((size_t)data_read > *sig_len) {
        ESP_LOGW(TAG, "Signature too large: %d > %zu", data_read, *sig_len);
        data_read = (int)*sig_len;
    }

    memcpy(signature, buffer, data_read);
    *sig_len = (size_t)data_read;

    ESP_LOGI(TAG, "Signature downloaded: %d bytes", data_read);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(buffer);

    return ESP_OK;
}

esp_err_t ota_download_check_update(const char *device_id,
                                    const char *current_version,
                                    ota_update_info_t *info)
{
    if (info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 初始化输出 */
    memset(info, 0, sizeof(ota_update_info_t));

    /* TODO: 实现版本检查 HTTP 请求 */
    /* 格式: GET /ota/check?device_id=xxx&version=xxx */

    ESP_LOGI(TAG, "Checking for updates (device: %s, version: %s)",
             device_id ? device_id : "N/A",
             current_version ? current_version : "N/A");

    /* 目前返回无更新 */
    info->level = OTA_UPDATE_NONE;

    return ESP_OK;
}

int ota_download_get_last_http_status(void)
{
    return s_last_http_status;
}

void ota_download_get_last_error_desc(char *buf, size_t buf_size)
{
    if (buf != NULL && buf_size > 0) {
        strncpy(buf, s_last_error_desc, buf_size - 1);
        buf[buf_size - 1] = '\0';
    }
}
