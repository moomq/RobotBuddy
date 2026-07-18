# RobotBuddy OTA 升级功能需求分析

> **版本:** 1.0
> **日期:** 2026-07-19
> **优先级:** P0（MVP 核心功能）
> **状态:** 需求分析完成

---

## 目录

1. [功能概述](#1-功能概述)
2. [硬件依赖分析](#2-硬件依赖分析)
3. [FreeRTOS 任务影响](#3-freertos-任务影响)
4. [云/网络依赖](#4-云网络依赖)
5. [异常处理](#5-异常处理)
6. [验收条件](#6-验收条件)
7. [功能需求清单](#7-功能需求清单)
8. [检查清单](#8-检查清单)

---

## 1. 功能概述

### 1.1 功能描述

**一句话定义：** 实现安全的远程固件升级功能，支持通过 HTTPS 从云端下载固件、签名验证、自动安装和回滚。

**触发条件：**
- MQTT 云端命令触发（服务器主动推送升级通知）
- 用户手动触发（通过 Web 控制台或 APP）
- 定时自动检查（每日固定时间检查更新）
- 系统启动后自动检查（开机联网后自动查询是否有新版本）

**期望结果：**
- 固件安全下载到备用分区
- RSA-2048 签名验证通过
- 新固件成功安装并启动
- 屏幕显示升级进度和状态
- 升级失败时自动回滚到上一个稳定版本

### 1.2 核心功能点

| 功能点 | 描述 | 优先级 |
|--------|------|--------|
| 版本检查 | 通过 HTTPS 查询云端是否有新固件 | P0 |
| 固件下载 | HTTPS 下载固件包到备用 OTA 分区 | P0 |
| 签名验证 | RSA-2048 签名验证 + SHA256 完整性校验 | P0 |
| 分区切换 | AB 分区滚动升级，永不覆盖 factory | P0 |
| 自动回滚 | 新固件运行异常时自动回滚到旧版本 | P0 |
| 进度显示 | 屏幕显示下载/安装进度百分比 | P1 |
| 断点续传 | 下载中断后支持从断点继续 | P1 |
| MQTT 命令 | 支持云端 MQTT 下发升级命令 | P1 |
| 电量检查 | 低电量时拒绝 OTA 升级 | P0 |
| Web 控制 | 本地 Web 页面手动触发升级 | P2 |

### 1.3 产品定位与价值

**在 RobotBuddy 产品中的定位：**
- **必备基础能力：** OTA 升级是 IoT 设备的标准功能，允许产品持续迭代和修复问题
- **安全基础：** 通过签名验证和加密传输，确保固件来源可信
- **用户体验：** 无需物理连接即可升级，降低维护成本
- **产品演进：** 为未来 V2.0/V3.0 功能迭代提供基础设施

**核心价值：**
```
开发者视角：
  └── 远程推送新功能/修复 Bug，无需返厂或数据线
      └── 降低维护成本，提高产品生命周期

用户视角：
  └── 机器人自动升级，获得新功能
      └── 无感升级体验，升级失败自动恢复

安全视角：
  └── RSA 签名验证，防止恶意固件注入
      └── HTTPS 传输，防止中间人攻击
```

---

## 2. 硬件依赖分析

### 2.1 Flash 分区布局

ESP32-S3 Flash 总容量：**16 MB**

#### 分区表设计

```csv
# Name,      Type,   SubType,  Offset,     Size,       Flags
nvs,         data,   nvs,      0x9000,     0x6000,
otadata,     data,   ota,      0xf000,     0x2000,
phy_init,    data,   phy,      0x11000,    0x1000,
factory,     app,    factory,  0x20000,    0x200000,
ota_0,       app,    ota_0,    0x220000,   0x200000,
ota_1,       app,    ota_1,    0x420000,   0x200000,
spiffs,      data,   spiffs,   0x620000,   0x400000,
```

#### 分区详细说明

| 分区名 | 大小 | 用途 | 升级策略 |
|--------|------|------|---------|
| **nvs** | 24 KB | 非易失存储（WiFi 凭据、设备配置） | OTA 保留，不覆盖 |
| **otadata** | 8 KB | OTA 状态数据（当前启动分区标记） | OTA 更新，标记启动分区 |
| **phy_init** | 4 KB | RF PHY 初始化数据 | OTA 保留 |
| **factory** | 2 MB | 出厂固件（保底恢复） | **永不 OTA 覆盖** |
| **ota_0** | 2 MB | OTA 分区 A | OTA 写入目标 |
| **ota_1** | 2 MB | OTA 分区 B | OTA 写入目标 |
| **spiffs** | 4 MB | 表情资源/音频文件/配置 | OTA 保留（独立升级） |

#### 容量分析

```
固件最大体积：2 MB（单分区上限）
当前固件典型体积：~1.2 MB（含 LVGL + 表情 + WiFi + AI）
剩余空间：~800 KB（安全余量）

结论：✅ Flash 容量满足 OTA 需求
```

### 2.2 PSRAM 影响

OTA 过程中 **不需要 PSRAM**：
- 固件下载采用流式写入 Flash，无需完整缓冲到 RAM
- 临时缓冲区（~4KB）使用 SRAM 即可
- 验证签名时固件已写入 Flash，直接从 Flash 读取

**PSRAM 可用于：**
- HTTP/TLS 临时缓冲（esp_http_client 内部管理）
- JSON 解析缓冲（版本信息解析）

**结论：** ✅ PSRAM 充足，OTA 不额外占用 PSRAM

### 2.3 电池与电源

**电量要求：**
- **最低电量：** ≥ 20% 才允许 OTA 升级
- **推荐电量：** ≥ 50% 才允许 OTA 升级
- **充电中：** 允许 OTA（最佳时机）

**电源状态检测：**
```c
// 通过 battery_monitor 服务获取当前电量
uint8_t battery_percent = battery_get_percent();
bool is_charging = battery_is_charging();

// OTA 前置检查
if (!is_charging && battery_percent < 20) {
    ESP_LOGW(TAG, "OTA rejected: battery too low (%d%%)", battery_percent);
    return ESP_ERR_INVALID_STATE;
}
```

**原因：**
- OTA 涉及 Flash 擦写（高功耗操作）
- 固件刷写到一半断电会导致分区损坏
- 需要足够电量完成下载 + 刷写 + 重启 + 健康检查

### 2.4 WiFi 网络依赖

**网络要求：**
- WiFi 连接稳定（RSSI ≥ -70 dBm 推荐）
- 互联网可达（OTA 服务器连通）
- HTTPS 下载速度 ≥ 100 KB/s（典型 2MB 固件需 20s）

**网络不稳定时的策略：**
- 支持断点续传（HTTP Range 请求）
- 下载超时自动重试（最多 3 次）
- WiFi 断开时暂停下载，重连后继续

### 2.5 硬件依赖总结

| 项目 | 状态 | 说明 |
|------|------|------|
| Flash 容量 | ✅ 满足 | 16MB，factory + ota_0 + ota_1 各 2MB |
| PSRAM | ✅ 充足 | OTA 不额外占用 |
| 电池监测 | ✅ 已有 | battery_monitor 服务提供电量查询 |
| WiFi | ✅ 已有 | wifi_manager 提供联网能力 |
| HTTPS/TLS | ✅ 支持 | ESP-IDF 内置 esp_http_client + mbedTLS |
| 屏幕显示 | ✅ 已有 | display_manager 提供进度显示能力 |

**结论：** ✅ 硬件能力完全满足 OTA 需求，无需新增硬件

---

## 3. FreeRTOS 任务影响

### 3.1 新增任务：ota_service_task

| 属性 | 值 | 说明 |
|------|-----|------|
| 任务名 | `ota_service_task` | OTA 服务主任务 |
| 优先级 | 1（低） | 后台服务，不抢占关键任务 |
| 栈大小 | 8 KB | HTTP 缓冲 + JSON 解析 |
| 核心 | Core 0 | 与 cloud_task 共享 WiFi 协议栈 |
| 周期 | 事件驱动 | 收到 MQTT 命令或定时触发 |
| 依赖 | WiFi、MQTT、Battery、Display | 需要网络、电量、显示服务 |

### 3.2 任务初始化顺序

```c
// app_main() 中的初始化顺序
void app_main(void)
{
    // Phase 1-4: 已有模块初始化
    // ...

    // Phase 5: 新增 OTA 服务初始化
    ota_service_init();

    // Phase 6: 启动任务
    // ...已有任务...
    xTaskCreatePinnedToCore(ota_service_task, "ota_svc", 8192, NULL, 1, NULL, 0);
}
```

### 3.3 任务间通信

#### 事件订阅

```c
// ota_service 订阅的事件
event_bus_subscribe(EVENT_SYS_WIFI_CONNECTED, on_wifi_connected);
event_bus_subscribe(EVENT_SYS_WIFI_DISCONNECTED, on_wifi_disconnected);
event_bus_subscribe(EVENT_CLOUD_OTA_COMMAND, on_ota_command);  // 新增事件
```

#### 事件发布

```c
// ota_service 发布的事件
event_bus_publish(EVENT_OTA_PROGRESS, &progress_data);      // 进度更新
event_bus_publish(EVENT_OTA_STATE_CHANGE, &state_data);     // 状态变更
event_bus_publish(EVENT_OTA_COMPLETE, &result_data);        // 升级完成
event_bus_publish(EVENT_OTA_ERROR, &error_data);            // 升级失败
```

### 3.4 事件 ID 扩展

```c
// robot_events.h — 新增 OTA 事件

// ===== OTA 事件 (0x0700-0x07FF) =====
EVENT_OTA_CHECK_UPDATE      = 0x0700,   // 检查更新
EVENT_OTA_UPDATE_AVAILABLE  = 0x0701,   // 有新版本可用
EVENT_OTA_DOWNLOAD_START    = 0x0702,   // 开始下载
EVENT_OTA_DOWNLOAD_PROGRESS = 0x0703,   // 下载进度
EVENT_OTA_DOWNLOAD_COMPLETE = 0x0704,   // 下载完成
EVENT_OTA_VERIFY_START      = 0x0705,   // 开始验证
EVENT_OTA_VERIFY_COMPLETE   = 0x0706,   // 验证完成
EVENT_OTA_APPLY_START       = 0x0707,   // 开始应用
EVENT_OTA_REBOOTING         = 0x0708,   // 准备重启
EVENT_OTA_COMPLETE          = 0x0709,   // 升级完成
EVENT_OTA_ROLLBACK          = 0x070A,   // 自动回滚
EVENT_OTA_ERROR             = 0x070B,   // 升级错误
EVENT_OTA_FACTORY_RESET     = 0x070C,   // 回退到出厂固件
```

### 3.5 任务负载分析

**CPU 占用：**
- 空闲状态：0%（事件等待）
- 检查更新：~5%（HTTP 请求，<1s）
- 下载固件：~10%（HTTPS 流式接收，持续 20-60s）
- 签名验证：~30%（SHA256 + RSA 计算，~2s）
- Flash 写入：~5%（流式写入，持续 20-60s）

**内存占用：**
- 任务栈：8 KB（SRAM）
- HTTP 缓冲：~4 KB（SRAM，内部临时分配）
- 签名验证：~1 KB（SRAM，mbedTLS 内部）

**带宽占用：**
- 下载流量：~2 MB（固件大小）
- 上报流量：<10 KB（状态上报）

### 3.6 与其他任务的协调

#### 与 cloud_task 的关系
- **资源共享：** 共享 WiFi 协议栈和 TLS 上下文
- **互斥：** OTA 下载时不进行 AI 对话（避免带宽抢占）
- **命令通道：** 通过 MQTT 接收云端升级命令

#### 与 display_task 的关系
- **进度显示：** 通过事件总线通知 display_manager 更新 UI
- **优先级：** display_task 优先级（6）高于 ota_service（1），保证 UI 流畅

#### 与 behavior_task 的关系
- **状态同步：** OTA 进行时，behavior_task 进入 OTA 状态（禁用语音/运动）
- **用户交互：** behavior_task 接收用户确认（如降级确认）

#### 与 battery_monitor 的关系
- **前置检查：** OTA 启动前检查电量
- **实时监控：** OTA 过程中持续监控电量，电量骤降时中止升级

---

## 4. 云/网络依赖

### 4.1 OTA 服务器架构

```
云端 OTA 服务器
├── HTTPS API 端点
│   ├── GET /ota/check        — 检查更新
│   ├── GET /ota/firmware.bin — 下载固件
│   └── GET /ota/signature.sig— 下载签名
│
├── MQTT 命令通道
│   ├── robotbuddy/{device_id}/ota/command  — 下发升级命令
│   └── robotbuddy/{device_id}/ota/status   — 上报升级状态
│
└── 文件存储
    ├── firmware_v1.0.0.bin
    ├── firmware_v1.0.0.sig
    └── version.json
```

### 4.2 API 接口设计

#### 4.2.1 检查更新接口

**请求：**
```http
GET /ota/check?device_id=XX:XX:XX:XX:XX:XX&current_version=1.0.0 HTTP/1.1
Host: ota.robotbuddy.local
Accept: application/json
```

**响应：**
```json
{
  "has_update": true,
  "latest_version": "1.1.0",
  "firmware_url": "https://ota.robotbuddy.local/firmware_v1.1.0.bin",
  "signature_url": "https://ota.robotbuddy.local/firmware_v1.1.0.sig",
  "firmware_size": 1572864,
  "sha256": "a1b2c3d4e5f6...",
  "update_level": "optional",  // optional | mandatory
  "release_date": "2026-07-20",
  "changelog": "新增差分升级支持"
}
```

#### 4.2.2 固件下载接口

**支持断点续传：**
```http
GET /firmware_v1.1.0.bin HTTP/1.1
Host: ota.robotbuddy.local
Range: bytes=524288-
```

**响应：**
```http
HTTP/1.1 206 Partial Content
Content-Range: bytes 524288-1572863/1572864
Content-Length: 1048576
```

### 4.3 MQTT 命令协议

#### 升级命令

**Topic:** `robotbuddy/{device_id}/ota/command`

**Payload:**
```json
{
  "cmd": "upgrade",
  "version": "1.1.0",
  "firmware_url": "https://ota.robotbuddy.local/firmware_v1.1.0.bin",
  "signature_url": "https://ota.robotbuddy.local/firmware_v1.1.0.sig",
  "mandatory": false,
  "timestamp": 1721380800
}
```

**支持命令：**
- `check` — 检查更新
- `upgrade` — 立即升级
- `rollback` — 回滚到上一版本
- `factory_reset` — 回退到出厂固件

### 4.4 离线降级方案

**WiFi 断开时：**
- 无法 OTA 升级（依赖网络）
- 保留本地已有的所有固件分区（factory + ota_0 + ota_1）
- 可通过本地 Web 页面或串口手动升级（V2.0 功能）

**OTA 服务器不可达时：**
- 重试机制：指数退避重试（5s / 15s / 30s）
- 超时放弃：等待服务器恢复后再次尝试
- 不影响设备正常运行（使用当前固件）

### 4.5 安全机制

#### 4.5.1 TLS 证书锁定

```c
// OTA 服务器证书 SHA256 指纹（硬编码到固件）
static const char OTA_SERVER_CERT_SHA256[] =
    "A1:B2:C3:D4:E5:F6:78:90:AB:CD:EF:12:34:56:78:90:"
    "AB:CD:EF:12:34:56:78:90:AB:CD:EF:12:34:56:78:90";
```

**作用：** 防止中间人攻击，即使系统根证书被污染也能保证安全

#### 4.5.2 固件签名验证

```
服务器端：
  固件 → SHA256 → RSA-2048 私钥签名 → .sig 文件

设备端：
  固件 → SHA256 计算哈希
  .sig 文件 → RSA-2048 公钥解签
  对比两个哈希 → 一致则通过
```

**公钥存储：**
- 编译时嵌入固件（`ota_public_key.pem`）
- 存储 NVS 加密分区或 eFuse（可选，更高安全性）

#### 4.5.3 签名 URL（防重放）

```http
GET /firmware_v1.1.0.bin?token=HMAC_SHA256(device_id+version+timestamp)&expires=1721384400 HTTP/1.1
```

**机制：**
- URL 携带签名和过期时间
- 设备端验证 URL 未过期
- 防止 URL 被盗用或重放攻击

---

## 5. 异常处理

### 5.1 WiFi 断开场景

| 阶段 | WiFi 断开影响 | 处理策略 |
|------|--------------|---------|
| 检查更新 | 无法查询服务器 | 返回错误，下次重试 |
| 下载中（<50%） | 下载中断 | 支持断点续传，重连后继续 |
| 下载中（>50%） | 下载中断 | 支持断点续传，重连后继续 |
| 下载中（>90%） | 下载中断 | 支持断点续传，重连后继续 |
| 验证签名 | 不依赖 WiFi | 继续执行 |
| 刷写 Flash | 不依赖 WiFi | 继续执行 |
| 重启后健康检查 | 需要联网验证 | 延长健康检查超时，允许离线模式 |

**实现：**
```c
// WiFi 事件处理
static void on_wifi_disconnected(const robot_event_t *event)
{
    if (g_ota_state == OTA_STATE_DOWNLOADING) {
        ESP_LOGW(TAG, "WiFi disconnected during download, pausing");
        g_ota_download_paused = true;
        // 保存断点信息到 NVS
        ota_save_resume_offset(g_downloaded_bytes);
    }
}

static void on_wifi_connected(const robot_event_t *event)
{
    if (g_ota_download_paused) {
        ESP_LOGI(TAG, "WiFi reconnected, resuming download");
        g_ota_download_paused = false;
        // 从断点继续下载
        ota_resume_download();
    }
}
```

### 5.2 固件下载失败

| 错误类型 | 错误码 | 处理策略 |
|---------|--------|---------|
| HTTP 4xx | 400-499 | 立即失败，记录错误日志 |
| HTTP 5xx | 500-599 | 重试 3 次，间隔递增（5s/15s/30s） |
| 网络超时 | TIMEOUT | 重试 3 次，支持断点续传 |
| 下载大小不匹配 | SIZE_MISMATCH | 重新下载，不续传 |
| Flash 写入失败 | WRITE_FAIL | 标记分区损坏，切换到另一个 OTA 分区 |

**重试策略：**
```c
// 指数退避重试
static const uint32_t RETRY_DELAYS[] = {5000, 15000, 30000};  // ms
static const uint8_t MAX_RETRIES = 3;

esp_err_t ota_download_with_retry(const ota_update_info_t *info)
{
    esp_err_t ret;
    for (int i = 0; i < MAX_RETRIES; i++) {
        ret = ota_download(info);
        if (ret == ESP_OK) break;

        ESP_LOGW(TAG, "Download failed (attempt %d/%d), retry in %lu ms",
                 i + 1, MAX_RETRIES, RETRY_DELAYS[i]);
        vTaskDelay(pdMS_TO_TICKS(RETRY_DELAYS[i]));
    }
    return ret;
}
```

### 5.3 签名验证失败

**场景：**
- 固件被篡改
- 签名文件损坏
- 公钥不匹配

**处理：**
```c
if (ota_verify_rsa_signature() != ESP_OK) {
    ESP_LOGE(TAG, "RSA signature verification FAILED!");
    // 拒绝升级
    ota_set_state(OTA_STATE_ERROR);
    // 标记当前下载分区为 INVALID（防止被重复使用）
    esp_ota_mark_app_invalid_rollback_and_reboot();
    // 上报云端
    mqtt_report_ota_error("signature_failed");
}
```

**保护机制：**
- 拒绝安装未验证的固件
- 不修改启动分区标记（继续使用当前固件）
- 清空下载的固件分区

### 5.4 电量不足

**电量检查时机：**
- OTA 启动前
- OTA 过程中（持续监控）

**处理策略：**
```c
// OTA 启动前检查
if (!battery_is_charging() && battery_get_percent() < 20) {
    ESP_LOGW(TAG, "OTA rejected: battery too low (%d%%)", battery_get_percent());
    mqtt_report_ota_error("low_battery");
    return ESP_ERR_INVALID_STATE;
}

// OTA 过程中监控
if (battery_get_percent() < 10) {
    ESP_LOGE(TAG, "Battery critically low during OTA, aborting");
    ota_abort_and_cleanup();
    return ESP_ERR_INVALID_STATE;
}
```

### 5.5 新固件运行异常

**自动回滚触发条件：**
1. **Watchdog 重启 ≥ 3 次**（NVS 计数）
2. **Panic 重启 ≥ 5 次**（NVS 计数）
3. **启动后 30s 未通过健康检查**
4. **用户手动触发回滚**

**健康检查项：**
```c
health_check_result_t ota_health_check(void)
{
    // 1. WiFi 是否连接
    if (!wifi_manager_is_connected()) {
        return HEALTH_CHECK_WIFI_FAIL;
    }

    // 2. MQTT 是否连接
    if (!mqtt_client_is_connected()) {
        return HEALTH_CHECK_MQTT_FAIL;
    }

    // 3. 显示是否正常（无花屏）
    if (!display_self_test()) {
        return HEALTH_CHECK_DISPLAY_FAIL;
    }

    // 4. I2C 总线是否可通信
    if (!mpu6050_is_present()) {
        return HEALTH_CHECK_SENSOR_FAIL;
    }

    return HEALTH_CHECK_PASS;
}
```

**自动回滚流程：**
```c
void app_main_after_ota(void)
{
    // 检查是否刚完成 OTA 升级
    if (ota_get_state() == OTA_STATE_VERIFYING_NEW) {
        // 启动健康检查定时器（30s 超时）
        TimerHandle_t timer = xTimerCreate("health_check", pdMS_TO_TICKS(30000),
                                           pdFALSE, NULL, health_check_timeout);

        // 执行健康检查
        if (ota_health_check() == HEALTH_CHECK_PASS) {
            ESP_LOGI(TAG, "Health check passed, committing OTA");
            ota_commit();  // 标记新固件为 VALID
        } else {
            ESP_LOGE(TAG, "Health check failed, rolling back");
            ota_rollback();  // 自动回滚
        }
    }
}
```

### 5.6 双分区损坏（极端情况）

**场景：**
- ota_0 和 ota_1 都标记为 INVALID
- 无法回滚到任何一个 OTA 分区

**处理：**
```c
if (ota_get_valid_partition_count() == 0) {
    ESP_LOGE(TAG, "All OTA partitions invalid, falling back to factory");
    ota_rollback_to_factory();
}
```

**factory 分区保护：**
- factory 分区永不 OTA 覆盖
- 作为最后的保底恢复手段
- 回退到 factory 后可能需要重新配网

### 5.7 异常处理矩阵

| 异常类型 | 检测时机 | 处理策略 | 用户提示 |
|---------|---------|---------|---------|
| WiFi 断开 | 全过程 | 暂停下载，重连后续传 | "网络中断，正在重连..." |
| HTTP 错误 | 下载中 | 重试 3 次后失败 | "下载失败，请稍后重试" |
| 签名验证失败 | 验证阶段 | 拒绝升级，上报云端 | "固件验证失败，升级已取消" |
| 电量不足 | OTA 前/中 | 拒绝/中止 OTA | "电量不足，请充电后升级" |
| Flash 写入失败 | 刷写阶段 | 标记分区损坏，切换分区 | "写入失败，正在重试..." |
| Watchdog 重启 | 启动后 | 计数 ≥3 自动回滚 | "系统异常，正在恢复..." |
| 健康检查失败 | 启动后 30s | 自动回滚 | "新固件异常，已恢复上一版本" |
| 双分区损坏 | 启动时 | 回退 factory | "系统恢复出厂固件" |

---

## 6. 验收条件

### 6.1 功能验收

| 测试场景 | 预期结果 | 验证方法 |
|---------|---------|---------|
| 正常升级 | 下载→验证→安装→重启→健康检查→提交成功 | 端到端测试 |
| 版本检查 | 正确识别新版本，返回版本信息 | 单元测试 |
| 断点续传 | 下载中断后从断点继续，不重新下载 | 集成测试 |
| 签名验证 | 正确签名通过，错误签名拒绝 | 单元测试 |
| 自动回滚 | 新固件异常时自动回滚到旧版本 | 集成测试 |
| 低电量拒绝 | 电量 <20% 时拒绝 OTA | 集成测试 |
| 进度显示 | 屏幕正确显示下载/安装进度 | 手动测试 |
| MQTT 命令 | 正确解析并执行云端升级命令 | 集成测试 |

### 6.2 性能指标

| 指标 | 目标值 | 测试方法 |
|------|--------|---------|
| 版本检查延迟 | < 2s | 自动化测试 |
| 固件下载速度 | ≥ 100 KB/s | 网络监控 |
| 签名验证时间 | < 3s | 性能测试 |
| 总升级时间 | < 3 分钟（含重启） | 端到端测试 |
| 内存占用峰值 | < 8 KB（任务栈）+ 4 KB（缓冲） | 内存分析 |
| Flash 写入速度 | ≥ 50 KB/s | 性能测试 |

### 6.3 安全验收

| 安全项 | 验收标准 | 验证方法 |
|--------|---------|---------|
| TLS 证书锁定 | 仅接受合法证书指纹 | 安全测试 |
| RSA 签名验证 | 拒绝未签名固件 | 安全测试 |
| SHA256 完整性校验 | 拒绝损坏固件 | 单元测试 |
| URL 防重放 | 过期 URL 拒绝使用 | 安全测试 |
| factory 分区保护 | OTA 永不覆盖 factory | 代码审查 |

### 6.4 可靠性验收

| 场景 | 验收标准 | 测试方法 |
|------|---------|---------|
| WiFi 断开恢复 | 断点续传成功 | 稳定性测试 |
| 下载中断 3 次 | 仍能完成升级 | 压力测试 |
| 升级过程掉电 | 重启后恢复，不损坏分区 | 破坏性测试 |
| 连续升级 10 次 | 无内存泄漏 | 长期测试 |
| 并发 OTA 请求 | 串行处理，不冲突 | 并发测试 |

### 6.5 边界条件

| 边界条件 | 测试方法 | 预期结果 |
|---------|---------|---------|
| 固件大小 = 2MB（分区上限） | 极限测试 | 正常升级 |
| 网络延迟 = 5s | 网络模拟 | 下载成功，耗时增加 |
| 电量 = 20%（临界值） | 边界测试 | 允许升级（充电中） |
| 电量 = 19% | 边界测试 | 拒绝升级 |
| URL 过期时间 = 1s | 安全测试 | URL 立即过期，拒绝使用 |
| 连续 watchdog 重启 2 次 | 稳定性测试 | 不触发回滚（阈值 3） |
| 连续 watchdog 重启 3 次 | 稳定性测试 | 触发自动回滚 |

---

## 7. 功能需求清单

### 7.1 P0 必需功能（MVP）

| ID | 功能 | 描述 |
|----|------|------|
| OTA-001 | 版本检查 | 通过 HTTPS 查询云端是否有新固件 |
| OTA-002 | 固件下载 | HTTPS 下载固件到备用 OTA 分区 |
| OTA-003 | RSA 签名验证 | RSA-2048 签名验证固件完整性 |
| OTA-004 | SHA256 校验 | SHA256 完整性校验固件哈希 |
| OTA-005 | AB 分区滚动升级 | 交替使用 ota_0 和 ota_1，永不覆盖 factory |
| OTA-006 | 自动回滚 | 新固件异常时自动回滚到旧版本 |
| OTA-007 | 电量检查 | 低电量（<20%）时拒绝 OTA |
| OTA-008 | TLS 证书锁定 | 硬编码 OTA 服务器证书指纹，防止中间人攻击 |

### 7.2 P1 推荐功能（增强）

| ID | 功能 | 描述 |
|----|------|------|
| OTA-009 | 断点续传 | 下载中断后支持从断点继续 |
| OTA-010 | 进度显示 | 屏幕显示下载/安装进度百分比 |
| OTA-011 | MQTT 命令 | 支持云端 MQTT 下发升级命令 |
| OTA-012 | 状态上报 | 通过 MQTT 上报升级状态和结果 |
| OTA-013 | 定时检查 | 每日固定时间自动检查更新 |
| OTA-014 | 签名 URL | OTA URL 携带签名和过期时间，防重放攻击 |

### 7.3 P2 可选功能（未来）

| ID | 功能 | 描述 |
|----|------|------|
| OTA-015 | Web 控制台 | 本地 Web 页面手动触发升级 |
| OTA-016 | 差分升级 | 仅下载固件差异部分，节省带宽 |
| OTA-017 | 资源升级 | 独立升级 SPIFFS 中的表情/音频资源 |
| OTA-018 | 降级确认 | 降级操作需要用户手动确认 |
| OTA-019 | 固件回退记录 | NVS 记录回滚原因，供云端诊断 |

---

## 8. 检查清单

### 8.1 Requirement Skill Checklist

- [x] **硬件能力是否满足需求？**
  - Flash 16MB，足够 factory + ota_0 + ota_1
  - PSRAM 8MB，OTA 不额外占用
  - ✅ 满足

- [x] **实时性是否能保证？**
  - OTA 为低优先级后台任务（P1）
  - 不抢占音频（P8/7）、显示（P6）等关键任务
  - 下载和验证过程允许中断
  - ✅ 满足

- [x] **功耗影响是否可接受？**
  - OTA 期间 Flash 擦写功耗增加（~100mA）
  - 升级时间 <3 分钟，总耗电 <5mAh
  - 电量检查确保安全余量
  - ✅ 满足

- [x] **是否有离线降级方案？**
  - WiFi 断开时无法 OTA（依赖网络）
  - 保留本地所有固件分区
  - factory 分区永不覆盖，可恢复出厂
  - ✅ 满足

- [x] **边界条件是否覆盖？**
  - 电量 20% 临界值测试
  - 固件大小 2MB 上限测试
  - 网络延迟 5s 极限测试
  - 连续 watchdog 重启 3 次触发回滚
  - ✅ 满足

- [x] **安全风险是否考虑？**
  - TLS 证书锁定防止中间人攻击
  - RSA-2048 签名验证固件来源
  - SHA256 校验固件完整性
  - 签名 URL 防重放攻击
  - factory 分区永不 OTA 覆盖
  - ✅ 满足

- [x] **依赖是否明确？**
  - WiFi（wifi_manager）
  - HTTPS/TLS（esp_http_client + mbedTLS）
  - MQTT（mqtt_client）
  - 电量监测（battery_monitor）
  - 显示（display_manager）
  - ✅ 满足

- [x] **验收条件是否明确可测？**
  - 功能验收：正常升级、断点续传、自动回滚等
  - 性能指标：下载速度 ≥100KB/s，总时间 <3 分钟
  - 安全验收：签名验证、证书锁定
  - 可靠性验收：WiFi 断开恢复、掉电恢复
  - ✅ 满足

### 8.2 RobotBuddy 核心约束检查

- [x] **响应延迟**
  - OTA 为后台任务，不影响实时响应
  - ✅ 满足

- [x] **表情帧率 ≥ 30 FPS**
  - OTA 任务优先级（1）低于显示任务（6）
  - OTA 过程中 UI 正常渲染
  - ✅ 满足

- [x] **音频质量 16kHz/16bit**
  - OTA 不占用 I2S 资源
  - OTA 过程中音频正常播放
  - ✅ 满足

- [x] **WiFi 自动重连**
  - OTA 支持断点续传
  - WiFi 重连后自动继续下载
  - ✅ 满足

- [x] **电池续航**
  - OTA 仅在电量充足时执行
  - 升级时间短，对续航影响小
  - ✅ 满足

---

## 附录

### A. OTA 状态机完整定义

```c
typedef enum {
    OTA_STATE_IDLE,             // 空闲，等待升级命令
    OTA_STATE_CHECKING,         // 检查服务器是否有新版本
    OTA_STATE_DOWNLOADING,      // 下载固件包
    OTA_STATE_VERIFYING,        // RSA-2048 签名校验 + SHA256 校验
    OTA_STATE_READY,            // 校验通过，等待应用
    OTA_STATE_REBOOTING,        // 设置启动分区，准备重启
    OTA_STATE_VERIFYING_NEW,    // 重启后：新固件运行中，健康检查中
    OTA_STATE_COMMITTED,        // 健康检查通过 → 确认生效，标记为稳定
    OTA_STATE_ROLLING_BACK,     // 健康检查失败 → 回滚到上一个正常分区
    OTA_STATE_FACTORY_RESET,    // 回滚失败 / 所有分区损坏 → 回退 factory
    OTA_STATE_ERROR,            // 不可恢复错误
} ota_state_t;
```

### B. OTA 数据结构定义

```c
// OTA 更新信息
typedef struct {
    char firmware_url[256];         // 固件下载 URL
    char signature_url[256];        // 签名文件 URL
    char version[32];               // 固件版本号 x.y.z
    size_t firmware_size;           // 固件大小 (bytes)
    char sha256[65];                // SHA256 校验和 (hex string)
    ota_update_level_t level;       // 升级级别
    uint32_t release_timestamp;     // 发布时间戳
    char changelog[512];            // 更新日志
} ota_update_info_t;

// OTA 进度
typedef struct {
    ota_state_t state;              // 当前状态
    int progress_percent;           // 下载/刷写进度 (0-100%)
    size_t bytes_downloaded;        // 已下载字节数
    size_t bytes_total;             // 总字节数
    uint32_t elapsed_ms;            // 已耗时
    uint32_t estimated_remaining_ms;// 预估剩余时间
} ota_progress_t;
```

### C. 相关文档链接

- [OTA Skill 文档](../../.claude/skills/ota/SKILL.md)
- [系统架构设计](../architecture/v1.0-mvp-architecture.md)
- [嵌入式编码规范](../../.claude/standards/embedded-coding.md)
- [桌面机器人设计需求](../桌面机器人设计需求.md)

---

> **文档版本:** 1.0
> **下次审查:** 架构设计完成后更新接口契约
> **变更记录:**
> - 2026-07-19: 初始版本，完成需求分析
