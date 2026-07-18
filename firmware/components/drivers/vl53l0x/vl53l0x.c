/**
 * @file vl53l0x.c
 * @brief VL53L0X Time-of-Flight distance sensor driver implementation for ESP32-S3.
 *
 * Uses the new ESP-IDF v5.x I²C master API (driver/i2c_master.h) to
 * communicate with the ST VL53L0X sensor.  Implements single-shot ranging
 * and continuous ranging modes with configurable range profiles.
 *
 * The initialisation sequence follows the VL53L0X datasheet "single ranging
 * polling" example: configure VHV / signal rate / SPADs, then trigger
 * measurements by writing REG_SYSRANGE_START and polling for completion.
 *
 * @copyright 2026 RobotBuddy contributors
 * @licence MIT
 */

#include "vl53l0x.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ------------------------------------------------------------------------ */
/*  Register map                                                             */
/* ------------------------------------------------------------------------ */

/* Identification registers */
#define REG_IDENTIFICATION_MODEL_ID        0x0010  /**< Model ID (reads 0xEE)       */
#define REG_IDENTIFICATION_REVISION_ID     0x0011  /**< Revision ID                  */

/* System registers */
#define REG_SYSRANGE_START                 0x0000  /**< System range start           */
#define REG_SYSTEM_THRESH_HIGH             0x000C  /**< High threshold               */
#define REG_SYSTEM_THRESH_LOW              0x000E  /**< Low threshold                */

/* System configuration */
#define REG_SYSTEM_SEQUENCE_CONFIG         0x0001  /**< Sequence configuration       */
#define REG_SYSTEM_INTERMEASUREMENT_PERIOD 0x0004  /**< Inter-measurement period     */

/* System interrupt */
#define REG_SYSTEM_INTERRUPT_CONFIG_GPIO   0x000A  /**< GPIO interrupt config        */
#define REG_SYSTEM_INTERRUPT_CLEAR         0x000B  /**< Interrupt clear              */

/* Result registers */
#define REG_RESULT_INTERRUPT_STATUS        0x0013  /**< Interrupt status             */
#define REG_RESULT_RANGE_STATUS            0x0014  /**< Range status (4 bytes)       */

/* SPAD (Single Photon Avalanche Diode) registers */
#define REG_SPAD_REF_START                 0x0040  /**< Reference SPAD start         */
#define REG_SPAD_REF_ENABLE_COUNT          0x0044  /**< Reference SPAD count         */

/* Signal rate registers */
#define REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT  0x0044
#define REG_FINAL_RANGE_CONFIG_VALID_PHASE_LOW           0x0056
#define REG_FINAL_RANGE_CONFIG_VALID_PHASE_HIGH          0x0057

/* VHV (Voltage High) config */
#define REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV  0x0089

/* MSRC (Macro Signal Rate Check) config */
#define REG_MSRC_CONFIG_CONTROL               0x0060

/* Dynamic SPAD selection */
#define REG_DYNAMIC_SPAD_REF_ENABLE_START     0x004E
#define REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD 0x004F
#define REG_GLOBAL_CONFIG_REF_ENABLE_START    0x0052
#define REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0  0x00B0

/* ------------------------------------------------------------------------ */
/*  Register values                                                          */
/* ------------------------------------------------------------------------ */

#define MODEL_ID_VALUE              0xEE   /**< Expected model identification  */

#define SYSRANGE_START_SINGLE       0x01   /**< Start single ranging           */
#define SYSRANGE_START_CONTINUOUS   0x02   /**< Start continuous ranging       */
#define SYSRANGE_START_STOP         0x00   /**< Stop ranging                   */

#define INTERRUPT_NEW_SAMPLE_READY  0x04   /**< New sample ready interrupt     */
#define RESULT_INTERRUPT_CLEAR      0x01   /**< Clear interrupt                */

/* Sequence config: enable VHV, phase cal, and final range */
#define SEQUENCE_CONFIG_DEFAULT     0xE8

/* SPAD configuration defaults */
#define SPAD_REF_ENABLE_START       0x01
#define SPAD_NUM_REQUESTED_REF      0x0C   /**< 12 reference SPADs             */
#define GLOBAL_CONFIG_REF_ENABLE    0xC4   /**< Reference SPAD enable          */

/* Signal rate limit for different range modes */
#define SIGNAL_RATE_LIMIT_SHORT     0x18   /**< Short range: 0.25 MCPS        */
#define SIGNAL_RATE_LIMIT_MEDIUM    0x08   /**< Medium range: 0.10 MCPS       */
#define SIGNAL_RATE_LIMIT_LONG      0x00   /**< Long range: no limit           */

/* VHV config */
#define VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV_VALUE  0xFF

/* MSRC config */
#define MSRC_CONFIG_CONTROL_VALUE  0x00

