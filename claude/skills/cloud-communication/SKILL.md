# Cloud Communication Skill — RobotBuddy

## Role

RobotBuddy 云端通信专家，负责 WiFi 连接管理、HTTP/WebSocket 通信协议、MQTT 实时推送和 OTA 固件升级。

## Domain

ESP32 WiFi 堆栈 (ESP-NETIF)、TLS/mbedTLS、HTTP Client、WebSocket、MQTT、ESP HTTPS OTA。

## Goal

构建稳定、安全、低延迟的机器人与云端 AI 后端通信链路。

## Inputs

- 云端 AI API 规范
- MQTT Broker 配置
- OTA 服务器部署信息

## Outputs

- `firmware/services/cloud/wifi_manager.c` — WiFi 连接管理
- `firmware/services/cloud/http_client.c` — HTTP 客户端封装
- `firmware/services/cloud/ws_client.c` — WebSocket 客户端
- `firmware/services/cloud/mqtt_client.c` — MQTT 客户端
- `firmware/services/cloud/ota_service.c` — OTA 升级服务
- `docs/cloud-protocol.md` — 云端通信协议文档

## Cloud Communication Architecture

```
┌──────────────────────────────────────────────────────────┐
│                  Cloud Communication Stack                 │
│                                                           │
│  ┌─────────────────────────────────────────────────────┐ │
│  │                   Application                        │ │
│  │  AI Dialog  │  Build Status │  Git Notify │ OTA     │ │
│  └──────────┬──────────┬──────────┬──────────┬─────────┘ │
│             │          │          │          │            │
│  ┌──────────↓──────────↓──────────↓──────────↓─────────┐ │
│  │                Transport Layer                       │ │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌───────┐  │ │
│  │  │ REST API │ │WebSocket │ │  MQTT    │ │ HTTPS │  │ │
│  │  │ (txt)    │ │(audio)   │ │(events)  │ │(OTA)  │  │ │
│  │  └────┬─────┘ └────┬─────┘ └────┬─────┘ └───┬───┘  │ │
│  └───────┼────────────┼────────────┼────────────┼──────┘ │
│          │            │            │            │         │
│  ┌───────↓────────────↓────────────↓────────────↓──────┐ │
│  │                  TLS / mbedTLS                       │ │
│  │             (证书验证 + 加密传输)                      │ │
│  └────────────────────────┬────────────────────────────┘ │
│                           │                               │
│  ┌────────────────────────↓────────────────────────────┐ │
│  │                  WiFi Manager                        │ │
│  │        (连接/断线/重连/信号监测)                       │ │
│  └─────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────┘
```

## WiFi Manager

```c
// WiFi 状态机
typedef enum {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_DISCONNECTING,
    WIFI_STATE_ERROR,
} wifi_state_t;

// WiFi 配置
typedef struct {
    char ssid[33];
    char password[65];
    bool use_static_ip;
    uint8_t retry_max;          // 最大重试次数
    uint32_t retry_interval_ms; // 重试间隔
} wifi_config_t;

// 初始化 + 连接
esp_err_t wifi_manager_init(const wifi_config_t *config);
esp_err_t wifi_manager_start(void);

// 获取状态
wifi_state_t wifi_get_state(void);
int8_t wifi_get_rssi(void);     // dBm

// 注册状态回调
void wifi_on_state_change(void (*callback)(wifi_state_t old, wifi_state_t new));
```

## HTTP Client

```c
// REST API 封装 — 主要用于 LLM 文本交互

// 同步请求 (用于简单 API)
esp_err_t http_post_json(const char *url, const char *json_body,
                         char *response_buf, size_t buf_size, int timeout_ms);

// 异步请求 (不阻塞调用 Task)
typedef void (*http_callback_t)(int status_code, const char *response, void *user_data);
esp_err_t http_post_json_async(const char *url, const char *json_body,
                               http_callback_t callback, void *user_data, int timeout_ms);

// AI Chat 请求
esp_err_t cloud_chat_send(const char *prompt, const char *context, http_callback_t callback);
```

## WebSocket Client (Audio Streaming)

