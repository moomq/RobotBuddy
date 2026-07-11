/**
 * @file battery.c
 * @brief Battery monitor driver implementation for RobotBuddy (ESP32-S3).
 *
 * Uses the ESP-IDF v5.x one-shot ADC API to read the battery voltage
 * through a resistive divider, then maps the result linearly to a
 * percentage.  TP4056 CHRG/STDBY pins are sampled as active-LOW GPIOs.
 *
 * @copyright 2026 RobotBuddy contributors
 * @licence MIT
 */

#include "battery.h"

#include <math.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ------------------------------------------------------------------------ */
/*  ADC configuration                                                       */
/* ------------------------------------------------------------------------ */

/** @brief ADC attenuation — 11 dB gives ~0–2600 mV measurable range. */
#define ADC_ATTEN           ADC_ATTEN_DB_12

/** @brief ADC bit width — 12 bits (0–4095). */
#define ADC_BITWIDTH        ADC_BITWIDTH_12

/** @brief Number of samples to average for a stable reading.
 * Reduced from 16 to 8 to limit ADC blocking time to ~8ms
 * (previously ~16ms, which consumed 32% of sensor_task's 50ms period). */
#define ADC_SAMPLE_COUNT    8

/* ------------------------------------------------------------------------ */
/*  Battery percentage mapping constants                                    */
/* ------------------------------------------------------------------------ */

/** @brief Voltage corresponding to 0 % charge (V). */
#define BATT_V_MIN   3.0f

/** @brief Voltage corresponding to 100 % charge (V). */
#define BATT_V_MAX   4.2f

/* ------------------------------------------------------------------------ */
/*  Module state                                                            */
/* ------------------------------------------------------------------------ */

static const char *TAG = "battery";

typedef struct {
    battery_config_t        config;
    adc_oneshot_unit_handle_t adc_handle;  /**< ADC unit handle            */
    adc_cali_handle_t       cali_handle;   /**< Calibration handle          */
    bool                    adc_initialised;
    bool                    gpio_initialised;
    bool                    initialised;
} battery_state_t;

static battery_state_t s_state = {0};

/* ------------------------------------------------------------------------ */
/*  Helpers                                                                 */
/* ------------------------------------------------------------------------ */

/**
 * @brief Configure a GPIO as input with pull-up (TP4056 open-drain output).
 *
 * @param pin  GPIO number; negative values are skipped.
 */
static esp_err_t config_chrg_gpio(int pin)
{
    if (pin < 0) {
        return ESP_OK;
    }
    gpio_config_t io_conf = {
        .pin_bit_mask  = (1ULL << pin),
        .mode          = GPIO_MODE_INPUT,
        .pull_up_en    = GPIO_PULLUP_ENABLE,
        .pull_down_en  = GPIO_PULLDOWN_DISABLE,
        .intr_type     = GPIO_INTR_DISABLE,
    };
    return gpio_config(&io_conf);
}

/**
 * @brief Initialise the ADC one-shot unit and calibration.
 */
static esp_err_t init_adc(void)
{
    /* ---- ADC unit ---- */
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = s_state.config.adc_unit,
    };
    esp_err_t err = adc_oneshot_new_unit(&unit_cfg, &s_state.adc_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC unit init failed: %s", esp_err_to_name(err));
        return err;
    }

    /* ---- ADC channel ---- */
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH,
    };
    err = adc_oneshot_config_channel(s_state.adc_handle,
                                     s_state.config.adc_channel,
                                     &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC channel config failed: %s", esp_err_to_name(err));
        adc_oneshot_del_unit(s_state.adc_handle);
        return err;
    }

    /* ---- Calibration (curve-fitting) ---- */
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = s_state.config.adc_unit,
        .chan     = s_state.config.adc_channel,
        .atten    = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH,
    };
    err = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_state.cali_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration unavailable (err=%s); readings will be raw",
                 esp_err_to_name(err));
        /* Calibration failure is non-fatal; we will fall back to a simple
         * linear approximation if cali_handle is NULL. */
        s_state.cali_handle = NULL;
    }

    s_state.adc_initialised = true;
    return ESP_OK;
}

/**
 * @brief Clamp an integer value to [lo, hi].
 */
static inline int clamp_int(int val, int lo, int hi)
{
    if (val < lo) { return lo; }
    if (val > hi) { return hi; }
    return val;
}

/**
 * @brief Clamp a float value to [lo, hi].
 */
static inline float clamp_f(float val, float lo, float hi)
{
    if (val < lo) { return lo; }
    if (val > hi) { return hi; }
    return val;
}

/* ------------------------------------------------------------------------ */
/*  Public API                                                              */
/* ------------------------------------------------------------------------ */

