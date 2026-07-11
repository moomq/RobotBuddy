/**
 * @file mpu6050.c
 * @brief MPU6050 6-axis IMU driver implementation for ESP32-S3 (ESP-IDF v5.x).
 *
 * Uses the ESP-IDF I²C master API to communicate with the MPU6050.
 * The I²C bus handle is expected to be provided by the BSP layer during
 * initialisation via i2c_master_bus_handle_t.
 *
 * @copyright 2026 RobotBuddy contributors
 * @licence MIT
 */

#include "mpu6050.h"

#include <math.h>
#include <string.h>

/* TODO(V1.1): Migrate to new I2C master API (driver/i2c_master.h).
 * The legacy driver/i2c.h API is functional but deprecated in ESP-IDF v5.x.
 * See: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/migration-guides/release-5.x/5.0/i2c.html */
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ------------------------------------------------------------------------ */
/*  Register map                                                            */
/* ------------------------------------------------------------------------ */

#define REG_SMPLRT_DIV   0x19  /**< Sample-rate divider                     */
#define REG_CONFIG       0x1A  /**< DLPF configuration                      */
#define REG_GYRO_CONFIG  0x1B  /**< Gyroscope full-scale range               */
#define REG_ACCEL_CONFIG 0x1C  /**< Accelerometer full-scale range           */
#define REG_PWR_MGMT_1  0x6B  /**< Power management 1                       */
#define REG_PWR_MGMT_2  0x6C  /**< Power management 2                       */
#define REG_WHO_AM_I    0x75  /**< Device identifier (reads 0x68)           */
#define REG_ACCEL_XOUT_H 0x3B /**< First byte of accel data (burst start)  */

/* ------------------------------------------------------------------------ */
/*  Register values                                                         */
/* ------------------------------------------------------------------------ */

#define WHO_AM_I_VALUE       0x68  /**< Expected WHO_AM_I reply             */
#define PWR_MGMT_1_RESET    0x80  /**< Device reset bit                     */
#define PWR_MGMT_1_SLEEP    0x40  /**< Sleep bit                            */
#define PWR_MGMT_1_CLK_PLL  0x01  /**< Clock source: PLL with X gyro ref    */
#define PWR_MGMT_2_NONE     0x00  /**< No axes in standby                   */

/* DLPF configuration: 94 Hz bandwidth, ~3 ms latency (register value 0x02) */
#define CONFIG_DLPF_94HZ     0x02

/* Default sample-rate divider: 1 kHz / (1 + 7) = 125 Hz */
#define SMPLRT_DIV_DEFAULT   0x07

/* ------------------------------------------------------------------------ */
/*  Constants                                                               */
/* ------------------------------------------------------------------------ */

static const char *TAG = "mpu6050";

/** @brief Burst-read length: 6 accel + 2 temp + 6 gyro = 14 bytes. */
#define BURST_READ_LEN 14

/** @brief I²C timeout in milliseconds. */
#define I2C_TIMEOUT_MS 100

/* ------------------------------------------------------------------------ */
/*  Module state                                                            */
/* ------------------------------------------------------------------------ */

typedef struct {
    i2c_port_t     i2c_port;
    uint8_t        addr;
    uint16_t       accel_range;   /**< Stored as ±N g  (2, 4, 8, 16)       */
    uint16_t       gyro_range;    /**< Stored as ±N °/s (250, 500, 1000, 2000) */
    float          accel_scale;   /**< LSB per g                              */
    float          gyro_scale;    /**< LSB per °/s                           */
    bool           initialised;
} mpu6050_state_t;

static mpu6050_state_t s_state = {0};

/* ------------------------------------------------------------------------ */
/*  Helpers                                                                 */
/* ------------------------------------------------------------------------ */

/**
 * @brief Write a single byte to an MPU6050 register.
 */
static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    esp_err_t err = i2c_master_write_to_device(
        s_state.i2c_port, s_state.addr, buf, sizeof(buf),
        pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C write failed reg=0x%02X val=0x%02X: %s",
                 reg, val, esp_err_to_name(err));
    }
    return err;
}

/**
 * @brief Read @p len bytes starting at register @p reg.
 */
