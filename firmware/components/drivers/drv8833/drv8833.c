/**
 * @file drv8833.c
 * @brief DRV8833 dual H-bridge motor driver implementation for ESP32-S3.
 *
 * Drives two DC motors through the TI DRV8833 using the ESP-IDF LEDC
 * peripheral.  Each motor is controlled by two PWM-capable GPIOs:
 *
 *   Motor A: AIN1 (channel 0), AIN2 (channel 1)
 *   Motor B: BIN1 (channel 2), BIN2 (channel 3)
 *
 * Truth table (per motor):
 *
 *   IN1  IN2  | Mode
 *   ----------|---------
 *    PWM   0  | Forward  (duty = |speed|)
 *     0   PWM | Reverse  (duty = |speed|)
 *     1    1  | Brake    (fast decay)
 *     0    0  | Coast    (slow decay / high-Z)
 *
 * @copyright Copyright (c) 2026 RobotBuddy Contributors
 * @license MIT
 */

#include "drv8833.h"

#include <string.h>
#include <math.h>

#include "esp_log.h"
#include "driver/gpio.h"

/* ------------------------------------------------------------------ */
/*  Logging                                                            */
/* ------------------------------------------------------------------ */

static const char *TAG = "drv8833";

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */

/** LEDC duty resolution — 8 bits maps directly to speed range 0–255. */
#define LEDC_DUTY_RES  LEDC_TIMER_8_BIT

/** Number of LEDC channels the driver manages (one per H-bridge input). */
#define NUM_CHANNELS   4

/** Maximum duty value at 8-bit resolution. */
#define DUTY_MAX       ((1 << 8) - 1)   /* 255 */

/* ------------------------------------------------------------------ */
/*  Module state (single instance)                                     */
/* ------------------------------------------------------------------ */

/** Whether drv8833_init() has been called successfully. */
static bool s_initialised = false;

/** Copy of the configuration supplied to drv8833_init(). */
static drv8833_config_t s_cfg = {0};

/** LEDC channel assignments — index order: AIN1, AIN2, BIN1, BIN2. */
static const ledc_channel_t s_channels[NUM_CHANNELS] = {
    LEDC_CHANNEL_0,  /**< AIN1 */
    LEDC_CHANNEL_1,  /**< AIN2 */
    LEDC_CHANNEL_2,  /**< BIN1 */
    LEDC_CHANNEL_3,  /**< BIN2 */
};

/** GPIO pin cache — index order matches s_channels. */
static int s_pins[NUM_CHANNELS] = {0};

/* ------------------------------------------------------------------ */
/*  Helper: set duty on a single LEDC channel                          */
/* ------------------------------------------------------------------ */

/**
 * @brief Set the duty cycle on a LEDC channel and update the output.
 *
 * @param ch    LEDC channel index (0 … NUM_CHANNELS-1)
 * @param duty  Duty cycle (0 … 255)
 * @return ESP_OK on success, or an ESP_ERR value from the LEDC API
 */
static esp_err_t set_duty(int ch, uint32_t duty)
{
    esp_err_t ret;

    ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, s_channels[ch], duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_set_duty failed on ch %d: %s",
                 ch, esp_err_to_name(ret));
        return ret;
    }

    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, s_channels[ch]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_update_duty failed on ch %d: %s",
                 ch, esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/*  Helper: get base channel index for a motor                         */
/* ------------------------------------------------------------------ */

/**
 * @brief Return the first LEDC channel index for the given motor.
 *
 * Motor A (LEFT)  uses channels 0 (IN1) and 1 (IN2).
 * Motor B (RIGHT) uses channels 2 (IN1) and 3 (IN2).
 *
 * @param motor DRV8833_MOTOR_LEFT or DRV8833_MOTOR_RIGHT
 * @return Channel base index (0 or 2), or -1 on invalid @p motor
 */
