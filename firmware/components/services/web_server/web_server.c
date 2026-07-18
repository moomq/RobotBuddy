/*
 * RobotBuddy — Web Server Implementation
 * =========================================
 * HTTP server providing REST API and web console for RobotBuddy.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "web_server.h"
#include "event_bus.h"
#include "robot_events.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include "nvs.h"

/* V2.0 module interfaces */
#include "emotion_engine.h"
#include "mqtt_client.h"
#include "ota_service.h"

#include <string.h>
#include <stdlib.h>

static const char *TAG = "web_server";

/* ============================================================
 * Configuration
 * ============================================================ */

#define WEB_SERVER_MAX_URI_HANDLERS  16
#define WEB_SERVER_RECV_BUF_SIZE     1024

/* ============================================================
 * Module State
 * ============================================================ */

static httpd_handle_t s_server = NULL;
static bool s_initialized = false;
static web_server_config_t s_config = {0};
static SemaphoreHandle_t s_mutex = NULL;

/* ============================================================
 * Embedded HTML Dashboard
 * ============================================================ */

static const char *HTML_DASHBOARD =
"<!DOCTYPE html>"
"<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>RobotBuddy</title>"
"<style>"
"body{font-family:Arial,sans-serif;margin:20px;background:#f5f5f5}"
".card{background:#fff;padding:20px;margin:10px 0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,.1)}"
"h1{color:#333}h2{color:#666;border-bottom:1px solid #eee;padding-bottom:5px}"
".status{display:flex;flex-wrap:wrap;gap:10px}"
".stat-item{background:#e3f2fd;padding:15px;border-radius:5px;min-width:100px;text-align:center}"
".stat-value{font-size:24px;font-weight:bold;color:#1976d2}"
".stat-label{font-size:12px;color:#666}"
"input,select{width:100%;padding:8px;margin:5px 0;box-sizing:border-box;border:1px solid #ddd;border-radius:4px}"
"button{background:#1976d2;color:#fff;padding:10px 20px;border:none;border-radius:4px;cursor:pointer;margin:5px 0}"
"button:hover{background:#1565c0}"
"form{margin:10px 0}"
".error{color:#d32f2f}"
".success{color:#388e3c}"
"</style></head>"
"<body>"
"<h1>🤖 RobotBuddy</h1>"
"<div class='card'><h2>Status</h2>"
"<div class='status' id='status'>Loading...</div></div>"
"<div class='card'><h2>WiFi Config</h2>"
"<form id='wifi-form'><input type='text' id='ssid' placeholder='SSID' required>"
"<input type='password' id='password' placeholder='Password'>"
"<button type='submit'>Save & Connect</button></form></div>"
"<div class='card'><h2>AI API Key</h2>"
"<form id='ai-form'><select id='provider'><option value='0'>Claude</option><option value='1'>OpenAI</option><option value='2'>DeepSeek</option></select>"
"<input type='text' id='api-key' placeholder='API Key'>"
"<button type='submit'>Save</button></form></div>"
"<div class='card'><h2>OTA Upgrade</h2>"
"<form id='ota-form'><input type='text' id='ota-url' placeholder='Firmware URL'>"
"<input type='text' id='ota-sha256' placeholder='SHA256 (optional)'>"
"<button type='submit'>Start Upgrade</button></form>"
"<div id='ota-progress'></div></div>"
"<div class='card'><h2>Control</h2>"
"<button onclick='setEmotion(0)'>Idle</button>"
"<button onclick='setEmotion(5)'>Happy</button>"
"<button onclick='setEmotion(7)'>Error</button>"
"<button onclick='startPomodoro()'>Start Pomodoro</button></div>"
"<script>"
"function loadStatus(){fetch('/api/status').then(r=>r.json()).then(d=>{"
"document.getElementById('status').innerHTML="
"+'<div class=\"stat-item\"><div class=\"stat-value\">'+d.emotion+'</div><div class=\"stat-label\">Emotion</div></div>'"
"+'<div class=\"stat-item\"><div class=\"stat-value\">'+d.battery_percent+'%</div><div class=\"stat-label\">Battery</div></div>'"
"+'<div class=\"stat-item\"><div class=\"stat-value\">'+d.wifi_rssi+'dBm</div><div class=\"stat-label\">WiFi</div></div>'"
"+'<div class=\"stat-item\"><div class=\"stat-value\">'+d.uptime_sec+'s</div><div class=\"stat-label\">Uptime</div></div>'"
"+'<div class=\"stat-item\"><div class=\"stat-value\">'+(d.mqtt_connected?'✓':'✗')+'</div><div class=\"stat-label\">MQTT</div></div>'"
";});}"
"loadStatus();setInterval(loadStatus,5000);"
"document.getElementById('wifi-form').onsubmit=function(e){e.preventDefault();"
"fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({ssid:document.getElementById('ssid').value,password:document.getElementById('password').value})})"
".then(r=>r.json()).then(d=>alert(d.message||'OK'));};"
"document.getElementById('ai-form').onsubmit=function(e){e.preventDefault();"
"fetch('/api/ai-key',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({provider:parseInt(document.getElementById('provider').value),api_key:document.getElementById('api-key').value})})"
".then(r=>r.json()).then(d=>alert(d.message||'OK'));};"
"document.getElementById('ota-form').onsubmit=function(e){e.preventDefault();"
"fetch('/api/ota',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({url:document.getElementById('ota-url').value,sha256:document.getElementById('ota-sha256').value})})"
".then(r=>r.json()).then(d=>{alert(d.message||'Starting...');checkOtaProgress();});};"
"function checkOtaProgress(){fetch('/api/ota-progress').then(r=>r.json()).then(d=>{"
"if(d.state==='downloading')document.getElementById('ota-progress').innerHTML='Progress: '+d.percent+'%';"
"else if(d.state==='error')document.getElementById('ota-progress').innerHTML='Error: '+d.error;"
"else if(d.state!=='idle')setTimeout(checkOtaProgress,1000);});}"
"function setEmotion(e){fetch('/api/emotion',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({emotion_id:e})});}"
"function startPomodoro(){fetch('/api/pomodoro',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action:'start'})});}"
"</script></body></html>";

