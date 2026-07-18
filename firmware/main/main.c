/*
 * RobotBuddy — Desktop AI Coding Assistant Robot
 * ================================================
 * ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM)
 *
 * Entry point: app_main()
 * Initializes all subsystems and starts FreeRTOS tasks.
 *
 * V1.0 MVP — Full system integration
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_task_wdt.h"

/* BSP */
#include "bsp_board.h"
#include "bsp_pinmap.h"

/* Framework */
#include "event_bus.h"
#include "robot_events.h"
#include "sysmon.h"

/* Drivers */
#include "st7789.h"
#include "drv8833.h"
#include "mpu6050.h"
#include "ir_sensor.h"
#include "battery.h"

/* Services */
#include "wifi_manager.h"
#include "display_manager.h"
#include "emotion_engine.h"
#include "audio_manager.h"
#include "cloud_manager.h"
#include "motion_manager.h"
#include "sensor_manager.h"
#include "battery_monitor.h"

/* Services — V2.0 */
#include "text_display.h"
#include "mqtt_client.h"
#include "web_server.h"
#include "ota_service.h"
#include "wake_word.h"
#include "power_manager.h"

/* Application */
#include "behavior_system.h"
#include "ai_dialog.h"

/* Application — V2.0 */
#include "pomodoro.h"

static const char *TAG = "RobotBuddy";

/* Firmware version — updated by /release command */
#define FIRMWARE_VERSION "0.3.0-v2"
#define HARDWARE_VERSION "V1.0"

/* V2.0 Configuration defaults (overridable via menuconfig) */
#ifndef CONFIG_MQTT_BROKER_URL
#define CONFIG_MQTT_BROKER_URL "mqtt://broker.emqx.io:1883"
#endif

#ifndef CONFIG_MQTT_USERNAME
#define CONFIG_MQTT_USERNAME ""
#endif

#ifndef CONFIG_MQTT_PASSWORD
#define CONFIG_MQTT_PASSWORD ""
#endif

#ifndef CONFIG_DEVICE_ID
#define CONFIG_DEVICE_ID "rb-001"
#endif

/* ============================================================
 * Display Task — renders emotion engine at 30 FPS
 * ============================================================ */
static void display_task(void *arg)
{
    ESP_LOGI(TAG, "Display task started (Core %d)", xPortGetCoreID());

    while (1) {
        uint16_t *fb = display_get_framebuffer();
        if (fb != NULL) {
            emotion_render_frame(fb, DISPLAY_WIDTH, DISPLAY_HEIGHT);
            text_display_render(fb, DISPLAY_WIDTH, DISPLAY_HEIGHT);
            display_commit_frame();
        }
        vTaskDelay(pdMS_TO_TICKS(33)); /* ~30 FPS */
        esp_task_wdt_reset();
    }
}

/* ============================================================
 * System Startup — Phase-based initialization
 * ============================================================ */

