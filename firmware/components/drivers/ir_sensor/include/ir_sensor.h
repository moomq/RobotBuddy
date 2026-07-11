/**
 * @file ir_sensor.h
 * @brief Infra-red obstacle / edge sensor driver for RobotBuddy (ESP32-S3).
 *
 * Each IR sensor module outputs a digital signal that is active-LOW when an
 * obstacle (or edge) is detected.  The driver configures four GPIO pins with
 * internal pull-ups and provides a single call to read all four channels.
 *
 * @copyright 2026 RobotBuddy contributors
 * @licence MIT
 */

#ifndef IR_SENSOR_H
#define IR_SENSOR_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */
/*  Type definitions                                                        */
/* ------------------------------------------------------------------------ */

/**
 * @brief GPIO assignment for the four IR sensor channels.
 *
 * Each member identifies the GPIO number connected to the corresponding
 * sensor module's digital output.
 */
typedef struct {
    int pin_obstacle_left;   /**< GPIO for left obstacle sensor    */
    int pin_obstacle_right;  /**< GPIO for right obstacle sensor   */
    int pin_edge_left;       /**< GPIO for left edge/cliff sensor  */
    int pin_edge_right;      /**< GPIO for right edge/cliff sensor */
} ir_sensor_config_t;

/**
 * @brief IR sensor state for all four channels.
 *
 * A member is @c true when the corresponding sensor has detected an
 * obstacle or edge (i.e. the digital output is LOW).
 */
typedef struct {
    bool obstacle_left;   /**< Left obstacle detected   */
    bool obstacle_right;  /**< Right obstacle detected  */
    bool edge_left;       /**< Left edge / cliff detected */
    bool edge_right;      /**< Right edge / cliff detected */
} ir_sensor_data_t;

/* ------------------------------------------------------------------------ */
/*  Public API                                                              */
/* ------------------------------------------------------------------------ */

/**
 * @brief Initialise the IR sensor driver.
 *
 * Configures all four GPIO pins as inputs with internal pull-up resistors.
 *
 * @param config  Pointer to a populated configuration struct.  Must not be
 *                NULL; the contents are copied internally.
 * @return ESP_OK on success; an ESP-IDF error code on failure.
 */
esp_err_t ir_sensor_init(const ir_sensor_config_t *config);

/**
 * @brief De-initialise the IR sensor driver.
 *
 * Resets all four GPIO pins to their default (reset) state.
 *
 * @return ESP_OK on success.
 */
esp_err_t ir_sensor_deinit(void);

/**
 * @brief Read the current state of all four IR sensors.
 *
 * Each channel is read via gpio_get_level().  Because the sensors are
 * active-LOW, a low level is translated to @c true (detected).
 *
 * @return An ir_sensor_data_t struct with the current sensor states.
 */
ir_sensor_data_t ir_sensor_read(void);

#ifdef __cplusplus
}
#endif

#endif /* IR_SENSOR_H */