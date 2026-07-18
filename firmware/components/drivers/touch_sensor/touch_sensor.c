/**
 * @file touch_sensor.c
 * @brief Capacitive touch sensor driver implementation for RobotBuddy (ESP32-S3).
 *
 * Uses the ESP32-S3 internal touch peripheral (touch_pad API) to detect
 * gestures on a single capacitive pad.  A FreeRTOS polling task reads
 * the touch pad at 50 Hz and applies debounce + gesture logic.
 *
 * Gesture detection state machine:
 *   IDLE -> PRESSED (touch detected, debounced)
 *   PRESSED -> RELEASED (touch released) -> SINGLE or wait for DOUBLE
 *   PRESSED -> LONG_PRESS (held > long_press_ms)
 *
 * Detected gestures are published to the event bus as:
 *   EVENT_TOUCH_SINGLE, EVENT_TOUCH_DOUBLE, EVENT_TOUCH_LONG
 *
 * @copyright 2026 RobotBuddy contributors
 * @licence MIT
 */

#include "touch_sensor.h"

#include <string.h>

#include "driver/touch_pad.h"
#include "esp_log.h"
#include "event_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ------------------------------------------------------------------------ */
/*  Constants                                                                */
/* ------------------------------------------------------------------------ */

static const char *TAG = "touch_sensor";

/** @brief Polling frequency in Hz. */
#define POLL_RATE_HZ        50

/** @brief Polling period in ms. */
#define POLL_PERIOD_MS      (1000 / POLL_RATE_HZ)

/** @brief Polling task stack size in bytes. */
#define POLL_TASK_STACK     2048

/** @brief Polling task priority. */
#define POLL_TASK_PRIORITY  5

/** @brief Run polling on Core 1 (application core). */
#define POLL_TASK_CORE      1

/** @brief Calibration sample count for baseline measurement. */
#define CALIBRATION_SAMPLES 64

/** @brief Threshold is set at baseline - 20% when auto-calibrating. */
#define CALIBRATION_MARGIN_PCT 20

/* ------------------------------------------------------------------------ */
/*  Gesture detection state machine                                          */
/* ------------------------------------------------------------------------ */

typedef enum {
    GESTURE_STATE_IDLE,         /**< No touch detected                      */
    GESTURE_STATE_DEBOUNCE,     /**< Touch detected, waiting for debounce   */
    GESTURE_STATE_PRESSED,      /**< Touch confirmed, waiting for release   */
    GESTURE_STATE_RELEASED,     /**< Released — may be single or double tap */
    GESTURE_STATE_LONG_PRESS,   /**< Long press confirmed                   */
} gesture_state_t;

/* ------------------------------------------------------------------------ */
/*  Module state                                                             */
/* ------------------------------------------------------------------------ */

typedef struct {
    touch_pad_t    touch_pad;       /**< Mapped touch pad number             */
    uint16_t       threshold;       /**< Detection threshold                 */
    uint16_t       debounce_ms;     /**< Debounce period (ms)                */
    uint16_t       double_tap_ms;   /**< Double-tap window (ms)              */
    uint16_t       long_press_ms;   /**< Long-press threshold (ms)           */
    uint16_t       baseline;        /**< Calibrated baseline reading         */

    /* Gesture detection state */
    gesture_state_t gesture_state;
    uint16_t       baseline_reading;   /**< Last stable baseline reading      */
    TickType_t     press_start_tick;   /**< Tick when press was confirmed     */
    TickType_t     release_tick;       /**< Tick when release was detected    */
    touch_gesture_t last_gesture;      /**< Last completed gesture            */

    bool           initialised;
    TaskHandle_t   poll_task;
} touch_sensor_state_t;

static touch_sensor_state_t s_state = {0};

/* ------------------------------------------------------------------------ */
/*  Helpers                                                                  */
/* ------------------------------------------------------------------------ */

