/*
 * RobotBuddy — WiFi Manager
 * ==========================
 * WiFi STA connection management with SmartConfig provisioning,
 * NVS credential storage, exponential-backoff reconnection, and
 * event-bus integration.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

/* ============================================================
 * Includes
 * ============================================================ */

#include "wifi_manager.h"

#include <string.h>

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "smartconfig_ack.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "event_bus.h"

/* ============================================================
 * Constants
 * ============================================================ */

static const char *const TAG = "wifi_manager";

/** @brief Maximum SSID length (per IEEE 802.11) */
#define WIFI_SSID_MAX_LEN    32

/** @brief Maximum password length (WPA2) */
#define WIFI_PASS_MAX_LEN    64

/** @brief Maximum reconnection backoff (seconds) */
#define WIFI_BACKOFF_MAX_SEC 30

/** @brief SmartConfig event bits */
#define SC_DONE_BIT          BIT0
#define SC_LINK_BIT          BIT1

/** @brief Internal event-group bits */
#define WIFI_CONNECTED_BIT   BIT0
#define WIFI_FAIL_BIT        BIT1

/* ============================================================
 * Types
 * ============================================================ */

/**
 * @brief Registered callback entry
 */
typedef struct {
    wifi_event_cb_t cb;   /**< Callback function (NULL = free slot) */
    void *arg;             /**< User context */
} callback_entry_t;

/**
 * @brief WiFi manager runtime state
 */
typedef struct {
    wifi_state_t       state;            /**< Current state machine state */
    SemaphoreHandle_t  mutex;            /**< Protects state & callbacks */
    bool               initialized;      /**< True after wifi_manager_init() */
    bool               started;          /**< True after wifi_manager_start() */
    bool               smartconfig_active; /**< True while SC is in progress */
    esp_netif_t       *sta_netif;        /**< STA netif handle */
    EventGroupHandle_t  wifi_event_group; /**< Connection event group */
    EventGroupHandle_t  sc_event_group;   /**< SmartConfig event group */
    TimerHandle_t       reconnect_timer;  /**< One-shot reconnection timer */
    uint32_t            backoff_sec;      /**< Current backoff delay (seconds) */
    callback_entry_t    callbacks[WIFI_MANAGER_MAX_CALLBACKS]; /**< Callback slots */
} wifi_mgr_t;

/* ============================================================
 * Module-level state
 * ============================================================ */

static wifi_mgr_t s_mgr = {
    .state            = WIFI_STATE_IDLE,
    .mutex            = NULL,
    .initialized      = false,
    .started          = false,
    .smartconfig_active = false,
    .sta_netif        = NULL,
    .wifi_event_group = NULL,
    .sc_event_group   = NULL,
    .reconnect_timer  = NULL,
    .backoff_sec      = 0,
};

/* ============================================================
 * Forward declarations
 * ============================================================ */

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *event_data);
static void sc_event_handler(void *arg, esp_event_base_t base,
                              int32_t id, void *event_data);
static void set_state(wifi_state_t new_state);
static void notify_callbacks(wifi_state_t state);
static void publish_wifi_event(robot_event_id_t event_id);
static void reconnect_timer_cb(TimerHandle_t timer);
static esp_err_t load_credentials(char *ssid, size_t ssid_cap,
                                   char *pass, size_t pass_cap);
static esp_err_t save_credentials(const char *ssid, const char *pass);
static void reset_backoff(void);
static uint32_t next_backoff(void);

/* ============================================================
 * State helpers
 * ============================================================ */

/**
 * @brief Transition to a new state and notify all subscribers.
 *
 * Must be called with s_mgr.mutex NOT held (acquires it internally).
 */
static void set_state(wifi_state_t new_state)
{
    xSemaphoreTake(s_mgr.mutex, portMAX_DELAY);
    wifi_state_t old = s_mgr.state;
    s_mgr.state = new_state;
    xSemaphoreGive(s_mgr.mutex);

    if (old != new_state) {
        ESP_LOGI(TAG, "State: %d → %d", (int)old, (int)new_state);
        notify_callbacks(new_state);
    }
}

/**
 * @brief Invoke every registered callback with the given state.
 */
static void notify_callbacks(wifi_state_t state)
{
    /* Snapshot callbacks under mutex so we can call them without it held */
    callback_entry_t snap[WIFI_MANAGER_MAX_CALLBACKS];
    xSemaphoreTake(s_mgr.mutex, portMAX_DELAY);
    memcpy(snap, s_mgr.callbacks, sizeof(snap));
    xSemaphoreGive(s_mgr.mutex);

    for (int i = 0; i < WIFI_MANAGER_MAX_CALLBACKS; i++) {
        if (snap[i].cb) {
            snap[i].cb(state, snap[i].arg);
        }
    }
}