```c
// WebSocket 客户端 — 用于音频流

typedef struct {
    char url[256];
    int reconnect_max;
    int ping_interval_sec;
} ws_client_config_t;

typedef enum {
    WS_STATE_DISCONNECTED,
    WS_STATE_CONNECTING,
    WS_STATE_CONNECTED,
    WS_STATE_ERROR,
} ws_state_t;

typedef void (*ws_on_message_t)(const uint8_t *data, size_t len, bool is_binary);

esp_err_t ws_client_init(const ws_client_config_t *config);

// 音频上行
esp_err_t ws_send_audio_chunk(const uint8_t *pcm, size_t len);

// 音频下行 (TTS)
esp_err_t ws_on_tts_data(ws_on_message_t on_data);

// 连接管理
esp_err_t ws_connect(void);
esp_err_t ws_disconnect(void);
```

## MQTT Client (Event Push)

```c
// MQTT 客户端 — 用于实时事件推送

// Topics:
//   robotbuddy/{device_id}/build/status     — 编译状态
//   robotbuddy/{device_id}/git/status       — Git 状态
//   robotbuddy/{device_id}/notification     — 通知
//   robotbuddy/{device_id}/ota/command      — OTA 升级命令
//   robotbuddy/{device_id}/status           — 机器人状态上报

typedef void (*mqtt_message_handler_t)(const char *topic, const char *payload);

esp_err_t mqtt_manager_init(const char *broker_url, const char *device_id);
esp_err_t mqtt_subscribe(const char *topic, mqtt_message_handler_t handler);
esp_err_t mqtt_publish(const char *topic, const char *payload, int qos);

// 预定义消息处理器
void mqtt_handle_build_status(const char *payload);      // VS Code plugin → 机器人
void mqtt_handle_git_status(const char *payload);        // Git webhook → 机器人
void mqtt_handle_ota_command(const char *payload);       // OTA 平台 → 机器人
```

## OTA Service

```c
// OTA 固件升级

typedef struct {
    char firmware_url[256];
    char firmware_version[32];
    size_t firmware_size;
    char sha256[65];
} ota_update_info_t;

// 检查更新
esp_err_t ota_check_update(ota_update_info_t *info);

// 下载 + 升级 + 重启
esp_err_t ota_perform_update(const ota_update_info_t *info,
                             void (*progress_cb)(int percent));

// 回滚到上一个版本
esp_err_t ota_rollback(void);
```

## Cloud API Endpoints

```c
#define CLOUD_API_BASE          "https://api.robotbuddy.local/v1"

// AI 对话
#define API_CHAT                CLOUD_API_BASE "/chat"
// POST { "prompt": "...", "context": [...], "model": "claude" }

// ASR (WebSocket)
#define WS_ASR                  "wss://api.robotbuddy.local/v1/asr/stream"

// TTS (WebSocket 或 REST)
#define API_TTS                 CLOUD_API_BASE "/tts"
#define WS_TTS                  "wss://api.robotbuddy.local/v1/tts/stream"

// 编译状态
#define API_BUILD_STATUS        CLOUD_API_BASE "/build/status"

// Git 状态
#define API_GIT_STATUS          CLOUD_API_BASE "/git/status"

// OTA
#define API_OTA_CHECK           CLOUD_API_BASE "/ota/check"
#define API_OTA_DOWNLOAD        CLOUD_API_BASE "/ota/download"
```

## Security Requirements

```
TLS:         mbedTLS, verify server certificate
Cert:        预置 CA 根证书 (PEM format)
Token:       Bearer token 认证 (设备注册时获取)
Device ID:   ESP32 内置 MAC → 唯一设备标识
Encryption:  所有通信 TLS 1.2+
OTA Sign:    RSA-2048 签名校验 → 仅刷入可信固件
```

## Rules

1. WiFi 断线自动重连，指数退避 (1s, 2s, 4s, ..., max 60s)
2. 所有 HTTP/WS 请求必须设置超时
3. MQTT Keep Alive ≤ 60s
4. TLS 证书必须验证（生产环境不能跳过）
5. 网络错误必须向上层报告（显示连接状态表情）
6. OTA 下载前检查 Flash 空间
7. OTA 升级成功后才标记 boot partition
8. WiFi 连接凭据存储在 NVS 加密分区

## Checklist

- [ ] WiFi 配网流程正常 (SmartConfig / BLE / 热点)
- [ ] WiFi 断线重连自动恢复
- [ ] TLS 握手成功 (证书验证通过)
- [ ] HTTP API 响应正确 (200 OK)
- [ ] WebSocket 双向音频流稳定 (无断流)
- [ ] MQTT 订阅/发布正常 (QoS 0/1)
- [ ] OTA 下载+升级+重启 全流程验证
- [ ] OTA 回滚功能正常
- [ ] 弱网环境下降级策略生效
- [ ] Device ID 唯一且持久
