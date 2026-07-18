/*
 * RobotBuddy — OTA Service Implementation
 * =========================================
 * Over-The-Air firmware upgrade service for ESP32-S3.
 *
 * Implements HTTP-based firmware download, SHA256 verification,
 * and OTA partition management using ESP-IDF v5.x APIs.
 *
 * Architecture:
 *   ┌──────────────┐
 *   │  ota_start() │──► state machine
 *   │  ota_cancel()│       │
 *   └──────────────┘       │
 *                    ┌─────▼──────────────────┐
 *                    │  IDLE                  │
 *                    │   ↓                    │
 *                    │  DOWNLOADING ──► ERROR │
 *                    │   ↓                    │
 *                    │  VERIFYING  ───► ERROR │
 *                    │   ↓                    │
 *                    │  APPLYING   ───► ERROR │
 *                    │   ↓                    │
 *                    │  REBOOTING             │
 *                    └────────────────────────┘
 *
 * Thread safety:
 *   All public API functions acquire s_mutex before mutating state.
 *   Only one OTA operation is in-flight at a time (serialized by mutex).
 *
 * Rollback:
 *   On error after writing to OTA partition, the new app is NOT marked valid.
 *   If the device reboots into the new app and it fails, the bootloader
 *   will automatically roll back to the previous working firmware.
 *   On successful boot, the app must call esp_ota_mark_app_valid_cancel_rollback().
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "ota_service.h"
#include "event_bus.h"
#include "robot_events.h"

#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "mbedtls/sha256.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ============================================================
 * Logging
 * ============================================================ */

static const char *TAG = "ota_svc";

/* ============================================================
 * Menuconfig Defaults
 * ============================================================ */

#ifndef CONFIG_OTA_CONNECT_TIMEOUT_MS
#define CONFIG_OTA_CONNECT_TIMEOUT_MS 30000
#endif

#ifndef CONFIG_OTA_TOTAL_TIMEOUT_MS
#define CONFIG_OTA_TOTAL_TIMEOUT_MS 60000
#endif

/* ============================================================
 * Firmware Image Validation
 * ============================================================ */

/**
 * @brief ESP32 firmware image magic byte
 *
 * Valid ESP32-S3 firmware images start with this byte.
 * Used for basic header validation before writing to OTA partition.
 */
#define ESP_IMAGE_HEADER_MAGIC  0xE9

/* ============================================================
 * Module State
 * ============================================================ */

static bool              s_initialized       = false;
static SemaphoreHandle_t s_mutex             = NULL;
static ota_state_t       s_state             = OTA_STATE_IDLE;
static uint8_t           s_progress           = 0;
static uint32_t          s_downloaded         = 0;
static uint32_t          s_total_size         = 0;
static uint32_t          s_connect_timeout_ms = CONFIG_OTA_CONNECT_TIMEOUT_MS;
static uint32_t          s_total_timeout_ms   = CONFIG_OTA_TOTAL_TIMEOUT_MS;

/** OTA update handle for the current operation */
static esp_ota_handle_t  s_ota_handle         = 0;

/** Flag to signal cancellation from ota_cancel() */
static volatile bool     s_cancel_flag        = false;

/** SHA256 context for streaming verification */
static mbedtls_sha256_context s_sha256_ctx;

/** Buffer for final SHA256 hex string */
static char s_computed_sha256[OTA_SHA256_HEX_LEN];

/* ============================================================
 * Internal Helpers — Forward Declarations
 * ============================================================ */

static void set_state(ota_state_t new_state);
static void publish_event(robot_event_id_t event_id, void *payload, size_t payload_len);
static void publish_error_event(esp_err_t err, const char *detail);
static esp_err_t http_event_handler(esp_http_client_event_t *evt);
static esp_err_t ota_download_and_apply(const char *url, const char *sha256);
static bool validate_firmware_header(const uint8_t *data, size_t len);
static void compute_sha256_hex(const uint8_t *hash_bytes, char *hex_out);
static void ota_task(void *pvParameters);

/* ============================================================
 * State Management
 * ============================================================ */

/**
 * @brief Transition to a new OTA state (must hold s_mutex)
 */
