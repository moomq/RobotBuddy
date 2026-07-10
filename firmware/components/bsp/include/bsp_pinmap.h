/*
 * RobotBuddy Pin Map — ESP32-S3-WROOM-1-N16R8
 * ================================================
 * All GPIO pin definitions for the RobotBuddy desktop robot.
 *
 * Pin allocation follows the /pin-check command analysis.
 * DO NOT use these constants directly in driver code —
 * include bsp_pinmap.h in board init only, drivers get pins
 * via their config structs.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "driver/ledc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * SPI — ST7789 LCD Display (VSPI / SPI3_HOST)
 * ============================================================ */
#define BSP_PIN_LCD_SCLK        GPIO_NUM_36  /*!< SPI clock */
#define BSP_PIN_LCD_MOSI        GPIO_NUM_35  /*!< SPI MOSI (data) */
#define BSP_PIN_LCD_MISO        GPIO_NUM_NC   /*!< SPI MISO (not connected) */
#define BSP_PIN_LCD_CS          GPIO_NUM_37  /*!< SPI chip select */
#define BSP_PIN_LCD_DC          GPIO_NUM_38  /*!< Data/Command */
#define BSP_PIN_LCD_RST         GPIO_NUM_39  /*!< Display reset */
#define BSP_PIN_LCD_BL          GPIO_NUM_40  /*!< Backlight control */

#define BSP_LCD_SPI_HOST         SPI3_HOST
#define BSP_LCD_SPI_FREQ_HZ      40000000   /*!< 40 MHz max for ST7789 */
#define BSP_LCD_SPI_MODE         0          /*!< SPI Mode 0 (CPOL=0, CPHA=0) */

/* ============================================================
 * I2S — Audio (INMP441 Microphone + MAX98357A Amplifier)
 * ============================================================ */
#define BSP_PIN_I2S_BCLK        GPIO_NUM_4   /*!< I2S bit clock */
#define BSP_PIN_I2S_WS          GPIO_NUM_5   /*!< I2S word select (LRCK) */
#define BSP_PIN_I2S_DIN         GPIO_NUM_6   /*!< I2S data in (INMP441) */
#define BSP_PIN_I2S_DOUT        GPIO_NUM_7   /*!< I2S data out (MAX98357A) */

#define BSP_I2S_NUM              I2S_NUM_0
#define BSP_I2S_SAMPLE_RATE      16000      /*!< 16 kHz sample rate */
#define BSP_I2S_BITS_PER_SAMPLE  I2S_BITS_PER_SAMPLE_16BIT

/* ============================================================
 * I2C — Sensors (MPU6050 IMU + VL53L0X TOF)
 * ============================================================ */
#define BSP_PIN_I2C_SDA         GPIO_NUM_8   /*!< I2C0 data */
#define BSP_PIN_I2C_SCL         GPIO_NUM_9   /*!< I2C0 clock */

#define BSP_I2C_NUM              I2C_NUM_0
#define BSP_I2C_FREQ_HZ          400000     /*!< 400 kHz fast mode */

/* I2C device addresses */
#define BSP_I2C_ADDR_MPU6050    0x68        /*!< MPU6050 IMU (AD0=GND) */
#define BSP_I2C_ADDR_VL53L0X    0x29        /*!< VL53L0X TOF (default) */

/* ============================================================
 * PWM — Motor Control (DRV8833)
 * ============================================================ */
#define BSP_PIN_MOTOR_AIN1      GPIO_NUM_10  /*!< Left motor IN1 */
#define BSP_PIN_MOTOR_AIN2      GPIO_NUM_11  /*!< Left motor IN2 */
#define BSP_PIN_MOTOR_BIN1      GPIO_NUM_12  /*!< Right motor IN1 */
#define BSP_PIN_MOTOR_BIN2      GPIO_NUM_13  /*!< Right motor IN2 */

#define BSP_MOTOR_PWM_FREQ_HZ    10000      /*!< 10 kHz PWM frequency */
#define BSP_MOTOR_PWM_RESOLUTION LEDC_TIMER_8_BIT  /*!< 8-bit resolution (0-255) */
#define BSP_MOTOR_PWM_TIMER     LEDC_TIMER_0
#define BSP_MOTOR_PWM_SPEED_MODE LEDC_LOW_SPEED_MODE

/* ============================================================
 * GPIO — IR Sensors (TCRT5000 + ITR20001)
 * ============================================================ */
#define BSP_PIN_IR_OBSTACLE_L   GPIO_NUM_14  /*!< Left obstacle detection (TCRT5000) */
#define BSP_PIN_IR_OBSTACLE_R   GPIO_NUM_15  /*!< Right obstacle detection (TCRT5000) */
#define BSP_PIN_IR_EDGE_L       GPIO_NUM_16  /*!< Left edge detection (ITR20001) */
#define BSP_PIN_IR_EDGE_R       GPIO_NUM_17  /*!< Right edge detection (ITR20001) */

/* ============================================================
 * GPIO — Charger & Power (TP4056)
 * ============================================================ */
#define BSP_PIN_CHRG            GPIO_NUM_18  /*!< TP4056 CHRG pin (LOW=charging) */
#define BSP_PIN_STDBY           GPIO_NUM_21  /*!< TP4056 STDBY pin (LOW=full) */

/* ============================================================
 * ADC — Battery Voltage
 * ============================================================ */
#define BSP_PIN_VBAT_ADC        GPIO_NUM_1   /*!< Battery voltage ADC (ADC1_CH0) */
#define BSP_ADC_UNIT             ADC_UNIT_1
#define BSP_ADC_CHANNEL          ADC_CHANNEL_0

/* Voltage divider: R1=R2=100kΩ → VBAT = ADC_VALUE × 2 × 3.3 / 4095 */
#define BSP_BATTERY_DIVIDER_RATIO 2.0f

/* ============================================================
 * USB-JTAG (DO NOT USE — reserved for debugging)
 * ============================================================ */
#define BSP_PIN_USB_DPLUS       GPIO_NUM_19  /*!< USB D+ (reserved) */
#define BSP_PIN_USB_DMINUS      GPIO_NUM_20  /*!< USB D- (reserved) */

/* ============================================================
 * UART0 (DO NOT USE — reserved for console)
 * ============================================================ */
#define BSP_PIN_UART_TX         GPIO_NUM_43  /*!< UART0 TX (USB-JTAG) */
#define BSP_PIN_UART_RX         GPIO_NUM_44  /*!< UART0 RX (USB-JTAG) */

/* ============================================================
 * Strapping Pins (use with caution)
 * ============================================================ */
#define BSP_PIN_BOOT            GPIO_NUM_0   /*!< Boot mode (HIGH=Flash, LOW=Download) */
#define BSP_PIN_JTAG_TDO       GPIO_NUM_3   /*!< JTAG TDO */
#define BSP_PIN_VDD_SPI         GPIO_NUM_45  /*!< VDD_SPI voltage select */
#define BSP_PIN_ROM_MSG         GPIO_NUM_46  /*!< ROM messages printing */

#ifdef __cplusplus
}
#endif