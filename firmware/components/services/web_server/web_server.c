/*
 * RobotBuddy — Web Server Implementation
 * ========================================
 * Local HTTP console for configuration and status monitoring.
 *
 * Architecture:
 *   ┌──────────────────────────────────────────────────────┐
 *   │  Embedded HTML Dashboard (GET /)                     │
 *   ├──────────────────────────────────────────────────────┤
 *   │  REST API Endpoints                                  │
 *   │   GET  /api/status        → robot state JSON          │
 *   │   GET  /api/config        → device config JSON        │
 *   │   POST /api/wifi          → save WiFi creds           │
 *   │   POST /api/ai-key        → save AI API key           │
 *   │   POST /api/ota           → trigger OTA upgrade       │
 *   │   POST /api/emotion       → set emotion state         │
 *   │   POST /api/motion        → send motion command       │
 *   │   POST /api/pomodoro      → control pomodoro timer    │
 *   │   GET  /api/ota-progress  → OTA progress JSON         │
 *   └──────────────────────────────────────────────────────┘
 *                    │
 *                    ▼
 *   ┌────────────────────────────┐
 *   │  esp_http_server (ESP-IDF) │
 *   └────────────────────────────┘
 *
 * Thread safety:
 *   esp_http_server handles request dispatch and serialization internally.
 *   Accesses to shared module state (OTA progress) use s_mutex.
 *
 * Copyright (c) 2026 RobotBuddy Project
 * SPDX-License-Identifier: MIT
 */

#include "web_server.h"
#include "event_bus.h"
#include "robot_events.h"
#include "wifi_manager.h"
#include "cloud_manager.h"
#include "battery_monitor.h"
#include "emotion_engine.h"
#include "motion_manager.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "mbedtls/base64.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Logging
 * ============================================================ */

static const char *TAG = "web_server";

/* ============================================================
 * Constants
 * ============================================================ */

#define WEB_SERVER_NVS_NAMESPACE  "web_srv"
#define WEB_SERVER_MAX_BODY_LEN   4096

/* ============================================================
 * Module State
 * ============================================================ */

static httpd_handle_t   s_server       = NULL;
static bool             s_initialized  = false;
static bool             s_running      = false;
static SemaphoreHandle_t s_mutex       = NULL;
static web_server_config_t s_config    = {
    .port            = WEB_SERVER_DEFAULT_PORT,
    .max_connections = WEB_SERVER_DEFAULT_MAX_CONNECTIONS,
    .auth_enabled    = false,
    .username        = "",
    .password        = "",
};

/** OTA progress tracking (updated via event bus) */
static ota_state_t  s_ota_state    = OTA_STATE_IDLE;
static uint8_t      s_ota_percent  = 0;

/** Uptime reference */
static int64_t      s_start_time   = 0;

/* ============================================================
 * Embedded HTML Dashboard (<4KB, inline CSS, no externals)
 * Auto-refreshes /api/status every 5 seconds via fetch().
 * ============================================================ */

