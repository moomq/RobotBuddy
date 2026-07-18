/*
 * RobotBuddy — MQTT Client
 * =========================
 * MQTT broker connection management with command subscription,
 * status publishing, and event-bus integration.
 *
 * Connects to an MQTT broker, subscribes to device-specific command
 * topics, and publishes periodic status updates. Incoming commands
 * are parsed with cJSON and dispatched as events on the central
 * event bus.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

/* ============================================================
 * Includes
 * ============================================================ */

#include "mqtt_client.h"

#include <string.h>

#include "esp_mqtt_client.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "event_bus.h"
#include "robot_events.h"

/* ============================================================
 * Constants
 * ============================================================ */

static const char *const TAG = "mqtt_client";

/** @brief Topic prefix for all RobotBuddy MQTT topics */
#define MQTT_TOPIC_PREFIX      "robotbuddy/"

/** @brief Command topic suffix — subscribe */
#define MQTT_TOPIC_CMD_SUFFIX  "/cmd/"

/** @brief Status topic suffix — publish */
#define MQTT_TOPIC_STATUS_SUFFIX "/status/"

/** @brief Maximum full topic length */
#define MQTT_TOPIC_MAX_LEN     160

/** @brief Maximum payload length for incoming messages */
#define MQTT_PAYLOAD_MAX_LEN   512

/** @brief Periodic status timer period (ms) */
#define MQTT_STATUS_TIMER_MS   (MQTT_STATUS_INTERVAL_SEC * 1000)

/* ============================================================
 * Command topic definitions
 * ============================================================ */

/** @brief List of command sub-topics to subscribe to on connect */
static const char *const s_cmd_topics[] = {
    "build",
    "git",
    "text",
    "motion",
    "emotion",
    "pomodoro",
    "ota",
};
#define CMD_TOPIC_COUNT  (sizeof(s_cmd_topics) / sizeof(s_cmd_topics[0]))

/* ============================================================
 * Types
 * ============================================================ */

/**
 * @brief MQTT client runtime state
 */
typedef struct {
    bool               initialized;      /**< True after mqtt_client_init() */
    bool               started;          /**< True after mqtt_client_start() */
    bool               connected;        /**< True when connected to broker */
    SemaphoreHandle_t  mutex;            /**< Protects state & publish ops */
    esp_mqtt_client_handle_t client;     /**< ESP-IDF MQTT client handle */
    TimerHandle_t      status_timer;     /**< Periodic status publish timer */
    mqtt_config_t      config;           /**< Stored configuration */
    mqtt_message_cb_t  msg_callback;     /**< Optional raw message callback */
    /* Cached status values for periodic publishing */
    uint8_t            battery_percent;  /**< Last known battery percentage */
    bool               wifi_connected;   /**< Last known WiFi state */
    emotion_id_t       current_emotion;  /**< Last known emotion state */
    uint8_t            power_state;      /**< Last known power state */
} mqtt_client_t;

/* ============================================================
 * Module-level state
 * ============================================================ */

static mqtt_client_t s_client = {
    .initialized      = false,
    .started          = false,
    .connected        = false,
    .mutex            = NULL,
    .client           = NULL,
    .status_timer     = NULL,
    .msg_callback     = NULL,
    .battery_percent  = 0,
    .wifi_connected   = false,
    .current_emotion  = EMOTION_IDLE,
    .power_state      = 0,
};

/* ============================================================
 * Forward declarations
 * ============================================================ */

/* MQTT event handler */
static void mqtt_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data);

/* Event bus subscribers */
static void on_battery_event(const robot_event_t *event);
static void on_wifi_event(const robot_event_t *event);
static void on_emotion_event(const robot_event_t *event);
static void on_power_event(const robot_event_t *event);

/* Status publishing */
static void status_timer_cb(TimerHandle_t timer);
static void publish_status_battery(void);
static void publish_status_wifi(void);
static void publish_status_emotion(void);
static void publish_status_power(void);

/* Command dispatch */
static void handle_cmd_message(const char *sub_topic,
                                const char *payload, size_t len);