/* ------------------------------------------------------------------------ */
/*  Constants                                                                */
/* ------------------------------------------------------------------------ */

static const char *TAG = "vl53l0x";

/** @brief I²C timeout in milliseconds for register operations. */
#define I2C_TIMEOUT_MS  100

/** @brief Polling interval while waiting for ranging completion (ms). */
#define RANGING_POLL_INTERVAL_MS  5

/* ------------------------------------------------------------------------ */
/*  Module state                                                             */
/* ------------------------------------------------------------------------ */

typedef struct {
    i2c_master_dev_handle_t dev_handle;  /**< I²C device handle              */
    uint8_t  i2c_addr;                   /**< Device I²C address             */
    uint32_t timeout_ms;                 /**< Ranging timeout (ms)           */
    vl53l0x_range_mode_t range_mode;     /**< Current ranging mode           */
    bool     initialised;
    bool     ranging_active;             /**< True if continuous ranging on  */
} vl53l0x_state_t;

static vl53l0x_state_t s_state = {0};

/* ------------------------------------------------------------------------ */
/*  I²C helpers (new ESP-IDF v5.x I2C master API)                           */
/* ------------------------------------------------------------------------ */

/**
 * @brief Write a single byte to a 16-bit VL53L0X register.
 *
 * The VL53L0X uses 16-bit register addresses (MSB first).
 *
 * @param reg  16-bit register address.
 * @param val  Byte value to write.
 * @return ESP_OK on success, or an ESP-IDF error code.
 */
static esp_err_t reg_write(uint16_t reg, uint8_t val)
{
    uint8_t buf[3] = {
        (uint8_t)(reg >> 8),   /* Register address high byte */
        (uint8_t)(reg & 0xFF), /* Register address low byte  */
        val,
    };
    esp_err_t err = i2c_master_transmit(
        s_state.dev_handle, buf, sizeof(buf), pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C write failed reg=0x%04X val=0x%02X: %s",
                 reg, val, esp_err_to_name(err));
    }
    return err;
}

/**
 * @brief Read @p len bytes starting at a 16-bit VL53L0X register.
 *
 * @param reg  16-bit register address.
 * @param buf  Buffer to receive the read data.
 * @param len  Number of bytes to read.
 * @return ESP_OK on success, or an ESP-IDF error code.
 */
static esp_err_t reg_read(uint16_t reg, uint8_t *buf, size_t len)
{
    uint8_t reg_buf[2] = {
        (uint8_t)(reg >> 8),
        (uint8_t)(reg & 0xFF),
    };
    esp_err_t err = i2c_master_transmit_receive(
        s_state.dev_handle, reg_buf, sizeof(reg_buf),
        buf, len, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C read failed reg=0x%04X len=%u: %s",
                 reg, (unsigned)len, esp_err_to_name(err));
    }
    return err;
}

/**
 * @brief Write a 16-bit value to a 16-bit VL53L0X register.
 *
 * @param reg  16-bit register address.
 * @param val  16-bit value to write (big-endian on wire).
 * @return ESP_OK on success, or an ESP-IDF error code.
 */
static esp_err_t reg_write_16(uint16_t reg, uint16_t val)
{
    uint8_t buf[4] = {
        (uint8_t)(reg >> 8),
        (uint8_t)(reg & 0xFF),
        (uint8_t)(val >> 8),
        (uint8_t)(val & 0xFF),
    };
    esp_err_t err = i2c_master_transmit(
        s_state.dev_handle, buf, sizeof(buf), pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C write16 failed reg=0x%04X val=0x%04X: %s",
                 reg, val, esp_err_to_name(err));
    }
    return err;
}

/* ------------------------------------------------------------------------ */
/*  Sensor initialisation sequence                                           */
/* ------------------------------------------------------------------------ */

/**
 * @brief Apply the VL53L0X default configuration for single ranging.
 *
 * Follows the ST-recommended initialisation sequence:
 *   1. Set VHV config
 *   2. Configure signal rate limit
 *   3. Configure SPADs
 *   4. Set sequence config
 *   5. Configure MSRC
 *
 * @return ESP_OK on success, or an ESP-IDF error code.
 */
