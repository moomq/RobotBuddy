/*
 * RobotBuddy — Event ID Definitions
 * ===================================
 * Central event definitions for the RobotBuddy event bus.
 * All modules publish and subscribe to these events.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Event ID Enumeration
 * ============================================================
 * Format: 0xXXYY where XX = module, YY = event within module
 */

typedef enum {
    /* System events (0x00xx) */
    EVENT_SYS_BOOT_COMPLETE     = 0x0001,
    EVENT_SYS_WIFI_CONNECTED    = 0x0002,
    EVENT_SYS_WIFI_DISCONNECTED = 0x0003,
    EVENT_SYS_LOW_BATTERY      = 0x0004,
    EVENT_SYS_CRITICAL_BATTERY = 0x0005,
    EVENT_SYS_CHARGING         = 0x0006,
    EVENT_SYS_CHARGE_COMPLETE  = 0x0007,

    /* Emotion events (0x01xx) */
    EVENT_EMOTION_STATE_CHANGE  = 0x0100,
    EVENT_EMOTION_BLINK         = 0x0101,
    EVENT_EMOTION_ANIMATION_DONE = 0x0102,

    /* Audio events (0x02xx) */
    EVENT_AUDIO_CAPTURE_START   = 0x0200,
    EVENT_AUDIO_CAPTURE_STOP    = 0x0201,
    EVENT_AUDIO_CAPTURE_DATA    = 0x0202,
    EVENT_AUDIO_PLAY_START      = 0x0210,
    EVENT_AUDIO_PLAY_STOP       = 0x0211,
    EVENT_AUDIO_PLAY_DONE       = 0x0212,
    EVENT_AUDIO_PLAY_ERROR      = 0x0213,

    /* Cloud events (0x03xx) */
    EVENT_CLOUD_CONNECTED       = 0x0300,
    EVENT_CLOUD_DISCONNECTED    = 0x0301,
    EVENT_CLOUD_ASR_RESULT     = 0x0302,
    EVENT_CLOUD_LLM_RESPONSE   = 0x0303,
    EVENT_CLOUD_TTS_DATA       = 0x0304,
    EVENT_CLOUD_ERROR           = 0x0310,

    /* Sensor events (0x04xx) */
    EVENT_SENSOR_IMU_DATA       = 0x0400,
    EVENT_SENSOR_OBSTACLE       = 0x0401,
    EVENT_SENSOR_EDGE           = 0x0402,
    EVENT_SENSOR_FALL_DETECTED  = 0x0403,
    EVENT_SENSOR_BATTERY        = 0x0404,

    /* Motion events (0x05xx) */
    EVENT_MOTION_COMMAND        = 0x0500,
    EVENT_MOTION_COMPLETE      = 0x0501,
    EVENT_MOTION_EMERGENCY_STOP = 0x0502,

    /* Behavior events (0x06xx) */
    EVENT_BEHAVIOR_STATE_CHANGE = 0x0600,
    EVENT_BEHAVIOR_COMMAND      = 0x0601,

    /* Display text events (0x07xx) — V2.0 */
    EVENT_DISPLAY_TEXT_MSG       = 0x0700,
    EVENT_DISPLAY_CLEAR_TEXT     = 0x0701,
    EVENT_DISPLAY_STATUS_ICON    = 0x0702,

    /* Touch events (0x08xx) — V2.0 */
    EVENT_TOUCH_SINGLE          = 0x0800,
    EVENT_TOUCH_DOUBLE          = 0x0801,
    EVENT_TOUCH_LONG            = 0x0802,

    /* Wake word events (0x09xx) — V2.0 */
    EVENT_WAKE_WORD_DETECTED    = 0x0900,

    /* MQTT events (0x0Axx) — V2.0 */
    EVENT_MQTT_CONNECTED        = 0x0A00,
    EVENT_MQTT_DISCONNECTED     = 0x0A01,
    EVENT_MQTT_MESSAGE          = 0x0A02,

    /* OTA events (0x0Bxx) — V2.0 */
    EVENT_OTA_START             = 0x0B00,
    EVENT_OTA_PROGRESS          = 0x0B01,
    EVENT_OTA_COMPLETE          = 0x0B02,
    EVENT_OTA_ERROR             = 0x0B03,

    /* Pomodoro events (0x0Cxx) — V2.0 */
    EVENT_POMODORO_START        = 0x0C00,
    EVENT_POMODORO_TICK         = 0x0C01,
    EVENT_POMODORO_DONE         = 0x0C02,
    EVENT_POMODORO_BREAK_DONE   = 0x0C03,

    /* Dev tool events (0x0Dxx) — V2.0 */
    EVENT_BUILD_STATUS          = 0x0D00,
    EVENT_GIT_STATUS            = 0x0D01,

    /* Power events (0x0Exx) — V2.0 */
    EVENT_POWER_STATE_CHANGE    = 0x0E00,
    EVENT_POWER_ENTER_SLEEP     = 0x0E01,
    EVENT_POWER_WAKEUP          = 0x0E02,

} robot_event_id_t;