static void dispatch_build_cmd(const char *payload, size_t len);
static void dispatch_git_cmd(const char *payload, size_t len);
static void dispatch_text_cmd(const char *payload, size_t len);
static void dispatch_motion_cmd(const char *payload, size_t len);
static void dispatch_emotion_cmd(const char *payload, size_t len);
static void dispatch_pomodoro_cmd(const char *payload, size_t len);
static void dispatch_ota_cmd(const char *payload, size_t len);

/* Helpers */
static void publish_event(robot_event_id_t event_id,
                           void *payload, size_t payload_len);
static void build_topic(char *buf, size_t buf_cap,
                         const char *suffix);
static void subscribe_cmd_topics(void);

/* ============================================================
 * Event publishing helpers
 * ============================================================ */

/**
 * @brief Publish an event to the central event bus with payload.
 *
 * Deep-copies the payload so stack-local data is safe.
 */
static void publish_event(robot_event_id_t event_id,
                           void *payload, size_t payload_len)
{
    robot_event_t event = {
        .id          = event_id,
        .timestamp   = 0, /* auto-filled by event_bus */
        .payload     = NULL,
        .payload_len = 0,
    };

    if (payload != NULL && payload_len > 0) {
        void *payload_copy = malloc(payload_len);
        if (payload_copy == NULL) {
            ESP_LOGE(TAG, "Failed to allocate payload for event 0x%04X", event_id);
            return;
        }
        memcpy(payload_copy, payload, payload_len);
        event.payload = payload_copy;
        event.payload_len = payload_len;
    }

    esp_err_t err = event_bus_publish(&event);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to publish event 0x%04X: %s",
                 event_id, esp_err_to_name(err));
        if (event.payload != NULL) {
            free(event.payload);
        }
    }
}

/**
 * @brief Build a full MQTT topic string.
 *
 * Produces: "robotbuddy/{device_id}{suffix}"
 */
static void build_topic(char *buf, size_t buf_cap, const char *suffix)
{
    snprintf(buf, buf_cap, "%s%s%s",
             MQTT_TOPIC_PREFIX,
             s_client.config.device_id,
             suffix);
}

/* ============================================================
 * Command topic subscription
 * ============================================================ */

/**
 * @brief Subscribe to all command sub-topics.
 *
 * Called once after the MQTT connection is established.
 */
static void subscribe_cmd_topics(void)
{
    char topic[MQTT_TOPIC_MAX_LEN];

    for (size_t i = 0; i < CMD_TOPIC_COUNT; i++) {
        snprintf(topic, sizeof(topic), "%s%s%s%s",
                 MQTT_TOPIC_PREFIX,
                 s_client.config.device_id,
                 MQTT_TOPIC_CMD_SUFFIX,
                 s_cmd_topics[i]);

        int msg_id = esp_mqtt_client_subscribe(s_client.client,
                                                topic,
                                                s_client.config.qos);
        if (msg_id < 0) {
            ESP_LOGE(TAG, "Failed to subscribe to %s", topic);
        } else {
            ESP_LOGI(TAG, "Subscribed to %s (msg_id=%d)", topic, msg_id);
        }
    }
}

/* ============================================================
 * Command dispatch
 * ============================================================ */

/**
 * @brief Route an incoming command message to the appropriate handler.
 *
 * @param sub_topic  The command sub-topic (e.g. "build", "git")
 * @param payload    Raw message payload
 * @param len        Payload length
 */
static void handle_cmd_message(const char *sub_topic,
                                const char *payload, size_t len)
{
    ESP_LOGI(TAG, "Command: %s (%zu bytes)", sub_topic, len);

    if (strcmp(sub_topic, "build") == 0) {
        dispatch_build_cmd(payload, len);
    } else if (strcmp(sub_topic, "git") == 0) {
        dispatch_git_cmd(payload, len);
    } else if (strcmp(sub_topic, "text") == 0) {
        dispatch_text_cmd(payload, len);
    } else if (strcmp(sub_topic, "motion") == 0) {
        dispatch_motion_cmd(payload, len);
    } else if (strcmp(sub_topic, "emotion") == 0) {
        dispatch_emotion_cmd(payload, len);
    } else if (strcmp(sub_topic, "pomodoro") == 0) {
        dispatch_pomodoro_cmd(payload, len);
    } else if (strcmp(sub_topic, "ota") == 0) {
        dispatch_ota_cmd(payload, len);
    } else {
        ESP_LOGW(TAG, "Unknown command sub-topic: %s", sub_topic);
    }
}

