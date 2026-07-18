/**
 * @file vl53l0x.h
 * @brief VL53L0X Time-of-Flight distance sensor driver for ESP32-S3 (ESP-IDF v5.x).
 *
 * Provides initialisation, configuration, and distance measurement for the
 * ST VL53L0X ToF sensor over I²C.  Uses the new ESP-IDF v5.x I²C master API
 * (driver/i2c_master.h) for communication.
 *
 * The driver operates the VL53L0X in single-ranging mode: each call to
 * vl53l0x_get_distance_mm() triggers a measurement and waits for the result.
 *
 * @copyright 2026 RobotBuddy contributors
 * @licence MIT
 */

#ifndef VL53L0X_H
#define VL53L0X_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */
/*  Constants                                                                */
/* ------------------------------------------------------------------------ */

#define VL53L0X_DEFAULT_I2C_ADDR  0x29  /**< Default I²C address (7-bit)    */
#define VL53L0X_DEFAULT_TIMEOUT_MS 30   /**< Default ranging timeout (ms)   */

/* ------------------------------------------------------------------------ */
/*  Type definitions                                                         */
/* ------------------------------------------------------------------------ */

/**
 * @brief VL53L0X ranging mode.
 *
 * Controls the maximum range and ambient light immunity of the sensor.
 */
typedef enum {
    VL53L0X_SHORT_RANGE  = 0,  /**< Max ~1.3 m, highest speed             */
    VL53L0X_MEDIUM_RANGE = 1,  /**< Max ~2 m, balanced                    */
    VL53L0X_LONG_RANGE   = 2,  /**< Max ~2 m, better ambient immunity     */
} vl53l0x_range_mode_t;

/**
 * @brief VL53L0X configuration.
 *
 * Pass a populated instance to vl53l0x_init().  The I²C master bus handle
 * must already be initialised (e.g. via bsp_board_init()).
 */
typedef struct {
    i2c_master_bus_handle_t i2c_bus;  /**< I²C master bus handle from BSP       */
    uint8_t  i2c_addr;                /**< Device I²C address (default: 0x29)   */
    uint32_t timeout_ms;              /**< Ranging timeout in ms (default: 30)  */
} vl53l0x_config_t;

/* ------------------------------------------------------------------------ */
/*  Public API                                                               */
/* ------------------------------------------------------------------------ */

/**
 * @brief Initialise the VL53L0X driver.
 *
 * Creates an I²C device handle from the bus handle, verifies the sensor
 * is present (reads identification registers), and applies the default
 * (short-range) configuration.
 *
 * @param config  Pointer to a populated configuration struct.  Must not be
 *                NULL; the contents are copied internally.
 * @return ESP_OK on success; ESP_ERR_INVALID_ARG if config is NULL;
 *         ESP_ERR_NOT_FOUND if the sensor does not respond;
 *         or an ESP-IDF error code on communication failure.
 */
esp_err_t vl53l0x_init(const vl53l0x_config_t *config);

/**
 * @brief De-initialise the VL53L0X driver.
 *
 * Releases the I²C device handle and resets internal state.
 *
 * @return ESP_OK on success.
 */
esp_err_t vl53l0x_deinit(void);

/**
 * @brief Start continuous ranging.
 *
 * Configures and starts the VL53L0X in continuous ranging mode.
 * Call vl53l0x_get_distance_mm() to read results.
 *
 * @return ESP_OK on success; ESP_ERR_INVALID_STATE if not initialised.
 */
esp_err_t vl53l0x_start_ranging(void);

/**
 * @brief Stop continuous ranging.
 *
 * Stops the VL53L0X ranging engine and puts it in standby.
 *
 * @return ESP_OK on success; ESP_ERR_INVALID_STATE if not initialised.
 */
esp_err_t vl53l0x_stop_ranging(void);

/**
 * @brief Perform a single distance measurement.
 *
 * Triggers a single ranging measurement and waits for the result.
 * The sensor is put into single-shot mode regardless of prior mode.
 *
 * @param[out] distance_mm  Pointer to receive the distance in millimetres.
 *                          Must not be NULL.  Set to 0 on error or timeout.
 * @return ESP_OK on success; ESP_ERR_INVALID_ARG if distance_mm is NULL;
 *         ESP_ERR_INVALID_STATE if not initialised;
 *         ESP_ERR_TIMEOUT if the measurement does not complete in time;
 *         or an ESP-IDF error code on communication failure.
 */
esp_err_t vl53l0x_get_distance_mm(uint16_t *distance_mm);

/**
 * @brief Set the ranging mode.
 *
 * Changes the maximum range and ambient light immunity of the sensor.
 * Takes effect on the next ranging operation.
 *
 * @param mode  The desired ranging mode.
 * @return ESP_OK on success; ESP_ERR_INVALID_STATE if not initialised.
 */
esp_err_t vl53l0x_set_range_mode(vl53l0x_range_mode_t mode);

/**
 * @brief Check whether a VL53L0X is present on the I²C bus.
 *
 * Reads the model identification register and verifies the expected value.
 *
 * @return true if a device responded with the correct ID; false otherwise.
 */
bool vl53l0x_is_present(void);

#ifdef __cplusplus
}
#endif

#endif /* VL53L0X_H */
