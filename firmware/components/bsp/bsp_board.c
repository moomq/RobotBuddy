/*
 * RobotBuddy Board Support Package — Board Initialization
 * ========================================================
 * Initializes all hardware buses (SPI, I2C, I2S) and GPIO pins
 * according to the pin map defined in bsp_pinmap.h.
 *
 * Initialization order follows the BSP skill's phased approach:
 *   Phase 1: Critical (GPIO, Clock)
 *   Phase 2: Bus (I2C, SPI, I2S)
 *   Phase 3: Peripheral (Display, Sensors, Motors)
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "bsp_board.h"
#include "bsp_pinmap.h"

#include "esp_log.h"
#include "esp_err.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/ledc.h"

static const char *TAG = "BSP";

/* ============================================================
 * Internal: Initialize GPIO pins
 * ============================================================ */
static esp_err_t bsp_init_gpio(void)
{
    ESP_LOGI(TAG, "Initializing GPIO pins...");

    /* Backlight — output, default OFF */
    gpio_config_t bl_conf = {
        .pin_bit_mask = (1ULL << BSP_PIN_LCD_BL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&bl_conf));
    gpio_set_level(BSP_PIN_LCD_BL, 0); /* Backlight OFF initially */

    /* IR sensors — input with pull-up */
    gpio_config_t ir_conf = {
        .pin_bit_mask = (1ULL << BSP_PIN_IR_OBSTACLE_L) |
                        (1ULL << BSP_PIN_IR_OBSTACLE_R) |
                        (1ULL << BSP_PIN_IR_EDGE_L) |
                        (1ULL << BSP_PIN_IR_EDGE_R),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&ir_conf));

    /* Charger status — input with pull-up */
    gpio_config_t chrg_conf = {
        .pin_bit_mask = (1ULL << BSP_PIN_CHRG) |
                        (1ULL << BSP_PIN_STDBY),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&chrg_conf));

    ESP_LOGI(TAG, "GPIO initialized");
    return ESP_OK;
}

/* ============================================================
 * Internal: Initialize I2C bus (MPU6050 + VL53L0X)
 * ============================================================ */
static esp_err_t bsp_init_i2c(void)
{
    ESP_LOGI(TAG, "Initializing I2C bus (SDA=%d, SCL=%d)...", BSP_PIN_I2C_SDA, BSP_PIN_I2C_SCL);

    i2c_master_bus_config_t i2c_bus_conf = {
        .i2c_port = BSP_I2C_NUM,
        .sda_io_num = BSP_PIN_I2C_SDA,
        .scl_io_num = BSP_PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = true,
        },
    };

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_conf, &bus_handle));

    ESP_LOGI(TAG, "I2C bus initialized at %d Hz", BSP_I2C_FREQ_HZ);
    return ESP_OK;
}

/* ============================================================
 * Internal: Initialize SPI bus (ST7789 LCD)
 * ============================================================ */
static esp_err_t bsp_init_spi(void)
{
    ESP_LOGI(TAG, "Initializing SPI bus (SCLK=%d, MOSI=%d)...", BSP_PIN_LCD_SCLK, BSP_PIN_LCD_MOSI);

    spi_bus_config_t bus_conf = {
        .mosi_io_num = BSP_PIN_LCD_MOSI,
        .miso_io_num = BSP_PIN_LCD_MISO,
        .sclk_io_num = BSP_PIN_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 240 * 240 * 2 + 8, /* 240x240 RGB565 + header */
        .flags = SPICOMMON_BUSFLAG_GPIO_PINS,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(BSP_LCD_SPI_HOST, &bus_conf, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "SPI bus initialized (DMA channel auto)");
    return ESP_OK;
}

/* ============================================================
 * Internal: Initialize I2S bus (INMP441 Mic + MAX98357A Amp)
 * ============================================================ */
static esp_err_t bsp_init_i2s(void)
{
    ESP_LOGI(TAG, "Initializing I2S bus (BCLK=%d, WS=%d, DIN=%d, DOUT=%d)...",
             BSP_PIN_I2S_BCLK, BSP_PIN_I2S_WS, BSP_PIN_I2S_DIN, BSP_PIN_I2S_DOUT);

    /* I2S RX channel (Microphone INMP441) */
    i2s_chan_handle_t rx_handle;
    i2s_chan_config_t rx_chan_conf = I2S_CHANNEL_DEFAULT_CONFIG(BSP_I2S_NUM, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_conf, &rx_handle, NULL));

    i2s_std_config_t rx_std_conf = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BSP_I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = GPIO_NUM_NC,
            .bclk = BSP_PIN_I2S_BCLK,
            .ws = BSP_PIN_I2S_WS,
            .dout = GPIO_NUM_NC,
            .din = BSP_PIN_I2S_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    /* Override slot config for INMP441: 32-bit slot width, left channel */
    rx_std_conf.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;
    rx_std_conf.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &rx_std_conf));

    /* I2S TX channel (Amplifier MAX98357A) */
    i2s_chan_handle_t tx_handle;
    i2s_chan_config_t tx_chan_conf = I2S_CHANNEL_DEFAULT_CONFIG(BSP_I2S_NUM, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_conf, NULL, &tx_handle));

    i2s_std_config_t tx_std_conf = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(BSP_I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = GPIO_NUM_NC,
            .bclk = BSP_PIN_I2S_BCLK,
            .ws = BSP_PIN_I2S_WS,
            .dout = BSP_PIN_I2S_DOUT,
            .din = GPIO_NUM_NC,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    /* Override slot config for MAX98357A: 32-bit slot width, left channel */
    tx_std_conf.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;
    tx_std_conf.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &tx_std_conf));

    /* NOTE: RX and TX share the same I2S port number but use separate channels.
     * For simultaneous full-duplex, I2S_NUM must support it (ESP32-S3 does).
     * Channels are enabled/disabled by audio_manager later. */

    ESP_LOGI(TAG, "I2S bus initialized (RX: INMP441, TX: MAX98357A)");
    return ESP_OK;
}