static const char *s_dashboard_html =
"<!DOCTYPE html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>RobotBuddy</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{font-family:system-ui,sans-serif;background:#0a0a14;color:#e0e0e0;padding:16px}"
"h1{font-size:1.4em;color:#00c8dc;margin-bottom:12px}"
"h2{font-size:1.1em;color:#00c8dc;margin:16px 0 8px;border-bottom:1px solid #1a3a4a;padding-bottom:4px}"
".card{background:#111827;border:1px solid #1e3a4a;border-radius:8px;padding:12px;margin-bottom:12px}"
".row{display:flex;justify-content:space-between;padding:4px 0}"
".label{color:#8899aa}.value{color:#e0e0e0;font-weight:bold}"
"input,select,button{background:#1a2a3a;color:#e0e0e0;border:1px solid #2a4a5a;border-radius:4px;padding:6px 10px;margin:2px 0;font-size:0.95em}"
"input{width:100%;max-width:260px}button{cursor:pointer;padding:8px 16px;margin-top:6px}"
"button:hover{background:#2a5a7a}.btn{margin-right:4px}"
"#msg{margin:8px 0;padding:8px;border-radius:4px;display:none}"
"</style></head><body>"
"<h1>RobotBuddy Console</h1><div id='msg'></div>"
"<h2>Status</h2><div class='card' id='status'>Loading...</div>"
"<h2>WiFi Configuration</h2><div class='card'>"
"<input id='ssid' placeholder='SSID'><br>"
"<input id='wpass' type='password' placeholder='Password'><br>"
"<button onclick='postJson(\"/api/wifi\",{ssid:$('ssid').value,password:$('wpass').value})'>Save &amp; Connect</button>"
"</div>"
"<h2>AI API Key</h2><div class='card'>"
"<select id='provider'><option value='0'>Claude</option><option value='1'>OpenAI</option><option value='2'>DeepSeek</option></select><br>"
"<input id='apikey' placeholder='API Key'><br>"
"<button onclick='postJson(\"/api/ai-key\",{provider:parseInt($('provider').value),api_key:$('apikey').value})'>Save Key</button>"
"</div>"
"<h2>OTA Upgrade</h2><div class='card'>"
"<input id='otaurl' placeholder='Firmware URL'><br>"
"<input id='otasha' placeholder='SHA256 (optional)'><br>"
"<button onclick='postJson(\"/api/ota\",{url:$('otaurl').value,sha256:$('otasha').value})'>Start OTA</button>"
"<div id='otaprog' style='margin-top:8px'></div>"
"</div>"
"<h2>Emotion Control</h2><div class='card'>"
"<button class='btn' onclick='postJson(\"/api/emotion\",{emotion_id:0})'>Idle</button>"
"<button class='btn' onclick='postJson(\"/api/emotion\",{emotion_id:4})'>Happy</button>"
"<button class='btn' onclick='postJson(\"/api/emotion\",{emotion_id:5})'>Confused</button>"
"<button class='btn' onclick='postJson(\"/api/emotion\",{emotion_id:10})'>Excited</button>"
"<button class='btn' onclick='postJson(\"/api/emotion\",{emotion_id:9})'>Sleep</button>"
"<button class='btn' onclick='postJson(\"/api/emotion\",{emotion_id:7})'>Error</button>"
"</div>"
"<h2>Motion Control</h2><div class='card'>"
"<button class='btn' onclick='postJson(\"/api/motion\",{cmd:1,speed:150,duration:1000})'>Fwd</button>"
"<button class='btn' onclick='postJson(\"/api/motion\",{cmd:2,speed:150,duration:1000})'>Back</button>"
"<button class='btn' onclick='postJson(\"/api/motion\",{cmd:3,speed:150,duration:500})'>Left</button>"
"<button class='btn' onclick='postJson(\"/api/motion\",{cmd:4,speed:150,duration:500})'>Right</button>"
"<button class='btn' onclick='postJson(\"/api/motion\",{cmd:0,speed:0,duration:0})'>Stop</button>"
"</div>"
"<h2>Pomodoro Timer</h2><div class='card'>"
"<button class='btn' onclick='postJson(\"/api/pomodoro\",{action:\"start\"})'>Start</button>"
"<button class='btn' onclick='postJson(\"/api/pomodoro\",{action:\"pause\"})'>Pause</button>"
"<button class='btn' onclick='postJson(\"/api/pomodoro\",{action:\"stop\"})'>Stop</button>"
"</div>"
"<script>"
"function $(id){return document.getElementById(id)}"
"function showMsg(t,c){var m=$('msg');m.textContent=t;m.style.display='block';m.style.background=c}"
"function postJson(u,d){fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)})"
".then(r=>r.json()).then(j=>{showMsg(j.message||j.error||'OK',j.error?'#3a1111':'#113a11')}).catch(e=>showMsg(e,'#3a1111'))}"
"function refresh(){fetch('/api/status').then(r=>r.json()).then(d=>{"
"var h='';"
"h+='<div class=\"row\"><span class=\"label\">Emotion</span><span class=\"value\">'+d.emotion+'</span></div>';"
"h+='<div class=\"row\"><span class=\"label\">Battery</span><span class=\"value\">'+d.battery_percent+'% ('+d.battery_voltage+'V)</span></div>';"
"h+='<div class=\"row\"><span class=\"label\">WiFi</span><span class=\"value\">'+(d.wifi_ssid||'N/A')+' ('+d.wifi_rssi+' dBm)</span></div>';"
"h+='<div class=\"row\"><span class=\"label\">IP</span><span class=\"value\">'+(d.ip||'N/A')+'</span></div>';"
"h+='<div class=\"row\"><span class=\"label\">Uptime</span><span class=\"value\">'+Math.floor(d.uptime_sec)+'s</span></div>';"
"h+='<div class=\"row\"><span class=\"label\">Free Heap</span><span class=\"value\">'+d.free_heap+'</span></div>';"
"h+='<div class=\"row\"><span class=\"label\">Free PSRAM</span><span class=\"value\">'+d.free_psram+'</span></div>';"
"h+='<div class=\"row\"><span class=\"label\">MQTT</span><span class=\"value\">'+(d.mqtt_connected?'Connected':'Disconnected')+'</span></div>';"
"h+='<div class=\"row\"><span class=\"label\">Pomodoro</span><span class=\"value\">'+d.pomodoro_state+' ('+d.pomodoro_remaining+'s)</span></div>';"
"$('status').innerHTML=h}).catch(e=>$('status').innerHTML='Error: '+e)}"
"refresh();setInterval(refresh,5000);"
"function refreshOta(){fetch('/api/ota-progress').then(r=>r.json()).then(d=>{"
"if(d.state==='idle'){$('otaprog').textContent=''}"
"else{$('otaprog').textContent=d.state+': '+d.percent+'%'}}).catch(()=>{})}"
"setInterval(refreshOta,2000);"
"</script></body></html>";

/* ============================================================
 * Helper Functions
 * ============================================================ */

