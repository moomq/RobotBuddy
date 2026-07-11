/**
 * @file st7789.h
 * @brief ST7789 LCD display driver for ESP32-S3 (RobotBuddy).
 *
 * Provides a low-level driver for Sitronix ST7789-based TFT LCD panels
 * connected via SPI.  All pin assignments come from the caller through
 * st7789_config_t so the driver stays board-independent.
 *
 * @version 1.0.0
 * @date    2026-07-11
 * @copyright Copyright (c) 2026 RobotBuddy project
 */

#ifndef ST7789_H
#define ST7789_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/*  Constants                                                           */
/* -------------------------------------------------------------------- */

/** Maximum SPI transaction size (in bytes) used for pixel transfers. */
#define ST7789_MAX_TRANSFER_SZ  4096

/** Default SPI clock frequency (40 MHz). */
#define ST7789_DEFAULT_SPI_FREQ  40000000

/** Default panel width in pixels. */
#define ST7789_DEFAULT_WIDTH     240

/** Default panel height in pixels. */
#define ST7789_DEFAULT_HEIGHT    320

/* -------------------------------------------------------------------- */
/*  Types                                                               */
/* -------------------------------------------------------------------- */

/**
 * @brief Configuration structure for the ST7789 driver.
 *
 * Every field must be set by the caller — the driver never falls back to
 * hard-coded pin numbers.  A typical source is a board-specific pinmap
 * header (e.g. bsp_pinmap.h).
 */
typedef struct {
    spi_host_device_t spi_host;   /**< SPI peripheral (FSPI / VSPI / HSPI)       */
    int pin_cs;                   /**< Chip-select GPIO                           */
    int pin_dc;                   /**< Data/Command GPIO                          */
    int pin_rst;                  /**< Hardware-reset GPIO                        */
    int pin_bl;                   /**< Backlight GPIO (driven by LEDC PWM)        */
    uint32_t spi_freq_hz;        /**< SPI clock in Hz (0 → ST7789_DEFAULT_SPI_FREQ) */
    uint16_t width;               /**< Panel width  (0 → ST7789_DEFAULT_WIDTH)   */
    uint16_t height;              /**< Panel height (0 → ST7789_DEFAULT_HEIGHT)  */
} st7789_config_t;

/* -------------------------------------------------------------------- */
/*  Public API                                                          */
/* -------------------------------------------------------------------- */

/**
 * @brief Initialise the ST7789 display.
 *
 * Configures SPI, performs a hardware reset, sends the standard init
 * command sequence (MADCTL, COLMOD RGB565, PORCTRL, GCTRL, VCOMS,
 * LCMCTRL, INVON, NORON, DISPON), and enables the backlight at 100 %.
 *
 * @param[in] config  Driver configuration.  The struct is copied internally;
 *                     the caller does not need to keep it alive after this call.
 * @return ESP_OK on success; ESP_ERR_INVALID_ARG or ESP_FAIL on error.
 */
esp_err_t st7789_init(const st7789_config_t *config);

/**
 * @brief De-initialise the ST7789 display and release all resources.
 *
 * Stops the LEDC PWM channel, removes the SPI device, and resets GPIO
 * configuration.  After this call the driver cannot be used until
 * st7789_init() is called again.
 *
 * @return ESP_OK on success; ESP_FAIL if the driver was not initialised.
 */
esp_err_t st7789_deinit(void);

/**
 * @brief Draw a rectangular bitmap on the display.
 *
 * Sets a column/row address window from (x, y) to (x+w-1, y+h-1) and
 * writes @p bitmap as RGB565 pixel data over SPI.
 *
 * @param x      Horizontal start coordinate (0 .. width-1).
 * @param y      Vertical   start coordinate (0 .. height-1).
 * @param w      Rectangle width  in pixels (must be > 0).
 * @param h      Rectangle height in pixels (must be > 0).
 * @param bitmap Pointer to the pixel buffer (w*h uint16_t values, big-endian
 *               RGB565).  Must remain valid for the duration of the call.
 * @return ESP_OK on success; ESP_ERR_INVALID_ARG or ESP_FAIL on error.
 */
esp_err_t st7789_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                              const uint16_t *bitmap);

/**
 * @brief Fill a rectangular area with a single colour.
 *
 * Sets the same column/row address window as draw_bitmap but transmits
 * @p color for every pixel, avoiding the need for a source buffer.
 *
 * @param x     Horizontal start coordinate.
 * @param y     Vertical   start coordinate.
 * @param w     Rectangle width  (must be > 0).
 * @param h     Rectangle height (must be > 0).
 * @param color RGB565 fill colour (native byte order).
 * @return ESP_OK on success; ESP_ERR_INVALID_ARG or ESP_FAIL on error.
 */
esp_err_t st7789_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                             uint16_t color);

/**
 * @brief Set the backlight brightness.
 *
 * Drives the BL pin through an LEDC PWM channel.  The @p brightness
 * value is mapped linearly to the PWM duty cycle (0 = off, 255 = full).
 *
 * @param brightness Brightness level (0-255).
 * @return ESP_OK on success; ESP_FAIL if the driver was not initialised.
 */
esp_err_t st7789_set_backlight(uint8_t brightness);

/**
 * @brief Put the display into or out of sleep mode.
 *
 * When @p sleep is true the ST7789 enters deep-sleep (SLPOUT → SLPIN)
 * and the backlight is turned off.  When false the display wakes up
 * (SLPIN → SLPOUT → DISPON) and the backlight is restored to the last
 * set brightness.
 *
 * @param sleep  true to enter sleep, false to wake up.
 * @return ESP_OK on success; ESP_FAIL on error.
 */
esp_err_t st7789_sleep(bool sleep);

#ifdef __cplusplus
}
#endif

#endif /* ST7789_H */