static int motor_ch_base(int motor)
{
    switch (motor) {
    case DRV8833_MOTOR_LEFT:  return 0;
    case DRV8833_MOTOR_RIGHT: return 2;
    default: return -1;
    }
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

esp_err_t drv8833_init(const drv8833_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "config is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* Basic GPIO validation — ESP32-S3 GPIOs 0–48 (some are strapping). */
    const int pins[4] = {
        config->pin_ain1, config->pin_ain2,
        config->pin_bin1, config->pin_bin2
    };
    for (int i = 0; i < 4; i++) {
        if (pins[i] < 0 || pins[i] > 48) {
            ESP_LOGE(TAG, "GPIO %d out of valid range 0–48", pins[i]);
            return ESP_ERR_INVALID_ARG;
        }
    }

    if (config->pwm_freq == 0) {
        ESP_LOGE(TAG, "pwm_freq must be > 0");
        return ESP_ERR_INVALID_ARG;
    }

    /* ---- LEDC timer configuration ---- */
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num       = config->pwm_timer,
        .freq_hz         = config->pwm_freq,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t ret = ledc_timer_config(&timer_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ---- Channel / GPIO configuration ---- */
    for (int i = 0; i < NUM_CHANNELS; i++) {
        ledc_channel_config_t ch_cfg = {
            .gpio_num   = pins[i],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel    = s_channels[i],
            .intr_type  = LEDC_INTR_DISABLE,
            .timer_sel  = config->pwm_timer,
            .duty       = 0,
            .hpoint     = 0,
        };
        ret = ledc_channel_config(&ch_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ledc_channel_config failed for GPIO %d: %s",
                     pins[i], esp_err_to_name(ret));
            /* Roll back any channels already configured. */
            for (int j = i - 1; j >= 0; j--) {
                ledc_channel_config_t rm = {
                    .gpio_num   = -1,
                    .speed_mode = LEDC_LOW_SPEED_MODE,
                    .channel    = s_channels[j],
                    .intr_type  = LEDC_INTR_DISABLE,
                    .timer_sel  = config->pwm_timer,
                    .duty       = 0,
                    .hpoint     = 0,
                };
                ledc_channel_config(&rm);
            }
            return ret;
        }
        s_pins[i] = pins[i];
    }

    /* Store config for later use. */
    memcpy(&s_cfg, config, sizeof(s_cfg));
    s_initialised = true;

    ESP_LOGI(TAG, "Initialised — AIN1=%d AIN2=%d BIN1=%d BIN2=%d "
                  "freq=%" PRIu32 " Hz timer=%d",
            config->pin_ain1, config->pin_ain2,
            config->pin_bin1, config->pin_bin2,
            config->pwm_freq, config->pwm_timer);

    return ESP_OK;
}

/* ------------------------------------------------------------------ */

esp_err_t drv8833_deinit(void)
{
    if (!s_initialised) {
        ESP_LOGW(TAG, "deinit called while not initialised");
        return ESP_OK;
    }

    /* Brake both motors first for safety. */
    drv8833_stop_all();

    /* Release LEDC channels — set GPIO to -1 to de-configure. */
    for (int i = 0; i < NUM_CHANNELS; i++) {
        ledc_channel_config_t ch_cfg = {
            .gpio_num   = -1,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel    = s_channels[i],
            .intr_type  = LEDC_INTR_DISABLE,
            .timer_sel  = s_cfg.pwm_timer,
            .duty       = 0,
            .hpoint     = 0,
        };
        ledc_channel_config(&ch_cfg);
    }

    /* Stop the LEDC timer. */
    ledc_timer_rst(LEDC_LOW_SPEED_MODE, s_cfg.pwm_timer);

    s_initialised = false;
    memset(&s_cfg, 0, sizeof(s_cfg));

    ESP_LOGI(TAG, "De-initialised");
    return ESP_OK;
}

/* ------------------------------------------------------------------ */

esp_err_t drv8833_set_speed(int motor, int speed)
{
    if (!s_initialised) {
        ESP_LOGE(TAG, "driver not initialised");
        return ESP_ERR_INVALID_STATE;
    }

    int base = motor_ch_base(motor);
    if (base < 0) {
        ESP_LOGE(TAG, "invalid motor %d (use DRV8833_MOTOR_LEFT/RIGHT)", motor);
        return ESP_ERR_INVALID_ARG;
    }

    /* Clamp speed to the valid range. */
    if (speed > DRV8833_SPEED_MAX) {
        speed = DRV8833_SPEED_MAX;
    } else if (speed < -DRV8833_SPEED_MAX) {
        speed = -DRV8833_SPEED_MAX;
    }

    esp_err_t ret;

    if (speed > 0) {
        /* Forward: IN1 = PWM(duty=speed), IN2 = LOW */
        ret = set_duty(base + 0, (uint32_t)speed);       /* IN1 — PWM  */
        if (ret != ESP_OK) return ret;
        ret = set_duty(base + 1, 0);                      /* IN2 — LOW  */
    } else if (speed < 0) {
        /* Reverse: IN1 = LOW, IN2 = PWM(duty=|speed|) */
        ret = set_duty(base + 0, 0);                      /* IN1 — LOW  */
        if (ret != ESP_OK) return ret;
        ret = set_duty(base + 1, (uint32_t)(-speed));     /* IN2 — PWM  */
    } else {
        /* Speed 0 → coast (both LOW). */
        ret = set_duty(base + 0, 0);
        if (ret != ESP_OK) return ret;
        ret = set_duty(base + 1, 0);
    }

    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "motor %d speed %d", motor, speed);
    }
    return ret;
}