static void print_system_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  RobotBuddy — Desktop AI Coding Buddy");
    ESP_LOGI(TAG, "  Firmware: %s  Hardware: %s", FIRMWARE_VERSION, HARDWARE_VERSION);
    ESP_LOGI(TAG, "========================================");

    ESP_LOGI(TAG, "Chip: %s with %d CPU core(s), WiFi%s%s",
             CONFIG_IDF_TARGET,
             chip_info.cores,
             (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "" : "",
             (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "");

    uint32_t flash_size;
    esp_flash_get_size(NULL, &flash_size);
    ESP_LOGI(TAG, "Flash: %luMB %s", flash_size / (1024 * 1024),
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    ESP_LOGI(TAG, "PSRAM: %s", CONFIG_SPIRAM_MODE_OCT ? "8MB OCTAL" : "disabled");
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Free PSRAM: %lu bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "========================================");
}

/* ============================================================
 * Phase 1: Critical infrastructure
 * ============================================================ */
static esp_err_t init_phase1_infrastructure(void)
{
    ESP_LOGI(TAG, "=== Phase 1: Infrastructure ===");

    /* NVS (required for WiFi, OTA, calibration) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "  [OK] NVS initialized");

    /* Event bus */
    ESP_ERROR_CHECK(event_bus_init());
    ESP_LOGI(TAG, "  [OK] Event bus initialized");

    /* System monitor */
    ESP_ERROR_CHECK(sysmon_init());
    ESP_LOGI(TAG, "  [OK] System monitor initialized");

    return ESP_OK;
}

/* ============================================================
 * Phase 2: Board and drivers
 * ============================================================ */
static esp_err_t init_phase2_board(void)
{
    ESP_LOGI(TAG, "=== Phase 2: Board & Drivers ===");

    /* BSP (GPIO, SPI, I2C, I2S, PWM, ADC) */
    ESP_ERROR_CHECK(bsp_board_init());
    ESP_LOGI(TAG, "  [OK] BSP initialized");

    /* System monitor — register all tasks */
    ESP_LOGI(TAG, "  Free heap after BSP: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "  Free PSRAM after BSP: %lu bytes",
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    return ESP_OK;
}

/* ============================================================
 * Phase 3: Services
 * ============================================================ */
static esp_err_t init_phase3_services(void)
{
    ESP_LOGI(TAG, "=== Phase 3: Services ===");

    /* WiFi Manager */
    esp_err_t ret = wifi_manager_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] WiFi manager initialized");
        wifi_manager_start();
    } else {
        ESP_LOGW(TAG, "  [WARN] WiFi manager init failed (will retry): %s", esp_err_to_name(ret));
    }

    /* Display Manager + Emotion Engine */
    display_config_t display_cfg = {
        .width = 240,
        .height = 240,
        .target_fps = 30,
    };
    ret = display_manager_init(&display_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] Display manager initialized");
    } else {
        ESP_LOGE(TAG, "  [FAIL] Display manager init failed: %s", esp_err_to_name(ret));
    }

    emotion_engine_config_t emotion_cfg = {
        .display_width = 240,
        .display_height = 240,
        .left_eye_cx = 80,
        .left_eye_cy = 120,
        .right_eye_cx = 160,
        .right_eye_cy = 120,
        .eye_radius = 35,
    };
    ret = emotion_engine_init(&emotion_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] Emotion engine initialized");
        emotion_set_state(EMOTION_IDLE);  /* Start with IDLE expression */
    } else {
        ESP_LOGE(TAG, "  [FAIL] Emotion engine init failed: %s", esp_err_to_name(ret));
    }

    /* Audio Manager */
    audio_config_t audio_cfg = {
        .sample_rate = 16000,
        .bits_per_sample = 16,
        .channels = 1,
        .capture_buf_size = 32 * 1024,
        .playback_buf_size = 64 * 1024,
    };
    ret = audio_manager_init(&audio_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] Audio manager initialized");
    } else {
        ESP_LOGW(TAG, "  [WARN] Audio manager init failed: %s", esp_err_to_name(ret));
    }

    /* Motion Manager */
    motion_config_t motion_cfg = {
        .pin_ain1 = BSP_PIN_MOTOR_AIN1,
        .pin_ain2 = BSP_PIN_MOTOR_AIN2,
        .pin_bin1 = BSP_PIN_MOTOR_BIN1,
        .pin_bin2 = BSP_PIN_MOTOR_BIN2,
        .pwm_freq = BSP_MOTOR_PWM_FREQ_HZ,
        .max_speed = 255,
        .default_speed = 150,
    };
    ret = motion_manager_init(&motion_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] Motion manager initialized");
    } else {
        ESP_LOGW(TAG, "  [WARN] Motion manager init failed: %s", esp_err_to_name(ret));
    }

    /* Sensor Manager */
    sensor_config_t sensor_cfg = {
        .pin_obstacle_left = BSP_PIN_IR_OBSTACLE_L,
        .pin_obstacle_right = BSP_PIN_IR_OBSTACLE_R,
        .pin_edge_left = BSP_PIN_IR_EDGE_L,
        .pin_edge_right = BSP_PIN_IR_EDGE_R,
        .i2c_port = BSP_I2C_NUM,
        .mpu6050_addr = BSP_I2C_ADDR_MPU6050,
        .adc_pin = BSP_PIN_VBAT_ADC,
        .battery_divider_ratio = BSP_BATTERY_DIVIDER_RATIO,
    };
    ret = sensor_manager_init(&sensor_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] Sensor manager initialized");
    } else {
        ESP_LOGW(TAG, "  [WARN] Sensor manager init failed: %s", esp_err_to_name(ret));
    }

    /* Battery Monitor */
    ret = battery_monitor_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] Battery monitor initialized");
    } else {
        ESP_LOGW(TAG, "  [WARN] Battery monitor init failed: %s", esp_err_to_name(ret));
    }

    return ESP_OK;
}