static esp_err_t reg_read(uint8_t reg, uint8_t *buf, size_t len)
{
    esp_err_t err = i2c_master_write_read_device(
        s_state.i2c_port, s_state.addr,
        &reg, 1, buf, len,
        pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C read failed reg=0x%02X len=%u: %s",
                 reg, (unsigned)len, esp_err_to_name(err));
    }
    return err;
}

/**
 * @brief Derive the accelerometer sensitivity (LSB/g) from the range.
 */
static float accel_scale_from_range(uint16_t range_g)
{
    switch (range_g) {
        case  2: return 16384.0f;
        case  4: return  8192.0f;
        case  8: return  4096.0f;
        case 16: return  2048.0f;
        default:
            ESP_LOGW(TAG, "Unknown accel range %u, defaulting to ±2 g", range_g);
            return 16384.0f;
    }
}

/**
 * @brief Derive the gyroscope sensitivity (LSB/°/s) from the range.
 */
static float gyro_scale_from_range(uint16_t range_dps)
{
    switch (range_dps) {
        case  250: return 131.0f;
        case  500: return  65.5f;
        case 1000: return  32.8f;
        case 2000: return  16.4f;
        default:
            ESP_LOGW(TAG, "Unknown gyro range %u, defaulting to ±250 °/s", range_dps);
            return 131.0f;
    }
}

/**
 * @brief Map the desired accelerometer range to ACCEL_CONFIG register value.
 */
static uint8_t accel_config_val(uint16_t range_g)
{
    switch (range_g) {
        case  2: return 0x00;
        case  4: return 0x01;
        case  8: return 0x02;
        case 16: return 0x03;
        default: return 0x00;
    }
}

/**
 * @brief Map the desired gyroscope range to GYRO_CONFIG register value.
 */
static uint8_t gyro_config_val(uint16_t range_dps)
{
    switch (range_dps) {
        case  250: return 0x00;
        case  500: return 0x01;
        case 1000: return 0x02;
        case 2000: return 0x03;
        default:   return 0x00;
    }
}

/* ------------------------------------------------------------------------ */
/*  Public API                                                              */
/* ------------------------------------------------------------------------ */

esp_err_t mpu6050_init(const mpu6050_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Copy configuration. */
    s_state.i2c_port    = config->i2c_port;
    s_state.addr        = config->addr;
    s_state.accel_range = config->accel_range;
    s_state.gyro_range  = config->gyro_range;
    s_state.accel_scale = accel_scale_from_range(config->accel_range);
    s_state.gyro_scale  = gyro_scale_from_range(config->gyro_range);

    /* ---- Verify device presence ---- */
    if (!mpu6050_is_present()) {
        ESP_LOGE(TAG, "MPU6050 not found at addr 0x%02X", s_state.addr);
        return ESP_ERR_NOT_FOUND;
    }

    /* ---- Software reset ---- */
    esp_err_t err = reg_write(REG_PWR_MGMT_1, PWR_MGMT_1_RESET);
    if (err != ESP_OK) {
        return err;
    }
    /* Wait for reset to complete (datasheet: >100 ms). */
    vTaskDelay(pdMS_TO_TICKS(150));

    /* ---- Wake up with PLL clock source ---- */
    err = reg_write(REG_PWR_MGMT_1, PWR_MGMT_1_CLK_PLL);
    if (err != ESP_OK) {
        return err;
    }

    /* ---- Sample-rate divider ---- */
    err = reg_write(REG_SMPLRT_DIV, SMPLRT_DIV_DEFAULT);
    if (err != ESP_OK) {
        return err;
    }

    /* ---- DLPF configuration ---- */
    err = reg_write(REG_CONFIG, CONFIG_DLPF_94HZ);
    if (err != ESP_OK) {
        return err;
    }

    /* ---- Accelerometer full-scale range ---- */
    err = reg_write(REG_ACCEL_CONFIG, accel_config_val(config->accel_range));
    if (err != ESP_OK) {
        return err;
    }

    /* ---- Gyroscope full-scale range ---- */
    err = reg_write(REG_GYRO_CONFIG, gyro_config_val(config->gyro_range));
    if (err != ESP_OK) {
        return err;
    }

    /* ---- No axes in standby ---- */
    err = reg_write(REG_PWR_MGMT_2, PWR_MGMT_2_NONE);
    if (err != ESP_OK) {
        return err;
    }

    s_state.initialised = true;
    ESP_LOGI(TAG, "Initialised — addr=0x%02X accel=±%u g gyro=±%u °/s",
             s_state.addr, s_state.accel_range, s_state.gyro_range);
    return ESP_OK;
}

