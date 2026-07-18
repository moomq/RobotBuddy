/*
 * RobotBuddy — Power Manager Implementation
 * =============================================
 * Dynamic power management with automatic sleep states.
 * Monitors user activity via event bus and transitions
 * through: ACTIVE -> DISPLAY_DIM -> WIFI_LIGHT_SLEEP -> DEEP_SLEEP
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "power_manager.h"
#include "display_manager.h"
#include "cloud_manager.h"
#include "event_bus.h"
#include "robot_events.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "power_mgr";

/* ============================================================
 * Module State
 * ============================================================ */

static bool s_initialized = false;
static power_state_t s_state = POWER_STATE_ACTIVE;
static int64_t s_last_activity_us = 0;       /* Last activity timestamp (us) */
static power_config_t s_config;
static TaskHandle_t s_check_task = NULL;
static SemaphoreHandle_t s_mutex = NULL;

/* ============================================================
 * Defaults
 * ============================================================ */

static const power_config_t s_default_config = {
    .dim_timeout_ms          = POWER_MANAGER_DEFAULT_DIM_TIMEOUT_MS,
    .light_sleep_timeout_ms  = POWER_MANAGER_DEFAULT_LIGHT_SLEEP_TIMEOUT_MS,
    .deep_sleep_timeout_ms   = POWER_MANAGER_DEFAULT_DEEP_SLEEP_TIMEOUT_MS,
    .dim_brightness          = POWER_MANAGER_DEFAULT_DIM_BRIGHTNESS,
    .active_brightness       = POWER_MANAGER_DEFAULT_ACTIVE_BRIGHTNESS,
};

/* ============================================================
 * Forward Declarations
 * ============================================================ */

static void power_check_task(void *arg);
static void activity_event_handler(const robot_event_t *event);
static void critical_battery_handler(const robot_event_t *event);
static esp_err_t transition_to(power_state_t new_state);
static void publish_power_event(robot_event_id_t event_id, power_state_t state);
static void subscribe_activity_events(void);
static void unsubscribe_activity_events(void);

/* ============================================================
 * Internal Helpers
 * ============================================================ */

/**
 * @brief Get current idle time in milliseconds
 */
static uint32_t get_idle_ms_internal(void)
{
    int64_t now = esp_timer_get_time();
    int64_t elapsed = now - s_last_activity_us;
    return (elapsed > 0) ? (uint32_t)(elapsed / 1000) : 0;
}

/**
 * @brief Publish a power event via the event bus
 */
static void publish_power_event(robot_event_id_t event_id, power_state_t state)
{
    power_event_t payload = {
        .state   = (uint8_t)state,
        .idle_ms = get_idle_ms_internal(),
    };

    robot_event_t event = {
        .id          = event_id,
        .timestamp   = (uint32_t)(esp_timer_get_time() / 1000),
        .payload     = &payload,
        .payload_len = sizeof(payload),
    };

    esp_err_t ret = event_bus_publish(&event);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to publish event 0x%04X: %s", event_id, esp_err_to_name(ret));
    }
}

/**
 * @brief Perform entry actions for DISPLAY_DIM state
 */
static void enter_dim(void)
{
    ESP_LOGI(TAG, "Entering DISPLAY_DIM (backlight -> %d)", s_config.dim_brightness);
    display_set_backlight(s_config.dim_brightness);
}

/**
 * @brief Perform entry actions for WIFI_LIGHT_SLEEP state
 */
static void enter_light_sleep(void)
{
    ESP_LOGI(TAG, "Entering WIFI_LIGHT_SLEEP (WiFi PS enabled)");

    /* Enable WiFi power save mode */
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    /* Stop cloud manager (MQTT client + web server) */
    cloud_manager_deinit();

    publish_power_event(EVENT_POWER_STATE_CHANGE, POWER_STATE_WIFI_LIGHT_SLEEP);
}

/**
 * @brief Perform entry actions for DEEP_SLEEP state
 */
static void enter_deep_sleep(void)
{
    ESP_LOGI(TAG, "Entering DEEP_SLEEP (wake in %ds + touch)", POWER_MANAGER_DEEP_SLEEP_WAKEUP_TIMER_SEC);

    publish_power_event(EVENT_POWER_ENTER_SLEEP, POWER_STATE_DEEP_SLEEP);

    /* Configure wake sources: timer + touch GPIO (GPIO0 = BOOT button as touch wake) */
    esp_sleep_enable_timer_wakeup((uint64_t)POWER_MANAGER_DEEP_SLEEP_WAKEUP_TIMER_SEC * 1000000ULL);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);  /* BOOT button (LOW = pressed) */

    /* Enter deep sleep — does not return */
    esp_deep_sleep_start();
}

/**
 * @brief Perform entry actions for ACTIVE state (wake up from lower power)
 */