/* ============================================================
 * Phase 3b: V2.0 Services (new modules)
 * ============================================================ */
static esp_err_t init_phase3b_v2_services(void)
{
    ESP_LOGI(TAG, "=== Phase 3b: V2.0 Services ===");

    /* Text Display (scrolling messages + status icons) */
    text_display_config_t text_cfg = {
        .y_start = 180,
        .height = 60,
        .scroll_speed = 2,
        .display_timeout_ms = 10000,
        .max_messages = 5,
    };
    esp_err_t ret = text_display_init(&text_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] Text display initialized");
    } else {
        ESP_LOGW(TAG, "  [WARN] Text display init failed: %s", esp_err_to_name(ret));
    }

    /* Wake Word Detection (ESP-SR) */
    wake_word_config_t wake_cfg = {
        .model_name = "hilexin",
        .detection_threshold = 0.5f,
        .channel = 0,
    };
    ret = wake_word_init(&wake_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] Wake word initialized");
    } else {
        ESP_LOGW(TAG, "  [WARN] Wake word init failed: %s", esp_err_to_name(ret));
    }

    /* MQTT Client */
    mqtt_config_t mqtt_cfg = {
        .broker_url = CONFIG_MQTT_BROKER_URL,
        .client_id = "robotbuddy-" CONFIG_DEVICE_ID,
        .username = CONFIG_MQTT_USERNAME,
        .password = CONFIG_MQTT_PASSWORD,
        .keepalive_sec = 60,
        .qos = 1,
        .device_id = CONFIG_DEVICE_ID,
    };
    ret = mqtt_client_init(&mqtt_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] MQTT client initialized");
    } else {
        ESP_LOGW(TAG, "  [WARN] MQTT client init failed: %s", esp_err_to_name(ret));
    }

    /* Web Server (HTTP console) */
    web_server_config_t web_cfg = {
        .port = 80,
        .max_connections = 4,
        .auth_enabled = false,
        .username = "",
        .password = "",
    };
    ret = web_server_init(&web_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] Web server initialized");
    } else {
        ESP_LOGW(TAG, "  [WARN] Web server init failed: %s", esp_err_to_name(ret));
    }

    /* OTA Service */
    ret = ota_service_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] OTA service initialized");
    } else {
        ESP_LOGW(TAG, "  [WARN] OTA service init failed: %s", esp_err_to_name(ret));
    }

    /* Power Manager */
    power_config_t power_cfg = {
        .dim_timeout_ms = 300000,        /* 5 minutes */
        .light_sleep_timeout_ms = 600000, /* 10 minutes */
        .deep_sleep_timeout_ms = 1800000, /* 30 minutes */
        .dim_brightness = 32,
        .active_brightness = 128,
    };
    ret = power_manager_init(&power_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] Power manager initialized");
    } else {
        ESP_LOGW(TAG, "  [WARN] Power manager init failed: %s", esp_err_to_name(ret));
    }

    return ESP_OK;
}

/* ============================================================
 * Phase 4: Application layer
 * ============================================================ */
