/**
 * @file st7789.c
 * @brief ST7789 LCD display driver implementation for ESP32-S3 (RobotBuddy).
 *
 * SPI-transaction based communication with LEDC PWM backlight control.
 * All pin assignments are received via st7789_config_t — no hard-coded GPIOs.
 *
 * @version 1.0.0
 * @date    2026-07-11
 * @copyright Copyright (c) 2026 RobotBuddy project
 */

#include "st7789.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

/* -------------------------------------------------------------------- */
/*  ST7789 command set                                                  */
/* -------------------------------------------------------------------- */

#define CMD_NOP         0x00  /**< No operation                            */
#define CMD_SWRST       0x01  /**< Software reset                          */
#define CMD_SLPIN       0x10  /**< Enter sleep mode                        */
#define CMD_SLPOUT      0x11  /**< Exit sleep mode                         */
#define CMD_INVON       0x21  /**< Inversion on                            */
#define CMD_DISPOFF     0x28  /**< Display off                             */
#define CMD_DISPON      0x29  /**< Display on                              */
#define CMD_CASET       0x2A  /**< Column address set                      */
#define CMD_RASET       0x2B  /**< Row address set                        */
#define CMD_RAMWR       0x2C  /**< Memory write                            */
#define CMD_MADCTL      0x36  /**< Memory data access control              */
#define CMD_COLMOD      0x3A  /**< Interface pixel format                  */
#define CMD_PORCTRL     0xB2  /**< Porch control                           */
#define CMD_GCTRL       0xB7  /**< Gate control                            */
#define CMD_VCOMS       0xBB  /**< VCOM setting                            */
#define CMD_LCMCTRL     0xC0  /**< LCM control                             */
#define CMD_NORON       0x13  /**< Normal display mode on                  */

/* -------------------------------------------------------------------- */
/*  MADCTL bit definitions                                              */
/* -------------------------------------------------------------------- */

#define MADCTL_MY       0x80  /**< Row address order                      */
#define MADCTL_MX       0x40  /**< Column address order                   */
#define MADCTL_MV       0x20  /**< Row/column exchange                    */
#define MADCTL_ML       0x10  /**< Vertical refresh order                 */
#define MADCTL_RGB      0x00  /**< RGB colour space                       */
#define MADCTL_BGR      0x08  /**< BGR colour space                       */

/* -------------------------------------------------------------------- */
/*  LEDC / PWM configuration                                            */
/* -------------------------------------------------------------------- */

/** LEDC timer used for backlight PWM. */
#define BL_LEDC_TIMER       LEDC_TIMER_0
/** LEDC channel used for backlight PWM. */
#define BL_LEDC_CHANNEL     LEDC_CHANNEL_0
/** PWM frequency for the backlight (5 kHz is inaudible and flicker-free). */
#define BL_PWM_FREQ_HZ      5000
/** LEDC resolution in bits — 8 bits matches the 0-255 brightness range. */
#define BL_LEDC_RESOLUTION  LEDC_TIMER_8_BIT

/* -------------------------------------------------------------------- */
/*  Timing constants                                                    */
/* -------------------------------------------------------------------- */

/** Duration of the RST low pulse (microseconds). */
#define RST_LOW_US          20
/** Duration to hold RST high after reset (milliseconds). */
#define RST_HIGH_MS         120
/** Delay after SLPOUT before sending further commands (ms). */
#define SLPOUT_DELAY_MS     120

/* -------------------------------------------------------------------- */
/*  Module handle (singleton)                                           */
/* -------------------------------------------------------------------- */

/**
 * @brief Runtime state for the driver.
 *
 * Only one instance is supported.  After st7789_init() succeeds the
 * fields are valid; after st7789_deinit() they are cleared.
 */
typedef struct {
    spi_device_handle_t spi;        /**< SPI device handle                 */
    st7789_config_t     cfg;        /**< Copy of the caller's config      */
    bool                initialised; /**< true between init/deinit         */
    uint8_t             brightness; /**< Last backlight level (0-255)     */
    bool                sleeping;   /**< true when in deep-sleep mode     */
} st7789_dev_t;

static st7789_dev_t s_dev;