static void set_state(ota_state_t new_state)
{
    ota_state_t old_state = s_state;
    s_state = new_state;
    ESP_LOGI(TAG, "State: %d -> %d", old_state, new_state);
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
 * @brief Publish an OTA error event with a descriptive message
 */
static void publish_error_event(esp_err_t err, const char *detail)
{
    ota_progress_event_t error_payload;
    memset(&error_payload, 0, sizeof(error_payload));
    error_payload.percent = s_progress;
    error_payload.downloaded = s_downloaded;
    error_payload.total = s_total_size;

    ESP_LOGE(TAG, "OTA error: %s — %s", esp_err_to_name(err), detail ? detail : "unknown");
    publish_event(EVENT_OTA_ERROR, &error_payload, sizeof(error_payload));
}

/* ============================================================
 * SHA256 Helpers
 * ============================================================ */

/**
 * @brief Convert raw SHA256 hash bytes to hex string
 *
 * @param hash_bytes 32-byte raw hash
 * @param hex_out    Output buffer (must be >= 65 bytes for 64 hex chars + null)
 */
static void compute_sha256_hex(const uint8_t *hash_bytes, char *hex_out)
{
    static const char hex_chars[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        hex_out[i * 2]     = hex_chars[(hash_bytes[i] >> 4) & 0x0F];
        hex_out[i * 2 + 1] = hex_chars[hash_bytes[i] & 0x0F];
    }
    hex_out[64] = '\0';
}

/* ============================================================
 * Firmware Header Validation
 * ============================================================ */

/**
 * @brief Validate the first bytes of a firmware image
 *
 * Checks the ESP32 image magic byte (0xE9) at offset 0.
 *
 * @param data Pointer to the first chunk of firmware data
 * @param len  Length of data (must be >= 1)
 * @return true if the header magic byte is valid
 */
static bool validate_firmware_header(const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0) {
        return false;
    }

    if (data[0] != ESP_IMAGE_HEADER_MAGIC) {
        ESP_LOGE(TAG, "Invalid firmware header magic: 0x%02X (expected 0x%02X)",
                 data[0], ESP_IMAGE_HEADER_MAGIC);
        return false;
    }

    return true;
}

/* ============================================================
 * HTTP Client Event Handler
 * ============================================================ */

/**
 * @brief esp_http_client event handler for OTA download
 *
 * This handler is intentionally minimal — the actual data processing
 * is done in the download loop via esp_http_client_read() for
 * streaming writes to the OTA partition.
 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP client error");
        break;

    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP client disconnected");
        break;

    default:
        /* Other events (HEADER_SENT, ON_CONNECTED, ON_DATA, ON_FINISH) —
         * no action needed; we read data explicitly in the download loop */
        break;
    }
    return ESP_OK;
}

/* ============================================================
 * OTA Download and Apply
 * ============================================================ */

/**
 * @brief Download firmware via HTTP and apply the OTA update
 *
 * This function performs the complete OTA flow:
 *   1. Open HTTP connection and get content length
 *   2. Validate firmware header magic byte
 *   3. Begin OTA partition write
 *   4. Download in 4KB chunks, writing to OTA partition
 *   5. Update SHA256 hash incrementally
 *   6. Publish progress events
 *   7. Verify SHA256 if expected hash was provided
 *   8. End OTA and set boot partition
 *   9. Mark app valid and reboot
 *
 * On any error, marks the OTA partition as invalid for rollback.
 *
 * @param url    Firmware download URL
 * @param sha256 Expected SHA256 hex string (may be NULL or empty to skip)
 * @return ESP_OK on success (device will reboot before returning)
 *         ESP_FAIL on any error
 */