/**
 * @brief Map a GPIO number to its touch pad index.
 *
 * The ESP32-S3 touch pad to GPIO mapping:
 *   TOUCH_PAD_NUM0 = GPIO1,  TOUCH_PAD_NUM1 = GPIO2,
 *   TOUCH_PAD_NUM2 = GPIO3,  TOUCH_PAD_NUM3 = GPIO4,
 *   TOUCH_PAD_NUM4 = GPIO5,  TOUCH_PAD_NUM5 = GPIO6,
 *   TOUCH_PAD_NUM6 = GPIO7,  TOUCH_PAD_NUM7 = GPIO8,
 *   TOUCH_PAD_NUM8 = GPIO9,  TOUCH_PAD_NUM9 = GPIO10,
 *   TOUCH_PAD_NUM10 = GPIO11, TOUCH_PAD_NUM11 = GPIO12,
 *   TOUCH_PAD_NUM12 = GPIO13, TOUCH_PAD_NUM13 = GPIO14,
 *   TOUCH_PAD_NUM14 = GPIO15
 *
 * @param gpio  GPIO number.
 * @return The corresponding touch_pad_t, or TOUCH_PAD_MAX on error.
 */
static touch_pad_t gpio_to_touch_pad(int gpio)
{
    /* ESP32-S3: TOUCH_PAD_NUMn = GPIO(n + 1) for n = 0..14 */
    if (gpio >= 1 && gpio <= 15) {
        return (touch_pad_t)(gpio - 1);
    }
    ESP_LOGE(TAG, "GPIO %d is not a valid touch pad", gpio);
    return TOUCH_PAD_MAX;
}

/**
 * @brief Calibrate the touch pad baseline and set the threshold.
 *
 * Reads the touch pad multiple times with no touch applied to
 * establish a baseline, then sets the threshold at baseline - 20%.
 *
 * @return ESP_OK on success, or an ESP-IDF error code.
 */
static esp_err_t calibrate_baseline(void)
{
    uint32_t sum = 0;
    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
        uint16_t val = 0;
        esp_err_t err = touch_pad_read(s_state.touch_pad, &val);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Calibration read failed at sample %d: %s",
                     i, esp_err_to_name(err));
            return err;
        }
        sum += val;
    }
    s_state.baseline = (uint16_t)(sum / CALIBRATION_SAMPLES);

    if (s_state.threshold == 0) {
        /* Auto-calibrate: set threshold at baseline - 20%. */
        s_state.threshold = s_state.baseline - (s_state.baseline * CALIBRATION_MARGIN_PCT / 100);
        ESP_LOGI(TAG, "Auto-calibrated: baseline=%u, threshold=%u",
                 s_state.baseline, s_state.threshold);
    } else {
        ESP_LOGI(TAG, "Baseline=%u, using configured threshold=%u",
                 s_state.baseline, s_state.threshold);
    }

    return ESP_OK;
}

/**
 * @brief Publish a touch event to the event bus.
 *
 * @param event_id  The event ID (EVENT_TOUCH_SINGLE, etc.).
 * @param gesture   The gesture type.
 */
static void publish_event(robot_event_id_t event_id, touch_gesture_t gesture)
{
    robot_event_t event = {
        .id          = event_id,
        .timestamp   = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS),
        .payload     = NULL,
        .payload_len = 0,
    };
    esp_err_t err = event_bus_publish(&event);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to publish event 0x%04X: %s",
                 event_id, esp_err_to_name(err));
    } else {
        ESP_LOGD(TAG, "Published event 0x%04X (gesture=%d)", event_id, gesture);
    }
}

/**
 * @brief Read the current touch pad value and determine if pressed.
 *
 * The ESP32-S3 touch peripheral reports lower values when the pad is
 * touched.  A reading below the threshold indicates a touch.
 *
 * @param[out] pressed  Set to true if touch is detected.
 * @return ESP_OK on success, or an ESP-IDF error code on read failure.
 */
static esp_err_t read_touch(bool *pressed)
{
    uint16_t val = 0;
    esp_err_t err = touch_pad_read(s_state.touch_pad, &val);
    if (err != ESP_OK) {
        *pressed = false;
        return err;
    }
    s_state.baseline_reading = val;
    *pressed = (val < s_state.threshold);
    return ESP_OK;
}

/* ------------------------------------------------------------------------ */
/*  Polling task                                                             */
/* ------------------------------------------------------------------------ */

/**
 * @brief Main polling task for touch gesture detection.
 *
 * Runs at 50 Hz (20 ms period).  Implements a debounce + gesture
 * state machine:
 *
 *   IDLE       : No touch.  On press -> DEBOUNCE.
 *   DEBOUNCE   : Wait debounce_ms of stable press.  On stable -> PRESSED.
 *                On release -> IDLE.
 *   PRESSED    : Touch confirmed.  If held > long_press_ms -> LONG_PRESS.
 *                On release -> RELEASED.
 *   RELEASED   : Wait double_tap_ms for second press.
 *                If second press -> DOUBLE_TAP.
 *                If timeout -> SINGLE_TAP.
 *   LONG_PRESS : Long press active.  On release -> IDLE.
 *
 * @param arg  Unused (NULL).
 */