/* ============================================================
 * Utility Functions
 * ============================================================ */

static esp_err_t set_content_type_json(httpd_req_t *req)
{
    return httpd_resp_set_type(req, "application/json");
}

static esp_err_t send_json_response(httpd_req_t *req, cJSON *root)
{
    char *json_str = cJSON_PrintUnformatted(root);
    esp_err_t ret = httpd_resp_sendstr(req, json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t send_json_error(httpd_req_t *req, const char *message)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "error", message);
    httpd_resp_set_status(req, HTTPD_400);
    set_content_type_json(req);
    return send_json_response(req, root);
}

static esp_err_t send_json_success(httpd_req_t *req, const char *message)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "message", message ? message : "OK");
    set_content_type_json(req);
    return send_json_response(req, root);
}

/* ============================================================
 * URI Handlers
 * ============================================================ */

/* GET / - Dashboard HTML */
static esp_err_t handler_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_sendstr(req, HTML_DASHBOARD);
}

/* GET /api/status - Robot status */
static esp_err_t handler_api_status(httpd_req_t *req)
{
    /* Gather status from various sources */
    extern const char *emotion_get_name(int emotion_id);
    extern int emotion_get_state(void);
    extern float battery_monitor_get_percentage(void);
    extern bool mqtt_client_is_connected(void);
    extern uint8_t ota_get_progress(void);
    extern int ota_get_state(void);

    cJSON *root = cJSON_CreateObject();

    /* Emotion */
    int emotion = emotion_get_state();
    cJSON_AddStringToObject(root, "emotion", emotion_get_name(emotion));

    /* Battery (placeholder - use actual battery_monitor API) */
    cJSON_AddNumberToObject(root, "battery_percent", 75);
    cJSON_AddNumberToObject(root, "battery_voltage", 3.85);

    /* WiFi (placeholder - use actual wifi_manager API) */
    cJSON_AddStringToObject(root, "wifi_ssid", "MyNetwork");
    cJSON_AddNumberToObject(root, "wifi_rssi", -45);
    cJSON_AddStringToObject(root, "ip", "192.168.1.100");

    /* System */
    cJSON_AddNumberToObject(root, "uptime_sec", (int)(esp_timer_get_time() / 1000000));
    cJSON_AddNumberToObject(root, "free_heap", esp_get_free_heap_size());

    /* MQTT */
    cJSON_AddBoolToObject(root, "mqtt_connected", mqtt_client_is_connected());

    /* Pomodoro (placeholder) */
    cJSON_AddStringToObject(root, "pomodoro_state", "idle");
    cJSON_AddNumberToObject(root, "pomodoro_remaining", 0);

    set_content_type_json(req);
    return send_json_response(req, root);
}

