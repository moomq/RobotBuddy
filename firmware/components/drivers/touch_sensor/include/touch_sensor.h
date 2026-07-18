/**
 * @file touch_sensor.h
 * @brief Capacitive touch sensor driver for RobotBuddy (ESP32-S3).
 *
 * Uses the ESP32-S3 internal touch peripheral to detect touch gestures
 * (single tap, double tap, long press) on a single capacitive pad.
 * Detected gestures are published to the RobotBuddy event bus.
 *
 * @copyright 2026 RobotBuddy contributors
 * @licence MIT
 */

#ifndef TOUCH_SENSOR_H
#define TOUCH_SENSOR_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "robot_events.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */
/*  Constants                                                                */
/* ------------------------------------------------------------------------ */

#define TOUCH_SENSOR_DEFAULT_GPIO        2     /**< Default touch pad GPIO (TOUCH_PAD_NUM2) */
#define TOUCH_SENSOR_DEFAULT_THRESHOLD   0     /**< 0 = auto-calibrate on init              */
#define TOUCH_SENSOR_DEFAULT_DEBOUNCE_MS 50    /**< Debounce period (ms)                    */
#define TOUCH_SENSOR_DEFAULT_DOUBLE_TAP_MS 300 /**< Double-tap window (ms)                  */
#define TOUCH_SENSOR_DEFAULT_LONG_PRESS_MS 1000 /**< Long-press threshold (ms)              */

/* ------------------------------------------------------------------------ */
/*  Type definitions                                                         */
/* ------------------------------------------------------------------------ */

/**
 * @brief Touch sensor configuration.
 *
 * Pass a populated instance to touch_sensor_init().  All timing fields
 * have sensible defaults — set them to 0 to accept the default value.
 */
typedef struct {
    int gpio_num;           /**< GPIO number for the touch pad (default: 2)       */
    uint16_t threshold;     /**< Detection threshold (0 = auto-calibrate)         */
    uint16_t debounce_ms;   /**< Debounce period in ms (default: 50)              */
    uint16_t double_tap_ms; /**< Double-tap window in ms (default: 300)           */
    uint16_t long_press_ms; /**< Long-press threshold in ms (default: 1000)       */
} touch_sensor_config_t;

/* ------------------------------------------------------------------------ */
/*  Public API                                                               */
/* ------------------------------------------------------------------------ */

/**
 * @brief Initialise the touch sensor driver.
 *
 * Configures the ESP32-S3 touch peripheral for the specified GPIO,
 * calibrates the baseline threshold (if threshold == 0), and starts
 * a background polling task for gesture detection.
 *
 * @param config  Pointer to a populated configuration struct.  Must not be
 *                NULL; the contents are copied internally.
 * @return ESP_OK on success; ESP_ERR_INVALID_ARG if config is NULL;
 *         ESP_ERR_INVALID_STATE if already initialised; or an ESP-IDF
 *         error code on peripheral failure.
 */
esp_err_t touch_sensor_init(const touch_sensor_config_t *config);

/**
 * @brief De-initialise the touch sensor driver.
 *
 * Stops the polling task and releases touch peripheral resources.
 *
 * @return ESP_OK on success.
 */
esp_err_t touch_sensor_deinit(void);

/**
 * @brief Get the last detected gesture.
 *
 * Returns the gesture that was most recently detected by the driver.
 * This is a non-blocking query that does not interact with the touch
 * peripheral directly.
 *
 * @return The current touch_gesture_t value.
 */
touch_gesture_t touch_sensor_get_gesture(void);

/**
 * @brief Check whether the touch pad is currently being pressed.
 *
 * Reads the current touch pad value and compares it against the
 * calibrated threshold.
 *
 * @return true if the pad is being touched; false otherwise.
 */
bool touch_sensor_is_pressed(void);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_SENSOR_H */
