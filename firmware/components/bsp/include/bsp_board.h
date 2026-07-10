/*
 * RobotBuddy Board Support Package — Board API
 * ================================================
 * Public API for board initialization and deinitialization.
 * Pin definitions are in bsp_pinmap.h.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize all board hardware
 *
 * Initializes in order:
 *   1. GPIO pins (backlight, IR sensors, charger status)
 *   2. I2C bus (MPU6050, VL53L0X)
 *   3. SPI bus (ST7789 LCD)
 *   4. I2S bus (INMP441 mic, MAX98357A amp)
 *   5. LEDC PWM (DRV8833 motor driver)
 *   6. ADC (battery voltage)
 *
 * @note Must be called before any driver initialization.
 * @note Calls app_main() delay of 100ms for power rail stabilization.
 *
 * @return ESP_OK on success, error code on failure
 */
esp_err_t bsp_board_init(void);

/**
 * @brief Deinitialize all board hardware
 *
 * Frees SPI and I2C buses, stops I2S channels, and
 * releases PWM resources.
 *
 * @return ESP_OK on success, error code on failure
 */
esp_err_t bsp_board_deinit(void);

#ifdef __cplusplus
}
#endif