static void poll_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "Poll task started on core %d", xPortGetCoreID());

    uint16_t debounce_stable_count = 0;
    const uint16_t debounce_ticks_needed =
        (s_state.debounce_ms + POLL_PERIOD_MS - 1) / POLL_PERIOD_MS;

    while (s_state.initialised) {
        bool pressed = false;
        esp_err_t err = read_touch(&pressed);
        if (err != ESP_OK) {
            /* Skip this cycle on read error. */
            vTaskDelay(pdMS_TO_TICKS(POLL_PERIOD_MS));
            continue;
        }

        TickType_t now = xTaskGetTickCount();

        switch (s_state.gesture_state) {

        /* ---- No touch detected ---- */
        case GESTURE_STATE_IDLE:
            if (pressed) {
                s_state.gesture_state = GESTURE_STATE_DEBOUNCE;
                debounce_stable_count = 1;
            }
            break;

        /* ---- Touch detected, waiting for stable reading ---- */
        case GESTURE_STATE_DEBOUNCE:
            if (pressed) {
                debounce_stable_count++;
                if (debounce_stable_count >= debounce_ticks_needed) {
                    s_state.gesture_state = GESTURE_STATE_PRESSED;
                    s_state.press_start_tick = now;
                    ESP_LOGD(TAG, "Touch confirmed (debounced)");
                }
            } else {
                /* False alarm — went back to unpressed. */
                s_state.gesture_state = GESTURE_STATE_IDLE;
                debounce_stable_count = 0;
            }
            break;

        /* ---- Touch confirmed, waiting for release or long press ---- */
        case GESTURE_STATE_PRESSED:
            if (!pressed) {
                /* Released before long-press threshold. */
                s_state.gesture_state = GESTURE_STATE_RELEASED;
                s_state.release_tick = now;
            } else {
                /* Check for long press. */
                uint32_t held_ms = (now - s_state.press_start_tick) * portTICK_PERIOD_MS;
                if (held_ms >= s_state.long_press_ms) {
                    s_state.gesture_state = GESTURE_STATE_LONG_PRESS;
                    s_state.last_gesture = TOUCH_GESTURE_LONG_PRESS;
                    ESP_LOGI(TAG, "Long press detected (%lu ms)", held_ms);
                    publish_event(EVENT_TOUCH_LONG, TOUCH_GESTURE_LONG_PRESS);
                }
            }
            break;

        /* ---- Released — determine single or double tap ---- */
        case GESTURE_STATE_RELEASED: {
            uint32_t since_release_ms =
                (now - s_state.release_tick) * portTICK_PERIOD_MS;

            if (pressed) {
                /* Second press within the double-tap window. */
                s_state.gesture_state = GESTURE_STATE_DEBOUNCE;
                debounce_stable_count = 1;
                s_state.last_gesture = TOUCH_GESTURE_DOUBLE_TAP;
                ESP_LOGI(TAG, "Double tap detected");
                publish_event(EVENT_TOUCH_DOUBLE, TOUCH_GESTURE_DOUBLE_TAP);
            } else if (since_release_ms >= s_state.double_tap_ms) {
                /* No second press — this was a single tap. */
                s_state.gesture_state = GESTURE_STATE_IDLE;
                s_state.last_gesture = TOUCH_GESTURE_SINGLE_TAP;
                ESP_LOGI(TAG, "Single tap detected");
                publish_event(EVENT_TOUCH_SINGLE, TOUCH_GESTURE_SINGLE_TAP);
            }
            /* else: still within the double-tap window, keep waiting. */
            break;
        }

        /* ---- Long press active, waiting for release ---- */
        case GESTURE_STATE_LONG_PRESS:
            if (!pressed) {
                s_state.gesture_state = GESTURE_STATE_IDLE;
                ESP_LOGD(TAG, "Long press released");
            }
            break;

        default:
            s_state.gesture_state = GESTURE_STATE_IDLE;
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_PERIOD_MS));
    }

    ESP_LOGI(TAG, "Poll task exiting");
    vTaskDelete(NULL);
}

/* ------------------------------------------------------------------------ */
/*  Public API                                                               */
/* ------------------------------------------------------------------------ */