/**
 * @brief Set CORS headers for local development
 */
static void set_cors_headers(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, Authorization");
}

/**
 * @brief Send a JSON response with 200 status
 */
static esp_err_t send_json_ok(httpd_req_t *req, cJSON *json_obj)
{
    char *json_str = cJSON_PrintUnformatted(json_obj);
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    set_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
    cJSON_free(json_str);
    return ESP_OK;
}

/**
 * @brief Send a JSON error response with specified HTTP error code
 */
static esp_err_t send_json_error(httpd_req_t *req, const char *message, httpd_err_code_t code)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    cJSON_AddStringToObject(root, "error", message);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    set_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send_err(req, code, json_str);
    cJSON_free(json_str);
    return ESP_FAIL;
}

/**
 * @brief Read the full request body into a heap buffer
 *
 * Caller must free() the returned buffer.
 *
 * @return Allocated buffer with null-terminated body, or NULL on error
 */
static char *read_request_body(httpd_req_t *req)
{
    size_t len = req->content_len;
    if (len == 0 || len > WEB_SERVER_MAX_BODY_LEN) {
        return NULL;
    }

    char *buf = malloc(len + 1);
    if (buf == NULL) {
        return NULL;
    }

    int ret = httpd_req_recv(req, buf, len);
    if (ret <= 0) {
        free(buf);
        return NULL;
    }
    buf[ret] = '\0';
    return buf;
}

/**
 * @brief Check HTTP Basic Auth if configured
 *
 * Uses mbedtls_base64_encode to encode the expected credentials
 * and compares with the Authorization header value.
 *
 * @return ESP_OK if auth passed or disabled, ESP_FAIL otherwise
 */
static esp_err_t check_auth(httpd_req_t *req)
{
    if (!s_config.auth_enabled) {
        return ESP_OK;
    }

    int hdr_len = httpd_req_get_hdr_value_len(req, "Authorization");
    if (hdr_len <= 0) {
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"RobotBuddy\"");
        httpd_resp_send_err(req, HTTPD_401, "Unauthorized");
        return ESP_FAIL;
    }

    char hdr_val[256] = {0};
    if (hdr_len >= (int)sizeof(hdr_val) ||
        httpd_req_get_hdr_value_str(req, "Authorization", hdr_val, sizeof(hdr_val)) != ESP_OK) {
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"RobotBuddy\"");
        httpd_resp_send_err(req, HTTPD_401, "Unauthorized");
        return ESP_FAIL;
    }

    /* Expect "Basic <base64>" */
    if (strncmp(hdr_val, "Basic ", 6) != 0) {
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"RobotBuddy\"");
        httpd_resp_send_err(req, HTTPD_401, "Unauthorized");
        return ESP_FAIL;
    }

    /* Build expected credential string and base64-encode it */
    char expected[WEB_SERVER_USERNAME_MAX_LEN + WEB_SERVER_PASSWORD_MAX_LEN + 2] = {0};
    snprintf(expected, sizeof(expected), "%s:%s", s_config.username, s_config.password);

    size_t expected_b64_len = 0;
    char expected_b64[256] = {0};
    mbedtls_base64_encode((unsigned char *)expected_b64, sizeof(expected_b64),
                          &expected_b64_len,
                          (const unsigned char *)expected, strlen(expected));

    /* Compare the base64 portion of the header with our encoded credentials */
    const char *provided_b64 = hdr_val + 6;
    if (strlen(provided_b64) != expected_b64_len ||
        memcmp(provided_b64, expected_b64, expected_b64_len) != 0) {
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"RobotBuddy\"");
        httpd_resp_send_err(req, HTTPD_401, "Unauthorized");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Get WiFi SSID from ESP-IDF WiFi STA API
 */
static void get_wifi_ssid(char *buf, size_t buf_len)
{
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        snprintf(buf, buf_len, "%s", (char *)ap.ssid);
    } else {
        snprintf(buf, buf_len, "%s", "");
    }
}

/**
 * @brief Get WiFi RSSI from ESP-IDF WiFi STA API
 */
static int8_t get_wifi_rssi(void)
{
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        return ap.rssi;
    }
    return 0;
}

/**
 * @brief Get WiFi STA IP address string
 */
static void get_ip_address(char *buf, size_t buf_len)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        snprintf(buf, buf_len, "%s", "");
        return;
    }
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        snprintf(buf, buf_len, IPSTR, IP2STR(&ip_info.ip));
    } else {
        snprintf(buf, buf_len, "%s", "");
    }
}

/* ============================================================
 * OTA Event Handler (subscribed via event bus)
 * ============================================================ */

/**
 * @brief Track OTA progress by subscribing to OTA events
 */