/* -------------------------------------------------------------------- */
/*  Logging tag                                                         */
/* -------------------------------------------------------------------- */

static const char *TAG = "st7789";

/* -------------------------------------------------------------------- */
/*  Helpers — low-level SPI / GPIO                                      */
/* -------------------------------------------------------------------- */

/**
 * @brief SPI pre-transfer callback — toggles the DC GPIO.
 *
 * The SPI master calls this before each transaction.  When the
 * transaction has no command phase we set DC HIGH (data); otherwise
 * DC LOW (command).
 *
 * @param[in] trans  Pointer to the SPI transaction about to start.
 */
static void spi_pre_transfer_cb(spi_transaction_t *trans)
{
    /* When tx_buffer is present and the command field is unused we
     * treat it as data (DC = 1).  When only cmd is sent we treat
     * it as a command (DC = 0).
     *
     * A cleaner approach: we encode DC into the transaction user
     * field.  0 = command (DC low), 1 = data (DC high).
     */
    int dc = (int)(intptr_t)trans->user;
    gpio_set_level((int)s_dev.cfg.pin_dc, dc);
}

/* -------------------------------------------------------------------- */
/*  SPI command helpers (DC-aware)                                      */
/* -------------------------------------------------------------------- */

/**
 * @brief Send a command byte with DC held LOW.
 *
 * Uses the @c user field to tell the pre-transfer callback to pull DC
 * low, which makes this a command transaction.
 */
static esp_err_t send_cmd_dc(uint8_t cmd)
{
    spi_transaction_t tx = {
        .length    = 8,
        .tx_buffer = &cmd,
        .user      = (void *)(intptr_t)0,  /* DC = LOW → command */
    };
    return spi_device_polling_transmit(s_dev.spi, &tx);
}

/**
 * @brief Send data bytes with DC held HIGH.
 *
 * Uses the @c user field to tell the pre-transfer callback to pull DC
 * high, which makes this a data transaction.
 */
static esp_err_t send_data_dc(const uint8_t *data, int len)
{
    if (len <= 0) {
        return ESP_OK;
    }
    spi_transaction_t tx = {
        .length    = (size_t)len * 8,
        .tx_buffer = data,
        .user      = (void *)(intptr_t)1,   /* DC = HIGH → data   */
    };
    return spi_device_polling_transmit(s_dev.spi, &tx);
}

/* -------------------------------------------------------------------- */
/*  Mid-level display helpers                                           */
/* -------------------------------------------------------------------- */

/**
 * @brief Set the active column address window.
 *
 * @param xs  Start column (0-based).
 * @param xe  End   column (inclusive).
 * @return ESP_OK on success.
 */
static esp_err_t set_column_addr(uint16_t xs, uint16_t xe)
{
    uint8_t buf[4] = {
        (uint8_t)(xs >> 8), (uint8_t)(xs & 0xFF),
        (uint8_t)(xe >> 8), (uint8_t)(xe & 0xFF),
    };
    return send_cmd_dc(CMD_CASET) != ESP_OK ? ESP_FAIL
         : send_data_dc(buf, 4);
}

/**
 * @brief Set the active row address window.
 *
 * @param ys  Start row (0-based).
 * @param ye  End   row (inclusive).
 * @return ESP_OK on success.
 */
static esp_err_t set_row_addr(uint16_t ys, uint16_t ye)
{
    uint8_t buf[4] = {
        (uint8_t)(ys >> 8), (uint8_t)(ys & 0xFF),
        (uint8_t)(ye >> 8), (uint8_t)(ye & 0xFF),
    };
    return send_cmd_dc(CMD_RASET) != ESP_OK ? ESP_FAIL
         : send_data_dc(buf, 4);
}

/**
 * @brief Set the pixel format to RGB565 (16-bit).
 *
 * @return ESP_OK on success.
 */
static esp_err_t set_colmod_rgb565(void)
{
    uint8_t fmt = 0x55; /* 16-bit RGB565 */
    return send_cmd_dc(CMD_COLMOD) != ESP_OK ? ESP_FAIL
         : send_data_dc(&fmt, 1);
}

/**
 * @brief Perform a hardware reset via the RST GPIO.
 *
 * Asserts RST low for ::RST_LOW_US, releases it, then waits for the
 * display's internal initialisation to finish.
 */
