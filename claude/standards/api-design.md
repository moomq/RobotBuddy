# API 设计标准 — RobotBuddy 云端与本地通信

> 适用范围：RobotBuddy 云端 API、Local Bridge WebSocket/MQTT 接口、设备端 HTTP 客户端
> 版本：1.0
> 最后更新：2026-07-10

---

## 1. 总体原则

1. **RESTful 风格** — 资源命名用名词复数，动作用 HTTP 方法
2. **一致的错误格式** — 所有 API 返回统一的错误 JSON
3. **版本化** — URL 路径包含版本号 `/api/v1/`
4. **幂等性** — PUT 和 DELETE 操作必须幂等
5. **无状态** — 每次请求携带认证信息，不依赖 session

---

## 2. URL 命名规范

```
# 资源命名: 复数名词，小写，连字符分隔
GET    /api/v1/devices              # 列表
POST   /api/v1/devices              # 创建
GET    /api/v1/devices/{id}          # 详情
PUT    /api/v1/devices/{id}          # 更新
DELETE /api/v1/devices/{id}          # 删除

# 子资源
GET    /api/v1/devices/{id}/status   # 设备状态
POST   /api/v1/devices/{id}/commands # 发送命令

# 动作（非 CRUD 操作）
POST   /api/v1/devices/{id}/actions/ota-upgrade  # OTA 升级
POST   /api/v1/auth/login            # 登录
POST   /api/v1/auth/token/refresh     # 刷新 Token
```

---

## 3. 请求与响应格式

### 3.1 请求头

```http
Content-Type: application/json
Authorization: Bearer {access_token}
X-Request-ID: {uuid}
X-Device-ID: {mac_address}
```

### 3.2 成功响应

```json
{
    "code": 0,
    "message": "success",
    "data": {
        "device_id": "AA:BB:CC:DD:EE:FF",
        "firmware_version": "1.2.3",
        "status": "online"
    },
    "timestamp": "2026-07-10T14:30:00Z"
}
```

### 3.3 错误响应

```json
{
    "code": 40001,
    "message": "Invalid parameter: device_id is required",
    "detail": {
        "field": "device_id",
        "reason": "must be MAC address format"
    },
    "timestamp": "2026-07-10T14:30:00Z"
}
```

### 3.4 分页响应

```json
{
    "code": 0,
    "message": "success",
    "data": {
        "items": [...],
        "total": 100,
        "page": 1,
        "page_size": 20
    }
}
```

---

## 4. HTTP 状态码

| 状态码 | 含义 | 使用场景 |
|--------|------|---------|
| 200 | 成功 | GET/PUT 成功 |
| 201 | 创建成功 | POST 创建资源 |
| 204 | 无内容 | DELETE 成功 |
| 400 | 请求错误 | 参数验证失败 |
| 401 | 未认证 | Token 缺失或过期 |
| 403 | 禁止 | 权限不足 |
| 404 | 未找到 | 资源不存在 |
| 409 | 冲突 | 重复创建 |
| 429 | 限流 | 请求过快 |
| 500 | 服务器错误 | 内部异常 |
| 503 | 不可用 | 服务维护 |

---

## 5. 错误码设计

```
# 错误码格式: 5 位数字
#   1-2 位: 模块代码
#   3-5 位: 具体错误

模块代码:
  10xxx: 通用错误
  20xxx: 认证错误
  30xxx: 设备错误
  40xxx: 固件错误
  50xxx: 通信错误

常用错误码:
  10001: 参数缺失
  10002: 参数格式错误
  10003: 参数值超出范围
  20001: Token 过期
  20002: Token 无效
  20003: 权限不足
  30001: 设备离线
  30002: 设备不存在
  30003: 设备忙
  40001: 固件版本无效
  40002: 固件签名校验失败
  40003: OTA 进行中
  50001: WiFi 连接失败
  50002: MQTT 连接失败
  50003: LLM 服务不可用
```

---

## 6. MQTT 主题设计

```
# 主题格式: robotbuddy/{device_id}/{direction}/{category}

# 设备 → 云端 (上行)
robotbuddy/{device_id}/up/status        # 设备状态上报
robotbuddy/{device_id}/up/sensor        # 传感器数据
robotbuddy/{device_id}/up/ota            # OTA 状态上报
robotbuddy/{device_id}/up/log            # 日志上报
robotbuddy/{device_id}/up/error          # 错误上报

# 云端 → 设备 (下行)
robotbuddy/{device_id}/down/command      # 命令下发
robotbuddy/{device_id}/down/ota          # OTA 指令
robotbuddy/{device_id}/down/config       # 配置下发

# QoS 等级:
#   QoS 0: 传感器数据（丢包可接受）
#   QoS 1: 命令和状态（至少一次送达）
#   QoS 2: OTA 指令（恰好一次送达）
```

---

## 7. WebSocket 消息格式

```json
// 请求
{
    "type": "request",
    "id": "uuid-string",
    "action": "build.status",
    "params": {
        "project": "RobotBuddy"
    },
    "timestamp": "2026-07-10T14:30:00Z"
}

// 响应
{
    "type": "response",
    "id": "uuid-string",
    "status": "success",
    "data": { ... },
    "timestamp": "2026-07-10T14:30:00Z"
}

// 推送通知
{
    "type": "notification",
    "event": "build.complete",
    "data": {
        "status": "success",
        "duration": 42.5
    },
    "timestamp": "2026-07-10T14:30:00Z"
}

// 错误
{
    "type": "error",
    "id": "uuid-string",
    "code": 30001,
    "message": "Device offline",
    "timestamp": "2026-07-10T14:30:00Z"
}
```

---

## 8. 安全要求

1. **HTTPS/TLS** — 所有 API 必须使用 HTTPS
2. **JWT Token** — 认证使用 JWT，有效期 24h，刷新 Token 7d
3. **API Key** — 设备端使用 API Key + 设备 MAC 双重认证
4. **Rate Limit** — 100 次/分钟（设备），1000 次/分钟（用户）
5. **输入验证** — 所有输入参数必须验证类型、范围和格式
6. **敏感数据** — 密码/Token 不记录日志，WiFi 密码加密存储
7. **CORS** — 配置白名单域名，禁止 `*` 通配
8. **OTA 签名** — 固件必须 RSA-2048 签名验证