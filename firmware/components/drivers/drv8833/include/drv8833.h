/**
 * @file drv8833.h
 * @brief DRV8833 dual H-bridge motor driver interface for ESP32-S3.
 *
 * Provides PWM-based speed and direction control for two DC motors
 * via the TI DRV8833 driver chip. Uses the ESP-IDF LEDC peripheral
 * for PWM generation.
 *
 * @attention **Thread Safety:** The DRV8833 driver is designed to be called
 * from a single task context (typically the motion task). It does NOT
 * employ internal mutexes. The exception is drv8833_stop_all() which
 * may be called from any task (including ISR via motion_emergency_stop)
 * for emergency braking. Concurrent calls to drv8833_set_speed() from
 * multiple tasks are NOT safe and will result in race conditions on
 * the LEDC duty registers.
 *
 * @copyright Copyright (c) 2026 RobotBuddy Contributors
 * @license MIT
 */

#ifndef DRV8833_H
#define DRV8833_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/ledc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Motor identifiers                                                  */
/* ------------------------------------------------------------------ */

#define DRV8833_MOTOR_LEFT  0   /**< Left motor channel  */
#define DRV8833_MOTOR_RIGHT 1   /**< Right motor channel */

/* ------------------------------------------------------------------ */
/*  Speed limits                                                       */
/* ------------------------------------------------------------------ */

#define DRV8833_SPEED_MAX  255   /**< Maximum absolute speed value */

/* ------------------------------------------------------------------ */
/*  Configuration                                                      */
/* ------------------------------------------------------------------ */

/**
 * @brief DRV8833 driver configuration.
 *
 * The caller must provide the four GPIO pins that connect to the
 * DRV8833 AIN1, AIN2, BIN1, BIN2 inputs, the desired PWM frequency,
 * and the LEDC timer to use.
 *
 * @note The LEDC timer resolution is fixed at 8 bits (0–255 duty),
 *       matching the DRV8833_SPEED_MAX constant.
 */
typedef struct {
    int pin_ain1;            /**< GPIO for motor A input 1 (AIN1) */
    int pin_ain2;            /**< GPIO for motor A input 2 (AIN2) */
    int pin_bin1;            /**< GPIO for motor B input 1 (BIN1) */
    int pin_bin2;            /**< GPIO for motor B input 2 (BIN2) */
    uint32_t pwm_freq;       /**< PWM frequency in Hz (typ. 5–20 kHz) */
    ledc_timer_t pwm_timer;  /**< LEDC timer index (LEDC_TIMER_0 …) */
} drv8833_config_t;

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialise the DRV8833 driver.
 *
 * Configures the LEDC timer (8-bit resolution, @p config->pwm_freq Hz)
 * and four LEDC channels — one per H-bridge input pin.  After a
 * successful call both motors are in the **coast** state (both pins
 * LOW).
 *
 * @param config  Pointer to a populated configuration structure.
 *                The object is copied; the caller may discard it after
 *                this function returns.
 * @return ESP_OK            on success
 * @return ESP_ERR_INVALID_ARG  if @p config is NULL or contains
 *                invalid GPIO / timer values
 * @return ESP_FAIL           if LEDC timer or channel configuration fails
 */
esp_err_t drv8833_init(const drv8833_config_t *config);

/**
 * @brief De-initialise the DRV8833 driver.
 *
 * Stops PWM output on all channels, resets each GPIO to a
 * high-impedance state, and frees the LEDC timer.  After this call
 * the driver may be re-initialised with drv8833_init().
 *
 * @return ESP_OK on success
 */
esp_err_t drv8833_deinit(void);

/**
 * @brief Set the speed and direction of a motor.
 *
 * Positive values drive the motor forward (AIN1/BIN1 = PWM,
 * AIN2/BIN2 = LOW).  Negative values drive the motor in reverse
 * (AIN1/BIN1 = LOW, AIN2/BIN2 = PWM).  A speed of 0 puts the motor
 * in the **coast** state (both inputs LOW).
 *
 * @param motor  DRV8833_MOTOR_LEFT or DRV8833_MOTOR_RIGHT
 * @param speed  Motor speed in the range -255 … +255.
 *               Values outside this range are clamped.
 * @return ESP_OK               on success
 * @return ESP_ERR_INVALID_ARG  if @p motor is out of range
 * @return ESP_FAIL             if the LEDC duty update failed
 */
esp_err_t drv8833_set_speed(int motor, int speed);

/**
 * @brief Actively brake a motor (fast decay).
 *
 * Both H-bridge inputs are driven HIGH, shorting the motor terminals
 * and causing rapid deceleration.
 *
 * @param motor  DRV8833_MOTOR_LEFT or DRV8833_MOTOR_RIGHT
 * @return ESP_OK               on success
 * @return ESP_ERR_INVALID_ARG  if @p motor is out of range
 * @return ESP_FAIL             if the LEDC duty update failed
 */
esp_err_t drv8833_brake(int motor);

/**
 * @brief Coast a motor (high-impedance / slow decay).
 *
 * Both H-bridge inputs are driven LOW, letting the motor freewheel.
 *
 * @param motor  DRV8833_MOTOR_LEFT or DRV8833_MOTOR_RIGHT
 * @return ESP_OK               on success
 * @return ESP_ERR_INVALID_ARG  if @p motor is out of range
 * @return ESP_FAIL             if the LEDC duty update failed
 */
esp_err_t drv8833_coast(int motor);

/**
 * @brief Emergency stop — brake both motors.
 *
 * Convenience wrapper that applies active braking to both motors
 * simultaneously.
 *
 * @return ESP_OK on success, or the first error encountered
 */
esp_err_t drv8833_stop_all(void);

#ifdef __cplusplus
}
#endif

#endif /* DRV8833_H */