static void ota_event_handler(const robot_event_t *event)
{
    if (event == NULL) {
        return;
    }

    switch (event->id) {
        case EVENT_OTA_START:
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            s_ota_state = OTA_STATE_DOWNLOADING;
            s_ota_percent = 0;
            xSemaphoreGive(s_mutex);
            break;

        case EVENT_OTA_PROGRESS:
            if (event->payload != NULL && event->payload_len >= sizeof(ota_progress_event_t)) {
                ota_progress_event_t *p = (ota_progress_event_t *)event->payload;
                xSemaphoreTake(s_mutex, portMAX_DELAY);
                s_ota_percent = p->percent;
                xSemaphoreGive(s_mutex);
            }
            break;

        case EVENT_OTA_COMPLETE:
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            s_ota_state = OTA_STATE_REBOOTING;
            s_ota_percent = 100;
            xSemaphoreGive(s_mutex);
            break;

        case EVENT_OTA_ERROR:
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            s_ota_state = OTA_STATE_ERROR;
            xSemaphoreGive(s_mutex);
            break;

        default:
            break;
    }
}

/* ============================================================
 * URI Handlers
 * ============================================================ */

/**
 * @brief GET / — Serve embedded HTML dashboard
 */
static esp_err_t handler_root(httpd_req_t *req)
{
    if (check_auth(req) != ESP_OK) {
        return ESP_FAIL;
    }

    set_cors_headers(req);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, s_dashboard_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief OPTIONS * — CORS preflight handler
 */
static esp_err_t handler_options(httpd_req_t *req)
{
    set_cors_headers(req);
    httpd_resp_set_hdr(req, "Access-Control-Max-Age", "86400");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/**
 * @brief GET /api/status — Return robot status as JSON
 *
 * Response: {emotion, battery_percent, battery_voltage, wifi_ssid,
 *            wifi_rssi, ip, uptime_sec, free_heap, free_psram,
 *            mqtt_connected, pomodoro_state, pomodoro_remaining}
 */
static esp_err_t handler_api_status(httpd_req_t *req)
{
    if (check_auth(req) != ESP_OK) {
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    /* Emotion */
    emotion_id_t eid = emotion_get_state();
    cJSON_AddStringToObject(root, "emotion", emotion_get_name(eid));

    /* Battery */
    cJSON_AddNumberToObject(root, "battery_percent", battery_monitor_get_percentage());
    cJSON_AddNumberToObject(root, "battery_voltage", (double)battery_monitor_get_voltage());

    /* WiFi */
    char ssid[33] = {0};
    get_wifi_ssid(ssid, sizeof(ssid));
    cJSON_AddStringToObject(root, "wifi_ssid", ssid);
    cJSON_AddNumberToObject(root, "wifi_rssi", (double)get_wifi_rssi());

    /* IP address */
    char ip[16] = {0};
    get_ip_address(ip, sizeof(ip));
    cJSON_AddStringToObject(root, "ip", ip);

    /* Uptime */
    int64_t now = esp_timer_get_time();
    double uptime = (double)(now - s_start_time) / 1000000.0;
    cJSON_AddNumberToObject(root, "uptime_sec", uptime);

    /* Memory */
    cJSON_AddNumberToObject(root, "free_heap", (double)esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "free_psram", (double)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    /* MQTT (simplified: default disconnected until MQTT module integrates) */
    cJSON_AddBoolToObject(root, "mqtt_connected", false);

    /* Pomodoro (default idle state) */
    const char *pomo_states[] = {"idle", "working", "break", "paused"};
    cJSON_AddStringToObject(root, "pomodoro_state", pomo_states[POMODORO_STATE_IDLE]);
    cJSON_AddNumberToObject(root, "pomodoro_remaining", 0);

    esp_err_t ret = send_json_ok(req, root);
    cJSON_Delete(root);
    return ret;
}

/**
 * @brief GET /api/config — Return device configuration as JSON
 *
 * Response: {wifi_ssid, cloud_provider, mqtt_broker, device_id}
 */
static esp_err_t handler_api_config(httpd_req_t *req)
{
    if (check_auth(req) != ESP_OK) {
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    /* WiFi SSID */
    char ssid[33] = {0};
    get_wifi_ssid(ssid, sizeof(ssid));
    cJSON_AddStringToObject(root, "wifi_ssid", ssid);

    /* Cloud provider */
    const char *providers[] = {"claude", "openai", "deepseek"};
    cloud_provider_t prov = cloud_get_provider();
    int prov_idx = (prov >= 0 && prov < CLOUD_PROVIDER_COUNT) ? (int)prov : 0;
    cJSON_AddStringToObject(root, "cloud_provider", providers[prov_idx]);

    /* MQTT broker — read from NVS if available */
    char mqtt_broker[128] = {0};
    nvs_handle_t handle;
    if (nvs_open("mqtt_cfg", NVS_READONLY, &handle) == ESP_OK) {
        size_t len = sizeof(mqtt_broker);
        if (nvs_get_str(handle, "broker", mqtt_broker, &len) != ESP_OK) {
            snprintf(mqtt_broker, sizeof(mqtt_broker), "%s", "");
        }
        nvs_close(handle);
    }
    cJSON_AddStringToObject(root, "mqtt_broker", mqtt_broker);

    /* Device ID — use ESP chip MAC */
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char device_id[18] = {0};
    snprintf(device_id, sizeof(device_id), "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    cJSON_AddStringToObject(root, "device_id", device_id);

    esp_err_t ret = send_json_ok(req, root);
    cJSON_Delete(root);
    return ret;
}

/**
 * @brief POST /api/wifi — Save WiFi credentials to NVS and reconnect
 *
 * Body: {ssid, password}
 */
static esp_err_t handler_api_wifi(httpd_req_t *req)
{
    if (check_auth(req) != ESP_OK) {
        return ESP_FAIL;
    }

    char *body = read_request_body(req);
    if (body == NULL) {
        return send_json_error(req, "Empty or too large body", HTTPD_400);
    }

    cJSON *root = cJSON_Parse(body);
    free(body);

    if (root == NULL) {
        return send_json_error(req, "Invalid JSON", HTTPD_400);
    }

    cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    cJSON *pass_item = cJSON_GetObjectItemCaseSensitive(root, "password");

    if (!cJSON_IsString(ssid_item) || ssid_item->valuestring[0] == '\0') {
        cJSON_Delete(root);
        return send_json_error(req, "Missing or empty 'ssid'", HTTPD_400);
    }

    const char *ssid = ssid_item->valuestring;
    const char *pass = (cJSON_IsString(pass_item)) ? pass_item->valuestring : "";

    /* Save credentials to NVS using wifi_manager's namespace and keys */
    nvs_handle_t handle;
    esp_err_t err = nvs_open(WIFI_MANAGER_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for WiFi save: %s", esp_err_to_name(err));
        cJSON_Delete(root);
        return send_json_error(req, "NVS write failed", HTTPD_500);
    }

    err = nvs_set_str(handle, WIFI_MANAGER_NVS_SSID_KEY, ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, WIFI_MANAGER_NVS_PASS_KEY, pass);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save WiFi credentials: %s", esp_err_to_name(err));
        cJSON_Delete(root);
        return send_json_error(req, "Failed to save credentials", HTTPD_500);
    }

    ESP_LOGI(TAG, "WiFi credentials saved, reconnecting to '%s'", ssid);

    /* Reconnect: stop then start WiFi manager to pick up new credentials */
    wifi_manager_stop();
    wifi_manager_start();

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "message", "WiFi credentials saved, reconnecting");
    esp_err_t ret = send_json_ok(req, resp);
    cJSON_Delete(resp);
    cJSON_Delete(root);
    return ret;
}

/**
 * @brief POST /api/ai-key — Save AI provider API key via cloud_manager NVS
 *
 * Body: {provider, api_key}
 */
static esp_err_t handler_api_ai_key(httpd_req_t *req)
{
    if (check_auth(req) != ESP_OK) {
        return ESP_FAIL;
    }

    char *body = read_request_body(req);
    if (body == NULL) {
        return send_json_error(req, "Empty or too large body", HTTPD_400);
    }

    cJSON *root = cJSON_Parse(body);
    free(body);

    if (root == NULL) {
        return send_json_error(req, "Invalid JSON", HTTPD_400);
    }

    cJSON *prov_item = cJSON_GetObjectItemCaseSensitive(root, "provider");
    cJSON *key_item  = cJSON_GetObjectItemCaseSensitive(root, "api_key");

    if (!cJSON_IsNumber(prov_item) || !cJSON_IsString(key_item)) {
        cJSON_Delete(root);
        return send_json_error(req, "Missing 'provider' or 'api_key'", HTTPD_400);
    }

    int provider = prov_item->valueint;
    if (provider < 0 || provider >= CLOUD_PROVIDER_COUNT) {
        cJSON_Delete(root);
        return send_json_error(req, "Invalid provider ID", HTTPD_400);
    }

    /* Save API key to cloud_manager's NVS namespace */
    char nvs_key[24] = {0};
    snprintf(nvs_key, sizeof(nvs_key), "%s%d", CLOUD_NVS_API_KEY_PREFIX, provider);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CLOUD_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for API key save: %s", esp_err_to_name(err));
        cJSON_Delete(root);
        return send_json_error(req, "NVS write failed", HTTPD_500);
    }

    err = nvs_set_str(handle, nvs_key, key_item->valuestring);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save API key: %s", esp_err_to_name(err));
        cJSON_Delete(root);
        return send_json_error(req, "Failed to save API key", HTTPD_500);
    }

    ESP_LOGI(TAG, "API key saved for provider %d", provider);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "message", "API key saved");
    esp_err_t ret = send_json_ok(req, resp);
    cJSON_Delete(resp);
    cJSON_Delete(root);
    return ret;
}

/**
 * @brief POST /api/ota — Trigger OTA firmware upgrade
 *
 * Body: {url, sha256}
 * Publishes EVENT_OTA_START with URL and optional SHA256.
 */
static esp_err_t handler_api_ota(httpd_req_t *req)
{
    if (check_auth(req) != ESP_OK) {
        return ESP_FAIL;
    }

    char *body = read_request_body(req);
    if (body == NULL) {
        return send_json_error(req, "Empty or too large body", HTTPD_400);
    }

    cJSON *root = cJSON_Parse(body);
    free(body);

    if (root == NULL) {
        return send_json_error(req, "Invalid JSON", HTTPD_400);
    }

    cJSON *url_item  = cJSON_GetObjectItemCaseSensitive(root, "url");
    cJSON *sha_item  = cJSON_GetObjectItemCaseSensitive(root, "sha256");

    if (!cJSON_IsString(url_item) || url_item->valuestring[0] == '\0') {
        cJSON_Delete(root);
        return send_json_error(req, "Missing or empty 'url'", HTTPD_400);
    }

    /* Allocate OTA start payload — event_bus deep-copies it */
    typedef struct {
        char url[256];
        char sha256[65];
    } ota_start_payload_t;

    ota_start_payload_t *payload = malloc(sizeof(ota_start_payload_t));
    if (payload == NULL) {
        cJSON_Delete(root);
        return send_json_error(req, "Out of memory", HTTPD_500);
    }

    snprintf(payload->url, sizeof(payload->url), "%s", url_item->valuestring);
    if (cJSON_IsString(sha_item) && sha_item->valuestring[0] != '\0') {
        snprintf(payload->sha256, sizeof(payload->sha256), "%s", sha_item->valuestring);
    } else {
        payload->sha256[0] = '\0';
    }

    robot_event_t event = {
        .id          = EVENT_OTA_START,
        .timestamp   = 0,
        .payload     = payload,
        .payload_len = sizeof(ota_start_payload_t),
    };
    event_bus_publish(&event);

    ESP_LOGI(TAG, "OTA upgrade triggered: %s", payload->url);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "message", "OTA upgrade started");
    esp_err_t ret = send_json_ok(req, resp);
    cJSON_Delete(resp);
    cJSON_Delete(root);
    return ret;
}

