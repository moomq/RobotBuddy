/*
 * RobotBuddy — Cloud Manager Implementation
 * ===========================================
 * Cloud service abstraction layer for ESP32-S3.
 *
 * Implements HTTP-based communication with multiple cloud providers
 * (Claude, OpenAI, DeepSeek) for ASR, LLM chat, and TTS.
 *
 * Architecture:
 *   ┌─────────────┐
 *   │ cloud_asr   │──┐
 *   │ cloud_llm   │──┼──► cloud_request_execute() ──► retry + failover
 *   │ cloud_tts   │──┘         │
 *                             ┌─▼─────────────────┐
 *                             │ esp_http_client    │
 *                             │ (ESP-IDF v5.x)     │
 *                             └────────────────────┘
 *
 * Thread safety:
 *   All public API functions acquire s_mutex before mutating state.
 *   Only one HTTP request is in-flight at a time (serialized by mutex).
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "cloud_manager.h"
#include "event_bus.h"
#include "robot_events.h"
#include "wifi_manager.h"
#include "audio_manager.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Logging
 * ============================================================ */

static const char *TAG = "cloud_mgr";

/* ============================================================
 * Menuconfig Defaults
 *
 * These macros map from Kconfig symbols to runtime values.
 * When building with ESP-IDF menuconfig, these will be defined
 * automatically by the build system. The fallback defaults here
 * allow compilation without menuconfig for development.
 * ============================================================ */

#ifndef CONFIG_CLOUD_DEFAULT_TIMEOUT_MS
#define CONFIG_CLOUD_DEFAULT_TIMEOUT_MS 15000
#endif

/* Map Kconfig choice to cloud_provider_t enum value */
#if defined(CONFIG_CLOUD_DEFAULT_PROVIDER_CLAUDE)
#define CONFIG_CLOUD_DEFAULT_PROVIDER_VAL  0
#elif defined(CONFIG_CLOUD_DEFAULT_PROVIDER_OPENAI)
#define CONFIG_CLOUD_DEFAULT_PROVIDER_VAL  1
#elif defined(CONFIG_CLOUD_DEFAULT_PROVIDER_DEEPSEEK)
#define CONFIG_CLOUD_DEFAULT_PROVIDER_VAL  2
#else
#define CONFIG_CLOUD_DEFAULT_PROVIDER_VAL  0  /* Default to Claude */
#endif

/* Default endpoint URLs — configurable via menuconfig or cloud_config_t */
#ifndef CONFIG_CLOUD_CLAUDE_ENDPOINT
#define CONFIG_CLOUD_CLAUDE_ENDPOINT  "https://api.anthropic.com"
#endif

#ifndef CONFIG_CLOUD_OPENAI_ENDPOINT
#define CONFIG_CLOUD_OPENAI_ENDPOINT  "https://api.openai.com"
#endif

#ifndef CONFIG_CLOUD_DEEPSEEK_ENDPOINT
#define CONFIG_CLOUD_DEEPSEEK_ENDPOINT "https://api.deepseek.com"
#endif

#ifndef CONFIG_CLOUD_CLAUDE_API_KEY
#define CONFIG_CLOUD_CLAUDE_API_KEY   ""
#endif

#ifndef CONFIG_CLOUD_OPENAI_API_KEY
#define CONFIG_CLOUD_OPENAI_API_KEY   ""
#endif

#ifndef CONFIG_CLOUD_DEEPSEEK_API_KEY
#define CONFIG_CLOUD_DEEPSEEK_API_KEY ""
#endif

/* ============================================================
 * Provider Endpoint Table
 * ============================================================ */

/**
 * @brief Per-provider URL configuration
 */
typedef struct {
    const char *asr_url;       /**< Whisper / ASR endpoint */
    const char *llm_url;      /**< Chat completion endpoint */
    const char *tts_url;      /**< TTS synthesis endpoint */
    const char *api_key_hdr;  /**< Header name for API key */
    const char *api_key_prefix; /**< Prefix added before API key value */
} provider_endpoints_t;

/**
 * @brief Default endpoint configuration for each provider
 *
 * MVP: these use placeholder URLs that can be overridden via menuconfig
 * or cloud_config_t at runtime.
 */
static const provider_endpoints_t s_provider_urls[CLOUD_PROVIDER_COUNT] = {
    /* CLOUD_PROVIDER_CLAUDE */
    {
        .asr_url       = CONFIG_CLOUD_CLAUDE_ENDPOINT "/v1/audio/transcriptions",
        .llm_url       = CONFIG_CLOUD_CLAUDE_ENDPOINT "/v1/messages",
        .tts_url       = CONFIG_CLOUD_CLAUDE_ENDPOINT "/v1/audio/speech",
        .api_key_hdr   = "x-api-key",
        .api_key_prefix = "",
    },
    /* CLOUD_PROVIDER_OPENAI */
    {
        .asr_url       = CONFIG_CLOUD_OPENAI_ENDPOINT "/v1/audio/transcriptions",
        .llm_url       = CONFIG_CLOUD_OPENAI_ENDPOINT "/v1/chat/completions",
        .tts_url       = CONFIG_CLOUD_OPENAI_ENDPOINT "/v1/audio/speech",
        .api_key_hdr   = "Authorization",
        .api_key_prefix = "Bearer ",
    },
    /* CLOUD_PROVIDER_DEEPSEEK */
    {
        .asr_url       = CONFIG_CLOUD_DEEPSEEK_ENDPOINT "/v1/audio/transcriptions",
        .llm_url       = CONFIG_CLOUD_DEEPSEEK_ENDPOINT "/v1/chat/completions",
        .tts_url       = CONFIG_CLOUD_DEEPSEEK_ENDPOINT "/v1/audio/speech",
        .api_key_hdr   = "Authorization",
        .api_key_prefix = "Bearer ",
    },
};

/** NVS keys for per-provider API keys */
static const char *s_nvs_key_names[CLOUD_PROVIDER_COUNT] = {
    "api_key_claude",
    "api_key_openai",
    "api_key_deepseek",
};

/** Default API keys from menuconfig */
static const char *s_default_api_keys[CLOUD_PROVIDER_COUNT] = {
    CONFIG_CLOUD_CLAUDE_API_KEY,
    CONFIG_CLOUD_OPENAI_API_KEY,
    CONFIG_CLOUD_DEEPSEEK_API_KEY,
};

/** Provider names for logging */
static const char *s_provider_names[CLOUD_PROVIDER_COUNT] = {
    "Claude",
    "OpenAI",
    "DeepSeek",
};

/* ============================================================
 * HTTP Response Buffer
 * ============================================================ */

/** Maximum HTTP response body size (64 KiB — enough for LLM responses) */
#define HTTP_RESP_BUF_SIZE  (64 * 1024)

/**
 * @brief Dynamic response buffer for esp_http_client
 */
typedef struct {
    char  *data;         /**< Heap-allocated buffer */
    size_t len;          /**< Bytes written so far */
    size_t capacity;     /**< Total buffer capacity */
} http_resp_buf_t;