/* POST /api/wifi - Configure WiFi */
static esp_err_t handler_api_wifi(httpd_req_t *req)
{
    char buf[WEB_SERVER_RECV_BUF_SIZE] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return send_json_error(req, "No data received");
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        return send_json_error(req, "Invalid JSON");
    }

    cJSON *ssid_item = cJSON_GetObjectItem(root, "ssid");
    cJSON *pass_item = cJSON_GetObjectItem(root, "password");

    if (!ssid_item || !cJSON_IsString(ssid_item)) {
        cJSON_Delete(root);
        return send_json_error(req, "Missing ssid");
    }

    const char *ssid = ssid_item->valuestring;
    const char *pass = pass_item ? pass_item->valuestring : "";

    /* Save to NVS */
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("wifi_config", NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        nvs_set_str(nvs, "ssid", ssid);
        nvs_set_str(nvs, "password", pass);
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    /* TODO: Trigger WiFi reconnection via event bus */
    robot_event_t event = {
        .id = EVENT_SYS_WIFI_DISCONNECTED,  /* Signal reconnection */
        .payload = NULL,
        .payload_len = 0,
    };
    event_bus_publish(&event);

    cJSON_Delete(root);
    return send_json_success(req, "WiFi credentials saved. Reconnecting...");
}

/* POST /api/ai-key - Configure AI API key */
static esp_err_t handler_api_ai_key(httpd_req_t *req)
{
    char buf[512] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return send_json_error(req, "No data received");
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        return send_json_error(req, "Invalid JSON");
    }

    cJSON *provider_item = cJSON_GetObjectItem(root, "provider");
    cJSON *key_item = cJSON_GetObjectItem(root, "api_key");

    if (!provider_item || !key_item) {
        cJSON_Delete(root);
        return send_json_error(req, "Missing provider or api_key");
    }

    int provider = provider_item->valueint;
    const char *api_key = key_item->valuestring;

    /* Use cloud_manager API if available */
    extern esp_err_t cloud_set_api_key(int provider, const char *key);
    esp_err_t err = cloud_set_api_key(provider, api_key);

    cJSON_Delete(root);

    if (err == ESP_OK) {
        return send_json_success(req, "API key saved");
    } else {
        return send_json_error(req, "Failed to save API key");
    }
}

/* POST /api/ota - Start OTA upgrade */
static esp_err_t handler_api_ota(httpd_req_t *req)
{
    char buf[1024] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return send_json_error(req, "No data received");
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        return send_json_error(req, "Invalid JSON");
    }

    cJSON *url_item = cJSON_GetObjectItem(root, "url");
    cJSON *sha_item = cJSON_GetObjectItem(root, "sha256");

    if (!url_item || !cJSON_IsString(url_item)) {
        cJSON_Delete(root);
        return send_json_error(req, "Missing url");
    }

    const char *url = url_item->valuestring;
    const char *sha256 = sha_item ? sha_item->valuestring : NULL;

    /* Publish OTA start event */
    ota_progress_event_t payload = {0};
    robot_event_t event = {
        .id = EVENT_OTA_START,
        .payload = &payload,
        .payload_len = sizeof(payload),
    };
    event_bus_publish(&event);

    /* Use OTA service if available */
    extern esp_err_t ota_start(const char *url, const char *sha256);
    esp_err_t err = ota_start(url, sha256);

    cJSON_Delete(root);

    if (err == ESP_OK) {
        return send_json_success(req, "OTA upgrade started");
    } else {
        return send_json_error(req, "Failed to start OTA");
    }
}

/* GET /api/ota-progress - OTA progress */
static esp_err_t handler_api_ota_progress(httpd_req_t *req)
{
    extern uint8_t ota_get_progress(void);
    extern int ota_get_state(void);

    cJSON *root = cJSON_CreateObject();

    int state = ota_get_state();
    const char *state_names[] = {"idle", "downloading", "verifying", "applying", "rebooting", "error"};
    cJSON_AddStringToObject(root, "state", state_names[state]);
    cJSON_AddNumberToObject(root, "percent", ota_get_progress());

    set_content_type_json(req);
    return send_json_response(req, root);
}

/* POST /api/emotion - Set emotion */
static esp_err_t handler_api_emotion(httpd_req_t *req)
{
    char buf[256] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return send_json_error(req, "No data received");
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        return send_json_error(req, "Invalid JSON");
    }

    cJSON *emotion_item = cJSON_GetObjectItem(root, "emotion_id");
    if (!emotion_item) {
        cJSON_Delete(root);
        return send_json_error(req, "Missing emotion_id");
    }

    int emotion_id = emotion_item->valueint;

    /* Publish emotion change event */
    emotion_event_t payload = {
        .emotion_id = emotion_id,
        .duration_ms = 0,
        .intensity = 255,
    };
    robot_event_t event = {
        .id = EVENT_EMOTION_STATE_CHANGE,
        .payload = &payload,
        .payload_len = sizeof(payload),
    };
    event_bus_publish(&event);

    cJSON_Delete(root);
    return send_json_success(req, "Emotion changed");
}

