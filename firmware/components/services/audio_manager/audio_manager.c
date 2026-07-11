/*
 * RobotBuddy — Audio Manager Implementation
 * ============================================
 * I2S audio capture and playback with ring buffers.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "audio_manager.h"
#include "bsp_pinmap.h"
#include "event_bus.h"
#include "robot_events.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "driver/i2s_std.h"
#include <string.h>

static const char *TAG = "audio_mgr";

/* ============================================================
 * Module State
 * ============================================================ */

static bool s_initialized = false;
static audio_state_t s_state = AUDIO_STATE_IDLE;
static audio_config_t s_config;

/* I2S channel handles (from BSP) */
static i2s_chan_handle_t s_rx_handle = NULL;
static i2s_chan_handle_t s_tx_handle = NULL;

/* Ring buffers for audio data */
static RingbufHandle_t s_capture_buf = NULL;
static RingbufHandle_t s_playback_buf = NULL;

/* Tasks */
static TaskHandle_t s_capture_task = NULL;
static TaskHandle_t s_playback_task = NULL;

/* Capture buffer (PSRAM) */
static int16_t *s_capture_dma_buf = NULL;
static size_t s_capture_dma_buf_size = 4096;

/* Playback buffer (PSRAM) */
static int16_t *s_playback_dma_buf = NULL;
static size_t s_playback_dma_buf_size = 4096;

/* ============================================================
 * I2S Initialization
 * ============================================================ */

static esp_err_t init_i2s_channels(void)
{
    /* RX channel (Microphone INMP441) */
    i2s_chan_config_t rx_chan_conf = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    rx_chan_conf.dma_desc_num = 6;
    rx_chan_conf.dma_frame_num = 240;
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_conf, &s_rx_handle, NULL));

    i2s_std_config_t rx_std_conf = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
        },
        .gpio_cfg = {
            .mclk = GPIO_NUM_NC,
            .bclk = BSP_PIN_I2S_BCLK,
            .ws = BSP_PIN_I2S_WS,
            .dout = GPIO_NUM_NC,
            .din = BSP_PIN_I2S_DIN,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx_handle, &rx_std_conf));

    /* TX channel (Amplifier MAX98357A) */
    i2s_chan_config_t tx_chan_conf = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    tx_chan_conf.dma_desc_num = 6;
    tx_chan_conf.dma_frame_num = 240;
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_conf, NULL, &s_tx_handle));

    i2s_std_config_t tx_std_conf = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
        },
        .gpio_cfg = {
            .mclk = GPIO_NUM_NC,
            .bclk = BSP_PIN_I2S_BCLK,
            .ws = BSP_PIN_I2S_WS,
            .dout = BSP_PIN_I2S_DOUT,
            .din = GPIO_NUM_NC,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx_handle, &tx_std_conf));

    ESP_LOGI(TAG, "I2S channels initialized (RX: INMP441, TX: MAX98357A)");
    return ESP_OK;
}

/* ============================================================
 * Audio Capture Task
 * ============================================================ */

static void audio_capture_task_fn(void *arg)
{
    ESP_LOGI(TAG, "Audio capture task started");

    size_t bytes_read = 0;

    while (1) {
        if (s_state != AUDIO_STATE_CAPTURING) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        /* Read from I2S RX */
        esp_err_t ret = i2s_channel_read(s_rx_handle, s_capture_dma_buf,
                                          s_capture_dma_buf_size, &bytes_read,
                                          pdMS_TO_TICKS(100));

        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "I2S read error: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (bytes_read > 0) {
            /* Send to capture ring buffer */
            BaseType_t rb_ret = xRingbufferSend(s_capture_buf, s_capture_dma_buf,
                                                 bytes_read, pdMS_TO_TICKS(10));
            if (rb_ret != pdTRUE) {
                ESP_LOGW(TAG, "Capture buffer full, dropping %zu bytes", bytes_read);
            }
        }

        esp_task_wdt_reset();
    }
}

/* ============================================================
 * Audio Playback Task
 * ============================================================ */