/* ------------------------------------------------------------------ */

esp_err_t drv8833_brake(int motor)
{
    if (!s_initialised) {
        ESP_LOGE(TAG, "driver not initialised");
        return ESP_ERR_INVALID_STATE;
    }

    int base = motor_ch_base(motor);
    if (base < 0) {
        ESP_LOGE(TAG, "invalid motor %d (use DRV8833_MOTOR_LEFT/RIGHT)", motor);
        return ESP_ERR_INVALID_ARG;
    }

    /* Both inputs HIGH → fast decay / active brake. */
    esp_err_t ret;
    ret = set_duty(base + 0, DUTY_MAX);
    if (ret != ESP_OK) return ret;
    ret = set_duty(base + 1, DUTY_MAX);
    if (ret != ESP_OK) return ret;

    ESP_LOGD(TAG, "motor %d brake", motor);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */

esp_err_t drv8833_coast(int motor)
{
    if (!s_initialised) {
        ESP_LOGE(TAG, "driver not initialised");
        return ESP_ERR_INVALID_STATE;
    }

    int base = motor_ch_base(motor);
    if (base < 0) {
        ESP_LOGE(TAG, "invalid motor %d (use DRV8833_MOTOR_LEFT/RIGHT)", motor);
        return ESP_ERR_INVALID_ARG;
    }

    /* Both inputs LOW → high-Z / coast. */
    esp_err_t ret;
    ret = set_duty(base + 0, 0);
    if (ret != ESP_OK) return ret;
    ret = set_duty(base + 1, 0);
    if (ret != ESP_OK) return ret;

    ESP_LOGD(TAG, "motor %d coast", motor);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */

esp_err_t drv8833_stop_all(void)
{
    if (!s_initialised) {
        ESP_LOGW(TAG, "stop_all called while not initialised — ignoring");
        return ESP_OK;
    }

    esp_err_t err_a = drv8833_brake(DRV8833_MOTOR_LEFT);
    esp_err_t err_b = drv8833_brake(DRV8833_MOTOR_RIGHT);

    if (err_a != ESP_OK) return err_a;
    if (err_b != ESP_OK) return err_b;

    ESP_LOGW(TAG, "emergency stop — both motors braked");
    return ESP_OK;
}