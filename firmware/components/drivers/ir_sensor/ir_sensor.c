/**
 * @file ir_sensor.c
 * @brief Infra-red obstacle / edge sensor driver implementation for RobotBuddy.
 *
 * Each IR sensor module outputs an active-LOW digital signal.  The driver
 * configures four GPIO pins as inputs with internal pull-ups and reads them
 * with gpio_get_level().
 *
 * @copyright 2026 RobotBuddy contributors
 * @licence MIT
 */

#include "ir_sensor.h"

#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"

/* ------------------------------------------------------------------------ */
/*  Constants                                                               */
/* ------------------------------------------------------------------------ */

static const char *TAG = "ir_sensor";

/* ------------------------------------------------------------------------ */
/*  Module state                                                            */
/* ------------------------------------------------------------------------ */

typedef struct {
    int pin_obstacle_left;
    int pin_obstacle_right;
    int pin_edge_left;
    int pin_edge_right;
    bool initialised;
} ir_sensor_state_t;

static ir_sensor_state_t s_state = {0};

/* ------------------------------------------------------------------------ */
/*  Helpers                                                                 */
/* ------------------------------------------------------------------------ */

/**
 * @brief Configure a single GPIO as input with pull-up.
 *
 * @param pin  GPIO number (negative values are silently skipped).
 * @return ESP_OK on success, or an ESP-IDF error code.
 */
static esp_err_t config_gpio_input(int pin)
{
    if (pin < 0) {
        ESP_LOGW(TAG, "Skipping unconfigured GPIO (%d)", pin);
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
 * @brief Reset a single GPIO to its default state.
 *
 * @param pin  GPIO number (negative values are silently skipped).
 */
static void reset_gpio(int pin)
{
    if (pin < 0) {
        return;
    }
    gpio_reset_pin((gpio_num_t)pin);
}

/* ------------------------------------------------------------------------ */
/*  Public API                                                              */
/* ------------------------------------------------------------------------ */

esp_err_t ir_sensor_init(const ir_sensor_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Store configuration. */
    s_state.pin_obstacle_left  = config->pin_obstacle_left;
    s_state.pin_obstacle_right = config->pin_obstacle_right;
    s_state.pin_edge_left      = config->pin_edge_left;
    s_state.pin_edge_right     = config->pin_edge_right;

    /* Configure each GPIO. */
    esp_err_t err;
    err = config_gpio_input(s_state.pin_obstacle_left);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure left obstacle GPIO %d: %s",
                 s_state.pin_obstacle_left, esp_err_to_name(err));
        return err;
    }
    err = config_gpio_input(s_state.pin_obstacle_right);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure right obstacle GPIO %d: %s",
                 s_state.pin_obstacle_right, esp_err_to_name(err));
        return err;
    }
    err = config_gpio_input(s_state.pin_edge_left);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure left edge GPIO %d: %s",
                 s_state.pin_edge_left, esp_err_to_name(err));
        return err;
    }
    err = config_gpio_input(s_state.pin_edge_right);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure right edge GPIO %d: %s",
                 s_state.pin_edge_right, esp_err_to_name(err));
        return err;
    }

    s_state.initialised = true;
    ESP_LOGI(TAG, "Initialised — L_obs=%d  R_obs=%d  L_edge=%d  R_edge=%d",
             s_state.pin_obstacle_left, s_state.pin_obstacle_right,
             s_state.pin_edge_left, s_state.pin_edge_right);
    return ESP_OK;
}

esp_err_t ir_sensor_deinit(void)
{
    if (!s_state.initialised) {
        ESP_LOGW(TAG, "De-init called but driver not initialised");
        return ESP_OK;
    }

    reset_gpio(s_state.pin_obstacle_left);
    reset_gpio(s_state.pin_obstacle_right);
    reset_gpio(s_state.pin_edge_left);
    reset_gpio(s_state.pin_edge_right);

    s_state.initialised = false;
    ESP_LOGI(TAG, "De-initialised");
    return ESP_OK;
}

ir_sensor_data_t ir_sensor_read(void)
{
    ir_sensor_data_t data = {0};

    if (!s_state.initialised) {
        ESP_LOGW(TAG, "Read called before init — returning all-false");
        return data;
    }

    /* Active-LOW: LOW (0) means obstacle/edge detected.
     * Skip GPIO reads for unconfigured (negative) pins — return false. */
    data.obstacle_left  = (s_state.pin_obstacle_left  >= 0) ?
                           (gpio_get_level((gpio_num_t)s_state.pin_obstacle_left)  == 0) : false;
    data.obstacle_right = (s_state.pin_obstacle_right >= 0) ?
                           (gpio_get_level((gpio_num_t)s_state.pin_obstacle_right) == 0) : false;
    data.edge_left       = (s_state.pin_edge_left       >= 0) ?
                           (gpio_get_level((gpio_num_t)s_state.pin_edge_left)      == 0) : false;
    data.edge_right      = (s_state.pin_edge_right      >= 0) ?
                           (gpio_get_level((gpio_num_t)s_state.pin_edge_right)     == 0) : false;

    return data;
}