static void hardware_reset(void)
{
    gpio_set_level(s_dev.cfg.pin_rst, 0);
    esp_rom_delay_us(RST_LOW_US);     /* 20 µs low */
    gpio_set_level(s_dev.cfg.pin_rst, 1);
    vTaskDelay(pdMS_TO_TICKS(RST_HIGH_MS));
}

/**
 * @brief Send the full ST7789 initialisation command sequence.
 *
 * This is the panel-specific init list: MADCTL, COLMOD, PORCTRL, GCTRL,
 * VCOMS, LCMCTRL, INVON, NORON, DISPON.
 *
 * @return ESP_OK on success.
 */
static esp_err_t send_init_cmds(void)
{
    /* ---- Exit sleep ---- */
    ESP_RETURN_ON_ERROR(send_cmd_dc(CMD_SLPOUT), TAG, "SLPOUT failed");
    vTaskDelay(pdMS_TO_TICKS(SLPOUT_DELAY_MS));

    /* ---- Memory Data Access Control ---- */
    /* Default orientation: RGB, no mirror/rotation.
     * Adjust MADCTL bits for your panel's native scan direction. */
    uint8_t madctl = MADCTL_RGB;
    ESP_RETURN_ON_ERROR(send_cmd_dc(CMD_MADCTL), TAG, "MADCTL cmd failed");
    ESP_RETURN_ON_ERROR(send_data_dc(&madctl, 1), TAG, "MADCTL data failed");

    /* ---- Interface Pixel Format: RGB565 ---- */
    ESP_RETURN_ON_ERROR(set_colmod_rgb565(), TAG, "COLMOD failed");

    /* ---- Porch Control ---- */
    /* Default porch settings: BP=0x0C, FP=0x0C, ENABLE=0x14 */
    {
        uint8_t porctrl[] = { 0x0C, 0x0C, 0x00, 0x33, 0x33 };
        ESP_RETURN_ON_ERROR(send_cmd_dc(CMD_PORCTRL), TAG, "PORCTRL cmd failed");
        ESP_RETURN_ON_ERROR(send_data_dc(porctrl, sizeof(porctrl)),
                            TAG, "PORCTRL data failed");
    }

    /* ---- Gate Control ---- */
    /* VGH = 12.54 V, VGL = -7.26 V (default) */
    {
        uint8_t gctrl = 0x35;
        ESP_RETURN_ON_ERROR(send_cmd_dc(CMD_GCTRL), TAG, "GCTRL cmd failed");
        ESP_RETURN_ON_ERROR(send_data_dc(&gctrl, 1), TAG, "GCTRL data failed");
    }

    /* ---- VCOM Setting ---- */
    /* VCOM = 1.175 V (default) */
    {
        uint8_t vcoms = 0x19;
        ESP_RETURN_ON_ERROR(send_cmd_dc(CMD_VCOMS), TAG, "VCOMS cmd failed");
        ESP_RETURN_ON_ERROR(send_data_dc(&vcoms, 1), TAG, "VCOMS data failed");
    }

    /* ---- LCM Control ---- */
    /* Default: XMH, XMH settings */
    {
        uint8_t lcmctrl = 0x2C;
        ESP_RETURN_ON_ERROR(send_cmd_dc(CMD_LCMCTRL), TAG, "LCMCTRL cmd failed");
        ESP_RETURN_ON_ERROR(send_data_dc(&lcmctrl, 1), TAG, "LCMCTRL data failed");
    }

    /* ---- Inversion On ---- */
    ESP_RETURN_ON_ERROR(send_cmd_dc(CMD_INVON), TAG, "INVON failed");

    /* ---- Normal Display Mode On ---- */
    ESP_RETURN_ON_ERROR(send_cmd_dc(CMD_NORON), TAG, "NORON failed");
    vTaskDelay(pdMS_TO_TICKS(20));

    /* ---- Display On ---- */
    ESP_RETURN_ON_ERROR(send_cmd_dc(CMD_DISPON), TAG, "DISPON failed");

    return ESP_OK;
}