static esp_err_t apply_sensor_config(void)
{
    esp_err_t err;

    /* ---- VHV config ---- */
    err = reg_write(REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV,
                    VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV_VALUE);
    if (err != ESP_OK) { return err; }

    /* ---- Signal rate limit based on range mode ---- */
    uint8_t signal_rate;
    switch (s_state.range_mode) {
    case VL53L0X_LONG_RANGE:
        signal_rate = SIGNAL_RATE_LIMIT_LONG;
        break;
    case VL53L0X_MEDIUM_RANGE:
        signal_rate = SIGNAL_RATE_LIMIT_MEDIUM;
        break;
    case VL53L0X_SHORT_RANGE:
    default:
        signal_rate = SIGNAL_RATE_LIMIT_SHORT;
        break;
    }
    /* Write signal rate as a 16-bit value (upper byte is integer, lower is fraction). */
    err = reg_write_16(REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT,
                       (uint16_t)signal_rate << 8);
    if (err != ESP_OK) { return err; }

    /* ---- SPAD configuration ---- */
    err = reg_write(REG_DYNAMIC_SPAD_REF_ENABLE_START, SPAD_REF_ENABLE_START);
    if (err != ESP_OK) { return err; }

    err = reg_write(REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD, SPAD_NUM_REQUESTED_REF);
    if (err != ESP_OK) { return err; }

    err = reg_write(REG_GLOBAL_CONFIG_REF_ENABLE_START, GLOBAL_CONFIG_REF_ENABLE);
    if (err != ESP_OK) { return err; }

    /* ---- Sequence config ---- */
    err = reg_write(REG_SYSTEM_SEQUENCE_CONFIG, SEQUENCE_CONFIG_DEFAULT);
    if (err != ESP_OK) { return err; }

    /* ---- MSRC config ---- */
    err = reg_write(REG_MSRC_CONFIG_CONTROL, MSRC_CONFIG_CONTROL_VALUE);
    if (err != ESP_OK) { return err; }

    /* ---- Valid phase range for long range mode ---- */
    if (s_state.range_mode == VL53L0X_LONG_RANGE) {
        err = reg_write(REG_FINAL_RANGE_CONFIG_VALID_PHASE_LOW, 0x08);
        if (err != ESP_OK) { return err; }
        err = reg_write(REG_FINAL_RANGE_CONFIG_VALID_PHASE_HIGH, 0x80);
        if (err != ESP_OK) { return err; }
    }

    return ESP_OK;
}

/* ------------------------------------------------------------------------ */
/*  Public API                                                               */
/* ------------------------------------------------------------------------ */

esp_err_t vl53l0x_init(const vl53l0x_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->i2c_bus == NULL) {
        ESP_LOGE(TAG, "I2C bus handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* ---- Copy configuration with defaults ---- */
    s_state.i2c_addr  = (config->i2c_addr   != 0) ? config->i2c_addr   : VL53L0X_DEFAULT_I2C_ADDR;
    s_state.timeout_ms = (config->timeout_ms != 0) ? config->timeout_ms : VL53L0X_DEFAULT_TIMEOUT_MS;
    s_state.range_mode = VL53L0X_SHORT_RANGE;

    /* ---- Create I²C device handle on the existing bus ---- */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = s_state.i2c_addr,
        .scl_speed_hz    = 400000,  /* VL53L0X supports 400 kHz */
    };
    esp_err_t err = i2c_master_bus_add_device(config->i2c_bus, &dev_cfg,
                                                &s_state.dev_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device at addr 0x%02X: %s",
                 s_state.i2c_addr, esp_err_to_name(err));
        return err;
    }

    /* ---- Verify device presence ---- */
    if (!vl53l0x_is_present()) {
        ESP_LOGE(TAG, "VL53L0X not found at addr 0x%02X", s_state.i2c_addr);
        i2c_master_bus_rm_device(s_state.dev_handle);
        s_state.dev_handle = NULL;
        return ESP_ERR_NOT_FOUND;
    }

    /* ---- Apply sensor configuration ---- */
    err = apply_sensor_config();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Sensor configuration failed: %s", esp_err_to_name(err));
        i2c_master_bus_rm_device(s_state.dev_handle);
        s_state.dev_handle = NULL;
        return err;
    }

    s_state.initialised = true;
    s_state.ranging_active = false;
    ESP_LOGI(TAG, "Initialised — addr=0x%02X timeout=%lu ms mode=%d",
             s_state.i2c_addr, s_state.timeout_ms, s_state.range_mode);
    return ESP_OK;
}

esp_err_t vl53l0x_deinit(void)
{
    if (!s_state.initialised) {
        ESP_LOGW(TAG, "De-init called but driver not initialised");
        return ESP_OK;
    }

    /* Stop ranging if active. */
    if (s_state.ranging_active) {
        vl53l0x_stop_ranging();
    }

    /* Remove I²C device handle. */
    if (s_state.dev_handle != NULL) {
        i2c_master_bus_rm_device(s_state.dev_handle);
        s_state.dev_handle = NULL;
    }

    s_state.initialised = false;
    ESP_LOGI(TAG, "De-initialised");
    return ESP_OK;
}

