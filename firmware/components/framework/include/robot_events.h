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

#ifdef __cplusplus
}
#endif