/* -------------------------------------------------------------------- */
/*  LEDC PWM helpers for the backlight                                  */
/* -------------------------------------------------------------------- */

/**
 * @brief Initialise LEDC PWM for the backlight GPIO.
 *
 * Configures a 5 kHz, 8-bit resolution PWM channel on the BL pin.
 *
 * @return ESP_OK on success.
 */
static esp_err_t backlight_pwm_init(void)
{
    const ledc_timer_config_t timer_cfg = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = BL_LEDC_RESOLUTION,
        .timer_num        = BL_LEDC_TIMER,
        .freq_hz          = BL_PWM_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG,
                        "LEDC timer config failed");

    const ledc_channel_config_t ch_cfg = {
        .gpio_num   = s_dev.cfg.pin_bl,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = BL_LEDC_CHANNEL,
        .timer_sel   = BL_LEDC_TIMER,
        .duty        = 0,           /* start OFF */
        .hpoint     = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ch_cfg), TAG,
                        "LEDC channel config failed");
    return ESP_OK;
}

/**
 * @brief De-initialise LEDC PWM (release timer & channel).
 */
static void backlight_pwm_deinit(void)
{
    ledc_stop(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL, 0);
    /* Timer deconfig is not strictly required; stop is sufficient. */
}

/* -------------------------------------------------------------------- */
/*  Public API                                                          */
/* -------------------------------------------------------------------- */

esp_err_t st7789_init(const st7789_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "config is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (s_dev.initialised) {
        ESP_LOGW(TAG, "Already initialised — call deinit() first");
        return ESP_FAIL;
    }

    /* Copy config and apply defaults for zero-valued fields. */
    s_dev.cfg = *config;
    if (s_dev.cfg.spi_freq_hz == 0) {
        s_dev.cfg.spi_freq_hz = ST7789_DEFAULT_SPI_FREQ;
    }
    if (s_dev.cfg.width == 0) {
        s_dev.cfg.width = ST7789_DEFAULT_WIDTH;
    }
    if (s_dev.cfg.height == 0) {
        s_dev.cfg.height = ST7789_DEFAULT_HEIGHT;
    }

    ESP_LOGI(TAG, "Initialising ST7789 %ux%u @ %lu Hz on SPI%d",
             s_dev.cfg.width, s_dev.cfg.height,
             (unsigned long)s_dev.cfg.spi_freq_hz,
             s_dev.cfg.spi_host + 1);

    /* ---- SPI device ---- */
    const spi_device_interface_config_t dev_cfg = {
        .command_bits     = 0,        /* we drive DC via pre-cb */
        .address_bits     = 0,
        .mode             = 0,        /* SPI mode 0 */
        .clock_speed_hz   = s_dev.cfg.spi_freq_hz,
        .spics_io_num     = s_dev.cfg.pin_cs,
        .queue_size        = 7,
        .pre_cb            = spi_pre_transfer_cb,
        .post_cb           = NULL,
        .flags             = SPI_DEVICE_HALFDUPLEX,
    };

    esp_err_t ret;
    ret = spi_bus_add_device(s_dev.cfg.spi_host, &dev_cfg, &s_dev.spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ---- GPIO for DC ---- */
    {
        const gpio_config_t dc_cfg = {
            .pin_bit_mask = 1ULL << s_dev.cfg.pin_dc,
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        ret = gpio_config(&dc_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "DC GPIO config failed: %s", esp_err_to_name(ret));
            goto cleanup_spi;
        }
        gpio_set_level(s_dev.cfg.pin_dc, 1);
    }

    /* ---- GPIO for RST ---- */
    {
        const gpio_config_t rst_cfg = {
            .pin_bit_mask = 1ULL << s_dev.cfg.pin_rst,
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        ret = gpio_config(&rst_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "RST GPIO config failed: %s", esp_err_to_name(ret));
            goto cleanup_dc;
        }
        gpio_set_level(s_dev.cfg.pin_rst, 1);
    }

    /* ---- Hardware reset ---- */
    hardware_reset();

    /* ---- Send init command sequence ---- */
    ret = send_init_cmds();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Init command sequence failed (0x%x)", ret);
        goto cleanup_rst;
    }

    /* ---- Backlight PWM ---- */
    ret = backlight_pwm_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Backlight PWM init failed (0x%x)", ret);
        goto cleanup_rst;
    }

    /* Turn backlight on at full brightness. */
    s_dev.brightness = 255;
    s_dev.sleeping   = false;
    ret = st7789_set_backlight(255);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Initial backlight set failed — continuing anyway");
    }

    s_dev.initialised = true;
    ESP_LOGI(TAG, "ST7789 initialised successfully");
    return ESP_OK;

    /* ---- Error cleanup (goto targets) ---- */
