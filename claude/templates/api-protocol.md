# Cloud API Protocol Template

> RobotBuddy 云端 AI 接口协议定义

---

## API 基础信息

| 项目 | 内容 |
|------|------|
| **Base URL** | `https://api.robotbuddy.local/v1` |
| **WebSocket URL** | `wss://api.robotbuddy.local/v1` |
| **认证方式** | Bearer Token |
| **数据格式** | JSON (REST), Binary (WS Audio) |
| **字符编码** | UTF-8 |
| **超时时间** | REST: 10s, WS: 30s |
| **协议版本** | 1.0 |

---

## 通用头

```http
POST /v1/chat HTTP/1.1
Host: api.robotbuddy.local
Content-Type: application/json
Authorization: Bearer {device_token}
X-Device-ID: {device_mac}
X-API-Version: 1.0
```

## 通用错误响应

```json
{
    "error": {
        "code": "invalid_token",
        "message": "认证令牌无效或已过期",
        "details": null
    }
}
```

### 错误码列表

| HTTP Status | Error Code | 说明 |
|-------------|-----------|------|
| 400 | `invalid_request` | 请求参数有误 |
| 401 | `invalid_token` | Token 无效或过期 |
| 403 | `insufficient_quota` | API 配额用尽 |
| 404 | `not_found` | 资源不存在 |
| 429 | `rate_limited` | 请求频率过高 |
| 500 | `internal_error` | 服务器内部错误 |
| 503 | `service_unavailable` | 服务暂时不可用 |

---

## 接口定义

### 1. AI 对话

**POST** `/v1/chat`

```json
// Request
{
    "model": "claude-sonnet-5",
    "messages": [
        {"role": "system", "content": "你是 RobotBuddy，一个桌面 AI 编程助手机器人。"},
        {"role": "user", "content": "帮我解释这段 Python 装饰器的原理"}
    ],
    "max_tokens": 1024,
    "temperature": 0.7,
    "stream": false
}

// Response
{
    "id": "chat_abc123",
    "model": "claude-sonnet-5",
    "choices": [
        {
            "index": 0,
            "message": {
                "role": "assistant",
                "content": "Python 装饰器本质上是一个..."
            },
            "finish_reason": "stop"
        }
    ],
    "usage": {
        "prompt_tokens": 50,
        "completion_tokens": 200,
        "total_tokens": 250
    }
}
```

### 2. 语音识别 (WebSocket)

**WS** `/v1/asr/stream`

```
// Client → Server (Binary messages)
[PCM 16kHz 16bit Mono audio chunks, 320 bytes per chunk (10ms)]

// Server → Client (JSON control messages)
{
    "type": "transcription",
    "text": "帮我解释",
    "is_final": false,
    "confidence": 0.95
}

{
    "type": "transcription",
    "text": "帮我解释这段 Python 装饰器的原理",
    "is_final": true,
    "confidence": 0.97
}
```

### 3. 语音合成 (WebSocket)

**WS** `/v1/tts/stream`

```
// Client → Server (JSON)
{
    "text": "Python 装饰器本质上是一个接受函数作为参数的函数...",
    "voice": "buddy_default",
    "speed": 1.0,
    "format": "pcm",
    "sample_rate": 16000
}

// Server → Client (Binary messages)
[PCM 16kHz 16bit Mono audio chunks, ~640 bytes per chunk (20ms)]

// Server → Client (JSON)
{
    "type": "end",
    "total_duration_ms": 3500
}
```

### 4. 编译状态推送

**PUT** `/v1/build/status`

```json
// Request (从 VS Code 插件/Local Bridge 发来)
{
    "device_id": "RB-2026-0001",
    "project": "robotbuddy-firmware",
    "status": "success",
    "errors": 0,
    "warnings": 2,
    "duration_ms": 12345,
    "output_summary": "Build complete. 2 warnings in motion_control.c"
}

// Response
{
    "received": true,
    "emotion_triggered": "HAPPY"
}
```

### 5. Git 状态推送

**PUT** `/v1/git/status`

```json
// Request
{
    "device_id": "RB-2026-0001",
    "branch": "feature/new-emotion",
    "ahead": 3,
    "behind": 0,
    "changed_files": 5,
    "untracked_files": 1,
    "last_commit_message": "Add EXCITED emotion animation"
}

// Response
{
    "received": true,
    "emotion_triggered": "INFO"
}
```

### 6. OTA 检查

**GET** `/v1/ota/check?device_id={id}&current_version={ver}`

```json
// Response (有更新)
{
    "has_update": true,
    "version": "1.1.0",
    "size": 2097152,
    "changelog": "- 新增代码片段滚动显示\n- 修复 WiFi 断连 Bug",
    "sha256": "a1b2c3d4...",
    "url": "https://ota.robotbuddy.local/firmware_v1.1.0.bin",
    "mandatory": false
}

// Response (无更新)
{
    "has_update": false,
    "version": "1.0.0"
}
```

---

## MQTT Topics

| Topic | 方向 | QoS | 说明 |
|-------|------|-----|------|
| `robotbuddy/{id}/build/status` | PC → Robot | 1 | 编译状态 |
| `robotbuddy/{id}/git/status` | PC → Robot | 1 | Git 状态 |
| `robotbuddy/{id}/notification` | PC/Cloud → Robot | 1 | 通知推送 |
| `robotbuddy/{id}/ota/command` | Cloud → Robot | 2 | OTA 升级指令 |
| `robotbuddy/{id}/status` | Robot → Cloud | 0 | 机器人状态上报 (60s) |
| `robotbuddy/{id}/heartbeat` | Robot → Cloud | 0 | 心跳 (10s) |

---

## 安全规范

```
1. 设备注册 → 获取唯一 Token
   POST /v1/device/register
   { "mac": "AA:BB:CC:DD:EE:FF", "model": "RobotBuddy-V1" }
   → { "device_id": "RB-2026-0001", "token": "eyJ..." }

2. Token 存储在 NVS 加密分区
   nvs_handle_t nvs;
   nvs_open("secure", NVS_READWRITE, &nvs);
   nvs_set_str(nvs, "cloud_token", token);

3. 每个请求携带:
   - Authorization: Bearer {token}
   - X-Device-ID: {device_id}

4. Token 过期 → 自动刷新
   401 → POST /v1/device/refresh → 新 Token

5. TLS 1.2+ 必须验证服务端证书
```