/* ============================================================
 * Emotion IDs
 * ============================================================ */

typedef enum {
    EMOTION_IDLE = 0,
    EMOTION_LISTENING,
    EMOTION_THINKING,
    EMOTION_ANSWERING,
    EMOTION_HAPPY,
    EMOTION_CONFUSED,
    EMOTION_WARNING,
    EMOTION_ERROR,
    EMOTION_FOCUS,
    EMOTION_SLEEP,
    EMOTION_EXCITED,
    EMOTION_COUNT,
} emotion_id_t;

/* ============================================================
 * Motion Commands
 * ============================================================ */

typedef enum {
    MOTION_CMD_STOP       = 0,
    MOTION_CMD_FORWARD    = 1,
    MOTION_CMD_BACKWARD   = 2,
    MOTION_CMD_TURN_LEFT  = 3,
    MOTION_CMD_TURN_RIGHT = 4,
    MOTION_CMD_ROTATE     = 5,
} motion_cmd_t;

/* ============================================================
 * Behavior States
 * ============================================================ */

typedef enum {
    BEHAVIOR_STATE_IDLE       = 0,
    BEHAVIOR_STATE_LISTENING  = 1,
    BEHAVIOR_STATE_THINKING  = 2,
    BEHAVIOR_STATE_ANSWERING = 3,
    BEHAVIOR_STATE_HAPPY     = 4,
    BEHAVIOR_STATE_WARNING   = 5,
    BEHAVIOR_STATE_ERROR     = 6,
    BEHAVIOR_STATE_SLEEP     = 7,
} behavior_state_t;

/* ============================================================
 * WiFi States
 * ============================================================ */

typedef enum {
    WIFI_STATE_IDLE         = 0,
    WIFI_STATE_CONNECTING   = 1,
    WIFI_STATE_CONNECTED    = 2,
    WIFI_STATE_DISCONNECTED = 3,
    WIFI_STATE_SMARTCONFIG = 4,
} wifi_state_t;

/* ============================================================
 * Cloud Providers
 * ============================================================ */

typedef enum {
    CLOUD_PROVIDER_CLAUDE   = 0,
    CLOUD_PROVIDER_OPENAI   = 1,
    CLOUD_PROVIDER_DEEPSEEK = 2,
} cloud_provider_t;

/* ============================================================
 * Event Data Structures
 * ============================================================ */

/**
 * @brief Generic event structure
 */
typedef struct {
    robot_event_id_t id;        /**< Event ID */
    uint32_t timestamp;          /**< Timestamp in ms */
    void *payload;               /**< Payload (dynamically allocated, consumer frees) */
    size_t payload_len;          /**< Payload length */
} robot_event_t;

/**
 * @brief Emotion state change payload
 */
typedef struct {
    emotion_id_t emotion_id;    /**< Target emotion */
    uint16_t duration_ms;        /**< Duration (0 = until next change) */
    uint8_t intensity;           /**< Intensity (0-255) */
} emotion_event_t;

/**
 * @brief Sensor data payload
 */
typedef struct {
    float accel_x, accel_y, accel_z;   /**< Acceleration (m/s²) */
    float gyro_x, gyro_y, gyro_z;      /**< Gyroscope (°/s) */
    float temperature;                  /**< Temperature (°C) */
    bool obstacle_left, obstacle_right; /**< Obstacle detection */
    bool edge_left, edge_right;         /**< Edge detection */
    float battery_voltage;              /**< Battery voltage (V) */
    uint8_t battery_percent;            /**< Battery percentage */
} sensor_data_t;