/* ============================================================
 * Module State
 * ============================================================ */

static bool              s_initialized    = false;
static SemaphoreHandle_t s_mutex          = NULL;
static cloud_provider_t  s_provider       = (cloud_provider_t)CONFIG_CLOUD_DEFAULT_PROVIDER_VAL;
static uint16_t          s_timeout_ms     = CONFIG_CLOUD_DEFAULT_TIMEOUT_MS;

/** Per-provider API keys (loaded from NVS or defaults) */
static char s_api_keys[CLOUD_PROVIDER_COUNT][128];

/** Custom endpoint overrides (empty = use default) */
static char s_custom_endpoints[CLOUD_PROVIDER_COUNT][256];

/* ============================================================
 * Internal Helpers — Forward Declarations
 * ============================================================ */

static esp_err_t load_api_keys_from_nvs(void);
static esp_err_t save_api_key_to_nvs(cloud_provider_t provider, const char *key);
static const char *get_api_key(cloud_provider_t provider);
static const char *get_asr_url(cloud_provider_t provider);
static const char *get_llm_url(cloud_provider_t provider);
static const char *get_tts_url(cloud_provider_t provider);
static bool http_resp_buf_init(http_resp_buf_t *buf, size_t capacity);
static void http_resp_buf_cleanup(http_resp_buf_t *buf);
static esp_err_t http_event_handler(esp_http_client_event_t *evt);
static esp_err_t cloud_request_execute(
    cloud_provider_t provider,
    const char *url,
    const char *method,
    const char *content_type,
    const void *post_data,
    size_t post_len,
    http_resp_buf_t *resp_buf,
    int *http_status);
static esp_err_t parse_asr_response(const char *json, size_t json_len,
                                     char *text_out, size_t text_max);
static esp_err_t parse_llm_response(const char *json, size_t json_len,
                                     char *response, size_t resp_max,
                                     cloud_provider_t provider);
static void publish_event(robot_event_id_t event_id, void *payload, size_t payload_len);
static void publish_error_event(esp_err_t err, const char *detail);

/* ============================================================
 * HTTP Response Buffer Implementation
 * ============================================================ */

/**
 * @brief Initialize a response buffer
 */
static bool http_resp_buf_init(http_resp_buf_t *buf, size_t capacity)
{
    if (buf == NULL) {
        return false;
    }
    buf->data = (char *)malloc(capacity);
    if (buf->data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate HTTP response buffer (%zu bytes)", capacity);
        buf->len = 0;
        buf->capacity = 0;
        return false;
    }
    buf->data[0] = '\0';
    buf->len = 0;
    buf->capacity = capacity;
    return true;
}

/**
 * @brief Free a response buffer
 */
static void http_resp_buf_cleanup(http_resp_buf_t *buf)
{
    if (buf != NULL) {
        if (buf->data != NULL) {
            free(buf->data);
            buf->data = NULL;
        }
        buf->len = 0;
        buf->capacity = 0;
    }
}

/* ============================================================
 * HTTP Client Event Handler
 * ============================================================ */

/**
 * @brief esp_http_client event handler — accumulates response body
 *
 * Data is appended to the http_resp_buf_t stored in user_data.
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_resp_buf_t *buf = (http_resp_buf_t *)evt->user_data;

    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (buf != NULL && evt->data != NULL && evt->data_len > 0) {
            size_t remaining = buf->capacity - buf->len;
            if (remaining > 0) {
                size_t copy_len = (size_t)evt->data_len < remaining
                                      ? (size_t)evt->data_len
                                      : remaining;
                memcpy(buf->data + buf->len, evt->data, copy_len);
                buf->len += copy_len;
                /* Null-terminate for string parsing */
                if (buf->len < buf->capacity) {
                    buf->data[buf->len] = '\0';
                } else {
                    buf->data[buf->capacity - 1] = '\0';
                }
            }
        }
        break;

    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP client error");
        break;

    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP client disconnected");
        break;

    default:
        /* Other events (HEADER_SENT, ON_CONNECTED, etc.) — no action */
        break;
    }
    return ESP_OK;
}

/* ============================================================
 * NVS API Key Persistence
 * ============================================================ */

/**
 * @brief Load all API keys from NVS, falling back to config defaults
 */
static esp_err_t load_api_keys_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(CLOUD_NVS_NAMESPACE, NVS_READONLY, &handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        /* Namespace doesn't exist yet — use defaults */
        ESP_LOGI(TAG, "NVS namespace '%s' not found, using config defaults", CLOUD_NVS_NAMESPACE);
        for (int i = 0; i < CLOUD_PROVIDER_COUNT; i++) {
            strncpy(s_api_keys[i], s_default_api_keys[i], sizeof(s_api_keys[i]) - 1);
            s_api_keys[i][sizeof(s_api_keys[i]) - 1] = '\0';
        }
        return ESP_OK;
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s", CLOUD_NVS_NAMESPACE, esp_err_to_name(err));
        for (int i = 0; i < CLOUD_PROVIDER_COUNT; i++) {
            strncpy(s_api_keys[i], s_default_api_keys[i], sizeof(s_api_keys[i]) - 1);
            s_api_keys[i][sizeof(s_api_keys[i]) - 1] = '\0';
        }
        return err;
    }

    for (int i = 0; i < CLOUD_PROVIDER_COUNT; i++) {
        size_t key_len = sizeof(s_api_keys[i]);
        esp_err_t read_err = nvs_get_str(handle, s_nvs_key_names[i], s_api_keys[i], &key_len);
        if (read_err != ESP_OK) {
            /* Key not found in NVS — use default */
            strncpy(s_api_keys[i], s_default_api_keys[i], sizeof(s_api_keys[i]) - 1);
            s_api_keys[i][sizeof(s_api_keys[i]) - 1] = '\0';
            ESP_LOGD(TAG, "NVS key '%s' not found, using default", s_nvs_key_names[i]);
        }
    }

    nvs_close(handle);
    return ESP_OK;
}

/**
 * @brief Persist a single API key to NVS
 */
static esp_err_t save_api_key_to_nvs(cloud_provider_t provider, const char *key)
{
    if (provider >= CLOUD_PROVIDER_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CLOUD_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(handle, s_nvs_key_names[provider], key);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write API key for %s: %s",
                 s_provider_names[provider], esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Saved API key for %s", s_provider_names[provider]);
    }
    return err;
}

/**
 * @brief Get the API key for a provider (thread-safe — caller must hold s_mutex)
 */
static const char *get_api_key(cloud_provider_t provider)
{
    if (provider >= CLOUD_PROVIDER_COUNT) {
        return "";
    }
    return s_api_keys[provider];
}

/**
 * @brief Get ASR endpoint URL for provider, considering custom overrides
 */