/**
 * @brief Handle cmd/build — dispatch EVENT_BUILD_STATUS
 *
 * Expected JSON: {"status": <0-3>, "msg": "<text>"}
 */
static void dispatch_build_cmd(const char *payload, size_t len)
{
    cJSON *root = cJSON_ParseWithLength(payload, len);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse build command JSON");
        return;
    }

    build_status_event_t evt = {0};

    cJSON *status_item = cJSON_GetObjectItem(root, "status");
    if (cJSON_IsNumber(status_item)) {
        evt.status = (uint8_t)status_item->valueint;
    }

    cJSON *msg_item = cJSON_GetObjectItem(root, "msg");
    if (cJSON_IsString(msg_item) && msg_item->valuestring != NULL) {
        strncpy(evt.msg, msg_item->valuestring, sizeof(evt.msg) - 1);
    }

    cJSON_Delete(root);

    publish_event(EVENT_BUILD_STATUS, &evt, sizeof(evt));
    ESP_LOGI(TAG, "Dispatched EVENT_BUILD_STATUS (status=%u, msg=%s)",
             evt.status, evt.msg);
}

/**
 * @brief Handle cmd/git — dispatch EVENT_GIT_STATUS
 *
 * Expected JSON: {"uncommitted": <n>, "conflicts": <n>}
 */
static void dispatch_git_cmd(const char *payload, size_t len)
{
    cJSON *root = cJSON_ParseWithLength(payload, len);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse git command JSON");
        return;
    }

    git_status_event_t evt = {0};

    cJSON *uncommitted = cJSON_GetObjectItem(root, "uncommitted");
    if (cJSON_IsNumber(uncommitted)) {
        evt.uncommitted = (uint8_t)uncommitted->valueint;
    }

    cJSON *conflicts = cJSON_GetObjectItem(root, "conflicts");
    if (cJSON_IsNumber(conflicts)) {
        evt.conflicts = (uint8_t)conflicts->valueint;
    }

    cJSON_Delete(root);

    publish_event(EVENT_GIT_STATUS, &evt, sizeof(evt));
    ESP_LOGI(TAG, "Dispatched EVENT_GIT_STATUS (uncommitted=%u, conflicts=%u)",
             evt.uncommitted, evt.conflicts);
}

/**
 * @brief Handle cmd/text — dispatch EVENT_DISPLAY_TEXT_MSG
 *
 * Expected JSON: {"text": "<msg>", "priority": <0-2>, "duration_ms": <n>}
 */
static void dispatch_text_cmd(const char *payload, size_t len)
{
    cJSON *root = cJSON_ParseWithLength(payload, len);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse text command JSON");
        return;
    }

    text_msg_event_t evt = {0};

    cJSON *text = cJSON_GetObjectItem(root, "text");
    if (cJSON_IsString(text) && text->valuestring != NULL) {
        strncpy(evt.text, text->valuestring, sizeof(evt.text) - 1);
    }

    cJSON *priority = cJSON_GetObjectItem(root, "priority");
    if (cJSON_IsNumber(priority)) {
        evt.priority = (uint8_t)priority->valueint;
    }

    cJSON *duration = cJSON_GetObjectItem(root, "duration_ms");
    if (cJSON_IsNumber(duration)) {
        evt.duration_ms = (uint16_t)duration->valueint;
    }

    cJSON_Delete(root);

    publish_event(EVENT_DISPLAY_TEXT_MSG, &evt, sizeof(evt));
    ESP_LOGI(TAG, "Dispatched EVENT_DISPLAY_TEXT_MSG (text=%.40s, priority=%u)",
             evt.text, evt.priority);
}

/**
 * @brief Handle cmd/motion — dispatch EVENT_MOTION_COMMAND
 *
 * Expected JSON: {"command": <0-5>, "speed": <n>, "angle": <n>, "duration_ms": <n>}
 */