static void enter_active(void)
{
    ESP_LOGI(TAG, "Entering ACTIVE (backlight -> %d)", s_config.active_brightness);

    /* Restore display backlight */
    display_set_backlight(s_config.active_brightness);

    /* Disable WiFi power save */
    esp_wifi_set_ps(WIFI_PS_NONE);

    /* Restart cloud manager if not running */
    cloud_manager_init(NULL);

    publish_power_event(EVENT_POWER_STATE_CHANGE, POWER_STATE_ACTIVE);
}

/**
 * @brief Transition to a new power state with proper entry actions
 */
static esp_err_t transition_to(power_state_t new_state)
{
    if (s_state == new_state) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "State transition: %d -> %d", s_state, new_state);

    power_state_t old_state = s_state;
    s_state = new_state;

    switch (new_state) {
    case POWER_STATE_ACTIVE:
        enter_active();
        break;

    case POWER_STATE_DISPLAY_DIM:
        enter_dim();
        publish_power_event(EVENT_POWER_STATE_CHANGE, POWER_STATE_DISPLAY_DIM);
        break;

    case POWER_STATE_WIFI_LIGHT_SLEEP:
        enter_light_sleep();
        break;

    case POWER_STATE_DEEP_SLEEP:
        enter_deep_sleep();
        /* Does not return */
        break;

    default:
        ESP_LOGW(TAG, "Unknown power state: %d", new_state);
        s_state = old_state;
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

/**
 * @brief Event handler for activity events (resets idle timer)
 */
static void activity_event_handler(const robot_event_t *event)
{
    (void)event;
    power_manager_notify_activity();
}

/**
 * @brief Event handler for critical battery (forces deep sleep)
 */
static void critical_battery_handler(const robot_event_t *event)
{
    (void)event;
    ESP_LOGW(TAG, "Critical battery detected, forcing deep sleep");

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        transition_to(POWER_STATE_DEEP_SLEEP);
        xSemaphoreGive(s_mutex);
    }
}

/**
 * @brief Subscribe to all activity events
 */
static void subscribe_activity_events(void)
{
    /* Touch events */
    event_bus_subscribe(EVENT_TOUCH_SINGLE,   activity_event_handler);
    event_bus_subscribe(EVENT_TOUCH_DOUBLE,   activity_event_handler);
    event_bus_subscribe(EVENT_TOUCH_LONG,     activity_event_handler);

    /* Wake word */
    event_bus_subscribe(EVENT_WAKE_WORD_DETECTED, activity_event_handler);

    /* Audio capture */
    event_bus_subscribe(EVENT_AUDIO_CAPTURE_START, activity_event_handler);

    /* MQTT message */
    event_bus_subscribe(EVENT_MQTT_MESSAGE,   activity_event_handler);

    /* Motion command */
    event_bus_subscribe(EVENT_MOTION_COMMAND,  activity_event_handler);

    /* Critical battery — special handler */
    event_bus_subscribe(EVENT_SYS_CRITICAL_BATTERY, critical_battery_handler);
}

/**
 * @brief Unsubscribe from all activity events
 */
static void unsubscribe_activity_events(void)
{
    event_bus_unsubscribe(EVENT_TOUCH_SINGLE,   activity_event_handler);
    event_bus_unsubscribe(EVENT_TOUCH_DOUBLE,   activity_event_handler);
    event_bus_unsubscribe(EVENT_TOUCH_LONG,     activity_event_handler);
    event_bus_unsubscribe(EVENT_WAKE_WORD_DETECTED, activity_event_handler);
    event_bus_unsubscribe(EVENT_AUDIO_CAPTURE_START, activity_event_handler);
    event_bus_unsubscribe(EVENT_MQTT_MESSAGE,   activity_event_handler);
    event_bus_unsubscribe(EVENT_MOTION_COMMAND,  activity_event_handler);
    event_bus_unsubscribe(EVENT_SYS_CRITICAL_BATTERY, critical_battery_handler);
}

/**
 * @brief Periodic check task — evaluates idle timeouts at 1 Hz
 */
static void power_check_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "Power check task started");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));  /* 1 Hz */

        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
            continue;
        }

        uint32_t idle_ms = get_idle_ms_internal();

        switch (s_state) {
        case POWER_STATE_ACTIVE:
            if (idle_ms >= s_config.dim_timeout_ms) {
                transition_to(POWER_STATE_DISPLAY_DIM);
            }
            break;

        case POWER_STATE_DISPLAY_DIM:
            if (idle_ms >= s_config.light_sleep_timeout_ms) {
                transition_to(POWER_STATE_WIFI_LIGHT_SLEEP);
            }
            /* Note: return to ACTIVE is handled by notify_activity() */
            break;

        case POWER_STATE_WIFI_LIGHT_SLEEP:
            if (idle_ms >= s_config.deep_sleep_timeout_ms) {
                transition_to(POWER_STATE_DEEP_SLEEP);
            }
            /* Note: return to ACTIVE is handled by notify_activity() */
            break;

        case POWER_STATE_DEEP_SLEEP:
            /* Should never reach here — deep sleep does not return */
            break;
        }

        xSemaphoreGive(s_mutex);
    }
}