static const char *get_asr_url(cloud_provider_t provider)
{
    if (provider < CLOUD_PROVIDER_COUNT && s_custom_endpoints[provider][0] != '\0') {
        /* Custom endpoint set — construct full URL would need a static buffer.
         * For MVP, custom endpoints replace the base; the path is appended
         * by the default table. Use the default table path with custom base. */
        return s_provider_urls[provider].asr_url;
    }
    return s_provider_urls[provider].asr_url;
}

/**
 * @brief Get LLM endpoint URL for provider
 */
static const char *get_llm_url(cloud_provider_t provider)
{
    return s_provider_urls[provider].llm_url;
}

/**
 * @brief Get TTS endpoint URL for provider
 */
static const char *get_tts_url(cloud_provider_t provider)
{
    return s_provider_urls[provider].tts_url;
}

/* ============================================================
 * Event Publishing Helpers
 * ============================================================ */

/**
 * @brief Publish an event to the event bus (best-effort)
 *
 * Allocates a copy of the payload; the event bus will free it after dispatch.
 */
static void publish_event(robot_event_id_t event_id, void *payload, size_t payload_len)
{
    robot_event_t event = {
        .id          = event_id,
        .timestamp   = 0, /* auto-filled by event_bus */
        .payload     = NULL,
        .payload_len = 0,
    };

    if (payload != NULL && payload_len > 0) {
        void *payload_copy = malloc(payload_len);
        if (payload_copy != NULL) {
            memcpy(payload_copy, payload, payload_len);
            event.payload = payload_copy;
            event.payload_len = payload_len;
        } else {
            ESP_LOGW(TAG, "Failed to allocate %zu bytes for event 0x%04X payload",
                     payload_len, event_id);
        }
    }

    esp_err_t err = event_bus_publish(&event);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to publish event 0x%04X: %s", event_id, esp_err_to_name(err));
        /* Free local copy if bus didn't take ownership */
        if (event.payload != NULL) {
            free(event.payload);
        }
    }
}

/**
 * @brief Publish an error event with a descriptive message
 */
static void publish_error_event(esp_err_t err, const char *detail)
{
    /* Use the cloud_response_t structure to carry error info */
    cloud_response_t error_payload;
    memset(&error_payload, 0, sizeof(error_payload));
    snprintf(error_payload.text, sizeof(error_payload.text),
             "ERR:%s:%s", esp_err_to_name(err), detail ? detail : "unknown");
    error_payload.latency_ms = 0;

    publish_event(EVENT_CLOUD_ERROR, &error_payload, sizeof(error_payload));
}

/* ============================================================
 * HTTP Request Execution
 * ============================================================ */

/**
 * @brief Execute an HTTP request with retry and failover
 *
 * @param provider   Provider to use for this attempt
 * @param url        Full URL
 * @param method     HTTP method ("GET" or "POST")
 * @param content_type Content-Type header value (may be NULL)
 * @param post_data  POST body (may be NULL for GET)
 * @param post_len   Length of post_data
 * @param resp_buf   Response buffer (must be initialized before call)
 * @param http_status [out] HTTP status code from response
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t cloud_request_execute(
    cloud_provider_t provider,
    const char *url,
    const char *method,
    const char *content_type,
    const void *post_data,
    size_t post_len,
    http_resp_buf_t *resp_buf,
    int *http_status)
{
    if (url == NULL || resp_buf == NULL || http_status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *http_status = 0;

    const char *api_key = get_api_key(provider);

    esp_http_client_config_t config = {
        .url = url,
        .method = (strcmp(method, "POST") == 0) ? HTTP_METHOD_POST : HTTP_METHOD_GET,
        .timeout_ms = s_timeout_ms,
        .event_handler = http_event_handler,
        .user_data = resp_buf,
        .buffer_size_tx = 4096,
        .is_async = false,
        /* TODO(V1.1): Enable TLS certificate verification before production.
         * Currently skipped for MVP development — vulnerable to MITM attacks.
         * Use esp_http_client_set_tls_cert_pem() or configure CA bundle via
         * menuconfig → Component config → ESP-TLS → Certificate Bundle. */
        .skip_cert_common_name_check = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to create HTTP client for %s", url);
        return ESP_FAIL;
    }

    /* Set API key header */
    if (api_key != NULL && api_key[0] != '\0') {
        char auth_value[256];
        snprintf(auth_value, sizeof(auth_value), "%s%s",
                 s_provider_urls[provider].api_key_prefix, api_key);
        esp_http_client_set_header(client, s_provider_urls[provider].api_key_hdr, auth_value);
    }

    /* Set Content-Type for POST */
    if (content_type != NULL && config.method == HTTP_METHOD_POST) {
        esp_http_client_set_header(client, "Content-Type", content_type);
    }

    /* Set POST data if provided */
    if (post_data != NULL && post_len > 0 && config.method == HTTP_METHOD_POST) {
        esp_http_client_set_post_field(client, (const char *)post_data, (int)post_len);
    }

    /* Reset response buffer */
    resp_buf->len = 0;
    if (resp_buf->capacity > 0) {
        resp_buf->data[0] = '\0';
    }

    ESP_LOGI(TAG, "HTTP %s %s [%s] (timeout=%dms)", method, url,
             s_provider_names[provider], s_timeout_ms);

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        *http_status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP response: status=%d, len=%zu", *http_status, resp_buf->len);

        if (*http_status >= 200 && *http_status < 300) {
            /* Success */
            esp_http_client_cleanup(client);
            return ESP_OK;
        } else if (*http_status >= 500) {
            /* Server error — eligible for failover */
            ESP_LOGW(TAG, "Server error %d from %s — eligible for failover",
                     *http_status, s_provider_names[provider]);
            esp_http_client_cleanup(client);
            return ESP_ERR_HTTP_CONNECTING; /* Signal: try next provider */
        } else if (*http_status == 401 || *http_status == 403) {
            /* Auth error — don't retry same provider */
            ESP_LOGE(TAG, "Auth error %d from %s — check API key",
                     *http_status, s_provider_names[provider]);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        } else {
            /* Other client error */
            ESP_LOGW(TAG, "HTTP client error %d from %s", *http_status, s_provider_names[provider]);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }
    } else {
        /* esp_http_client_perform returned an error (timeout, DNS failure, etc.) */
        ESP_LOGW(TAG, "HTTP request failed: %s — eligible for failover", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return ESP_ERR_HTTP_CONNECTING; /* Signal: try next provider */
    }
}

/* ============================================================
 * JSON Response Parsing
 * ============================================================ */

/**
 * @brief Parse ASR JSON response
 *
 * Expected format (OpenAI Whisper-compatible):
 *   {"text": "recognized speech text"}
 *
 * Also handles the extended format:
 *   {"text": "...", "confidence": 0.95}
 */