esp_err_t vl53l0x_start_ranging(void)
{
    if (!s_state.initialised) {
        ESP_LOGE(TAG, "Driver not initialised");
        return ESP_ERR_INVALID_STATE;
    }

    /* Configure interrupt for new sample ready. */
    esp_err_t err = reg_write(REG_SYSTEM_INTERRUPT_CONFIG_GPIO,
                              INTERRUPT_NEW_SAMPLE_READY);
    if (err != ESP_OK) { return err; }

    /* Clear any pending interrupt. */
    err = reg_write(REG_SYSTEM_INTERRUPT_CLEAR, RESULT_INTERRUPT_CLEAR);
    if (err != ESP_OK) { return err; }

    /* Start continuous ranging. */
    err = reg_write(REG_SYSRANGE_START, SYSRANGE_START_CONTINUOUS);
    if (err != ESP_OK) { return err; }

    s_state.ranging_active = true;
    ESP_LOGI(TAG, "Continuous ranging started");
    return ESP_OK;
}

esp_err_t vl53l0x_stop_ranging(void)
{
    if (!s_state.initialised) {
        ESP_LOGE(TAG, "Driver not initialised");
        return ESP_ERR_INVALID_STATE;
    }

    /* Stop ranging. */
    esp_err_t err = reg_write(REG_SYSRANGE_START, SYSRANGE_START_STOP);
    if (err != ESP_OK) { return err; }

    /* Clear any pending interrupt. */
    reg_write(REG_SYSTEM_INTERRUPT_CLEAR, RESULT_INTERRUPT_CLEAR);

    s_state.ranging_active = false;
    ESP_LOGI(TAG, "Ranging stopped");
    return ESP_OK;
}

esp_err_t vl53l0x_get_distance_mm(uint16_t *distance_mm)
{
    if (distance_mm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_state.initialised) {
        ESP_LOGE(TAG, "Driver not initialised");
        return ESP_ERR_INVALID_STATE;
    }

    *distance_mm = 0;

    /* ---- Trigger single ranging ---- */
    esp_err_t err = reg_write(REG_SYSRANGE_START, SYSRANGE_START_SINGLE);
    if (err != ESP_OK) {
        return err;
    }

    /* ---- Poll for completion ---- */
    uint32_t elapsed_ms = 0;
    while (elapsed_ms < s_state.timeout_ms) {
        uint8_t int_status = 0;
        err = reg_read(REG_RESULT_INTERRUPT_STATUS, &int_status, 1);
        if (err != ESP_OK) {
            return err;
        }
        /* Check if new sample is ready (bit 2 set). */
        if ((int_status & 0x07) != 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(RANGING_POLL_INTERVAL_MS));
        elapsed_ms += RANGING_POLL_INTERVAL_MS;
    }

    if (elapsed_ms >= s_state.timeout_ms) {
        ESP_LOGW(TAG, "Ranging timeout after %lu ms", s_state.timeout_ms);
        return ESP_ERR_TIMEOUT;
    }

    /* ---- Read result (4 bytes at REG_RESULT_RANGE_STATUS) ---- */
    uint8_t result_buf[4];
    err = reg_read(REG_RESULT_RANGE_STATUS, result_buf, sizeof(result_buf));
    if (err != ESP_OK) {
        return err;
    }

    /* ---- Clear interrupt ---- */
    reg_write(REG_SYSTEM_INTERRUPT_CLEAR, RESULT_INTERRUPT_CLEAR);

    /* ---- Parse distance ---- */
    /* Byte 0: range status (bits 4:0)
     * Byte 1: distance high byte
     * Byte 2: distance low byte
     * Byte 3: signal rate (not used here)
     *
     * Distance in mm = (byte1 << 8) | byte2
     */
    uint8_t range_status = result_buf[0] & 0x1F;
    if (range_status != 0) {
        ESP_LOGD(TAG, "Range status error: 0x%02X", range_status);
    }

    *distance_mm = ((uint16_t)result_buf[1] << 8) | result_buf[2];

    ESP_LOGD(TAG, "Distance: %u mm (status=0x%02X)", *distance_mm, range_status);
    return ESP_OK;
}

esp_err_t vl53l0x_set_range_mode(vl53l0x_range_mode_t mode)
{
    if (!s_state.initialised) {
        ESP_LOGE(TAG, "Driver not initialised");
        return ESP_ERR_INVALID_STATE;
    }

    s_state.range_mode = mode;

    /* Re-apply sensor configuration with new range mode. */
    esp_err_t err = apply_sensor_config();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply range mode %d: %s", mode, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Range mode set to %d", mode);
    return ESP_OK;
}

bool vl53l0x_is_present(void)
{
    uint8_t model_id = 0;
    esp_err_t err = reg_read(REG_IDENTIFICATION_MODEL_ID, &model_id, 1);
    if (err != ESP_OK) {
        return false;
    }
    ESP_LOGD(TAG, "Model ID = 0x%02X (expected 0x%02X)", model_id, MODEL_ID_VALUE);
    return (model_id == MODEL_ID_VALUE);
}