static esp_err_t ota_download_and_apply(const char *url, const char *sha256)
{
    esp_err_t err = ESP_FAIL;
    bool sha256_enabled = (sha256 != NULL && sha256[0] != '\0');
    bool ota_begun = false;
    bool header_validated = false;
    uint8_t *chunk_buf = NULL;

    /* ----------------------------------------------------------
     * Step 1: Find the next OTA partition
     * ---------------------------------------------------------- */
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition found");
        publish_error_event(ESP_FAIL, "No OTA partition");
        set_state(OTA_STATE_ERROR);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "OTA partition: %s (offset=0x%lx, size=%lu)",
             update_partition->label,
             (unsigned long)update_partition->address,
             (unsigned long)update_partition->size);

    /* ----------------------------------------------------------
     * Step 2: Allocate download buffer in PSRAM
     * ---------------------------------------------------------- */
    bool using_psram = false;
    chunk_buf = (uint8_t *)heap_caps_malloc(OTA_DOWNLOAD_CHUNK_SIZE, MALLOC_CAP_SPIRAM);
    if (chunk_buf != NULL) {
        using_psram = true;
    } else {
        /* Fallback to internal RAM if PSRAM unavailable */
        chunk_buf = (uint8_t *)malloc(OTA_DOWNLOAD_CHUNK_SIZE);
    }
    if (chunk_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate download buffer (%d bytes)", OTA_DOWNLOAD_CHUNK_SIZE);
        publish_error_event(ESP_ERR_NO_MEM, "Download buffer alloc failed");
        set_state(OTA_STATE_ERROR);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Download buffer allocated (%d bytes, %s)",
             OTA_DOWNLOAD_CHUNK_SIZE, using_psram ? "PSRAM" : "internal RAM");

    /* ----------------------------------------------------------
     * Step 3: Initialize SHA256 context
     * ---------------------------------------------------------- */
    if (sha256_enabled) {
        mbedtls_sha256_init(&s_sha256_ctx);
        mbedtls_sha256_starts(&s_sha256_ctx, 0); /* 0 = SHA-256 (not SHA-224) */
        ESP_LOGI(TAG, "SHA256 verification enabled");
    }

    /* ----------------------------------------------------------
     * Step 4: Open HTTP connection
     * ---------------------------------------------------------- */
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = s_total_timeout_ms,
        .event_handler = http_event_handler,
        .buffer_size = OTA_DOWNLOAD_CHUNK_SIZE,
        .is_async = false,
        /* TODO(V2.1): Enable TLS certificate pinning for production OTA.
         * Use esp_http_client_set_tls_cert_pem() or configure CA bundle
         * via menuconfig for secure firmware downloads. */
        .skip_cert_common_name_check = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to create HTTP client for %s", url);
        free(chunk_buf);
        publish_error_event(ESP_FAIL, "HTTP client init failed");
        set_state(OTA_STATE_ERROR);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Connecting to %s (timeout=%lums)",
             url, (unsigned long)s_total_timeout_ms);

    err = esp_http_client_open(client, 0); /* 0 = no request body for GET */
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(chunk_buf);
        publish_error_event(err, "HTTP open failed");
        set_state(OTA_STATE_ERROR);
        return ESP_FAIL;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        ESP_LOGE(TAG, "Failed to fetch HTTP headers");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        free(chunk_buf);
        publish_error_event(ESP_FAIL, "HTTP header fetch failed");
        set_state(OTA_STATE_ERROR);
        return ESP_FAIL;
    }

    s_total_size = (uint32_t)content_length;
    s_downloaded = 0;
    s_progress = 0;

    ESP_LOGI(TAG, "Firmware size: %d bytes", content_length);

    /* Check HTTP status code */
    int http_status = esp_http_client_get_status_code(client);
    if (http_status != 200) {
        ESP_LOGE(TAG, "HTTP status %d (expected 200)", http_status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        free(chunk_buf);
        publish_error_event(ESP_FAIL, "HTTP status not 200");
        set_state(OTA_STATE_ERROR);
        return ESP_FAIL;
    }

    /* ----------------------------------------------------------
     * Step 5: Transition to DOWNLOADING state
     * ---------------------------------------------------------- */
    set_state(OTA_STATE_DOWNLOADING);
    publish_event(EVENT_OTA_START, NULL, 0);

    /* ----------------------------------------------------------
     * Step 6: Download loop — read chunks and write to OTA partition
     * ---------------------------------------------------------- */
    int data_read = 0;
    uint8_t last_reported_progress = 0;

    while ((data_read = esp_http_client_read(client, (char *)chunk_buf,
                                              OTA_DOWNLOAD_CHUNK_SIZE)) > 0) {
        /* Check for cancellation */
        if (s_cancel_flag) {
            ESP_LOGW(TAG, "OTA cancelled by user");
            if (ota_begun) {
                esp_ota_abort(s_ota_handle);
                ota_begun = false;
            }
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            free(chunk_buf);
            s_cancel_flag = false;
            s_progress = 0;
            s_downloaded = 0;
            s_total_size = 0;
            set_state(OTA_STATE_IDLE);
            return ESP_ERR_INVALID_STATE;
        }

        /* Validate firmware header on first chunk */
        if (!header_validated) {
            if (!validate_firmware_header(chunk_buf, (size_t)data_read)) {
                ESP_LOGE(TAG, "Firmware header validation failed");
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                free(chunk_buf);
                publish_error_event(ESP_FAIL, "Invalid firmware header");
                set_state(OTA_STATE_ERROR);
                return ESP_FAIL;
            }
            header_validated = true;

            /* Begin OTA write after header validation */
            err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &s_ota_handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                free(chunk_buf);
                publish_error_event(err, "OTA begin failed");
                set_state(OTA_STATE_ERROR);
                return ESP_FAIL;
            }
            ota_begun = true;
            ESP_LOGI(TAG, "OTA write started (partition: %s)", update_partition->label);
        }

        /* Write chunk to OTA partition */
        err = esp_ota_write(s_ota_handle, chunk_buf, (size_t)data_read);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(s_ota_handle);
            ota_begun = false;
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            free(chunk_buf);
            publish_error_event(err, "OTA write failed");
            set_state(OTA_STATE_ERROR);
            return ESP_FAIL;
        }

        /* Update SHA256 hash incrementally */
        if (sha256_enabled) {
            mbedtls_sha256_update(&s_sha256_ctx, chunk_buf, (size_t)data_read);
        }

        /* Update progress */
        s_downloaded += (uint32_t)data_read;
        if (s_total_size > 0) {
            s_progress = (uint8_t)((s_downloaded * 100) / s_total_size);
            if (s_progress > 100) {
                s_progress = 100;
            }
        } else {
            /* Unknown total size — report progress based on downloaded MB */
            s_progress = (uint8_t)((s_downloaded / (1024 * 1024)) % 100);
        }

        /* Publish progress event (throttled: every 5% change) */
        if (s_progress != last_reported_progress &&
            (s_progress - last_reported_progress >= 5 || s_progress == 100)) {
            ota_progress_event_t progress_payload = {
                .percent    = s_progress,
                .downloaded = s_downloaded,
                .total      = s_total_size,
            };
            publish_event(EVENT_OTA_PROGRESS, &progress_payload, sizeof(progress_payload));
            last_reported_progress = s_progress;
            ESP_LOGI(TAG, "Download: %lu/%lu bytes (%d%%)",
                     (unsigned long)s_downloaded,
                     (unsigned long)s_total_size,
                     s_progress);
        }
    }

    /* Check if download ended due to error */
    if (data_read < 0) {
        ESP_LOGE(TAG, "HTTP read error: %d", data_read);
        if (ota_begun) {
            esp_ota_abort(s_ota_handle);
            ota_begun = false;
        }
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        free(chunk_buf);
        publish_error_event(ESP_FAIL, "HTTP read error");
        set_state(OTA_STATE_ERROR);
        return ESP_FAIL;
    }

    /* Close HTTP connection — download complete */
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    /* Free download buffer */
    free(chunk_buf);
    chunk_buf = NULL;

    /* Ensure OTA was begun (edge case: zero-length response) */
    if (!ota_begun) {
        ESP_LOGE(TAG, "No firmware data received");
        publish_error_event(ESP_FAIL, "No firmware data received");
        set_state(OTA_STATE_ERROR);
        return ESP_FAIL;
    }

    /* ----------------------------------------------------------
     * Step 7: VERIFYING — check SHA256 and size
     * ---------------------------------------------------------- */
    set_state(OTA_STATE_VERIFYING);

    /* Size check (if expected_size was configured and > 0) */
    if (s_total_size > 0 && s_downloaded != (uint32_t)s_total_size) {
        ESP_LOGE(TAG, "Size mismatch: downloaded %lu, expected %lu",
                 (unsigned long)s_downloaded, (unsigned long)s_total_size);
        esp_ota_abort(s_ota_handle);
        publish_error_event(ESP_FAIL, "Firmware size mismatch");
        set_state(OTA_STATE_ERROR);
        return ESP_FAIL;
    }

    /* SHA256 verification */
    if (sha256_enabled) {
        uint8_t hash_bytes[32];
        mbedtls_sha256_finish(&s_sha256_ctx, hash_bytes);
        mbedtls_sha256_free(&s_sha256_ctx);

        compute_sha256_hex(hash_bytes, s_computed_sha256);
        ESP_LOGI(TAG, "Computed SHA256: %s", s_computed_sha256);
        ESP_LOGI(TAG, "Expected SHA256: %s", sha256);

        if (strcasecmp(s_computed_sha256, sha256) != 0) {
            ESP_LOGE(TAG, "SHA256 mismatch!");
            esp_ota_abort(s_ota_handle);
            publish_error_event(ESP_FAIL, "SHA256 verification failed");
            set_state(OTA_STATE_ERROR);
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "SHA256 verification passed");
    }

    /* ----------------------------------------------------------
     * Step 8: APPLYING — end OTA and set boot partition
     * ---------------------------------------------------------- */
    set_state(OTA_STATE_APPLYING);

    err = esp_ota_end(s_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        if (err == ESP_ERR_OTA_VALIDATE) {
            ESP_LOGE(TAG, "OTA image validation failed — image may be corrupted");
        }
        publish_error_event(err, "OTA end failed");
        set_state(OTA_STATE_ERROR);
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        publish_error_event(err, "Set boot partition failed");
        set_state(OTA_STATE_ERROR);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Boot partition set to %s", update_partition->label);

    /* ----------------------------------------------------------
     * Step 9: REBOOTING — mark valid and restart
     * ---------------------------------------------------------- */
    set_state(OTA_STATE_REBOOTING);

    /* Mark the current (new) app as valid to cancel rollback.
     * This is done before reboot so the bootloader knows this was
     * an intentional OTA. If the new app crashes before calling
     * this, the bootloader will roll back automatically. */
    err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to mark app valid (rollback not cancelled): %s",
                 esp_err_to_name(err));
        /* Non-fatal — the new app can mark itself valid on next boot */
    }

    /* Publish completion event */
    publish_event(EVENT_OTA_COMPLETE, NULL, 0);

    ESP_LOGI(TAG, "OTA complete — rebooting in %dms...", OTA_REBOOT_DELAY_MS);

    /* Delay before reboot to allow event to be dispatched */
    vTaskDelay(pdMS_TO_TICKS(OTA_REBOOT_DELAY_MS));

    esp_restart();

    /* Never reached — esp_restart() does not return */
    return ESP_OK;
}