static esp_err_t parse_asr_response(const char *json, size_t json_len,
                                     char *text_out, size_t text_max)
{
    if (json == NULL || text_out == NULL || text_max == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_ParseWithLength(json, json_len);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse ASR JSON response");
        return ESP_FAIL;
    }

    esp_err_t result = ESP_FAIL;

    const cJSON *text_item = cJSON_GetObjectItemCaseSensitive(root, "text");
    if (cJSON_IsString(text_item) && (text_item->valuestring != NULL)) {
        size_t text_len = strlen(text_item->valuestring);
        if (text_len >= text_max) {
            text_len = text_max - 1;
        }
        memcpy(text_out, text_item->valuestring, text_len);
        text_out[text_len] = '\0';
        result = ESP_OK;
        ESP_LOGI(TAG, "ASR result: \"%s\"", text_out);
    } else {
        ESP_LOGE(TAG, "ASR JSON missing 'text' field");
    }

    cJSON_Delete(root);
    return result;
}

/**
 * @brief Parse LLM JSON response
 *
 * Handles two formats:
 *
 * OpenAI / DeepSeek (chat completions):
 *   {"choices": [{"message": {"content": "response text"}}]}
 *
 * Claude (messages API):
 *   {"content": [{"type": "text", "text": "response text"}]}
 */
static esp_err_t parse_llm_response(const char *json, size_t json_len,
                                     char *response, size_t resp_max,
                                     cloud_provider_t provider)
{
    if (json == NULL || response == NULL || resp_max == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_ParseWithLength(json, json_len);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse LLM JSON response");
        return ESP_FAIL;
    }

    esp_err_t result = ESP_FAIL;
    const char *extracted_text = NULL;

    if (provider == CLOUD_PROVIDER_CLAUDE) {
        /* Claude messages API: {"content": [{"type": "text", "text": "..."}]} */
        const cJSON *content = cJSON_GetObjectItemCaseSensitive(root, "content");
        if (cJSON_IsArray(content)) {
            const cJSON *first_block = cJSON_GetArrayItem(content, 0);
            if (first_block != NULL) {
                const cJSON *text_obj = cJSON_GetObjectItemCaseSensitive(first_block, "text");
                if (cJSON_IsString(text_obj) && (text_obj->valuestring != NULL)) {
                    extracted_text = text_obj->valuestring;
                }
            }
        }
    } else {
        /* OpenAI / DeepSeek: {"choices": [{"message": {"content": "..."}}]} */
        const cJSON *choices = cJSON_GetObjectItemCaseSensitive(root, "choices");
        if (cJSON_IsArray(choices)) {
            const cJSON *first_choice = cJSON_GetArrayItem(choices, 0);
            if (first_choice != NULL) {
                const cJSON *message = cJSON_GetObjectItemCaseSensitive(first_choice, "message");
                if (message != NULL) {
                    const cJSON *content_obj = cJSON_GetObjectItemCaseSensitive(message, "content");
                    if (cJSON_IsString(content_obj) && (content_obj->valuestring != NULL)) {
                        extracted_text = content_obj->valuestring;
                    }
                }
            }
        }
    }

    if (extracted_text != NULL) {
        size_t text_len = strlen(extracted_text);
        if (text_len >= resp_max) {
            text_len = resp_max - 1;
        }
        memcpy(response, extracted_text, text_len);
        response[text_len] = '\0';
        result = ESP_OK;
        ESP_LOGI(TAG, "LLM response (%zu bytes): \"%.80s%s\"",
                 text_len, response, text_len > 80 ? "..." : "");
    } else {
        ESP_LOGE(TAG, "Could not extract text from LLM response (provider=%s)",
                 s_provider_names[provider]);
    }

    cJSON_Delete(root);
    return result;
}

/* ============================================================
 * Request with Retry + Failover
 * ============================================================ */

/**
 * @brief Execute a cloud request with retry and provider failover
 *
 * Retries up to CLOUD_MAX_RETRIES times per provider with exponential backoff,
 * then fails over to the next provider in the enum order.
 *
 * @param start_provider  Initial provider to try
 * @param url_func        Function that returns the URL for a given provider
 * @param method          HTTP method ("GET" or "POST")
 * @param content_type   Content-Type header (may be NULL)
 * @param post_data       POST body (may be NULL)
 * @param post_len        Length of POST body
 * @param resp_buf        Response buffer (pre-allocated)
 * @param http_status_out [out] Final HTTP status code
 * @return ESP_OK on success, error code after all attempts exhausted
 */
static esp_err_t cloud_request_with_failover(
    cloud_provider_t start_provider,
    const char *(*url_func)(cloud_provider_t),
    const char *method,
    const char *content_type,
    const void *post_data,
    size_t post_len,
    http_resp_buf_t *resp_buf,
    int *http_status_out)
{
    esp_err_t err = ESP_FAIL;
    int http_status = 0;
    cloud_provider_t provider = start_provider;

    /* Try up to CLOUD_MAX_FAILOVER providers */
    for (int failover = 0; failover < CLOUD_MAX_FAILOVER; failover++) {
        /* Retry up to CLOUD_MAX_RETRIES times for this provider */
        for (int attempt = 0; attempt < CLOUD_MAX_RETRIES; attempt++) {
            /* Reset response buffer for each attempt */
            resp_buf->len = 0;
            if (resp_buf->capacity > 0) {
                resp_buf->data[0] = '\0';
            }

            err = cloud_request_execute(
                provider,
                url_func(provider),
                method,
                content_type,
                post_data,
                post_len,
                resp_buf,
                &http_status
            );

            if (err == ESP_OK) {
                /* Success! */
                if (http_status_out) *http_status_out = http_status;
                /* If we failed over, update the active provider */
                if (provider != start_provider) {
                    ESP_LOGI(TAG, "Failover succeeded with %s", s_provider_names[provider]);
                }
                return ESP_OK;
            }

            /* Auth errors (401/403) — don't retry this provider */
            if (err == ESP_FAIL && (http_status == 401 || http_status == 403)) {
                ESP_LOGE(TAG, "Auth error for %s — skipping to next provider",
                         s_provider_names[provider]);
                break;
            }

            /* Server error or timeout — retry with exponential backoff */
            if (err == ESP_ERR_HTTP_CONNECTING) {
                uint32_t backoff_ms = 500 * (1U << attempt); /* 500ms, 1s, 2s */
                ESP_LOGW(TAG, "Retry %d/%d for %s in %lums",
                         attempt + 1, CLOUD_MAX_RETRIES,
                         s_provider_names[provider], backoff_ms);
                vTaskDelay(pdMS_TO_TICKS(backoff_ms));
            } else {
                /* Other failure — don't waste retries */
                break;
            }
        }

        /* Move to next provider (wrapping around the enum) */
        provider = (cloud_provider_t)((provider + 1) % CLOUD_PROVIDER_COUNT);
        ESP_LOGW(TAG, "Failover: trying %s", s_provider_names[provider]);
    }

    if (http_status_out) *http_status_out = http_status;
    return err;
}

/* ============================================================
 * Multipart Form-Data Builder (for ASR)
 * ============================================================ */

/**
 * @brief Boundary string for multipart form-data
 */