/* ============================================================
 * Internal: Initialize LEDC PWM for motor control
 * ============================================================ */
static esp_err_t bsp_init_pwm(void)
{
    ESP_LOGI(TAG, "Initializing LEDC PWM for motor control...");

    /* Timer configuration */
    ledc_timer_config_t timer_conf = {
        .speed_mode = BSP_MOTOR_PWM_SPEED_MODE,
        .duty_resolution = BSP_MOTOR_PWM_RESOLUTION,
        .timer_num = BSP_MOTOR_PWM_TIMER,
        .freq_hz = BSP_MOTOR_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

    /* Motor channel configurations */
    ledc_channel_config_t motor_channels[] = {
        {
            .gpio_num = BSP_PIN_MOTOR_AIN1,
            .speed_mode = BSP_MOTOR_PWM_SPEED_MODE,
            .channel = LEDC_CHANNEL_0,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = BSP_MOTOR_PWM_TIMER,
            .duty = 0,
            .hpoint = 0,
        },
        {
            .gpio_num = BSP_PIN_MOTOR_AIN2,
            .speed_mode = BSP_MOTOR_PWM_SPEED_MODE,
            .channel = LEDC_CHANNEL_1,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = BSP_MOTOR_PWM_TIMER,
            .duty = 0,
            .hpoint = 0,
        },
        {
            .gpio_num = BSP_PIN_MOTOR_BIN1,
            .speed_mode = BSP_MOTOR_PWM_SPEED_MODE,
            .channel = LEDC_CHANNEL_2,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = BSP_MOTOR_PWM_TIMER,
            .duty = 0,
            .hpoint = 0,
        },
        {
            .gpio_num = BSP_PIN_MOTOR_BIN2,
            .speed_mode = BSP_MOTOR_PWM_SPEED_MODE,
            .channel = LEDC_CHANNEL_3,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = BSP_MOTOR_PWM_TIMER,
            .duty = 0,
            .hpoint = 0,
        },
    };

    for (int i = 0; i < sizeof(motor_channels) / sizeof(motor_channels[0]); i++) {
        ESP_ERROR_CHECK(ledc_channel_config(&motor_channels[i]));
    }

    ESP_LOGI(TAG, "LEDC PWM initialized (%d Hz, %d-bit)", BSP_MOTOR_PWM_FREQ_HZ, BSP_MOTOR_PWM_RESOLUTION);
    return ESP_OK;
}

/* ============================================================
 * Internal: Configure ADC for battery voltage reading
 * ============================================================ */
static esp_err_t bsp_init_adc(void)
{
    ESP_LOGI(TAG, "Initializing ADC for battery monitoring...");

    /* ADC1 channel 0 (GPIO1) for battery voltage */
    /* Configuration is done in battery_monitor module later,
     * here we just configure the GPIO as analog input */
    gpio_config_t adc_conf = {
        .pin_bit_mask = (1ULL << BSP_PIN_VBAT_ADC),
        .mode = GPIO_MODE_DISABLE, /* ADC mode configured by adc_oneshot driver */
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&adc_conf));

    ESP_LOGI(TAG, "ADC GPIO configured (GPIO%d for VBAT)", BSP_PIN_VBAT_ADC);
    return ESP_OK;
}

/* ============================================================
 * Public API: Initialize all board hardware
 * ============================================================ */
esp_err_t bsp_board_init(void)
{
    ESP_LOGI(TAG, "=== Board Initialization Start ===");

    /* Phase 1: Critical — GPIO */
    ESP_ERROR_CHECK(bsp_init_gpio());

    /* Phase 2: Bus — I2C, SPI, I2S */
    ESP_ERROR_CHECK(bsp_init_i2c());
    ESP_ERROR_CHECK(bsp_init_spi());
    ESP_ERROR_CHECK(bsp_init_i2s());

    /* Phase 3: Peripheral — PWM, ADC */
    ESP_ERROR_CHECK(bsp_init_pwm());
    ESP_ERROR_CHECK(bsp_init_adc());

    /* Phase 4: Board-specific delays */
    vTaskDelay(pdMS_TO_TICKS(100)); /* Wait for power rails to stabilize */

    ESP_LOGI(TAG, "=== Board Initialization Complete ===");
    ESP_LOGI(TAG, "Free heap: %lu bytes, Free PSRAM: %lu bytes",
             esp_get_free_heap_size(),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    return ESP_OK;
}

/* ============================================================
 * Public API: Deinitialize all board hardware
 * ============================================================ */
esp_err_t bsp_board_deinit(void)
{
    ESP_LOGI(TAG, "Board deinitialization...");

    /* Stop I2S channels first */
    /* NOTE: I2S channel handles should be stored globally or
     * passed by audio_manager for proper cleanup */

    /* Free SPI bus (display should be deinitialized first) */
    spi_bus_free(BSP_LCD_SPI_HOST);

    /* Free I2C bus */
    /* NOTE: I2C bus handle should be stored for proper cleanup */

    ESP_LOGI(TAG, "Board deinitialized");
    return ESP_OK;
}