static void dispatch_motion_cmd(const char *payload, size_t len)
{
    cJSON *root = cJSON_ParseWithLength(payload, len);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse motion command JSON");
        return;
    }

    motion_cmd_payload_t evt = {0};

    cJSON *command = cJSON_GetObjectItem(root, "command");
    if (cJSON_IsNumber(command)) {
        evt.command = (motion_cmd_t)command->valueint;
    }

    cJSON *speed = cJSON_GetObjectItem(root, "speed");
    if (cJSON_IsNumber(speed)) {
        evt.speed = (int16_t)speed->valueint;
    }

    cJSON *angle = cJSON_GetObjectItem(root, "angle");
    if (cJSON_IsNumber(angle)) {
        evt.angle = (int16_t)angle->valueint;
    }

    cJSON *duration = cJSON_GetObjectItem(root, "duration_ms");
    if (cJSON_IsNumber(duration)) {
        evt.duration_ms = (uint16_t)duration->valueint;
    }

    cJSON_Delete(root);

    publish_event(EVENT_MOTION_COMMAND, &evt, sizeof(evt));
    ESP_LOGI(TAG, "Dispatched EVENT_MOTION_COMMAND (cmd=%d, speed=%d)",
             (int)evt.command, (int)evt.speed);
}

/**
 * @brief Handle cmd/emotion — dispatch EVENT_EMOTION_STATE_CHANGE
 *
 * Expected JSON: {"emotion_id": <0-11>, "duration_ms": <n>, "intensity": <0-255>}
 */
static void dispatch_emotion_cmd(const char *payload, size_t len)
{
    cJSON *root = cJSON_ParseWithLength(payload, len);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse emotion command JSON");
        return;
    }

    emotion_event_t evt = {0};

    cJSON *emotion_id = cJSON_GetObjectItem(root, "emotion_id");
    if (cJSON_IsNumber(emotion_id)) {
        evt.emotion_id = (emotion_id_t)emotion_id->valueint;
    }

    cJSON *duration = cJSON_GetObjectItem(root, "duration_ms");
    if (cJSON_IsNumber(duration)) {
        evt.duration_ms = (uint16_t)duration->valueint;
    }

    cJSON *intensity = cJSON_GetObjectItem(root, "intensity");
    if (cJSON_IsNumber(intensity)) {
        evt.intensity = (uint8_t)intensity->valueint;
    }

    cJSON_Delete(root);

    publish_event(EVENT_EMOTION_STATE_CHANGE, &evt, sizeof(evt));
    ESP_LOGI(TAG, "Dispatched EVENT_EMOTION_STATE_CHANGE (emotion=%d)",
             (int)evt.emotion_id);
}

/**
 * @brief Handle cmd/pomodoro — dispatch EVENT_POMODORO_START
 *
 * Expected JSON: {"duration_min": <n>} (optional fields)
 */
static void dispatch_pomodoro_cmd(const char *payload, size_t len)
{
    (void)payload;
    (void)len;

    /* Pomodoro start is a simple trigger event — no payload needed */
    publish_event(EVENT_POMODORO_START, NULL, 0);
    ESP_LOGI(TAG, "Dispatched EVENT_POMODORO_START");
}

/**
 * @brief Handle cmd/ota — dispatch EVENT_OTA_START
 *
 * Expected JSON: {"url": "<firmware_url>"} (optional)
 */
static void dispatch_ota_cmd(const char *payload, size_t len)
{
    (void)payload;
    (void)len;

    /* OTA start is a simple trigger event — the OTA service
     * will read the URL from its own configuration. */
    publish_event(EVENT_OTA_START, NULL, 0);
    ESP_LOGI(TAG, "Dispatched EVENT_OTA_START");
}

/* ============================================================
 * Status publishing
 * ============================================================ */

/**
 * @brief Publish battery status to the broker.
 *
 * Topic: robotbuddy/{device_id}/status/battery
 * Payload: {"percent": <n>}
 */
static void publish_status_battery(void)
{
    char topic[MQTT_TOPIC_MAX_LEN];
    build_topic(topic, sizeof(topic), MQTT_TOPIC_STATUS_SUFFIX "battery");

    char payload[64];
    int n = snprintf(payload, sizeof(payload),
                     "{\"percent\":%u}", s_client.battery_percent);

    xSemaphoreTake(s_client.mutex, portMAX_DELAY);
    int msg_id = esp_mqtt_client_publish(s_client.client,
                                          topic, payload, n,
                                          s_client.config.qos, 0);
    xSemaphoreGive(s_client.mutex);

    if (msg_id < 0) {
        ESP_LOGW(TAG, "Failed to publish battery status");
    } else {
        ESP_LOGD(TAG, "Published battery: %u%%", s_client.battery_percent);
    }
}