/* ============================================================
 * Deep Sleep Wakeup Handler
 * ============================================================ */

/**
 * @brief Check for deep sleep wakeup cause on boot
 *
 * Called from power_manager_init() to detect if we are waking
 * from deep sleep and publish the appropriate event.
 */
static void handle_deep_sleep_wakeup(void)
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_TIMER) {
        ESP_LOGI(TAG, "Wakeup from deep sleep (timer)");
    } else if (cause == ESP_SLEEP_WAKEUP_EXT0) {
        ESP_LOGI(TAG, "Wakeup from deep sleep (touch/button)");
    } else {
        return;  /* Not a deep sleep wakeup */
    }

    publish_power_event(EVENT_POWER_WAKEUP, POWER_STATE_ACTIVE);
}

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t power_manager_init(const power_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Power manager already initialized");
        return ESP_OK;
    }

    /* Apply configuration */
    if (config != NULL) {
        memcpy(&s_config, config, sizeof(s_config));
        /* Fill in defaults for zeroed fields */
        if (s_config.dim_timeout_ms == 0) {
            s_config.dim_timeout_ms = s_default_config.dim_timeout_ms;
        }
        if (s_config.light_sleep_timeout_ms == 0) {
            s_config.light_sleep_timeout_ms = s_default_config.light_sleep_timeout_ms;
        }
        if (s_config.deep_sleep_timeout_ms == 0) {
            s_config.deep_sleep_timeout_ms = s_default_config.deep_sleep_timeout_ms;
        }
        if (s_config.dim_brightness == 0) {
            s_config.dim_brightness = s_default_config.dim_brightness;
        }
        if (s_config.active_brightness == 0) {
            s_config.active_brightness = s_default_config.active_brightness;
        }
    } else {
        memcpy(&s_config, &s_default_config, sizeof(s_config));
    }

    /* Create mutex for thread-safe state access */
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Initialize state */
    s_state = POWER_STATE_ACTIVE;
    s_last_activity_us = esp_timer_get_time();

    /* Check for deep sleep wakeup */
    handle_deep_sleep_wakeup();

    /* Subscribe to activity events */
    subscribe_activity_events();

    /* Start periodic check task (low priority, 2KB stack) */
    BaseType_t ret = xTaskCreate(
        power_check_task,
        "power_chk",
        2048,
        NULL,
        tskIDLE_PRIORITY + 1,   /* Low priority */
        &s_check_task
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create check task");
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        unsubscribe_activity_events();
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Power manager initialized (dim=%lums, light=%lums, deep=%lums)",
             (unsigned long)s_config.dim_timeout_ms,
             (unsigned long)s_config.light_sleep_timeout_ms,
             (unsigned long)s_config.deep_sleep_timeout_ms);
    return ESP_OK;
}

esp_err_t power_manager_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    /* Stop check task */
    if (s_check_task != NULL) {
        vTaskDelete(s_check_task);
        s_check_task = NULL;
    }

    /* Unsubscribe from events */
    unsubscribe_activity_events();

    /* Delete mutex */
    if (s_mutex != NULL) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "Power manager deinitialized");
    return ESP_OK;
}

power_state_t power_manager_get_state(void)
{
    if (!s_initialized) {
        return POWER_STATE_ACTIVE;
    }

    power_state_t state;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = s_state;
        xSemaphoreGive(s_mutex);
    } else {
        state = s_state;  /* Best effort read */
    }
    return state;
}

void power_manager_notify_activity(void)
{
    if (!s_initialized) {
        return;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_last_activity_us = esp_timer_get_time();

        /* Wake up from dim or light sleep on any activity */
        if (s_state == POWER_STATE_DISPLAY_DIM || s_state == POWER_STATE_WIFI_LIGHT_SLEEP) {
            transition_to(POWER_STATE_ACTIVE);
        }

        xSemaphoreGive(s_mutex);
    }
}

esp_err_t power_manager_set_state(power_state_t state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        ret = transition_to(state);
        xSemaphoreGive(s_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to acquire mutex for set_state");
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}

uint32_t power_manager_get_idle_ms(void)
{
    if (!s_initialized) {
        return 0;
    }

    uint32_t idle_ms;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        idle_ms = get_idle_ms_internal();
        xSemaphoreGive(s_mutex);
    } else {
        idle_ms = get_idle_ms_internal();  /* Best effort read */
    }
    return idle_ms;
}