/**
 * @brief POST /api/emotion — Set robot emotion
 *
 * Body: {emotion_id}
 * Publishes EVENT_EMOTION_STATE_CHANGE.
 */
static esp_err_t handler_api_emotion(httpd_req_t *req)
{
    if (check_auth(req) != ESP_OK) {
        return ESP_FAIL;
    }

    char *body = read_request_body(req);
    if (body == NULL) {
        return send_json_error(req, "Empty or too large body", HTTPD_400);
    }

    cJSON *root = cJSON_Parse(body);
    free(body);

    if (root == NULL) {
        return send_json_error(req, "Invalid JSON", HTTPD_400);
    }

    cJSON *eid_item = cJSON_GetObjectItemCaseSensitive(root, "emotion_id");
    if (!cJSON_IsNumber(eid_item)) {
        cJSON_Delete(root);
        return send_json_error(req, "Missing 'emotion_id'", HTTPD_400);
    }

    int eid = eid_item->valueint;
    if (eid < 0 || eid >= EMOTION_COUNT) {
        cJSON_Delete(root);
        return send_json_error(req, "Invalid emotion_id", HTTPD_400);
    }

    /* Publish emotion state change event */
    emotion_event_t *payload = malloc(sizeof(emotion_event_t));
    if (payload == NULL) {
        cJSON_Delete(root);
        return send_json_error(req, "Out of memory", HTTPD_500);
    }

    payload->emotion_id  = (emotion_id_t)eid;
    payload->duration_ms = 0;
    payload->intensity   = 255;

    robot_event_t event = {
        .id          = EVENT_EMOTION_STATE_CHANGE,
        .timestamp   = 0,
        .payload     = payload,
        .payload_len = sizeof(emotion_event_t),
    };
    event_bus_publish(&event);

    ESP_LOGI(TAG, "Emotion set to %s (%d)", emotion_get_name((emotion_id_t)eid), eid);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "message", "Emotion updated");
    esp_err_t ret = send_json_ok(req, resp);
    cJSON_Delete(resp);
    cJSON_Delete(root);
    return ret;
}