/**
 * @brief Motion command payload
 */
typedef struct {
    motion_cmd_t command;        /**< Motion command */
    int16_t speed;               /**< Speed (0-255) */
    int16_t angle;               /**< Angle (for rotation, ±360°) */
    uint16_t duration_ms;        /**< Duration */
} motion_cmd_payload_t;

/**
 * @brief Cloud response payload
 */
typedef struct {
    char text[512];               /**< LLM response text */
    cloud_provider_t provider;    /**< AI provider */
    uint16_t latency_ms;          /**< Response latency (ms) */
} cloud_response_t;

/**
 * @brief ASR result payload
 */
typedef struct {
    char text[256];               /**< Recognized text */
    float confidence;             /**< Confidence (0.0 - 1.0) */
} asr_result_t;

/* ============================================================
 * V2.0 Event Payload Structures
 * ============================================================ */

/**
 * @brief Text message payload (for scrolling display)
 */
typedef struct {
    char text[200];               /**< Message text */
    uint8_t priority;             /**< Priority (0=low, 1=medium, 2=high) */
    uint16_t duration_ms;         /**< Display duration (0=default 10s) */
} text_msg_event_t;

/**
 * @brief Wake word detection payload
 */
typedef struct {
    char keyword[32];             /**< Detected keyword */
    float confidence;             /**< Detection confidence (0.0 - 1.0) */
} wake_word_event_t;

/**
 * @brief MQTT message payload
 */
typedef struct {
    char topic[128];              /**< Topic path */
    char payload[512];            /**< Message content */
    size_t payload_len;           /**< Content length */
} mqtt_message_event_t;

/**
 * @brief OTA progress payload
 */
typedef struct {
    uint8_t percent;              /**< Progress percentage (0-100) */
    uint32_t downloaded;          /**< Bytes downloaded */
    uint32_t total;               /**< Total bytes */
} ota_progress_event_t;

/**
 * @brief Build status payload (from VS Code / CI)
 */
typedef struct {
    uint8_t status;               /**< 0=running, 1=success, 2=fail, 3=warning */
    char msg[64];                 /**< Status message */
} build_status_event_t;

/**
 * @brief Git status payload
 */
typedef struct {
    uint8_t uncommitted;          /**< Uncommitted file count */
    uint8_t conflicts;            /**< Conflict file count */
} git_status_event_t;

/**
 * @brief Pomodoro timer payload
 */
typedef struct {
    uint16_t remaining_sec;       /**< Remaining seconds */
    uint8_t round;                /**< Current round number */
    bool is_break;                /**< True if in break period */
} pomodoro_event_t;

/**
 * @brief Power state change payload
 */
typedef struct {
    uint8_t state;                /**< power_state_t value */
    uint32_t idle_ms;             /**< Idle duration in ms */
} power_event_t;

/**
 * @brief Touch gesture type
 */
typedef enum {
    TOUCH_GESTURE_NONE = 0,
    TOUCH_GESTURE_SINGLE_TAP,
    TOUCH_GESTURE_DOUBLE_TAP,
    TOUCH_GESTURE_LONG_PRESS,
} touch_gesture_t;

/**
 * @brief Pomodoro state
 */
typedef enum {
    POMODORO_STATE_IDLE = 0,
    POMODORO_STATE_WORKING,
    POMODORO_STATE_BREAK,
    POMODORO_STATE_PAUSED,
} pomodoro_state_t;

/**
 * @brief Power management state
 */
typedef enum {
    POWER_STATE_ACTIVE = 0,
    POWER_STATE_DISPLAY_DIM,
    POWER_STATE_WIFI_LIGHT_SLEEP,
    POWER_STATE_DEEP_SLEEP,
} power_state_t;

/**
 * @brief OTA service state
 */
typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_VERIFYING,
    OTA_STATE_APPLYING,
    OTA_STATE_REBOOTING,
    OTA_STATE_ERROR,
} ota_state_t;

#ifdef __cplusplus
}
#endif