cleanup_rst:
    gpio_reset_pin(s_dev.cfg.pin_rst);
cleanup_dc:
    gpio_reset_pin(s_dev.cfg.pin_dc);
cleanup_spi:
    spi_bus_remove_device(s_dev.spi);
    s_dev.spi = NULL;
    return ret;
}

/* -------------------------------------------------------------------- */

esp_err_t st7789_deinit(void)
{
    if (!s_dev.initialised) {
        ESP_LOGW(TAG, "Not initialised — nothing to do");
        return ESP_FAIL;
    }

    /* Turn off display and backlight first. */
    send_cmd_dc(CMD_DISPOFF);
    backlight_pwm_deinit();

    /* Remove SPI device. */
    ESP_RETURN_ON_ERROR(spi_bus_remove_device(s_dev.spi),
                        TAG, "SPI device remove failed");

    /* Reset GPIOs to default state. */
    gpio_reset_pin(s_dev.cfg.pin_dc);
    gpio_reset_pin(s_dev.cfg.pin_rst);
    gpio_reset_pin(s_dev.cfg.pin_bl);

    memset(&s_dev, 0, sizeof(s_dev));
    s_dev.initialised = false;
    ESP_LOGI(TAG, "ST7789 de-initialised");
    return ESP_OK;
}

/* -------------------------------------------------------------------- */

esp_err_t st7789_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                              const uint16_t *bitmap)
{
    if (!s_dev.initialised) {
        ESP_LOGE(TAG, "Driver not initialised");
        return ESP_FAIL;
    }
    if (bitmap == NULL) {
        ESP_LOGE(TAG, "bitmap is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (w == 0 || h == 0) {
        ESP_LOGE(TAG, "Invalid dimensions %ux%u", w, h);
        return ESP_ERR_INVALID_ARG;
    }
    if ((x + w) > s_dev.cfg.width || (y + h) > s_dev.cfg.height) {
        ESP_LOGE(TAG, "Region (%u,%u)+%ux%u out of bounds %ux%u",
                 x, y, w, h, s_dev.cfg.width, s_dev.cfg.height);
        return ESP_ERR_INVALID_ARG;
    }

    /* Set address window. */
    ESP_RETURN_ON_ERROR(set_column_addr(x, x + w - 1), TAG, "Column addr failed");
    ESP_RETURN_ON_ERROR(set_row_addr(y, y + h - 1),    TAG, "Row addr failed");

    /* Issue RAMWR command. */
    ESP_RETURN_ON_ERROR(send_cmd_dc(CMD_RAMWR), TAG, "RAMWR failed");

    /* Send pixel data in chunks that fit within the SPI transfer limit.
     * Each pixel is 2 bytes (RGB565). */
    const int max_pixels = ST7789_MAX_TRANSFER_SZ / 2;
    const int total_pixels = (int)w * (int)h;
    int offset = 0;

    while (offset < total_pixels) {
        int chunk = total_pixels - offset;
        if (chunk > max_pixels) {
            chunk = max_pixels;
        }
        const uint8_t *ptr = (const uint8_t *)(bitmap + offset);
        ESP_RETURN_ON_ERROR(send_data_dc(ptr, chunk * 2), TAG,
                            "Bitmap data send failed");
        offset += chunk;
    }

    return ESP_OK;
}

/* -------------------------------------------------------------------- */

esp_err_t st7789_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                             uint16_t color)
{
    if (!s_dev.initialised) {
        ESP_LOGE(TAG, "Driver not initialised");
        return ESP_FAIL;
    }
    if (w == 0 || h == 0) {
        ESP_LOGE(TAG, "Invalid dimensions %ux%u", w, h);
        return ESP_ERR_INVALID_ARG;
    }
    if ((x + w) > s_dev.cfg.width || (y + h) > s_dev.cfg.height) {
        ESP_LOGE(TAG, "Region (%u,%u)+%ux%u out of bounds %ux%u",
                 x, y, w, h, s_dev.cfg.width, s_dev.cfg.height);
        return ESP_ERR_INVALID_ARG;
    }

    /* Set address window. */
    ESP_RETURN_ON_ERROR(set_column_addr(x, x + w - 1), TAG, "Column addr failed");
    ESP_RETURN_ON_ERROR(set_row_addr(y, y + h - 1),    TAG, "Row addr failed");
    ESP_RETURN_ON_ERROR(send_cmd_dc(CMD_RAMWR),        TAG, "RAMWR failed");

    /* Build a line buffer of repeated colour values.  The buffer is
     * sized to the maximum SPI transfer length so we can reuse it
     * across iterations. */
    const int line_pixels = (int)w;
    const int max_pixels  = ST7789_MAX_TRANSFER_SZ / 2;
    const int chunk_pixels = (line_pixels < max_pixels) ? line_pixels
                                                        : max_pixels;
    const int chunk_bytes = chunk_pixels * 2;

    /* Allocate the fill buffer on the heap (SPI DMA alignment). */
    uint8_t *fill_buf = heap_caps_malloc(chunk_bytes, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (fill_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate fill buffer (%d bytes)", chunk_bytes);
        return ESP_ERR_NO_MEM;
    }

    /* Pre-fill the buffer with the 16-bit colour (big-endian for SPI). */
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);
    for (int i = 0; i < chunk_pixels; i++) {
        fill_buf[i * 2]     = hi;
        fill_buf[i * 2 + 1] = lo;
    }

    const int total_pixels = (int)w * (int)h;
    int sent = 0;
    esp_err_t ret = ESP_OK;

    while (sent < total_pixels) {
        int count = total_pixels - sent;
        if (count > chunk_pixels) {
            count = chunk_pixels;
        }
        ret = send_data_dc(fill_buf, count * 2);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Fill data send failed");
            break;
        }
        sent += count;
    }

    heap_caps_free(fill_buf);
    return ret;
}