static esp_err_t init_phase4_application(void)
{
    ESP_LOGI(TAG, "=== Phase 4: Application ===");

    /* Behavior System */
    esp_err_t ret = behavior_system_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] Behavior system initialized");
    } else {
        ESP_LOGE(TAG, "  [FAIL] Behavior system init failed: %s", esp_err_to_name(ret));
    }

    /* AI Dialog (requires WiFi — may initialize later) */
    ai_dialog_config_t dialog_cfg = {
        .provider = CLOUD_PROVIDER_CLAUDE,
        .max_rounds = AI_DIALOG_MAX_ROUNDS,
        .asr_timeout_ms = AI_DIALOG_ASR_TIMEOUT_MS,
        .llm_timeout_ms = AI_DIALOG_LLM_TIMEOUT_MS,
    };
    ret = ai_dialog_init(&dialog_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] AI dialog initialized");
    } else {
        ESP_LOGW(TAG, "  [WARN] AI dialog init failed (WiFi may not be connected yet): %s",
                 esp_err_to_name(ret));
    }

    /* Pomodoro Timer — V2.0 */
    pomodoro_config_t pomo_cfg = {
        .work_duration_min = 25,
        .break_duration_min = 5,
        .max_rounds = 4,
    };
    ret = pomodoro_init(&pomo_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  [OK] Pomodoro initialized");
    } else {
        ESP_LOGW(TAG, "  [WARN] Pomodoro init failed: %s", esp_err_to_name(ret));
    }

    return ESP_OK;
}

/* ============================================================
 * Phase 5: Start tasks
 * ============================================================ */
static void init_phase5_start_tasks(void)
{
    ESP_LOGI(TAG, "=== Phase 5: Starting Tasks ===");

    TaskHandle_t h_display = NULL;

    /* Display task (Core 1, Priority 6) */
    xTaskCreatePinnedToCore(
        display_task,
        "display",
        4096,
        NULL,
        6,
        &h_display,
        1  /* Core 1 */
    );

    /* Register tasks with system monitor */
    if (h_display != NULL) {
        sysmon_register_task("display", h_display);
    }

    /* Start V2.0 services (after WiFi is potentially connected) */
    if (wifi_manager_is_connected()) {
        /* Start MQTT client */
        esp_err_t ret = mqtt_client_start();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  MQTT client started");
        }

        /* Start Web server */
        ret = web_server_start();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  Web server started on port 80");
        }
    } else {
        ESP_LOGW(TAG, "  WiFi not connected — MQTT and Web server will start when WiFi connects");
    }

    /* Start wake word detection */
    wake_word_start();
    ESP_LOGI(TAG, "  Wake word detection started");

    /* Note: Other tasks (audio_capture, audio_playback, cloud, etc.)
     * are created internally by their respective managers.
     * The behavior_task, sensor_task, and motion_task are also
     * created by their manager init functions. */

    ESP_LOGI(TAG, "All tasks started");
}

/* ============================================================
 * Application Entry Point
 * ============================================================ */
void app_main(void)
{
    ESP_LOGI(TAG, "Starting RobotBuddy firmware v%s", FIRMWARE_VERSION);

    /* Phase 1: Critical infrastructure */
    init_phase1_infrastructure();

    /* Phase 2: Board & drivers */
    init_phase2_board();

    /* Phase 3: Services */
    init_phase3_services();

    /* Phase 3b: V2.0 Services (new modules) */
    init_phase3b_v2_services();

    /* Phase 4: Application */
    init_phase4_application();

    /* Phase 5: Start tasks */
    init_phase5_start_tasks();

    /* Final status report */
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  RobotBuddy is running!");
    ESP_LOGI(TAG, "  Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "  Free PSRAM: %lu bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "========================================");

    /* Publish boot complete event */
    robot_event_t boot_event = {
        .id = EVENT_SYS_BOOT_COMPLETE,
        .timestamp = 0,
        .payload = NULL,
        .payload_len = 0,
    };
    event_bus_publish(&boot_event);

    /* Main loop — system health monitoring */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        /* Check battery and power mode */
        power_mode_t power_mode = battery_monitor_get_power_mode();
        if (power_mode == POWER_MODE_DEEP_SLEEP) {
            ESP_LOGW(TAG, "Critical battery! Entering deep sleep in 1 second...");

            /* Publish a critical battery event before sleeping */
            robot_event_t crit_event = {
                .id = EVENT_SYS_CRITICAL_BATTERY,
                .timestamp = 0,
                .payload = NULL,
                .payload_len = 0,
            };
            event_bus_publish(&crit_event);

            vTaskDelay(pdMS_TO_TICKS(1000));

            /* Configure wake sources:
             * 1. Timer wake: check battery status every 60 seconds
             * 2. GPIO wake: button press (if configured) */
            esp_sleep_enable_timer_wakeup(60ULL * 1000000);  /* 60 seconds */

            esp_deep_sleep_start();
        }
    }
}