/* POST /api/pomodoro - Pomodoro control */
static esp_err_t handler_api_pomodoro(httpd_req_t *req)
{
    char buf[256] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return send_json_error(req, "No data received");
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        return send_json_error(req, "Invalid JSON");
    }

    cJSON *action_item = cJSON_GetObjectItem(root, "action");
    if (!action_item) {
        cJSON_Delete(root);
        return send_json_error(req, "Missing action");
    }

    const char *action = action_item->valuestring;

    robot_event_t event = {.payload = NULL, .payload_len = 0};

    if (strcmp(action, "start") == 0) {
        event.id = EVENT_POMODORO_START;
    } else if (strcmp(action, "pause") == 0 || strcmp(action, "stop") == 0) {
        /* Use touch event to pause/resume */
        event.id = EVENT_TOUCH_SINGLE;
    } else {
        cJSON_Delete(root);
        return send_json_error(req, "Unknown action");
    }

    event_bus_publish(&event);

    cJSON_Delete(root);
    return send_json_success(req, "Pomodoro command sent");
}

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t web_server_init(const web_server_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Web server already initialized");
        return ESP_OK;
    }

    if (config != NULL) {
        s_config = *config;
    } else {
        s_config.port = 80;
        s_config.max_connections = 4;
        s_config.auth_enabled = false;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Web server initialized (port=%d)", s_config.port);
    return ESP_OK;
}

esp_err_t web_server_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }

    if (s_mutex) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "Web server deinitialized");
    return ESP_OK;
}

esp_err_t web_server_start(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (s_server) {
        xSemaphoreGive(s_mutex);
        return ESP_OK;  /* Already running */
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = s_config.port;
    config.max_uri_handlers = WEB_SERVER_MAX_URI_HANDLERS;
    config.max_open_sockets = s_config.max_connections;
    config.lru_purge_enable = true;

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        xSemaphoreGive(s_mutex);
        return ret;
    }

    /* Register URI handlers */
    httpd_uri_t uri_root = {
        .uri = "/", .method = HTTP_GET, .handler = handler_root, .user_ctx = NULL
    };
    httpd_register_uri_handler(s_server, &uri_root);

    httpd_uri_t uri_status = {
        .uri = "/api/status", .method = HTTP_GET, .handler = handler_api_status, .user_ctx = NULL
    };
    httpd_register_uri_handler(s_server, &uri_status);

    httpd_uri_t uri_wifi = {
        .uri = "/api/wifi", .method = HTTP_POST, .handler = handler_api_wifi, .user_ctx = NULL
    };
    httpd_register_uri_handler(s_server, &uri_wifi);

    httpd_uri_t uri_ai_key = {
        .uri = "/api/ai-key", .method = HTTP_POST, .handler = handler_api_ai_key, .user_ctx = NULL
    };
    httpd_register_uri_handler(s_server, &uri_ai_key);

    httpd_uri_t uri_ota = {
        .uri = "/api/ota", .method = HTTP_POST, .handler = handler_api_ota, .user_ctx = NULL
    };
    httpd_register_uri_handler(s_server, &uri_ota);

    httpd_uri_t uri_ota_progress = {
        .uri = "/api/ota-progress", .method = HTTP_GET, .handler = handler_api_ota_progress, .user_ctx = NULL
    };
    httpd_register_uri_handler(s_server, &uri_ota_progress);

    httpd_uri_t uri_emotion = {
        .uri = "/api/emotion", .method = HTTP_POST, .handler = handler_api_emotion, .user_ctx = NULL
    };
    httpd_register_uri_handler(s_server, &uri_emotion);

    httpd_uri_t uri_pomodoro = {
        .uri = "/api/pomodoro", .method = HTTP_POST, .handler = handler_api_pomodoro, .user_ctx = NULL
    };
    httpd_register_uri_handler(s_server, &uri_pomodoro);

    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Web server started on port %d", s_config.port);
    return ESP_OK;
}

esp_err_t web_server_stop(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "Web server stopped");
    }

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

bool web_server_is_running(void)
{
    return s_server != NULL;
}