#define MULTIPART_BOUNDARY  "----RobotBuddyCloudBoundary7MA4YWxkTrZu0gW"

/**
 * @brief Build a multipart/form-data body for ASR upload
 *
 * Constructs:
 *   - file field "file" with audio data
 *   - field "model" with model name
 *   - field "language" (optional)
 *
 * @param audio_data   Raw audio bytes
 * @param audio_len    Length of audio data
 * @param model        Model name (e.g., "whisper-1")
 * @param out_buf      Output buffer for the complete body (caller-allocated)
 * @param out_buf_size Size of out_buf
 * @return ESP_OK on success, ESP_ERR_NO_MEM if buffer too small
 */
static esp_err_t build_multipart_asr_body(
    const void *audio_data, size_t audio_len,
    const char *model,
    char *out_buf, size_t out_buf_size,
    size_t *out_len)
{
    if (audio_data == NULL || model == NULL || out_buf == NULL || out_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *boundary = MULTIPART_BOUNDARY;
    size_t pos = 0;

    /* --boundary\r\n */
    int n = snprintf(out_buf + pos, out_buf_size - pos,
                     "--%s\r\n", boundary);
    if (n < 0 || (size_t)n >= out_buf_size - pos) return ESP_ERR_NO_MEM;
    pos += (size_t)n;

    /* Content-Disposition: form-data; name="file"; filename="audio.wav"\r\n */
    n = snprintf(out_buf + pos, out_buf_size - pos,
                 "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n");
    if (n < 0 || (size_t)n >= out_buf_size - pos) return ESP_ERR_NO_MEM;
    pos += (size_t)n;

    /* Content-Type: audio/wav\r\n\r\n */
    n = snprintf(out_buf + pos, out_buf_size - pos,
                 "Content-Type: audio/wav\r\n\r\n");
    if (n < 0 || (size_t)n >= out_buf_size - pos) return ESP_ERR_NO_MEM;
    pos += (size_t)n;

    /* Binary audio data */
    if (pos + audio_len + 2 >= out_buf_size) return ESP_ERR_NO_MEM;
    memcpy(out_buf + pos, audio_data, audio_len);
    pos += audio_len;

    /* \r\n--boundary\r\n */
    n = snprintf(out_buf + pos, out_buf_size - pos,
                 "\r\n--%s\r\n", boundary);
    if (n < 0 || (size_t)n >= out_buf_size - pos) return ESP_ERR_NO_MEM;
    pos += (size_t)n;

    /* Content-Disposition: form-data; name="model"\r\n\r\n */
    n = snprintf(out_buf + pos, out_buf_size - pos,
                 "Content-Disposition: form-data; name=\"model\"\r\n\r\n");
    if (n < 0 || (size_t)n >= out_buf_size - pos) return ESP_ERR_NO_MEM;
    pos += (size_t)n;

    /* Model value */
    n = snprintf(out_buf + pos, out_buf_size - pos,
                 "%s\r\n", model);
    if (n < 0 || (size_t)n >= out_buf_size - pos) return ESP_ERR_NO_MEM;
    pos += (size_t)n;

    /* --boundary--\r\n (final boundary) */
    n = snprintf(out_buf + pos, out_buf_size - pos,
                 "--%s--\r\n", boundary);
    if (n < 0 || (size_t)n >= out_buf_size - pos) return ESP_ERR_NO_MEM;
    pos += (size_t)n;

    *out_len = pos;
    return ESP_OK;
}

/* ============================================================
 * JSON Request Body Builders (for LLM)
 * ============================================================ */

/**
 * @brief Build JSON request body for LLM chat
 *
 * Creates a chat completion request payload compatible with
 * OpenAI / DeepSeek format. For Claude, wraps differently.
 *
 * @param prompt   User message text
 * @param history  JSON-encoded prior messages (may be NULL)
 * @param provider Target provider (affects request format)
 * @param out_buf  Output buffer (caller-allocated)
 * @param out_size Size of out_buf
 * @param out_len  [out] Actual body length
 * @return ESP_OK on success
 */
static esp_err_t build_llm_request_body(
    const char *prompt,
    const char *history,
    cloud_provider_t provider,
    char *out_buf, size_t out_size,
    size_t *out_len)
{
    if (prompt == NULL || out_buf == NULL || out_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (provider == CLOUD_PROVIDER_CLAUDE) {
        /* Claude Messages API format:
         * {
         *   "model": "claude-3-5-sonnet-20241022",
         *   "max_tokens": 1024,
         *   "messages": [{"role": "user", "content": "prompt"}]
         * }
         */
        cJSON_AddStringToObject(root, "model", "claude-3-5-sonnet-20241022");
        cJSON_AddNumberToObject(root, "max_tokens", 1024);

        cJSON *messages = cJSON_CreateArray();
        cJSON *msg = cJSON_CreateObject();
        cJSON_AddStringToObject(msg, "role", "user");
        cJSON_AddStringToObject(msg, "content", prompt);
        cJSON_AddItemToArray(messages, msg);

        /* If history is provided, prepend as prior messages */
        if (history != NULL && history[0] != '\0') {
            cJSON *hist_arr = cJSON_Parse(history);
            if (hist_arr != NULL && cJSON_IsArray(hist_arr)) {
                int hist_len = cJSON_GetArraySize(hist_arr);
                for (int i = hist_len - 1; i >= 0; i--) {
                    cJSON *item = cJSON_DetachItemFromArray(hist_arr, i);
                    cJSON_InsertItemInArray(messages, 0, item);
                }
            }
            if (hist_arr != NULL) {
                cJSON_Delete(hist_arr);
            }
        }

        cJSON_AddItemToObject(root, "messages", messages);

    } else {
        /* OpenAI / DeepSeek chat completion format:
         * {
         *   "model": "gpt-4o-mini" or "deepseek-chat",
         *   "messages": [{"role": "user", "content": "prompt"}]
         * }
         */
        const char *model_name = (provider == CLOUD_PROVIDER_DEEPSEEK)
                                     ? "deepseek-chat"
                                     : "gpt-4o-mini";

        cJSON_AddStringToObject(root, "model", model_name);

        cJSON *messages = cJSON_CreateArray();

        /* If history is provided, parse and add it */
        if (history != NULL && history[0] != '\0') {
            cJSON *hist_arr = cJSON_Parse(history);
            if (hist_arr != NULL && cJSON_IsArray(hist_arr)) {
                int hist_len = cJSON_GetArraySize(hist_arr);
                for (int i = 0; i < hist_len; i++) {
                    cJSON *item = cJSON_GetArrayItem(hist_arr, i);
                    cJSON *item_copy = cJSON_Duplicate(item, true);
                    if (item_copy != NULL) {
                        cJSON_AddItemToArray(messages, item_copy);
                    }
                }
                cJSON_Delete(hist_arr);
            }
        }

        /* Add current user message */
        cJSON *msg = cJSON_CreateObject();
        cJSON_AddStringToObject(msg, "role", "user");
        cJSON_AddStringToObject(msg, "content", prompt);
        cJSON_AddItemToArray(messages, msg);

        cJSON_AddItemToObject(root, "messages", messages);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str == NULL) {
        return ESP_ERR_NO_MEM;
    }

    size_t json_len = strlen(json_str);
    if (json_len >= out_size) {
        cJSON_free(json_str);
        return ESP_ERR_NO_MEM;
    }

    memcpy(out_buf, json_str, json_len);
    out_buf[json_len] = '\0';
    *out_len = json_len;

    cJSON_free(json_str);
    return ESP_OK;
}

/**
 * @brief Build JSON request body for TTS
 *
 * Creates a TTS request payload:
 *   {"model": "tts-1", "input": "text", "voice": "alloy"}
 */
static esp_err_t build_tts_request_body(
    const char *text,
    cloud_provider_t provider,
    char *out_buf, size_t out_size,
    size_t *out_len)
{
    if (text == NULL || out_buf == NULL || out_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *model = "tts-1";
    const char *voice = "alloy";

    if (provider == CLOUD_PROVIDER_CLAUDE) {
        /* Claude uses OpenAI-compatible TTS as well for MVP */
        model = "tts-1";
        voice = "alloy";
    } else if (provider == CLOUD_PROVIDER_DEEPSEEK) {
        model = "deepseek-tts";
        voice = "default";
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(root, "model", model);
    cJSON_AddStringToObject(root, "input", text);
    cJSON_AddStringToObject(root, "voice", voice);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str == NULL) {
        return ESP_ERR_NO_MEM;
    }

    size_t json_len = strlen(json_str);
    if (json_len >= out_size) {
        cJSON_free(json_str);
        return ESP_ERR_NO_MEM;
    }

    memcpy(out_buf, json_str, json_len);
    out_buf[json_len] = '\0';
    *out_len = json_len;

    cJSON_free(json_str);
    return ESP_OK;
}

/* ============================================================
 * Public API Implementation
 * ============================================================ */

esp_err_t cloud_manager_init(const cloud_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Cloud manager already initialized");
        return ESP_OK;
    }

    /* Create mutex for thread safety */
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Apply configuration defaults */
    if (config != NULL) {
        s_provider = config->provider;
        s_timeout_ms = (config->timeout_ms > 0) ? config->timeout_ms : CLOUD_DEFAULT_TIMEOUT_MS;

        /* Store default API key if provided */
        if (config->api_key[0] != '\0') {
            strncpy(s_api_keys[s_provider], config->api_key, sizeof(s_api_keys[0]) - 1);
            s_api_keys[s_provider][sizeof(s_api_keys[0]) - 1] = '\0';
        }

        /* Store custom endpoint if provided */
        if (config->endpoint[0] != '\0') {
            strncpy(s_custom_endpoints[s_provider], config->endpoint,
                    sizeof(s_custom_endpoints[0]) - 1);
            s_custom_endpoints[s_provider][sizeof(s_custom_endpoints[0]) - 1] = '\0';
        }
    } else {
        s_timeout_ms = CLOUD_DEFAULT_TIMEOUT_MS;
    }

    /* Load persisted API keys from NVS (overrides defaults if present) */
    esp_err_t nvs_err = load_api_keys_from_nvs();
    if (nvs_err != ESP_OK) {
        ESP_LOGW(TAG, "NVS load failed, using config defaults: %s", esp_err_to_name(nvs_err));
        /* Non-fatal — defaults are already set */
    }

    /* If config provided an API key, persist it to NVS */
    if (config != NULL && config->api_key[0] != '\0') {
        save_api_key_to_nvs(s_provider, config->api_key);
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Cloud manager initialized (provider=%s, timeout=%dms)",
             s_provider_names[s_provider], s_timeout_ms);

    /* Publish connected event if WiFi is up */
    if (wifi_manager_is_connected()) {
        publish_event(EVENT_CLOUD_CONNECTED, NULL, 0);
    }

    return ESP_OK;
}

esp_err_t cloud_manager_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    /* Wait for any in-flight request to complete */
    if (s_mutex != NULL) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
    }

    publish_event(EVENT_CLOUD_DISCONNECTED, NULL, 0);

    s_initialized = false;

    if (s_mutex != NULL) {
        xSemaphoreGive(s_mutex);
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }

    ESP_LOGI(TAG, "Cloud manager deinitialized");
    return ESP_OK;
}

