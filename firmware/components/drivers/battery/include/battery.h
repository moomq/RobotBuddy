/**
 * @file battery.h
 * @brief Battery monitor driver for RobotBuddy (ESP32-S3, ESP-IDF v5.x).
 *
 * Reads battery voltage through an ADC-connected voltage divider and reports
 * an estimated charge percentage.  Also monitors TP4056 CHRG / STDBY pins
 * for charging and full-charge status.
 *
 * @copyright 2026 RobotBuddy contributors
 * @licence MIT
 */

#ifndef BATTERY_H
#define BATTERY_H

#include <stdbool.h>
#include <stdint.h>
#include "hal/adc_types.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */
/*  Configuration                                                           */
/* ------------------------------------------------------------------------ */

/** @brief GPIO for TP4056 CHRG (charge-in-progress) output — active LOW. */
#ifndef BATTERY_PIN_CHRG
#define BATTERY_PIN_CHRG  -1   /**< Default: not configured */
#endif

/** @brief GPIO for TP4056 STDBY (charge-complete) output — active LOW. */
#ifndef BATTERY_PIN_STDBY
#define BATTERY_PIN_STDBY -1   /**< Default: not configured */
#endif

/* ------------------------------------------------------------------------ */
/*  Type definitions                                                        */
/* ------------------------------------------------------------------------ */

/**
 * @brief Battery monitor configuration.
 *
 * The ADC channel must correspond to @p pin_adc as determined by the
 * ESP32-S3 GPIO-to-ADC mapping (see the ESP-IDF ADC documentation).
 */
typedef struct {
    int pin_adc;                /**< ADC GPIO pin number                        */
    adc_unit_t adc_unit;        /**< ADC unit (ADC_UNIT_1 or ADC_UNIT_2)       */
    adc_channel_t adc_channel;  /**< ADC channel associated with @p pin_adc    */
    float divider_ratio;        /**< Voltage-divider ratio (V_bat = V_adc × ratio).
                                     Default 2.0 for a 1:1 divider.             */
} battery_config_t;

/**
 * @brief Battery state snapshot.
 */
typedef struct {
    float voltage;       /**< Battery terminal voltage (V)               */
    uint8_t percentage;  /**< Estimated charge level 0–100 %             */
    bool is_charging;    /**< true if TP4056 CHRG pin is active (LOW)    */
    bool is_full;        /**< true if TP4056 STDBY pin is active (LOW)   */
} battery_data_t;

/* ------------------------------------------------------------------------ */
/*  Public API                                                              */
/* ------------------------------------------------------------------------ */

/**
 * @brief Initialise the battery monitor driver.
 *
 * Configures the ADC in one-shot mode, creates a calibration handle, and
 * sets up the TP4056 CHRG / STDBY GPIO inputs (if valid pin numbers were
 * supplied).
 *
 * @param config  Pointer to a populated configuration struct.  Must not be
 *                NULL; the contents are copied internally.
 * @return ESP_OK on success; an ESP-IDF error code on failure.
 */
esp_err_t battery_init(const battery_config_t *config);

/**
 * @brief De-initialise the battery monitor driver.
 *
 * Releases the ADC handle and calibration resources; resets GPIOs.
 *
 * @return ESP_OK on success.
 */
esp_err_t battery_deinit(void);

/**
 * @brief Read the current battery state.
 *
 * Converts the raw ADC reading to a voltage, applies the divider ratio,
 * and maps the result linearly to a 0–100 % range (3.0 V → 0 %,
 * 4.2 V → 100 %).  CHRG / STDBY pins are sampled concurrently.
 *
 * @param[out] data  Pointer to a caller-allocated struct to receive data.
 *                   Must not be NULL.
 * @return ESP_OK on success; ESP_ERR_INVALID_STATE if not initialised.
 */
esp_err_t battery_read(battery_data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_H */