/**
 * @brief Publish a WiFi-related event to the central event bus.
 */
static void publish_wifi_event(robot_event_id_t event_id)
{
    robot_event_t evt = {
        .id          = event_id,
        .timestamp   = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS),
        .payload     = NULL,
        .payload_len = 0,
    };
    esp_err_t ret = event_bus_publish(&evt);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to publish event 0x%04X: %s",
                 event_id, esp_err_to_name(ret));
    }
}

/* ============================================================
 * Backoff helpers
 * ============================================================ */

/** @brief Reset backoff to initial value */
static void reset_backoff(void)
{
    s_mgr.backoff_sec = 0;
}

/**
 * @brief Compute the next backoff delay using exponential growth.
 *
 * Sequence: 1 → 2 → 4 → 8 → 16 → 30 → 30 → …
 *
 * @return Delay in seconds
 */
static uint32_t next_backoff(void)
{
    if (s_mgr.backoff_sec == 0) {
        s_mgr.backoff_sec = 1;
    } else if (s_mgr.backoff_sec < WIFI_BACKOFF_MAX_SEC) {
        s_mgr.backoff_sec <<= 1;  /* Double */
        if (s_mgr.backoff_sec > WIFI_BACKOFF_MAX_SEC) {
            s_mgr.backoff_sec = WIFI_BACKOFF_MAX_SEC;
        }
    }
    /* Else: already at max, stay at 30 */
    return s_mgr.backoff_sec;
}

/**
 * @brief Timer callback — triggered when the reconnection backoff expires.
 *
 * Restarts the STA connection attempt.
 */
static void reconnect_timer_cb(TimerHandle_t timer)
{
    (void)timer;
    ESP_LOGI(TAG, "Reconnect backoff expired, retrying connection…");
    esp_wifi_connect();
    set_state(WIFI_STATE_CONNECTING);
}

/* ============================================================
 * NVS credential helpers
 * ============================================================ */

/**
 * @brief Load WiFi credentials from NVS.
 *
 * @param ssid     Output buffer for SSID  (at least WIFI_SSID_MAX_LEN + 1)
 * @param ssid_cap Capacity of ssid buffer
 * @param pass     Output buffer for password (at least WIFI_PASS_MAX_LEN + 1)
 * @param pass_cap Capacity of pass buffer
 * @return ESP_OK on success, ESP_ERR_NVS_NOT_FOUND if no credentials stored
 */
static esp_err_t load_credentials(char *ssid, size_t ssid_cap,
                                   char *pass, size_t pass_cap)
{
    nvs_handle_t h = 0;
    esp_err_t err = nvs_open(WIFI_MANAGER_NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "NVS open for read failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Read SSID */
    size_t len = ssid_cap;
    err = nvs_get_str(h, WIFI_MANAGER_NVS_SSID_KEY, ssid, &len);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "NVS read SSID failed: %s", esp_err_to_name(err));
        nvs_close(h);
        return err;
    }

    /* Read password (may be empty for open networks) */
    len = pass_cap;
    err = nvs_get_str(h, WIFI_MANAGER_NVS_PASS_KEY, pass, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        /* Open network — treat as empty password */
        pass[0] = '\0';
        err = ESP_OK;
    }

    nvs_close(h);
    return err;
}

/**
 * @brief Save WiFi credentials to NVS.
 *
 * @param ssid SSID string
 * @param pass Password string
 * @return ESP_OK on success
 */
