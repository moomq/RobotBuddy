/**
 * @file mpu6050.h
 * @brief MPU6050 6-axis IMU driver for ESP32-S3 (ESP-IDF v5.x).
 *
 * Provides initialisation, configuration, and data-reading for the InvenSense
 * MPU6050 accelerometer + gyroscope over I²C.  The driver expects an
 * I²C master bus handle (obtained from the BSP layer) and handles all
 * register-level interaction internally.
 *
 * @copyright 2026 RobotBuddy contributors
 * @licence MIT
 */

#ifndef MPU6050_H
#define MPU6050_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */
/*  Full-scale range constants                                              */
/* ------------------------------------------------------------------------ */

#define MPU6050_ACCEL_RANGE_2G     2    /**< ±2 g  accelerometer range */
#define MPU6050_ACCEL_RANGE_4G     4    /**< ±4 g  accelerometer range */
#define MPU6050_ACCEL_RANGE_8G     8    /**< ±8 g  accelerometer range */
#define MPU6050_ACCEL_RANGE_16G   16    /**< ±16 g accelerometer range */

#define MPU6050_GYRO_RANGE_250   250   /**< ±250 °/s  gyroscope range */
#define MPU6050_GYRO_RANGE_500   500   /**< ±500 °/s  gyroscope range */
#define MPU6050_GYRO_RANGE_1000 1000   /**< ±1000 °/s gyroscope range */
#define MPU6050_GYRO_RANGE_2000 2000   /**< ±2000 °/s gyroscope range */

/* ------------------------------------------------------------------------ */
/*  Type definitions                                                        */
/* ------------------------------------------------------------------------ */

/**
 * @brief 6-axis inertial-measurement data with temperature.
 *
 * All values are in SI / engineering units:
 *  • Accelerometer — m/s²
 *  • Gyroscope     — °/s
 *  • Temperature   — °C
 */
typedef struct {
    float accel_x;      /**< X-axis acceleration (m/s²) */
    float accel_y;      /**< Y-axis acceleration (m/s²) */
    float accel_z;      /**< Z-axis acceleration (m/s²) */
    float gyro_x;       /**< X-axis angular rate (°/s) */
    float gyro_y;       /**< Y-axis angular rate (°/s) */
    float gyro_z;       /**< Z-axis angular rate (°/s) */
    float temperature;  /**< Die temperature (°C) */
} mpu6050_data_t;

/**
 * @brief MPU6050 configuration.
 *
 * Pass a populated instance to mpu6050_init().  The I²C master bus handle
 * must already be initialised (e.g. via bsp_board_init()).
 */
typedef struct {
    i2c_port_t i2c_port;    /**< I²C port number (0 or 1)                      */
    uint8_t addr;           /**< Device I²C address: 0x68 or 0x69 (AD0 pin)    */
    uint16_t accel_range;   /**< Accelerometer full-scale: 2, 4, 8, or 16 (g) */
    uint16_t gyro_range;    /**< Gyroscope full-scale: 250, 500, 1000, or 2000 (°/s) */
} mpu6050_config_t;

/* ------------------------------------------------------------------------ */
/*  Public API                                                              */
/* ------------------------------------------------------------------------ */

/**
 * @brief Initialise the MPU6050.
 *
 * Resets the device, configures sample-rate divider, DLPF, and full-scale
 * ranges according to @p config, then leaves the device in active mode.
 *
 * @param config  Pointer to a populated configuration struct.  Must not be
 *                NULL; the contents are copied internally.
 * @return ESP_OK on success; ESP_FAIL or an ESP-IDF error code on failure.
 */
esp_err_t mpu6050_init(const mpu6050_config_t *config);

/**
 * @brief De-initialise the MPU6050 driver.
 *
 * Places the device in sleep mode and releases any driver resources.
 *
 * @return ESP_OK on success.
 */
esp_err_t mpu6050_deinit(void);

/**
 * @brief Read the latest sensor data from the MPU6050.
 *
 * Burst-reads 14 bytes starting at ACCEL_XOUT_H and converts the raw
 * values to physical units according to the configured full-scale ranges.
 *
 * @param[out] data  Pointer to a caller-allocated struct to receive data.
 *                   Must not be NULL.
 * @return ESP_OK on success; ESP_ERR_INVALID_STATE if not initialised.
 */
esp_err_t mpu6050_read(mpu6050_data_t *data);

/**
 * @brief Put the MPU6050 into low-power sleep mode.
 *
 * Current consumption drops to ~8 µA.  Call mpu6050_wakeup() to resume.
 *
 * @return ESP_OK on success.
 */
esp_err_t mpu6050_sleep(void);

/**
 * @brief Wake the MPU6050 from sleep mode.
 *
 * The device is left in the same configuration that was active before
 * mpu6050_sleep() was called.
 *
 * @return ESP_OK on success.
 */
esp_err_t mpu6050_wakeup(void);

/**
 * @brief Check whether an MPU6050 is present on the I²C bus.
 *
 * Reads the WHO_AM_I register and verifies the expected value (0x68).
 *
 * @return true if a device responded with the correct ID; false otherwise.
 */
bool mpu6050_is_present(void);

#ifdef __cplusplus
}
#endif

#endif /* MPU6050_H */