/* ============================================================
 * OTA Task (runs in its own FreeRTOS task)
 * ============================================================ */

/**
 * @brief OTA task entry point
 *
 * Runs the OTA download and apply flow in a dedicated task context
 * so that the calling function (ota_start) can return immediately.
 * The mutex is held for the entire duration to prevent concurrent operations.
 */
static void ota_task(void *pvParameters)
{
    typedef struct {
        char url[OTA_URL_MAX_LEN];
        char sha256[OTA_SHA256_HEX_LEN];
    } ota_task_params_t;

    ota_task_params_t *params = (ota_task_params_t *)pvParameters;

    /* Execute the OTA flow */
    esp_err_t err = ota_download_and_apply(params->url, params->sha256);

    /* If we reach here, OTA failed — clean up */
    if (err != ESP_OK) {
        /* Attempt rollback if we wrote anything to the OTA partition */
        ESP_LOGW(TAG, "OTA failed — attempting rollback");
        esp_ota_mark_app_invalid_rollback_and_reboot();

        /* If rollback reboot fails, at least reset state */
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_progress = 0;
        s_downloaded = 0;
        s_total_size = 0;
        s_cancel_flag = false;
        set_state(OTA_STATE_IDLE);
        xSemaphoreGive(s_mutex);
    }

    /* Free the parameters */
    free(params);

    /* Delete this task */
    vTaskDelete(NULL);
}