static esp_err_t save_credentials(const char *ssid, const char *pass)
{
    nvs_handle_t h = 0;
    esp_err_t err = nvs_open(WIFI_MANAGER_NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open for write failed: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(h, WIFI_MANAGER_NVS_SSID_KEY, ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS write SSID failed: %s", esp_err_to_name(err));
        nvs_close(h);
        return err;
    }

    err = nvs_set_str(h, WIFI_MANAGER_NVS_PASS_KEY, pass);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS write password failed: %s", esp_err_to_name(err));
        nvs_close(h);
        return err;
    }

    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

/* ============================================================
 * WiFi event handler
 * ============================================================ */

/**
 * @brief ESP-IDF WiFi / IP event handler
 *
 * Handles STA_START, STA_CONNECTED, STA_DISCONNECTED, GOT_IP.
 * Registered with the default event loop during init.
 */
static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *event_data)
{
    /* ----- WIFI_EVENT ----- */
    if (base == WIFI_EVENT) {
        switch (id) {

        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "STA started");
            if (!s_mgr.smartconfig_active) {
                esp_wifi_connect();
                set_state(WIFI_STATE_CONNECTING);
            }
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "STA connected to AP");
            reset_backoff();
            break;

        case WIFI_EVENT_STA_DISCONNECTED: {
            ESP_LOGW(TAG, "STA disconnected (reason %d)",
                     ((wifi_event_sta_disconnected_t *)event_data)->reason);

            /* Clear connection bits */
            if (s_mgr.wifi_event_group) {
                xEventGroupClearBits(s_mgr.wifi_event_group,
                                      WIFI_CONNECTED_BIT);
            }

            /* If SmartConfig was active, just signal failure */
            if (s_mgr.smartconfig_active) {
                break;
            }

            set_state(WIFI_STATE_DISCONNECTED);
            publish_wifi_event(EVENT_SYS_WIFI_DISCONNECTED);

            /* Schedule reconnect with exponential backoff */
            uint32_t delay_sec = next_backoff();
            ESP_LOGI(TAG, "Reconnecting in %lu s…", (unsigned long)delay_sec);
            if (s_mgr.reconnect_timer) {
                xTimerChangePeriod(s_mgr.reconnect_timer,
                                   pdMS_TO_TICKS(delay_sec * 1000),
                                   0);
            }
            break;
        }

        case WIFI_EVENT_STA_AUTHMODE_CHANGE:
            ESP_LOGI(TAG, "AP auth mode changed");
            break;

        default:
            break;
        }
    }

    /* ----- IP_EVENT ----- */
    if (base == IP_EVENT) {
        if (id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *evt = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&evt->ip_info.ip));
            reset_backoff();

            if (s_mgr.wifi_event_group) {
                xEventGroupSetBits(s_mgr.wifi_event_group, WIFI_CONNECTED_BIT);
            }

            set_state(WIFI_STATE_CONNECTED);
            publish_wifi_event(EVENT_SYS_WIFI_CONNECTED);
        }
    }
}

/* ============================================================
 * SmartConfig event handler
 * ============================================================ */

/**
 * @brief ESP-TOUCH SmartConfig event handler
 */
static void sc_event_handler(void *arg, esp_event_base_t base,
                              int32_t id, void *event_data)
{
    (void)arg;
    (void)base;

    switch (id) {
    case SC_EVENT_GOT_SSID_PSWD: {
        ESP_LOGI(TAG, "SmartConfig: received SSID/password");
        smartconfig_event_got_ssid_pswd_t *evt =
            (smartconfig_event_got_ssid_pswd_t *)event_data;

        char ssid[WIFI_SSID_MAX_LEN + 1] = {0};
        char pass[WIFI_PASS_MAX_LEN + 1] = {0};

        /* SSID and password are already null-terminated strings in v5.x.
         * Use strncpy with array size for safety. */
        strncpy(ssid, (const char *)evt->ssid, sizeof(ssid) - 1);
        strncpy(pass, (const char *)evt->password, sizeof(pass) - 1);

        ESP_LOGI(TAG, "SSID: %s", ssid);

        /* Save to NVS for auto-connect on next boot */
        esp_err_t err = save_credentials(ssid, pass);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save credentials to NVS");
        }

        /* Configure WiFi with received credentials */
        wifi_config_t wifi_cfg = {
            .sta = {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
        };
        strlcpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid));
        strlcpy((char *)wifi_cfg.sta.password, pass, sizeof(wifi_cfg.sta.password));

        esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);

        /* Stop SmartConfig and connect */
        s_mgr.smartconfig_active = false;
        esp_smartconfig_stop();

        if (s_mgr.sc_event_group) {
            xEventGroupSetBits(s_mgr.sc_event_group, SC_DONE_BIT);
        }

        esp_wifi_connect();
        set_state(WIFI_STATE_CONNECTING);
        break;
    }

    case SC_EVENT_SEND_ACK_DONE:
        ESP_LOGI(TAG, "SmartConfig: ACK sent to phone");
        break;

    default:
        break;
    }
}

/* ============================================================
 * SmartConfig task
 * ============================================================ */

/**
 * @brief FreeRTOS task that runs the SmartConfig provisioning loop.
 *
 * Blocks until credentials are received or a timeout occurs,
 * then deletes itself.
 */