esp_err_t touch_sensor_init(const touch_sensor_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_state.initialised) {
        ESP_LOGW(TAG, "Already initialised");
        return ESP_ERR_INVALID_STATE;
    }

    /* ---- Map GPIO to touch pad ---- */
    int gpio = (config->gpio_num != 0) ? config->gpio_num : TOUCH_SENSOR_DEFAULT_GPIO;
    touch_pad_t pad = gpio_to_touch_pad(gpio);
    if (pad >= TOUCH_PAD_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    /* ---- Copy configuration with defaults ---- */
    s_state.touch_pad     = pad;
    s_state.threshold     = config->threshold;
    s_state.debounce_ms   = (config->debounce_ms   != 0) ? config->debounce_ms   : TOUCH_SENSOR_DEFAULT_DEBOUNCE_MS;
    s_state.double_tap_ms = (config->double_tap_ms  != 0) ? config->double_tap_ms : TOUCH_SENSOR_DEFAULT_DOUBLE_TAP_MS;
    s_state.long_press_ms = (config->long_press_ms  != 0) ? config->long_press_ms : TOUCH_SENSOR_DEFAULT_LONG_PRESS_MS;

    /* ---- Initialise touch pad peripheral ---- */
    esp_err_t err = touch_pad_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "touch_pad_init() failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Set voltage range for high-to-low detection. */
    err = touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "touch_pad_set_voltage() failed: %s", esp_err_to_name(err));
        touch_pad_deinit();
        return err;
    }

    /* Configure the specific touch pad. */
    err = touch_pad_config(s_state.touch_pad);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "touch_pad_config(pad=%d) failed: %s",
                 s_state.touch_pad, esp_err_to_name(err));
        touch_pad_deinit();
        return err;
    }

    /* Enable touch pad FSM (start measurements). */
    err = touch_pad_fsm_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "touch_pad_fsm_start() failed: %s", esp_err_to_name(err));
        touch_pad_deinit();
        return err;
    }

    /* Wait a moment for the FSM to stabilise before calibrating. */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* ---- Calibrate baseline threshold ---- */
    err = calibrate_baseline();
    if (err != ESP_OK) {
        touch_pad_fsm_stop();
        touch_pad_deinit();
        return err;
    }

    /* ---- Reset gesture state ---- */
    s_state.gesture_state  = GESTURE_STATE_IDLE;
    s_state.last_gesture   = TOUCH_GESTURE_NONE;
    s_state.press_start_tick = 0;
    s_state.release_tick   = 0;

    /* ---- Start polling task on Core 1 ---- */
    BaseType_t ret = xTaskCreatePinnedToCore(
        poll_task,
        "touch_poll",
        POLL_TASK_STACK,
        NULL,
        POLL_TASK_PRIORITY,
        &s_state.poll_task,
        POLL_TASK_CORE
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create poll task");
        touch_pad_fsm_stop();
        touch_pad_deinit();
        return ESP_ERR_NO_MEM;
    }

    s_state.initialised = true;
    ESP_LOGI(TAG, "Initialised — GPIO=%d pad=%d threshold=%u debounce=%u ms "
             "double_tap=%u ms long_press=%u ms",
             gpio, s_state.touch_pad, s_state.threshold,
             s_state.debounce_ms, s_state.double_tap_ms,
             s_state.long_press_ms);
    return ESP_OK;
}

esp_err_t touch_sensor_deinit(void)
{
    if (!s_state.initialised) {
        ESP_LOGW(TAG, "De-init called but driver not initialised");
        return ESP_OK;
    }

    /* Stop the polling task. */
    s_state.initialised = false;
    if (s_state.poll_task != NULL) {
        /* The task checks s_state.initialised and exits on its own. */
        vTaskDelay(pdMS_TO_TICKS(POLL_PERIOD_MS * 2));
        s_state.poll_task = NULL;
    }

    /* Stop touch FSM and release peripheral resources. */
    touch_pad_fsm_stop();
    touch_pad_deinit();

    ESP_LOGI(TAG, "De-initialised");
    return ESP_OK;
}

touch_gesture_t touch_sensor_get_gesture(void)
{
    return s_state.last_gesture;
}

bool touch_sensor_is_pressed(void)
{
    if (!s_state.initialised) {
        return false;
    }
    bool pressed = false;
    esp_err_t err = read_touch(&pressed);
    if (err != ESP_OK) {
        return false;
    }
    return pressed;
}