/**
 * @brief Publish WiFi status to the broker.
 *
 * Topic: robotbuddy/{device_id}/status/wifi
 * Payload: {"connected": <true|false>}
 */
static void publish_status_wifi(void)
{
    char topic[MQTT_TOPIC_MAX_LEN];
    build_topic(topic, sizeof(topic), MQTT_TOPIC_STATUS_SUFFIX "wifi");

    char payload[64];
    int n = snprintf(payload, sizeof(payload),
                     "{\"connected\":%s}",
                     s_client.wifi_connected ? "true" : "false");

    xSemaphoreTake(s_client.mutex, portMAX_DELAY);
    int msg_id = esp_mqtt_client_publish(s_client.client,
                                          topic, payload, n,
                                          s_client.config.qos, 0);
    xSemaphoreGive(s_client.mutex);

    if (msg_id < 0) {
        ESP_LOGW(TAG, "Failed to publish wifi status");
    } else {
        ESP_LOGD(TAG, "Published wifi: %s",
                 s_client.wifi_connected ? "connected" : "disconnected");
    }
}

/**
 * @brief Publish emotion status to the broker.
 *
 * Topic: robotbuddy/{device_id}/status/emotion
 * Payload: {"emotion_id": <n>}
 */
static void publish_status_emotion(void)
{
    char topic[MQTT_TOPIC_MAX_LEN];
    build_topic(topic, sizeof(topic), MQTT_TOPIC_STATUS_SUFFIX "emotion");

    char payload[64];
    int n = snprintf(payload, sizeof(payload),
                     "{\"emotion_id\":%d}", (int)s_client.current_emotion);

    xSemaphoreTake(s_client.mutex, portMAX_DELAY);
    int msg_id = esp_mqtt_client_publish(s_client.client,
                                          topic, payload, n,
                                          s_client.config.qos, 0);
    xSemaphoreGive(s_client.mutex);

    if (msg_id < 0) {
        ESP_LOGW(TAG, "Failed to publish emotion status");
    } else {
        ESP_LOGD(TAG, "Published emotion: %d", (int)s_client.current_emotion);
    }
}

/**
 * @brief Publish power state to the broker.
 *
 * Topic: robotbuddy/{device_id}/status/power
 * Payload: {"state": <n>}
 */
static void publish_status_power(void)
{
    char topic[MQTT_TOPIC_MAX_LEN];
    build_topic(topic, sizeof(topic), MQTT_TOPIC_STATUS_SUFFIX "power");

    char payload[64];
    int n = snprintf(payload, sizeof(payload),
                     "{\"state\":%u}", s_client.power_state);

    xSemaphoreTake(s_client.mutex, portMAX_DELAY);
    int msg_id = esp_mqtt_client_publish(s_client.client,
                                          topic, payload, n,
                                          s_client.config.qos, 0);
    xSemaphoreGive(s_client.mutex);

    if (msg_id < 0) {
        ESP_LOGW(TAG, "Failed to publish power status");
    } else {
        ESP_LOGD(TAG, "Published power: state=%u", s_client.power_state);
    }
}

/**
 * @brief Periodic status timer callback.
 *
 * Publishes all status topics every MQTT_STATUS_INTERVAL_SEC.
 */
static void status_timer_cb(TimerHandle_t timer)
{
    (void)timer;

    if (!s_client.connected) {
        return;
    }

    publish_status_battery();
    publish_status_wifi();
    publish_status_emotion();
    publish_status_power();
}

/* ============================================================
 * Event bus subscribers
 * ============================================================ */

/**
 * @brief Handle EVENT_SENSOR_BATTERY — cache battery percentage.
 */
static void on_battery_event(const robot_event_t *event)
{
    if (event->payload != NULL &&
        event->payload_len >= sizeof(sensor_data_t)) {
        sensor_data_t *data = (sensor_data_t *)event->payload;
        s_client.battery_percent = data->battery_percent;
    }
}

/**
 * @brief Handle EVENT_SYS_WIFI_CONNECTED / EVENT_SYS_WIFI_DISCONNECTED.
 */