static void smartconfig_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "Starting SmartConfig (ESP-TOUCH)…");

    esp_err_t err = esp_smartconfig_set_type(SC_TYPE_ESPTOUCH);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SmartConfig set type failed: %s", esp_err_to_name(err));
        s_mgr.smartconfig_active = false;
        set_state(WIFI_STATE_DISCONNECTED);
        vTaskDelete(NULL);
        return;
    }

    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    err = esp_smartconfig_start(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SmartConfig start failed: %s", esp_err_to_name(err));
        s_mgr.smartconfig_active = false;
        set_state(WIFI_STATE_DISCONNECTED);
        vTaskDelete(NULL);
        return;
    }

    /* Wait for SC_DONE_BIT (set by sc_event_handler on success) */
    if (s_mgr.sc_event_group) {
        xEventGroupWaitBits(s_mgr.sc_event_group,
                            SC_DONE_BIT,
                            pdTRUE,   /* clear on exit */
                            pdTRUE,   /* wait for all bits */
                            portMAX_DELAY);
    }

    ESP_LOGI(TAG, "SmartConfig complete");
    vTaskDelete(NULL);
}

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t wifi_manager_init(void)
{
    if (s_mgr.initialized) {
        ESP_LOGD(TAG, "Already initialized");
        return ESP_OK;
    }

    /* Create mutex */
    s_mgr.mutex = xSemaphoreCreateMutex();
    if (s_mgr.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Create event groups */
    s_mgr.wifi_event_group = xEventGroupCreate();
    if (s_mgr.wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create WiFi event group");
        vSemaphoreDelete(s_mgr.mutex);
        s_mgr.mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_mgr.sc_event_group = xEventGroupCreate();
    if (s_mgr.sc_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create SC event group");
        vEventGroupDelete(s_mgr.wifi_event_group);
        s_mgr.wifi_event_group = NULL;
        vSemaphoreDelete(s_mgr.mutex);
        s_mgr.mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    /* Create one-shot reconnect timer (idle until first disconnect) */
    s_mgr.reconnect_timer = xTimerCreate("wifi_reconnect",
                                          pdMS_TO_TICKS(1000), /* default period */
                                          pdFALSE,             /* one-shot */
                                          NULL,                /* timer ID */
                                          reconnect_timer_cb);
    if (s_mgr.reconnect_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create reconnect timer");
        vEventGroupDelete(s_mgr.sc_event_group);
        s_mgr.sc_event_group = NULL;
        vEventGroupDelete(s_mgr.wifi_event_group);
        s_mgr.wifi_event_group = NULL;
        vSemaphoreDelete(s_mgr.mutex);
        s_mgr.mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    /* Initialize TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Create default STA netif */
    s_mgr.sta_netif = esp_netif_create_default_wifi_sta();
    if (s_mgr.sta_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create default WiFi STA netif");
        xTimerDelete(s_mgr.reconnect_timer, 0);
        s_mgr.reconnect_timer = NULL;
        vEventGroupDelete(s_mgr.sc_event_group);
        s_mgr.sc_event_group = NULL;
        vEventGroupDelete(s_mgr.wifi_event_group);
        s_mgr.wifi_event_group = NULL;
        vSemaphoreDelete(s_mgr.mutex);
        s_mgr.mutex = NULL;
        return ESP_FAIL;
    }

    /* Initialize WiFi with default config */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Register event handlers */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        SC_EVENT, ESP_EVENT_ANY_ID, &sc_event_handler, NULL, NULL));

    /* Set STA mode */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    /* Initialize callback slots to NULL */
    memset(s_mgr.callbacks, 0, sizeof(s_mgr.callbacks));

    s_mgr.state            = WIFI_STATE_IDLE;
    s_mgr.backoff_sec      = 0;
    s_mgr.smartconfig_active = false;
    s_mgr.initialized      = true;
    s_mgr.started          = false;

    ESP_LOGI(TAG, "WiFi manager initialized");
    return ESP_OK;
}

esp_err_t wifi_manager_start(void)
{
    if (!s_mgr.initialized) {
        ESP_LOGE(TAG, "Not initialized — call wifi_manager_init() first");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_mgr.started) {
        ESP_LOGD(TAG, "Already started");
        return ESP_OK;
    }

    /* Attempt to load stored credentials */
    char ssid[WIFI_SSID_MAX_LEN + 1] = {0};
    char pass[WIFI_PASS_MAX_LEN + 1] = {0};
    esp_err_t err = load_credentials(ssid, sizeof(ssid), pass, sizeof(pass));

    if (err == ESP_OK && strlen(ssid) > 0) {
        /* Stored credentials found — configure and connect */
        wifi_config_t wifi_cfg = {
            .sta = {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
        };
        strlcpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid));
        strlcpy((char *)wifi_cfg.sta.password, pass, sizeof(wifi_cfg.sta.password));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
        ESP_LOGI(TAG, "Loaded stored SSID: %s", ssid);
    } else {
        ESP_LOGW(TAG, "No stored WiFi credentials found");
        /* No credentials — will enter DISCONNECTED after STA_START */
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    s_mgr.started = true;

    ESP_LOGI(TAG, "WiFi manager started");
    return ESP_OK;
}

esp_err_t wifi_manager_stop(void)
{
    if (!s_mgr.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* Cancel reconnect timer */
    if (s_mgr.reconnect_timer) {
        xTimerStop(s_mgr.reconnect_timer, 0);
    }

    /* Stop SmartConfig if active */
    if (s_mgr.smartconfig_active) {
        esp_smartconfig_stop();
        s_mgr.smartconfig_active = false;
    }

    /* Stop WiFi */
    esp_wifi_disconnect();
    esp_wifi_stop();

    s_mgr.started = false;
    set_state(WIFI_STATE_IDLE);

    if (s_mgr.wifi_event_group) {
        xEventGroupClearBits(s_mgr.wifi_event_group, WIFI_CONNECTED_BIT);
    }

    ESP_LOGI(TAG, "WiFi manager stopped");
    return ESP_OK;
}

wifi_state_t wifi_manager_get_state(void)
{
    wifi_state_t st;
    xSemaphoreTake(s_mgr.mutex, portMAX_DELAY);
    st = s_mgr.state;
    xSemaphoreGive(s_mgr.mutex);
    return st;
}

esp_err_t wifi_manager_start_smartconfig(void)
{
    if (!s_mgr.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_mgr.smartconfig_active) {
        ESP_LOGW(TAG, "SmartConfig already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    /* Cancel any pending reconnection */
    if (s_mgr.reconnect_timer) {
        xTimerStop(s_mgr.reconnect_timer, 0);
    }

    /* If WiFi is not started yet, start it now */
    if (!s_mgr.started) {
        ESP_ERROR_CHECK(esp_wifi_start());
        s_mgr.started = true;
    }

    /* Disconnect first if currently connected */
    if (s_mgr.state == WIFI_STATE_CONNECTED) {
        esp_wifi_disconnect();
    }

    s_mgr.smartconfig_active = true;
    reset_backoff();
    set_state(WIFI_STATE_SMARTCONFIG);

    /* Clear previous SC bits */
    if (s_mgr.sc_event_group) {
        xEventGroupClearBits(s_mgr.sc_event_group, SC_DONE_BIT | SC_LINK_BIT);
    }

    /* Launch SmartConfig task */
    BaseType_t ret = xTaskCreate(smartconfig_task, "smartconfig_task",
                                  4096, NULL, 3, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create SmartConfig task");
        s_mgr.smartconfig_active = false;
        set_state(WIFI_STATE_DISCONNECTED);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "SmartConfig provisioning started");
    return ESP_OK;
}

esp_err_t wifi_manager_register_callback(wifi_event_cb_t cb, void *arg)
{
    if (cb == NULL) {
        ESP_LOGE(TAG, "Callback cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = ESP_ERR_NO_MEM;

    xSemaphoreTake(s_mgr.mutex, portMAX_DELAY);

    /* Look for an empty slot, or reject duplicates */
    for (int i = 0; i < WIFI_MANAGER_MAX_CALLBACKS; i++) {
        if (s_mgr.callbacks[i].cb == cb && s_mgr.callbacks[i].arg == arg) {
            /* Already registered with same arg — idempotent */
            result = ESP_OK;
            break;
        }
        if (s_mgr.callbacks[i].cb == NULL) {
            s_mgr.callbacks[i].cb  = cb;
            s_mgr.callbacks[i].arg = arg;
            result = ESP_OK;
            break;
        }
    }

    xSemaphoreGive(s_mgr.mutex);

    if (result == ESP_ERR_NO_MEM) {
        ESP_LOGW(TAG, "No free callback slots (max %d)",
                 WIFI_MANAGER_MAX_CALLBACKS);
    }

    return result;
}

bool wifi_manager_is_connected(void)
{
    return wifi_manager_get_state() == WIFI_STATE_CONNECTED;
}