esp_err_t mpu6050_deinit(void)
{
    if (s_state.initialised) {
        mpu6050_sleep();
    }
    s_state.initialised = false;
    ESP_LOGI(TAG, "De-initialised");
    return ESP_OK;
}

esp_err_t mpu6050_read(mpu6050_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_state.initialised) {
        ESP_LOGE(TAG, "Driver not initialised");
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buf[BURST_READ_LEN];
    esp_err_t err = reg_read(REG_ACCEL_XOUT_H, buf, sizeof(buf));
    if (err != ESP_OK) {
        return err;
    }

    /* ---- Parse raw 16-bit signed values (big-endian) ---- */
    int16_t raw_ax = (int16_t)((buf[0]  << 8) | buf[1]);
    int16_t raw_ay = (int16_t)((buf[2]  << 8) | buf[3]);
    int16_t raw_az = (int16_t)((buf[4]  << 8) | buf[5]);
    int16_t raw_t  = (int16_t)((buf[6]  << 8) | buf[7]);
    int16_t raw_gx = (int16_t)((buf[8]  << 8) | buf[9]);
    int16_t raw_gy = (int16_t)((buf[10] << 8) | buf[11]);
    int16_t raw_gz = (int16_t)((buf[12] << 8) | buf[13]);

    /* ---- Convert to engineering units ---- */
    /* Accelerometer: raw / sensitivity * 9.80665  (m/s²) */
    data->accel_x = (float)raw_ax / s_state.accel_scale * 9.80665f;
    data->accel_y = (float)raw_ay / s_state.accel_scale * 9.80665f;
    data->accel_z = (float)raw_az / s_state.accel_scale * 9.80665f;

    /* Gyroscope: raw / sensitivity  (°/s) */
    data->gyro_x = (float)raw_gx / s_state.gyro_scale;
    data->gyro_y = (float)raw_gy / s_state.gyro_scale;
    data->gyro_z = (float)raw_gz / s_state.gyro_scale;

    /* Temperature: (raw / 340.0) + 36.53  (°C), per datasheet. */
    data->temperature = ((float)raw_t / 340.0f) + 36.53f;

    return ESP_OK;
}

esp_err_t mpu6050_sleep(void)
{
    if (!s_state.initialised) {
        ESP_LOGW(TAG, "Sleep requested but driver not initialised");
        return ESP_ERR_INVALID_STATE;
    }
    /* Set SLEEP bit, keeping current clock source. */
    esp_err_t err = reg_write(REG_PWR_MGMT_1, PWR_MGMT_1_SLEEP | PWR_MGMT_1_CLK_PLL);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Entering sleep mode");
    }
    return err;
}

esp_err_t mpu6050_wakeup(void)
{
    if (!s_state.initialised) {
        ESP_LOGW(TAG, "Wakeup requested but driver not initialised");
        return ESP_ERR_INVALID_STATE;
    }
    /* Clear SLEEP bit, keep PLL clock source. */
    esp_err_t err = reg_write(REG_PWR_MGMT_1, PWR_MGMT_1_CLK_PLL);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Woke up from sleep");
        /* Datasheet: sensor data stable after ~6 ms. */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return err;
}

bool mpu6050_is_present(void)
{
    uint8_t id = 0;
    esp_err_t err = reg_read(REG_WHO_AM_I, &id, 1);
    if (err != ESP_OK) {
        return false;
    }
    ESP_LOGD(TAG, "WHO_AM_I = 0x%02X (expected 0x%02X)", id, WHO_AM_I_VALUE);
    return (id == WHO_AM_I_VALUE);
}