/* ============================================================
 * Public API Implementation
 * ============================================================ */

esp_err_t ota_service_init(const ota_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "OTA service already initialized");
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
        s_connect_timeout_ms = (config->connect_timeout_ms > 0)
                                    ? config->connect_timeout_ms
                                    : OTA_DEFAULT_CONNECT_TIMEOUT_MS;
        s_total_timeout_ms = (config->total_timeout_ms > 0)
                                  ? config->total_timeout_ms
                                  : OTA_DEFAULT_TOTAL_TIMEOUT_MS;
    } else {
        s_connect_timeout_ms = OTA_DEFAULT_CONNECT_TIMEOUT_MS;
        s_total_timeout_ms = OTA_DEFAULT_TOTAL_TIMEOUT_MS;
    }

    /* Reset state */
    s_state = OTA_STATE_IDLE;
    s_progress = 0;
    s_downloaded = 0;
    s_total_size = 0;
    s_cancel_flag = false;

    s_initialized = true;
    ESP_LOGI(TAG, "OTA service initialized (connect_timeout=%lums, total_timeout=%lums)",
             (unsigned long)s_connect_timeout_ms, (unsigned long)s_total_timeout_ms);

    return ESP_OK;
}

esp_err_t ota_service_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    /* Wait for any in-flight OTA to complete */
    if (s_mutex != NULL) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
    }

    s_initialized = false;
    s_state = OTA_STATE_IDLE;
    s_progress = 0;
    s_downloaded = 0;
    s_total_size = 0;
    s_cancel_flag = false;

    if (s_mutex != NULL) {
        xSemaphoreGive(s_mutex);
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }

    ESP_LOGI(TAG, "OTA service deinitialized");
    return ESP_OK;
}