/**
 * @brief POST /api/motion — Send motion command
 *
 * Body: {cmd, speed, duration}
 * Publishes EVENT_MOTION_COMMAND.
 */
static esp_err_t handler_api_motion(httpd_req_t *req)
{
    if (check_auth(req) != ESP_OK) {
        return ESP_FAIL;
    }

    char *body = read_request_body(req);
    if (body == NULL) {
        return send_json_error(req, "Empty or too large body", HTTPD_400);
    }

    cJSON *root = cJSON_Parse(body);
    free(body);

    if (root == NULL) {
        return send_json_error(req, "Invalid JSON", HTTPD_400);
    }

    cJSON *cmd_item = cJSON_GetObjectItemCaseSensitive(root, "cmd");
    cJSON *spd_item = cJSON_GetObjectItemCaseSensitive(root, "speed");
    cJSON *dur_item = cJSON_GetObjectItemCaseSensitive(root, "duration");

    if (!cJSON_IsNumber(cmd_item)) {
        cJSON_Delete(root);
        return send_json_error(req, "Missing 'cmd'", HTTPD_400);
    }

    int cmd = cmd_item->valueint;
    if (cmd < 0 || cmd > (int)MOTION_CMD_ROTATE) {
        cJSON_Delete(root);
        return send_json_error(req, "Invalid motion command", HTTPD_400);
    }

    /* Publish motion command event */
    motion_cmd_payload_t *payload = malloc(sizeof(motion_cmd_payload_t));
    if (payload == NULL) {
        cJSON_Delete(root);
        return send_json_error(req, "Out of memory", HTTPD_500);
    }

    payload->command     = (motion_cmd_t)cmd;
    payload->speed       = (int16_t)(cJSON_IsNumber(spd_item) ? spd_item->valueint : 150);
    payload->angle       = 0;
    payload->duration_ms = (uint16_t)(cJSON_IsNumber(dur_item) ? dur_item->valueint : 0);

    robot_event_t event = {
        .id          = EVENT_MOTION_COMMAND,
        .timestamp   = 0,
        .payload     = payload,
        .payload_len = sizeof(motion_cmd_payload_t),
    };
    event_bus_publish(&event);

    ESP_LOGI(TAG, "Motion command: cmd=%d speed=%d duration=%d",
             cmd, payload->speed, payload->duration_ms);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "message", "Motion command sent");
    esp_err_t ret = send_json_ok(req, resp);
    cJSON_Delete(resp);
    cJSON_Delete(root);
    return ret;
}