static void on_wifi_event(const robot_event_t *event)
{
    (void)event;
    s_client.wifi_connected =
        (event->id == EVENT_SYS_WIFI_CONNECTED);
}

/**
 * @brief Handle EVENT_EMOTION_STATE_CHANGE — cache current emotion.
 */
static void on_emotion_event(const robot_event_t *event)
{
    if (event->payload != NULL &&
        event->payload_len >= sizeof(emotion_event_t)) {
        emotion_event_t *data = (emotion_event_t *)event->payload;
        s_client.current_emotion = data->emotion_id;
    }
}

/**
 * @brief Handle EVENT_POWER_STATE_CHANGE — cache power state.
 */
static void on_power_event(const robot_event_t *event)
{
    if (event->payload != NULL &&
        event->payload_len >= sizeof(power_event_t)) {
        power_event_t *data = (power_event_t *)event->payload;
        s_client.power_state = data->state;
    }
}

/* ============================================================
 * MQTT event handler
 * ============================================================ */

/**
 * @brief ESP-IDF MQTT event handler
 *
 * Handles CONNECTED, DISCONNECTED, DATA, and ERROR events.
 */
static void mqtt_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    (void)arg;
    (void)base;

    esp_mqtt_event_handle_t evt = (esp_mqtt_event_handle_t)event_data;

    switch (event_id) {

    case MQTT_EVENT_CONNECTED: {
        ESP_LOGI(TAG, "Connected to MQTT broker");
        xSemaphoreTake(s_client.mutex, portMAX_DELAY);
        s_client.connected = true;
        xSemaphoreGive(s_client.mutex);

        /* Notify the system via event bus */
        publish_event(EVENT_MQTT_CONNECTED, NULL, 0);

        /* Subscribe to all command topics */
        subscribe_cmd_topics();

        /* Start periodic status publishing */
        if (s_client.status_timer != NULL) {
            xTimerStart(s_client.status_timer, 0);
        }

        /* Publish initial status */
        publish_status_battery();
        publish_status_wifi();
        publish_status_emotion();
        publish_status_power();
        break;
    }

    case MQTT_EVENT_DISCONNECTED: {
        ESP_LOGW(TAG, "Disconnected from MQTT broker");
        xSemaphoreTake(s_client.mutex, portMAX_DELAY);
        s_client.connected = false;
        xSemaphoreGive(s_client.mutex);

        /* Stop status timer while disconnected */
        if (s_client.status_timer != NULL) {
            xTimerStop(s_client.status_timer, 0);
        }

        /* Notify the system via event bus */
        publish_event(EVENT_MQTT_DISCONNECTED, NULL, 0);
        break;
    }

    case MQTT_EVENT_DATA: {
        /* Extract topic and payload from the event */
        char topic[MQTT_TOPIC_MAX_LEN] = {0};
        size_t topic_len = evt->topic_len;
        if (topic_len >= sizeof(topic)) {
            topic_len = sizeof(topic) - 1;
        }
        memcpy(topic, evt->topic, topic_len);
        topic[topic_len] = '\0';

        char payload_buf[MQTT_PAYLOAD_MAX_LEN] = {0};
        size_t payload_len = evt->data_len;
        if (payload_len >= sizeof(payload_buf)) {
            payload_len = sizeof(payload_buf) - 1;
            ESP_LOGW(TAG, "Payload truncated from %u to %u bytes",
                     (unsigned)evt->data_len, (unsigned)payload_len);
        }
        memcpy(payload_buf, evt->data, payload_len);
        payload_buf[payload_len] = '\0';

        ESP_LOGD(TAG, "Received: topic=%s, len=%zu", topic, payload_len);

        /* Invoke raw message callback if registered */
        if (s_client.msg_callback != NULL) {
            s_client.msg_callback(topic, payload_buf, payload_len);
        }

        /* Parse the topic to extract the command sub-topic.
         * Expected format: robotbuddy/{device_id}/cmd/{sub_topic} */
        char cmd_prefix[MQTT_TOPIC_MAX_LEN];
        snprintf(cmd_prefix, sizeof(cmd_prefix), "%s%s%s",
                 MQTT_TOPIC_PREFIX,
                 s_client.config.device_id,
                 MQTT_TOPIC_CMD_SUFFIX);

        size_t prefix_len = strlen(cmd_prefix);
        if (strncmp(topic, cmd_prefix, prefix_len) == 0) {
            const char *sub_topic = topic + prefix_len;
            handle_cmd_message(sub_topic, payload_buf, payload_len);
        } else {
            ESP_LOGD(TAG, "Topic does not match cmd prefix: %s", topic);
        }
        break;
    }

    case MQTT_EVENT_ERROR: {
        ESP_LOGE(TAG, "MQTT error event");
        if (evt->error_handle != NULL) {
            ESP_LOGE(TAG, "Last error: 0x%04X", evt->error_handle->error_type);
        }
        break;
    }

    default:
        break;
    }
}

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t mqtt_client_init(const mqtt_config_t *config)
{
    if (s_client.initialized) {
        ESP_LOGD(TAG, "Already initialized");
        return ESP_OK;
    }

    if (config == NULL) {
        ESP_LOGE(TAG, "Config cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* Create mutex */
    s_client.mutex = xSemaphoreCreateMutex();
    if (s_client.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Create periodic status timer (auto-reload, initially stopped) */
    s_client.status_timer = xTimerCreate("mqtt_status",
                                          pdMS_TO_TICKS(MQTT_STATUS_TIMER_MS),
                                          pdTRUE,    /* auto-reload */
                                          NULL,      /* timer ID */
                                          status_timer_cb);
    if (s_client.status_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create status timer");
        vSemaphoreDelete(s_client.mutex);
        s_client.mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    /* Store configuration with defaults */
    memcpy(&s_client.config, config, sizeof(mqtt_config_t));
    if (s_client.config.keepalive_sec == 0) {
        s_client.config.keepalive_sec = MQTT_DEFAULT_KEEPALIVE;
    }
    if (s_client.config.qos == 0) {
        s_client.config.qos = MQTT_DEFAULT_QOS;
    }

    /* Subscribe to events for status caching */
    event_bus_subscribe(EVENT_SENSOR_BATTERY, on_battery_event);
    event_bus_subscribe(EVENT_SYS_WIFI_CONNECTED, on_wifi_event);
    event_bus_subscribe(EVENT_SYS_WIFI_DISCONNECTED, on_wifi_event);
    event_bus_subscribe(EVENT_EMOTION_STATE_CHANGE, on_emotion_event);
    event_bus_subscribe(EVENT_POWER_STATE_CHANGE, on_power_event);

    s_client.initialized = true;
    s_client.started     = false;
    s_client.connected   = false;
    s_client.client      = NULL;
    s_client.msg_callback = NULL;

    ESP_LOGI(TAG, "MQTT client initialized (broker=%s, device=%s)",
             s_client.config.broker_url, s_client.config.device_id);
    return ESP_OK;
}

esp_err_t mqtt_client_deinit(void)
{
    if (!s_client.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* Stop client if running */
    if (s_client.started) {
        mqtt_client_stop();
    }

    /* Unsubscribe from event bus */
    event_bus_unsubscribe(EVENT_SENSOR_BATTERY, on_battery_event);
    event_bus_unsubscribe(EVENT_SYS_WIFI_CONNECTED, on_wifi_event);
    event_bus_unsubscribe(EVENT_SYS_WIFI_DISCONNECTED, on_wifi_event);
    event_bus_unsubscribe(EVENT_EMOTION_STATE_CHANGE, on_emotion_event);
    event_bus_unsubscribe(EVENT_POWER_STATE_CHANGE, on_power_event);

    /* Delete status timer */
    if (s_client.status_timer != NULL) {
        xTimerDelete(s_client.status_timer, 0);
        s_client.status_timer = NULL;
    }

    /* Delete mutex */
    if (s_client.mutex != NULL) {
        vSemaphoreDelete(s_client.mutex);
        s_client.mutex = NULL;
    }

    s_client.initialized = false;
    s_client.client      = NULL;

    ESP_LOGI(TAG, "MQTT client deinitialized");
    return ESP_OK;
}

esp_err_t mqtt_client_start(void)
{
    if (!s_client.initialized) {
        ESP_LOGE(TAG, "Not initialized — call mqtt_client_init() first");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_client.started) {
        ESP_LOGD(TAG, "Already started");
        return ESP_OK;
    }

    /* Configure ESP-IDF MQTT client */
    esp_mqtt_client_config_t cfg = {
        .broker.uri            = s_client.config.broker_url,
        .credentials.client_id = s_client.config.client_id,
        .credentials.username  = s_client.config.username[0] ? s_client.config.username : NULL,
        .credentials.authentication.password =
            s_client.config.password[0] ? s_client.config.password : NULL,
        .session.keepalive     = s_client.config.keepalive_sec,
        .session.disable_clean_session = false,
    };

    s_client.client = esp_mqtt_client_init(&cfg);
    if (s_client.client == NULL) {
        ESP_LOGE(TAG, "Failed to create MQTT client handle");
        return ESP_FAIL;
    }

    /* Register MQTT event handler */
    esp_err_t err = esp_mqtt_client_register_event(s_client.client,
                                                     ESP_EVENT_ANY_ID,
                                                     mqtt_event_handler,
                                                     NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MQTT event handler: %s",
                 esp_err_to_name(err));
        esp_mqtt_client_destroy(s_client.client);
        s_client.client = NULL;
        return err;
    }

    /* Start the client — this initiates the connection */
    err = esp_mqtt_client_start(s_client.client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s",
                 esp_err_to_name(err));
        esp_mqtt_client_destroy(s_client.client);
        s_client.client = NULL;
        return err;
    }

    s_client.started = true;

    ESP_LOGI(TAG, "MQTT client started (connecting to %s…)",
             s_client.config.broker_url);
    return ESP_OK;
}

esp_err_t mqtt_client_stop(void)
{
    if (!s_client.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_client.started) {
        ESP_LOGD(TAG, "Not started");
        return ESP_OK;
    }

    /* Stop status timer */
    if (s_client.status_timer != NULL) {
        xTimerStop(s_client.status_timer, 0);
    }

    /* Stop and destroy the MQTT client */
    if (s_client.client != NULL) {
        esp_mqtt_client_stop(s_client.client);
        esp_mqtt_client_destroy(s_client.client);
        s_client.client = NULL;
    }

    xSemaphoreTake(s_client.mutex, portMAX_DELAY);
    s_client.started   = false;
    s_client.connected = false;
    xSemaphoreGive(s_client.mutex);

    ESP_LOGI(TAG, "MQTT client stopped");
    return ESP_OK;
}

esp_err_t mqtt_client_publish(const char *topic,
                               const char *data,
                               size_t len)
{
    if (!s_client.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (topic == NULL || data == NULL) {
        ESP_LOGE(TAG, "Topic and data cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_client.connected) {
        ESP_LOGW(TAG, "Not connected — cannot publish");
        return ESP_ERR_INVALID_STATE;
    }

    /* Build full topic if not already prefixed */
    char full_topic[MQTT_TOPIC_MAX_LEN];
    if (strncmp(topic, MQTT_TOPIC_PREFIX, strlen(MQTT_TOPIC_PREFIX)) == 0) {
        /* Already a full topic path */
        strncpy(full_topic, topic, sizeof(full_topic) - 1);
        full_topic[sizeof(full_topic) - 1] = '\0';
    } else {
        /* Prepend robotbuddy/{device_id}/status/ */
        snprintf(full_topic, sizeof(full_topic), "%s%s%s%s",
                 MQTT_TOPIC_PREFIX,
                 s_client.config.device_id,
                 MQTT_TOPIC_STATUS_SUFFIX,
                 topic);
    }

    xSemaphoreTake(s_client.mutex, portMAX_DELAY);
    int msg_id = esp_mqtt_client_publish(s_client.client,
                                          full_topic, data, (int)len,
                                          s_client.config.qos, 0);
    xSemaphoreGive(s_client.mutex);

    if (msg_id < 0) {
        ESP_LOGW(TAG, "Publish failed on %s", full_topic);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Published to %s (%zu bytes, msg_id=%d)",
             full_topic, len, msg_id);
    return ESP_OK;
}

bool mqtt_client_is_connected(void)
{
    bool connected;
    xSemaphoreTake(s_client.mutex, portMAX_DELAY);
    connected = s_client.connected;
    xSemaphoreGive(s_client.mutex);
    return connected;
}