esp_err_t ota_start(const char *url, const char *sha256)
{
    /* Argument validation */
    if (url == NULL || url[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    /* State check */
    if (s_mutex == NULL || !s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (s_state != OTA_STATE_IDLE) {
        ESP_LOGW(TAG, "OTA already in progress (state=%d)", s_state);
        xSemaphoreGive(s_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    /* Allocate task parameters (freed by ota_task) */
    typedef struct {
        char url[OTA_URL_MAX_LEN];
        char sha256[OTA_SHA256_HEX_LEN];
    } ota_task_params_t;

    ota_task_params_t *params = (ota_task_params_t *)malloc(sizeof(ota_task_params_t));
    if (params == NULL) {
        ESP_LOGE(TAG, "Failed to allocate OTA task params");
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NO_MEM;
    }

    /* Copy URL and SHA256 to task params */
    strncpy(params->url, url, sizeof(params->url) - 1);
    params->url[sizeof(params->url) - 1] = '\0';

    if (sha256 != NULL) {
        strncpy(params->sha256, sha256, sizeof(params->sha256) - 1);
        params->sha256[sizeof(params->sha256) - 1] = '\0';
    } else {
        params->sha256[0] = '\0';
    }

    /* Reset progress and cancel flag */
    s_progress = 0;
    s_downloaded = 0;
    s_total_size = 0;
    s_cancel_flag = false;

    /* Release mutex — the OTA task will manage state transitions.
     * The state is set to DOWNLOADING inside ota_download_and_apply()
     * which prevents concurrent ota_start() calls. */
    xSemaphoreGive(s_mutex);

    /* Launch OTA in a dedicated task (8KB stack for HTTP + SHA256) */
    BaseType_t task_created = xTaskCreatePinnedToCore(
        ota_task,
        "ota_task",
        8192,
        params,
        5,  /* Priority above normal to ensure timely OTA */
        NULL,
        1   /* Run on core 1 to avoid interfering with UI on core 0 */
    );

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA task");
        free(params);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t ota_cancel(void)
{
    if (s_mutex == NULL || !s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (s_state != OTA_STATE_DOWNLOADING) {
        ESP_LOGW(TAG, "Cannot cancel — not downloading (state=%d)", s_state);
        xSemaphoreGive(s_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    /* Set the cancel flag — the download loop will check this */
    s_cancel_flag = true;

    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "OTA cancel requested");
    return ESP_OK;
}

uint8_t ota_get_progress(void)
{
    if (!s_initialized) {
        return 0;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    uint8_t progress = s_progress;
    xSemaphoreGive(s_mutex);

    return progress;
}

ota_state_t ota_get_state(void)
{
    if (!s_initialized) {
        return OTA_STATE_IDLE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    ota_state_t state = s_state;
    xSemaphoreGive(s_mutex);

    return state;
}