/**
 * @brief POST /api/pomodoro — Control pomodoro timer
 *
 * Body: {action: "start"/"pause"/"stop"}
 * Publishes appropriate pomodoro event via event_bus.
 */
static esp_err_t handler_api_pomodoro(httpd_req_t *req)
{
    if (check_auth(req) != ESP_OK) {
        return ESP_FAIL;
    }

    char *body = read_request_body(req);
    if (body == NULL) {
        return send_json_error(req, "Empty or too large body", HTTPD_400);
    }

    cJSON *root = cJSON_Parse(body);
    free(body);

    if (root == NULL) {
        return send_json_error(req, "Invalid JSON", HTTPD_400);
    }

    cJSON *action_item = cJSON_GetObjectItemCaseSensitive(root, "action");
    if (!cJSON_IsString(action_item)) {
        cJSON_Delete(root);
        return send_json_error(req, "Missing 'action'", HTTPD_400);
    }

    const char *action = action_item->valuestring;
    robot_event_id_t event_id;

    if (strcmp(action, "start") == 0) {
        event_id = EVENT_POMODORO_START;
    } else if (strcmp(action, "pause") == 0) {
        event_id = EVENT_POMODORO_START;  /* Pause signaled via payload flag */
    } else if (strcmp(action, "stop") == 0) {
        event_id = EVENT_POMODORO_DONE;
    } else {
        cJSON_Delete(root);
        return send_json_error(req, "Invalid action (use start, pause, or stop)", HTTPD_400);
    }

    /* Publish pomodoro event with payload */
    pomodoro_event_t *payload = malloc(sizeof(pomodoro_event_t));
    if (payload == NULL) {
        cJSON_Delete(root);
        return send_json_error(req, "Out of memory", HTTPD_500);
    }

    memset(payload, 0, sizeof(pomodoro_event_t));
    if (strcmp(action, "pause") == 0) {
        payload->remaining_sec = 1;  /* Flag to indicate pause request */
    }

    robot_event_t event = {
        .id          = event_id,
        .timestamp   = 0,
        .payload     = payload,
        .payload_len = sizeof(pomodoro_event_t),
    };
    event_bus_publish(&event);

    ESP_LOGI(TAG, "Pomodoro action: %s", action);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "message", "Pomodoro command sent");
    esp_err_t ret = send_json_ok(req, resp);
    cJSON_Delete(resp);
    cJSON_Delete(root);
    return ret;
}

/**
 * @brief GET /api/ota-progress — Return OTA upgrade progress
 *
 * Response: {state, percent}
 */