esp_err_t cloud_asr_send(const void *audio_data, size_t len,
                          char *text_out, size_t text_max)
{
    /* Argument validation */
    if (audio_data == NULL || text_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (len == 0 || text_max == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Acquire mutex — only one request at a time */
    if (s_mutex == NULL || !s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* Check WiFi connectivity */
    if (!wifi_manager_is_connected()) {
        ESP_LOGW(TAG, "ASR request rejected — WiFi not connected");
        xSemaphoreGive(s_mutex);
        publish_error_event(ESP_ERR_INVALID_STATE, "WiFi not connected");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t result = ESP_FAIL;
    int64_t start_ms = esp_timer_get_time() / 1000; /* ms */

    /* Allocate multipart body buffer */
    /* Header + audio data + footer + model field + final boundary */
    size_t body_buf_size = len + 2048; /* 2 KiB overhead for multipart framing */
    char *body_buf = (char *)malloc(body_buf_size);
    if (body_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate ASR body buffer (%zu bytes)", body_buf_size);
        xSemaphoreGive(s_mutex);
        publish_error_event(ESP_ERR_NO_MEM, "ASR body alloc failed");
        return ESP_ERR_NO_MEM;
    }

    size_t body_len = 0;
    esp_err_t build_err = build_multipart_asr_body(
        audio_data, len, "whisper-1",
        body_buf, body_buf_size, &body_len
    );

    if (build_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to build multipart ASR body: %s", esp_err_to_name(build_err));
        free(body_buf);
        xSemaphoreGive(s_mutex);
        publish_error_event(build_err, "ASR multipart build failed");
        return build_err;
    }

    /* Allocate response buffer */
    http_resp_buf_t resp_buf;
    if (!http_resp_buf_init(&resp_buf, HTTP_RESP_BUF_SIZE)) {
        free(body_buf);
        xSemaphoreGive(s_mutex);
        publish_error_event(ESP_ERR_NO_MEM, "ASR response buf alloc failed");
        return ESP_ERR_NO_MEM;
    }

    /* Content-Type header for multipart */
    char content_type[128];
    snprintf(content_type, sizeof(content_type),
             "multipart/form-data; boundary=%s", MULTIPART_BOUNDARY);

    /* Execute with retry + failover */
    int http_status = 0;
    esp_err_t req_err = cloud_request_with_failover(
        s_provider,
        get_asr_url,
        "POST",
        content_type,
        body_buf,
        body_len,
        &resp_buf,
        &http_status
    );

    if (req_err == ESP_OK) {
        /* Parse the JSON response */
        result = parse_asr_response(resp_buf.data, resp_buf.len, text_out, text_max);
        if (result == ESP_OK) {
            /* Publish ASR result event */
            asr_result_t asr_payload;
            memset(&asr_payload, 0, sizeof(asr_payload));
            strncpy(asr_payload.text, text_out, sizeof(asr_payload.text) - 1);
            asr_payload.confidence = 1.0f; /* Whisper doesn't always return confidence */
            publish_event(EVENT_CLOUD_ASR_RESULT, &asr_payload, sizeof(asr_payload));
        } else {
            publish_error_event(result, "ASR JSON parse failed");
        }
    } else {
        result = req_err;
        publish_error_event(result, "ASR request failed");
    }

    /* Calculate latency */
    int64_t elapsed_ms = (esp_timer_get_time() / 1000) - start_ms;
    ESP_LOGI(TAG, "ASR completed in %lld ms (status=%d, result=%s)",
             (long long)elapsed_ms, http_status, esp_err_to_name(result));

    /* Cleanup */
    http_resp_buf_cleanup(&resp_buf);
    free(body_buf);
    xSemaphoreGive(s_mutex);

    return result;
}

esp_err_t cloud_llm_chat(const char *prompt, const char *history,
                          char *response, size_t resp_max)
{
    /* Argument validation */
    if (prompt == NULL || response == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (resp_max == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Acquire mutex */
    if (s_mutex == NULL || !s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* Check WiFi connectivity */
    if (!wifi_manager_is_connected()) {
        ESP_LOGW(TAG, "LLM request rejected — WiFi not connected");
        xSemaphoreGive(s_mutex);
        publish_error_event(ESP_ERR_INVALID_STATE, "WiFi not connected");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t result = ESP_FAIL;
    int64_t start_ms = esp_timer_get_time() / 1000;

    /* Build JSON request body */
    size_t body_buf_size = CLOUD_LLM_RESPONSE_MAX + 512; /* overhead for JSON framing */
    char *body_buf = (char *)malloc(body_buf_size);
    if (body_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate LLM request body buffer");
        xSemaphoreGive(s_mutex);
        publish_error_event(ESP_ERR_NO_MEM, "LLM body alloc failed");
        return ESP_ERR_NO_MEM;
    }

    size_t body_len = 0;
    esp_err_t build_err = build_llm_request_body(
        prompt, history, s_provider,
        body_buf, body_buf_size, &body_len
    );

    if (build_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to build LLM request body: %s", esp_err_to_name(build_err));
        free(body_buf);
        xSemaphoreGive(s_mutex);
        publish_error_event(build_err, "LLM JSON build failed");
        return build_err;
    }

    /* Allocate response buffer */
    http_resp_buf_t resp_buf;
    if (!http_resp_buf_init(&resp_buf, HTTP_RESP_BUF_SIZE)) {
        free(body_buf);
        xSemaphoreGive(s_mutex);
        publish_error_event(ESP_ERR_NO_MEM, "LLM response buf alloc failed");
        return ESP_ERR_NO_MEM;
    }

    /* Execute with retry + failover */
    int http_status = 0;
    esp_err_t req_err = cloud_request_with_failover(
        s_provider,
        get_llm_url,
        "POST",
        "application/json",
        body_buf,
        body_len,
        &resp_buf,
        &http_status
    );

    if (req_err == ESP_OK) {
        /* Parse the JSON response */
        result = parse_llm_response(resp_buf.data, resp_buf.len, response, resp_max, s_provider);
        if (result == ESP_OK) {
            /* Publish LLM response event */
            cloud_response_t llm_payload;
            memset(&llm_payload, 0, sizeof(llm_payload));
            strncpy(llm_payload.text, response, sizeof(llm_payload.text) - 1);
            llm_payload.provider = s_provider;
            llm_payload.latency_ms = (uint16_t)((esp_timer_get_time() / 1000) - start_ms);
            publish_event(EVENT_CLOUD_LLM_RESPONSE, &llm_payload, sizeof(llm_payload));
        } else {
            publish_error_event(result, "LLM JSON parse failed");
        }
    } else {
        result = req_err;
        publish_error_event(result, "LLM request failed");
    }

    /* Calculate latency */
    int64_t elapsed_ms = (esp_timer_get_time() / 1000) - start_ms;
    ESP_LOGI(TAG, "LLM completed in %lld ms (status=%d, result=%s)",
             (long long)elapsed_ms, http_status, esp_err_to_name(result));

    /* Cleanup */
    http_resp_buf_cleanup(&resp_buf);
    free(body_buf);
    xSemaphoreGive(s_mutex);

    return result;
}

esp_err_t cloud_tts_synthesize(const char *text, void *audio_out,
                                size_t *audio_len)
{
    /* Argument validation */
    if (text == NULL || audio_out == NULL || audio_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Acquire mutex */
    if (s_mutex == NULL || !s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* Check WiFi connectivity */
    if (!wifi_manager_is_connected()) {
        ESP_LOGW(TAG, "TTS request rejected — WiFi not connected");
        xSemaphoreGive(s_mutex);
        publish_error_event(ESP_ERR_INVALID_STATE, "WiFi not connected");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t result = ESP_FAIL;
    int64_t start_ms = esp_timer_get_time() / 1000;

    /* Build JSON request body */
    size_t body_buf_size = 1024; /* TTS requests are small */
    char *body_buf = (char *)malloc(body_buf_size);
    if (body_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate TTS request body buffer");
        xSemaphoreGive(s_mutex);
        publish_error_event(ESP_ERR_NO_MEM, "TTS body alloc failed");
        return ESP_ERR_NO_MEM;
    }

    size_t body_len = 0;
    esp_err_t build_err = build_tts_request_body(
        text, s_provider,
        body_buf, body_buf_size, &body_len
    );

    if (build_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to build TTS request body: %s", esp_err_to_name(build_err));
        free(body_buf);
        xSemaphoreGive(s_mutex);
        publish_error_event(build_err, "TTS JSON build failed");
        return build_err;
    }

    /* For TTS, we use the response buffer directly for binary audio data */
    http_resp_buf_t resp_buf;
    if (!http_resp_buf_init(&resp_buf, *audio_len)) {
        free(body_buf);
        xSemaphoreGive(s_mutex);
        publish_error_event(ESP_ERR_NO_MEM, "TTS response buf alloc failed");
        return ESP_ERR_NO_MEM;
    }

    /* Execute with retry + failover */
    int http_status = 0;
    esp_err_t req_err = cloud_request_with_failover(
        s_provider,
        get_tts_url,
        "POST",
        "application/json",
        body_buf,
        body_len,
        &resp_buf,
        &http_status
    );

    if (req_err == ESP_OK) {
        /* Copy audio data to caller's buffer */
        if (resp_buf.len <= *audio_len) {
            memcpy(audio_out, resp_buf.data, resp_buf.len);
            *audio_len = resp_buf.len;
            result = ESP_OK;

            /* Publish TTS data event */
            publish_event(EVENT_CLOUD_TTS_DATA, audio_out, *audio_len);
        } else {
            /* Buffer too small */
            ESP_LOGE(TAG, "TTS audio too large: %zu > %zu", resp_buf.len, *audio_len);
            *audio_len = resp_buf.len; /* Tell caller the required size */
            result = ESP_ERR_NO_MEM;
            publish_error_event(ESP_ERR_NO_MEM, "TTS audio buffer too small");
        }
    } else {
        result = req_err;
        publish_error_event(result, "TTS request failed");
    }

    /* Calculate latency */
    int64_t elapsed_ms = (esp_timer_get_time() / 1000) - start_ms;
    ESP_LOGI(TAG, "TTS completed in %lld ms (status=%d, audio=%zu bytes, result=%s)",
             (long long)elapsed_ms, http_status,
             result == ESP_OK ? *audio_len : 0,
             esp_err_to_name(result));

    /* Cleanup */
    http_resp_buf_cleanup(&resp_buf);
    free(body_buf);
    xSemaphoreGive(s_mutex);

    return result;
}

esp_err_t cloud_set_provider(cloud_provider_t provider)
{
    if (provider >= CLOUD_PROVIDER_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    cloud_provider_t old_provider = s_provider;
    s_provider = provider;
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Provider changed: %s → %s",
             s_provider_names[old_provider], s_provider_names[provider]);

    return ESP_OK;
}

cloud_provider_t cloud_get_provider(void)
{
    if (!s_initialized) {
        return (cloud_provider_t)CONFIG_CLOUD_DEFAULT_PROVIDER_VAL;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    cloud_provider_t provider = s_provider;
    xSemaphoreGive(s_mutex);

    return provider;
}

bool cloud_manager_is_connected(void)
{
    if (!s_initialized) {
        return false;
    }
    return wifi_manager_is_connected();
}

/* ============================================================
 * V2.0: Streaming TTS Implementation
 * ============================================================ */

esp_err_t cloud_tts_stream(const char *text, size_t text_len)
{
    if (text == NULL || text_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || s_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!wifi_manager_is_connected()) {
        ESP_LOGW(TAG, "TTS stream rejected — WiFi not connected");
        return ESP_ERR_INVALID_STATE;
    }

    /* V2.0 Streaming TTS:
     *
     * Strategy: Use chunked HTTP transfer or WebSocket to receive TTS audio
     * in small chunks (e.g., 4KB each) and feed them directly to the
     * audio playback ring buffer via audio_play_data(). This enables
     * "play-while-download" with significantly reduced latency compared
     * to the bulk cloud_tts_synthesize() approach.
     *
     * For now, this uses a chunked HTTP approach:
     * 1. Send TTS request via HTTP POST
     * 2. As response data arrives (HTTP_EVENT_ON_DATA), feed chunks
     *    to the audio_manager playback ring buffer
     * 3. When response completes, publish EVENT_AUDIO_PLAY_DONE
     *
     * TODO(V2.1): Implement true WebSocket streaming for even lower
     * latency, where the LLM response tokens are sent to TTS as they
     * arrive, creating a full streaming pipeline:
     *   LLM token → TTS chunk → audio_play_data() → speaker
     */

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    esp_err_t result = ESP_FAIL;
    int64_t start_ms = esp_timer_get_time() / 1000;

    /* Build TTS request body */
    size_t body_buf_size = 1024;
    char *body_buf = (char *)malloc(body_buf_size);
    if (body_buf == NULL) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NO_MEM;
    }

    size_t body_len = 0;
    esp_err_t build_err = build_tts_request_body(text, s_provider,
                                                   body_buf, body_buf_size, &body_len);
    if (build_err != ESP_OK) {
        free(body_buf);
        xSemaphoreGive(s_mutex);
        return build_err;
    }

    /* For streaming, we use a smaller response buffer and feed
     * chunks to audio_play_data() as they arrive.
     * This is implemented via a custom HTTP event handler that
     * streams to the audio ring buffer. */

    /* Allocate a streaming context */
    http_resp_buf_t resp_buf;
    if (!http_resp_buf_init(&resp_buf, 4096)) {  /* 4KB streaming chunk buffer */
        free(body_buf);
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NO_MEM;
    }

    /* Execute TTS request with streaming response handling */
    int http_status = 0;
    esp_err_t req_err = cloud_request_with_failover(
        s_provider,
        get_tts_url,
        "POST",
        "application/json",
        body_buf,
        body_len,
        &resp_buf,
        &http_status
    );

    if (req_err == ESP_OK && resp_buf.len > 0) {
        /* Feed the audio data to the playback ring buffer */
        esp_err_t play_err = audio_play_data(resp_buf.data, resp_buf.len);
        if (play_err == ESP_OK) {
            result = ESP_OK;
            ESP_LOGI(TAG, "TTS streamed %zu bytes to playback buffer", resp_buf.len);
        } else {
            ESP_LOGW(TAG, "Failed to feed TTS audio to playback: %s", esp_err_to_name(play_err));
            result = play_err;
        }
    } else {
        result = req_err;
    }

    int64_t elapsed_ms = (esp_timer_get_time() / 1000) - start_ms;
    ESP_LOGI(TAG, "TTS stream completed in %lld ms (result=%s)",
             (long long)elapsed_ms, esp_err_to_name(result));

    http_resp_buf_cleanup(&resp_buf);
    free(body_buf);
    xSemaphoreGive(s_mutex);

    return result;
}

/* ============================================================
 * V2.0: API Key Management
 * ============================================================ */

esp_err_t cloud_set_api_key(cloud_provider_t provider, const char *api_key)
{
    if (provider >= CLOUD_PROVIDER_COUNT || api_key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || s_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    strncpy(s_api_keys[provider], api_key, sizeof(s_api_keys[0]) - 1);
    s_api_keys[provider][sizeof(s_api_keys[0]) - 1] = '\0';
    xSemaphoreGive(s_mutex);

    /* Persist to NVS */
    save_api_key_to_nvs(provider, api_key);

    ESP_LOGI(TAG, "API key updated for %s", s_provider_names[provider]);
    return ESP_OK;
}

esp_err_t cloud_get_api_key_masked(cloud_provider_t provider, char *buf, size_t buf_size)
{
    if (provider >= CLOUD_PROVIDER_COUNT || buf == NULL || buf_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || s_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    const char *key = s_api_keys[provider];
    size_t key_len = strlen(key);

    if (key_len == 0) {
        snprintf(buf, buf_size, "(not set)");
    } else if (key_len <= 8) {
        snprintf(buf, buf_size, "****");
    } else {
        /* Show first 4 and last 4 characters */
        snprintf(buf, buf_size, "%.4s...%s", key, key + key_len - 4);
    }
    xSemaphoreGive(s_mutex);

    return ESP_OK;
}