static void audio_playback_task_fn(void *arg)
{
    ESP_LOGI(TAG, "Audio playback task started");

    size_t bytes_written = 0;

    while (1) {
        if (s_state != AUDIO_STATE_PLAYING) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        /* Receive from playback ring buffer */
        size_t item_size = 0;
        char *data = (char *)xRingbufferReceiveUpTo(s_playback_buf, &item_size,
                                                      pdMS_TO_TICKS(100), s_playback_dma_buf_size);

        if (data != NULL && item_size > 0) {
            /* Write to I2S TX */
            esp_err_t ret = i2s_channel_write(s_tx_handle, data, item_size,
                                                &bytes_written, pdMS_TO_TICKS(100));
            vRingbufferReturnItem(s_playback_buf, data);

            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "I2S write error: %s", esp_err_to_name(ret));
            }
        }

        esp_task_wdt_reset();
    }
}

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t audio_manager_init(const audio_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Audio manager already initialized");
        return ESP_OK;
    }

    /* Apply config */
    if (config != NULL) {
        s_config = *config;
    } else {
        s_config.sample_rate = AUDIO_SAMPLE_RATE;
        s_config.bits_per_sample = AUDIO_BITS_PER_SAMPLE;
        s_config.channels = AUDIO_CHANNELS;
        s_config.capture_buf_size = AUDIO_CAPTURE_BUF_SIZE;
        s_config.playback_buf_size = AUDIO_PLAYBACK_BUF_SIZE;
    }

    /* Initialize I2S channels */
    esp_err_t ret = init_i2s_channels();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Allocate DMA buffers in PSRAM */
    s_capture_dma_buf = (int16_t *)heap_caps_malloc(s_capture_dma_buf_size, MALLOC_CAP_SPIRAM);
    s_playback_dma_buf = (int16_t *)heap_caps_malloc(s_playback_dma_buf_size, MALLOC_CAP_SPIRAM);
    if (s_capture_dma_buf == NULL || s_playback_dma_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate DMA buffers");
        if (s_capture_dma_buf != NULL) {
            heap_caps_free(s_capture_dma_buf);
            s_capture_dma_buf = NULL;
        }
        if (s_playback_dma_buf != NULL) {
            heap_caps_free(s_playback_dma_buf);
            s_playback_dma_buf = NULL;
        }
        return ESP_ERR_NO_MEM;
    }

    /* Create ring buffers */
    s_capture_buf = xRingbufferCreate(s_config.capture_buf_size, RINGBUF_TYPE_BYTEBUF);
    s_playback_buf = xRingbufferCreate(s_config.playback_buf_size, RINGBUF_TYPE_BYTEBUF);
    if (s_capture_buf == NULL || s_playback_buf == NULL) {
        ESP_LOGE(TAG, "Failed to create ring buffers");
        return ESP_ERR_NO_MEM;
    }

    /* Create audio tasks (but don't start I2S channels yet) */
    BaseType_t task_ret;

    task_ret = xTaskCreatePinnedToCore(
        audio_capture_task_fn, "audio_cap",
        AUDIO_TASK_STACK_SIZE, NULL,
        AUDIO_CAPTURE_PRIORITY, &s_capture_task,
        AUDIO_TASK_CORE_ID
    );
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create capture task");
        return ESP_ERR_NO_MEM;
    }

    task_ret = xTaskCreatePinnedToCore(
        audio_playback_task_fn, "audio_play",
        AUDIO_TASK_STACK_SIZE, NULL,
        AUDIO_PLAYBACK_PRIORITY, &s_playback_task,
        AUDIO_TASK_CORE_ID
    );
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create playback task");
        vTaskDelete(s_capture_task);
        return ESP_ERR_NO_MEM;
    }

    s_state = AUDIO_STATE_IDLE;
    s_initialized = true;

    ESP_LOGI(TAG, "Audio manager initialized (%dHz, %dbit, %dch)",
             s_config.sample_rate, s_config.bits_per_sample, s_config.channels);
    return ESP_OK;
}

esp_err_t audio_manager_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    /* Stop any active I2S channels */
    i2s_channel_disable(s_rx_handle);
    i2s_channel_disable(s_tx_handle);

    /* Delete tasks */
    if (s_capture_task != NULL) {
        vTaskDelete(s_capture_task);
        s_capture_task = NULL;
    }
    if (s_playback_task != NULL) {
        vTaskDelete(s_playback_task);
        s_playback_task = NULL;
    }

    /* Free buffers */
    if (s_capture_buf != NULL) {
        vRingbufferDelete(s_capture_buf);
        s_capture_buf = NULL;
    }
    if (s_playback_buf != NULL) {
        vRingbufferDelete(s_playback_buf);
        s_playback_buf = NULL;
    }
    if (s_capture_dma_buf != NULL) {
        heap_caps_free(s_capture_dma_buf);
        s_capture_dma_buf = NULL;
    }
    if (s_playback_dma_buf != NULL) {
        heap_caps_free(s_playback_dma_buf);
        s_playback_dma_buf = NULL;
    }

    /* Delete I2S channels */
    i2s_del_channel(s_rx_handle);
    i2s_del_channel(s_tx_handle);

    s_initialized = false;
    ESP_LOGI(TAG, "Audio manager deinitialized");
    return ESP_OK;
}