static esp_err_t handler_api_ota_progress(httpd_req_t *req)
{
    if (check_auth(req) != ESP_OK) {
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    const char *state_names[] = {
        "idle", "downloading", "verifying", "applying", "rebooting", "error"
    };

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int state_idx = (s_ota_state <= OTA_STATE_ERROR) ? (int)s_ota_state : 0;
    uint8_t percent = s_ota_percent;
    xSemaphoreGive(s_mutex);

    cJSON_AddStringToObject(root, "state", state_names[state_idx]);
    cJSON_AddNumberToObject(root, "percent", percent);

    esp_err_t ret = send_json_ok(req, root);
    cJSON_Delete(root);
    return ret;
}

/* ============================================================
 * URI Registration Table
 * ============================================================ */

static const httpd_uri_t s_uris[] = {
    { .uri = "/",                 .method = HTTP_GET,     .handler = handler_root,             .user_ctx = NULL },
    { .uri = "/api/status",       .method = HTTP_GET,     .handler = handler_api_status,       .user_ctx = NULL },
    { .uri = "/api/config",       .method = HTTP_GET,     .handler = handler_api_config,       .user_ctx = NULL },
    { .uri = "/api/wifi",         .method = HTTP_POST,    .handler = handler_api_wifi,         .user_ctx = NULL },
    { .uri = "/api/ai-key",       .method = HTTP_POST,    .handler = handler_api_ai_key,       .user_ctx = NULL },
    { .uri = "/api/ota",          .method = HTTP_POST,    .handler = handler_api_ota,          .user_ctx = NULL },
    { .uri = "/api/emotion",      .method = HTTP_POST,    .handler = handler_api_emotion,      .user_ctx = NULL },
    { .uri = "/api/motion",       .method = HTTP_POST,    .handler = handler_api_motion,       .user_ctx = NULL },
    { .uri = "/api/pomodoro",     .method = HTTP_POST,    .handler = handler_api_pomodoro,     .user_ctx = NULL },
    { .uri = "/api/ota-progress", .method = HTTP_GET,     .handler = handler_api_ota_progress, .user_ctx = NULL },
    { .uri = "/*",                .method = HTTP_OPTIONS,  .handler = handler_options,          .user_ctx = NULL },
};

#define URI_COUNT (sizeof(s_uris) / sizeof(s_uris[0]))

/* ============================================================
 * Public API
 * ============================================================ */

esp_err_t web_server_init(const web_server_config_t *config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    /* Create mutex for shared state */
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Apply config or defaults */
    if (config != NULL) {
        memcpy(&s_config, config, sizeof(s_config));
    } else {
        s_config.port            = WEB_SERVER_DEFAULT_PORT;
        s_config.max_connections = WEB_SERVER_DEFAULT_MAX_CONNECTIONS;
        s_config.auth_enabled    = false;
        memset(s_config.username, 0, sizeof(s_config.username));
        memset(s_config.password, 0, sizeof(s_config.password));
    }

    /* Subscribe to OTA events for progress tracking */
    event_bus_subscribe(EVENT_OTA_START, ota_event_handler);
    event_bus_subscribe(EVENT_OTA_PROGRESS, ota_event_handler);
    event_bus_subscribe(EVENT_OTA_COMPLETE, ota_event_handler);
    event_bus_subscribe(EVENT_OTA_ERROR, ota_event_handler);

    s_start_time  = esp_timer_get_time();
    s_ota_state   = OTA_STATE_IDLE;
    s_ota_percent = 0;
    s_initialized = true;

    ESP_LOGI(TAG, "Initialized (port=%u, max_conn=%u, auth=%s)",
             s_config.port, s_config.max_connections,
             s_config.auth_enabled ? "enabled" : "disabled");

    return ESP_OK;
}

esp_err_t web_server_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    /* Stop server if running */
    if (s_running) {
        web_server_stop();
    }

    /* Unsubscribe from OTA events */
    event_bus_unsubscribe(EVENT_OTA_START, ota_event_handler);
    event_bus_unsubscribe(EVENT_OTA_PROGRESS, ota_event_handler);
    event_bus_unsubscribe(EVENT_OTA_COMPLETE, ota_event_handler);
    event_bus_unsubscribe(EVENT_OTA_ERROR, ota_event_handler);

    if (s_mutex != NULL) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "Deinitialized");

    return ESP_OK;
}

esp_err_t web_server_start(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_running) {
        ESP_LOGW(TAG, "Already running");
        return ESP_OK;
    }

    /* Configure and start the HTTP server */
    httpd_config_t httpd_cfg = HTTPD_DEFAULT_CONFIG();
    httpd_cfg.server_port      = s_config.port;
    httpd_cfg.max_uri_handlers = 16;
    httpd_cfg.max_open_sockets = s_config.max_connections;
    httpd_cfg.lru_purge_enable = true;
    httpd_cfg.stack_size       = 8192;

    ESP_LOGI(TAG, "Starting HTTP server on port %u", s_config.port);

    esp_err_t err = httpd_start(&s_server, &httpd_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    /* Register all URI handlers */
    for (size_t i = 0; i < URI_COUNT; i++) {
        err = httpd_register_uri_handler(s_server, &s_uris[i]);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register URI %s: %s",
                     s_uris[i].uri, esp_err_to_name(err));
        }
    }

    s_start_time = esp_timer_get_time();
    s_running = true;

    ESP_LOGI(TAG, "HTTP server running on port %u", s_config.port);
    return ESP_OK;
}

esp_err_t web_server_stop(void)
{
    if (!s_running) {
        ESP_LOGW(TAG, "Not running");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Stopping HTTP server");
    esp_err_t err = httpd_stop(s_server);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    s_server = NULL;
    s_running = false;

    ESP_LOGI(TAG, "HTTP server stopped");
    return ESP_OK;
}

bool web_server_is_running(void)
{
    return s_running;
}