/* -------------------------------------------------------------------- */

esp_err_t st7789_set_backlight(uint8_t brightness)
{
    if (!s_dev.initialised) {
        ESP_LOGE(TAG, "Driver not initialised");
        return ESP_FAIL;
    }

    s_dev.brightness = brightness;

    /* Map 0-255 to duty cycle (8-bit resolution → direct mapping). */
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL,
                                   (uint32_t)brightness);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_set_duty failed (0x%x)", ret);
        return ret;
    }
    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_update_duty failed (0x%x)", ret);
    }
    return ret;
}

/* -------------------------------------------------------------------- */

esp_err_t st7789_sleep(bool sleep)
{
    if (!s_dev.initialised) {
        ESP_LOGE(TAG, "Driver not initialised");
        return ESP_FAIL;
    }

    esp_err_t ret;

    if (sleep) {
        ESP_LOGI(TAG, "Entering sleep mode");

        /* Turn off backlight first to save power. */
        ret = st7789_set_backlight(0);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Could not turn off backlight");
        }

        /* Send SLPIN command. */
        ret = send_cmd_dc(CMD_SLPIN);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SLPIN command failed");
            return ret;
        }

        s_dev.sleeping = true;
    } else {
        ESP_LOGI(TAG, "Waking from sleep");

        /* Send SLPOUT command. */
        ret = send_cmd_dc(CMD_SLPOUT);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SLPOUT command failed");
            return ret;
        }

        /* The display needs at least 120 ms after SLPOUT. */
        vTaskDelay(pdMS_TO_TICKS(SLPOUT_DELAY_MS));

        /* Re-send DISPON to ensure the display is active. */
        ret = send_cmd_dc(CMD_DISPON);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "DISPON command failed");
            return ret;
        }

        s_dev.sleeping = false;

        /* Restore previous backlight level. */
        ret = st7789_set_backlight(s_dev.brightness);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Could not restore backlight");
        }
    }

    return ESP_OK;
}