esp_err_t audio_capture_start(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = i2s_channel_enable(s_rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S RX: %s", esp_err_to_name(ret));
        return ret;
    }

    s_state = AUDIO_STATE_CAPTURING;

    /* Publish event */
    robot_event_t event = { .id = EVENT_AUDIO_CAPTURE_START, .payload = NULL, .payload_len = 0 };
    event_bus_publish(&event);

    ESP_LOGI(TAG, "Audio capture started");
    return ESP_OK;
}

esp_err_t audio_capture_stop(void)
{
    if (!s_initialized || s_state != AUDIO_STATE_CAPTURING) {
        return ESP_ERR_INVALID_STATE;
    }

    i2s_channel_disable(s_rx_handle);
    s_state = AUDIO_STATE_IDLE;

    robot_event_t event = { .id = EVENT_AUDIO_CAPTURE_STOP, .payload = NULL, .payload_len = 0 };
    event_bus_publish(&event);

    ESP_LOGI(TAG, "Audio capture stopped");
    return ESP_OK;
}

size_t audio_capture_read(void *buf, size_t len, uint32_t timeout_ms)
{
    if (!s_initialized || s_capture_buf == NULL || buf == NULL) {
        return 0;
    }

    size_t item_size = 0;
    char *data = (char *)xRingbufferReceiveUpTo(s_capture_buf, &item_size,
                                                   pdMS_TO_TICKS(timeout_ms), len);
    if (data != NULL && item_size > 0) {
        size_t copy_len = (item_size < len) ? item_size : len;
        memcpy(buf, data, copy_len);
        vRingbufferReturnItem(s_capture_buf, data);
        return copy_len;
    }
    return 0;
}

esp_err_t audio_play_start(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = i2s_channel_enable(s_tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S TX: %s", esp_err_to_name(ret));
        return ret;
    }

    s_state = AUDIO_STATE_PLAYING;

    robot_event_t event = { .id = EVENT_AUDIO_PLAY_START, .payload = NULL, .payload_len = 0 };
    event_bus_publish(&event);

    ESP_LOGI(TAG, "Audio playback started");
    return ESP_OK;
}

esp_err_t audio_play_stop(void)
{
    if (!s_initialized || s_state != AUDIO_STATE_PLAYING) {
        return ESP_ERR_INVALID_STATE;
    }

    i2s_channel_disable(s_tx_handle);
    s_state = AUDIO_STATE_IDLE;

    robot_event_t event = { .id = EVENT_AUDIO_PLAY_STOP, .payload = NULL, .payload_len = 0 };
    event_bus_publish(&event);

    ESP_LOGI(TAG, "Audio playback stopped");
    return ESP_OK;
}

esp_err_t audio_play_data(const void *data, size_t len)
{
    if (!s_initialized || s_playback_buf == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    BaseType_t ret = xRingbufferSend(s_playback_buf, data, len, pdMS_TO_TICKS(100));
    if (ret != pdTRUE) {
        ESP_LOGW(TAG, "Playback buffer full, dropping %zu bytes", len);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t audio_play_tone(uint16_t freq, uint16_t duration_ms)
{
    /* TODO: Implement tone generation using sine wave */
    /* For MVP, this plays a simple 440Hz tone */
    ESP_LOGI(TAG, "Tone: %d Hz for %d ms (not yet implemented)", freq, duration_ms);
    (void)freq;
    (void)duration_ms;
    return ESP_OK;
}

audio_state_t audio_get_state(void)
{
    return s_state;
}

bool audio_is_capturing(void)
{
    return s_state == AUDIO_STATE_CAPTURING;
}

bool audio_is_playing(void)
{
    return s_state == AUDIO_STATE_PLAYING;
}