esp_err_t battery_init(const battery_config_t *config)
{
    /* Guard against double initialization (sensor_manager and battery_monitor
     * both call battery_init in main.c). */
    if (s_state.initialised) {
        ESP_LOGW(TAG, "Battery driver already initialized, skipping");
        return ESP_OK;
    }

    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->divider_ratio <= 0.0f) {
        ESP_LOGE(TAG, "divider_ratio must be > 0 (got %.2f)", config->divider_ratio);
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_state.config, config, sizeof(*config));

    /* ---- Initialise ADC ---- */
    esp_err_t err = init_adc();
    if (err != ESP_OK) {
        return err;
    }

    /* ---- Initialise TP4056 GPIOs (CHRG / STDBY) ---- */
    err = config_chrg_gpio(BATTERY_PIN_CHRG);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure CHRG GPIO %d: %s",
                 BATTERY_PIN_CHRG, esp_err_to_name(err));
        goto fail_adc;
    }
    err = config_chrg_gpio(BATTERY_PIN_STDBY);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure STDBY GPIO %d: %s",
                 BATTERY_PIN_STDBY, esp_err_to_name(err));
        goto fail_adc;
    }
    s_state.gpio_initialised = true;

    s_state.initialised = true;
    ESP_LOGI(TAG, "Initialised — ADC pin=%d unit=%d ch=%d ratio=%.2f",
             config->pin_adc, config->adc_unit, config->adc_channel,
             config->divider_ratio);
    return ESP_OK;

fail_adc:
    if (s_state.adc_initialised) {
        if (s_state.cali_handle) {
            adc_cali_delete_scheme_curve_fitting(s_state.cali_handle);
            s_state.cali_handle = NULL;
        }
        adc_oneshot_del_unit(s_state.adc_handle);
        s_state.adc_handle = NULL;
        s_state.adc_initialised = false;
    }
    return err;
}

esp_err_t battery_deinit(void)
{
    if (!s_state.initialised) {
        ESP_LOGW(TAG, "De-init called but driver not initialised");
        return ESP_OK;
    }

    /* Release ADC calibration. */
    if (s_state.cali_handle) {
        adc_cali_delete_scheme_curve_fitting(s_state.cali_handle);
        s_state.cali_handle = NULL;
    }

    /* Release ADC unit. */
    if (s_state.adc_initialised) {
        adc_oneshot_del_unit(s_state.adc_handle);
        s_state.adc_handle = NULL;
        s_state.adc_initialised = false;
    }

    /* Reset GPIOs. */
    if (BATTERY_PIN_CHRG >= 0) {
        gpio_reset_pin((gpio_num_t)BATTERY_PIN_CHRG);
    }
    if (BATTERY_PIN_STDBY >= 0) {
        gpio_reset_pin((gpio_num_t)BATTERY_PIN_STDBY);
    }
    s_state.gpio_initialised = false;
    s_state.initialised = false;

    ESP_LOGI(TAG, "De-initialised");
    return ESP_OK;
}

esp_err_t battery_read(battery_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_state.initialised) {
        ESP_LOGE(TAG, "Driver not initialised");
        return ESP_ERR_INVALID_STATE;
    }

    /* ---- Average multiple ADC samples for stability ---- */
    int raw_sum = 0;
    for (int i = 0; i < ADC_SAMPLE_COUNT; i++) {
        int raw = 0;
        esp_err_t err = adc_oneshot_read(s_state.adc_handle,
                                          s_state.config.adc_channel, &raw);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(err));
            return err;
        }
        raw_sum += raw;
    }
    int raw_avg = raw_sum / ADC_SAMPLE_COUNT;

    /* ---- Convert raw → millivolts ---- */
    int mv = 0;
    if (s_state.cali_handle) {
        /* Use calibration curve. */
        esp_err_t err = adc_cali_raw_to_voltage(s_state.cali_handle, raw_avg, &mv);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "ADC calibration conversion failed: %s — falling back",
                     esp_err_to_name(err));
            /* Fallback: crude linear estimate assuming 3.3 V reference and
             * 12-bit resolution with 11 dB attenuation. */
            mv = (int)((float)raw_avg * 2600.0f / 4095.0f);
        }
    } else {
        /* No calibration handle; use crude linear estimate. */
        mv = (int)((float)raw_avg * 2600.0f / 4095.0f);
    }

    /* ---- Compute battery voltage ---- */
    float v_adc   = (float)mv / 1000.0f;               /* ADC input in V   */
    float v_batt   = v_adc * s_state.config.divider_ratio; /* Battery voltage */

    data->voltage = v_batt;

    /* ---- Map voltage → percentage (linear) ---- */
    float pct = ((v_batt - BATT_V_MIN) / (BATT_V_MAX - BATT_V_MIN)) * 100.0f;
    data->percentage = (uint8_t)clamp_int((int)pct, 0, 100);

    /* ---- TP4056 charge status (active-LOW) ---- */
    data->is_charging = (BATTERY_PIN_CHRG >= 0)
                      ? (gpio_get_level((gpio_num_t)BATTERY_PIN_CHRG) == 0)
                      : false;

    data->is_full = (BATTERY_PIN_STDBY >= 0)
                  ? (gpio_get_level((gpio_num_t)BATTERY_PIN_STDBY) == 0)
                  : false;

    ESP_LOGD(TAG, "raw_avg=%d mv=%d v=%.3f pct=%u chg=%d full=%d",
             raw_avg, mv, v_batt, data->percentage,
             data->is_charging, data->is_full);
    return ESP_OK;
}