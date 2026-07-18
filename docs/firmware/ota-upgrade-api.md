# RobotBuddy OTA 升级功能 API 文档

> **版本:** 1.0
> **日期:** 2026-07-19
> **模块:** ota_service
> **状态:** API 文档完成

---

## 目录

1. [概述](#1-概述)
2. [快速开始](#2-快速开始)
3. [API 参考](#3-api-参考)
4. [数据结构](#4-数据结构)
5. [配置选项](#5-配置选项)
6. [错误码](#6-错误码)
7. [使用示例](#7-使用示例)
8. [常见问题](#8-常见问题)

---

## 1. 概述

### 1.1 模块介绍

`ota_service` 模块为 RobotBuddy 提供安全的远程固件升级（OTA）功能，支持：

- **HTTPS 下载**：安全固件传输
- **RSA-2048 签名验证**：防止恶意固件注入
- **SHA256 完整性校验**：确保固件完整
- **AB 分区滚动升级**：可靠升级策略
- **自动回滚**：异常时自动恢复
- **断点续传**：网络中断后继续下载
- **进度显示**：实时升级进度

### 1.2 依赖关系

```
ota_service
├── esp_http_client (HTTPS 下载)
├── esp_ota (OTA 分区管理)
├── mbedTLS (RSA 签名验证)
├── nvs_flash (状态持久化)
├── event_bus (事件通知)
├── wifi_manager (网络连接)
├── battery_monitor (电量检查)
└── display_manager (进度显示)
```

### 1.3 初始化顺序

```c
void app_main(void)
{
    // 1. 基础设施
    nvs_flash_init();
    event_bus_init();

    // 2. 硬件驱动
    bsp_board_init();

    // 3. 服务层
    wifi_manager_init();
    battery_monitor_init();
    display_manager_init();

    // 4. OTA 服务（最后初始化）
    ota_service_init();

    // 5. 创建任务
    xTaskCreate(ota_service_task, ...);
}
```

---

## 2. 快速开始

### 2.1 基本使用

```c
#include "ota_service.h"

void upgrade_firmware(void)
{
    // 1. 检查更新
    ota_update_info_t info;
    esp_err_t ret = ota_service_check_update(&info);

    if (ret == ESP_OK && info.version[0] != '\0') {
        printf("发现新版本: %s\n", info.version);

        // 2. 开始升级
        ret = ota_service_start_upgrade(&info);
        if (ret == ESP_OK) {
            printf("升级已开始，进度: %d%%\n",
                   ota_service_get_progress(NULL)->progress_percent);
        }
    } else {
        printf("已是最新版本\n");
    }
}
```

### 2.2 监听升级进度

```c
void on_ota_progress(const ota_progress_t *progress)
{
    printf("OTA 进度: %d%% (%zu/%zu bytes)\n",
           progress->progress_percent,
           progress->bytes_downloaded,
           progress->bytes_total);
}

void on_ota_state_change(ota_state_t old_state, ota_state_t new_state)
{
    printf("OTA 状态: %d → %d\n", old_state, new_state);
}

void on_ota_result(bool success, const char *version, const char *error)
{
    if (success) {
        printf("OTA 升级成功: %s\n", version);
    } else {
        printf("OTA 升级失败: %s\n", error);
    }
}

void setup_ota_callbacks(void)
{
    ota_service_register_progress_callback(on_ota_progress);
    ota_service_register_state_callback(on_ota_state_change);
    ota_service_register_result_callback(on_ota_result);
}
```

---

## 3. API 参考

### 3.1 初始化

#### `ota_service_init()`

```c
esp_err_t ota_service_init(void);
```

初始化 OTA 服务。

**参数：** 无

**返回：**
- `ESP_OK`: 成功
- `ESP_ERR_INVALID_STATE`: 已初始化
- `ESP_ERR_NO_MEM`: 内存不足

**示例：**
```c
esp_err_t ret = ota_service_init();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "OTA service init failed: %s", esp_err_to_name(ret));
    return;
}
```

---

### 3.2 版本检查

#### `ota_service_check_update()`

```c
esp_err_t ota_service_check_update(ota_update_info_t *info);
```

通过 HTTPS 查询云端 OTA 服务器，检查是否有新固件版本。

**参数：**
- `info` (out): 更新信息（如果有新版本）

**返回：**
- `ESP_OK`: 检查成功
- `ESP_ERR_INVALID_STATE`: WiFi 未连接
- `ESP_ERR_HTTP_CONNECT`: HTTP 连接失败
- `ESP_ERR_INVALID_RESPONSE`: 服务器响应无效

**示例：**
```c
ota_update_info_t info;
esp_err_t ret = ota_service_check_update(&info);

if (ret == ESP_OK) {
    if (info.version[0] != '\0') {
        printf("新版本: %s\n", info.version);
        printf("更新日志: %s\n", info.changelog);
    } else {
        printf("已是最新版本\n");
    }
}
```

---

### 3.3 升级控制

#### `ota_service_start_upgrade()`

```c
esp_err_t ota_service_start_upgrade(const ota_update_info_t *info);
```

开始 OTA 升级。

**参数：**
- `info` (in): 更新信息（从 `ota_service_check_update` 获取）

**返回：**
- `ESP_OK`: 开始成功
- `ESP_ERR_INVALID_STATE`: 已在升级中 或 电量不足
- `ESP_ERR_INVALID_ARG`: 参数无效

**示例：**
```c
esp_err_t ret = ota_service_start_upgrade(&info);
if (ret == ESP_OK) {
    printf("升级已开始\n");
}
```

#### `ota_service_cancel()`

```c
esp_err_t ota_service_cancel(void);
```

取消 OTA 升级（仅在 DOWNLOADING 状态下有效）。

**参数：** 无

**返回：**
- `ESP_OK`: 取消成功
- `ESP_ERR_INVALID_STATE`: 不在升级中

**示例：**
```c
esp_err_t ret = ota_service_cancel();
if (ret == ESP_OK) {
    printf("升级已取消\n");
}
```

#### `ota_service_rollback()`

```c
esp_err_t ota_service_rollback(void);
```

手动触发回滚到上一个有效的 OTA 分区。

**参数：** 无

**返回：**
- `ESP_OK`: 成功（将重启）
- `ESP_ERR_INVALID_STATE`: 无可回滚的分区

**示例：**
```c
ota_service_rollback(); // 成功后会重启，不会返回
```

#### `ota_service_factory_reset()`

```c
esp_err_t ota_service_factory_reset(void);
```

回退到出厂固件（factory 分区）。

**参数：** 无

**返回：**
- `ESP_OK`: 成功（将重启）
- `ESP_FAIL`: factory 分区不存在或损坏

---

### 3.4 状态查询

#### `ota_service_get_state()`

```c
ota_state_t ota_service_get_state(void);
```

获取 OTA 当前状态。

**参数：** 无

**返回：** OTA 状态枚举

**示例：**
```c
ota_state_t state = ota_service_get_state();
printf("OTA 状态: %d\n", state);
```

#### `ota_service_get_progress()`

```c
esp_err_t ota_service_get_progress(ota_progress_t *progress);
```

获取 OTA 进度信息。

**参数：**
- `progress` (out): 进度信息

**返回：**
- `ESP_OK`: 成功
- `ESP_ERR_INVALID_ARG`: 参数无效

**示例：**
```c
ota_progress_t progress;
ota_service_get_progress(&progress);
printf("进度: %d%%\n", progress.progress_percent);
```

#### `ota_service_get_running_partition()`

```c
esp_err_t ota_service_get_running_partition(char *label, size_t label_size);
```

获取当前运行分区标签。

**参数：**
- `label` (out): 分区标签（"factory", "ota_0", "ota_1"）
- `label_size` (in): label 缓冲区大小

**返回：**
- `ESP_OK`: 成功
- `ESP_ERR_INVALID_ARG`: 参数无效

---

### 3.5 回调注册

#### `ota_service_register_state_callback()`

```c
esp_err_t ota_service_register_state_callback(ota_state_callback_t cb);
```

注册状态变更回调。

**参数：**
- `cb` (in): 回调函数

**返回：**
- `ESP_OK`: 成功
- `ESP_ERR_INVALID_ARG`: 参数无效

#### `ota_service_register_progress_callback()`

```c
esp_err_t ota_service_register_progress_callback(ota_progress_callback_t cb);
```

注册进度更新回调。

**参数：**
- `cb` (in): 回调函数

**返回：**
- `ESP_OK`: 成功
- `ESP_ERR_INVALID_ARG`: 参数无效

#### `ota_service_register_result_callback()`

```c
esp_err_t ota_service_register_result_callback(ota_result_callback_t cb);
```

注册结果回调（升级完成时触发）。

**参数：**
- `cb` (in): 回调函数

**返回：**
- `ESP_OK`: 成功
- `ESP_ERR_INVALID_ARG`: 参数无效

---

### 3.6 MQTT 命令处理

#### `ota_service_handle_mqtt_command()`

```c
void ota_service_handle_mqtt_command(const char *payload_json);
```

处理 MQTT OTA 命令。

**参数：**
- `payload_json` (in): JSON 字符串

**命令格式：**

```json
// 检查更新
{"cmd":"check"}

// 立即升级
{
  "cmd": "upgrade",
  "version": "1.1.0",
  "url": "https://ota.example.com/firmware.bin",
  "signature_url": "https://ota.example.com/firmware.sig"
}

// 回滚
{"cmd":"rollback"}

// 回退出厂
{"cmd":"factory_reset"}
```

**示例：**
```c
const char *json = "{\"cmd\":\"check\"}";
ota_service_handle_mqtt_command(json);
```

---

## 4. 数据结构

### 4.1 `ota_state_t`

OTA 状态枚举。

```c
typedef enum {
    OTA_STATE_IDLE,             // 空闲
    OTA_STATE_CHECKING,         // 检查更新中
    OTA_STATE_DOWNLOADING,      // 下载中
    OTA_STATE_VERIFYING,        // 验证中
    OTA_STATE_READY,            // 就绪（等待应用）
    OTA_STATE_REBOOTING,        // 重启中
    OTA_STATE_VERIFYING_NEW,    // 新固件健康检查中
    OTA_STATE_COMMITTED,        // 升级成功
    OTA_STATE_ROLLING_BACK,     // 回滚中
    OTA_STATE_FACTORY_RESET,    // 回退出厂
    OTA_STATE_ERROR,            // 错误
} ota_state_t;
```

### 4.2 `ota_update_info_t`

OTA 更新信息。

```c
typedef struct {
    char firmware_url[256];     // 固件下载 URL
    char signature_url[256];    // 签名文件 URL
    char version[32];           // 版本号 x.y.z
    size_t firmware_size;       // 固件大小 (bytes)
    char sha256[65];            // SHA256 校验和
    ota_update_level_t level;   // 升级级别
    uint32_t release_timestamp; // 发布时间戳
    char changelog[512];        // 更新日志
} ota_update_info_t;
```

### 4.3 `ota_progress_t`

OTA 进度信息。

```c
typedef struct {
    ota_state_t state;          // 当前状态
    int progress_percent;       // 进度百分比 (0-100)
    size_t bytes_downloaded;    // 已下载字节数
    size_t bytes_total;         // 总字节数
    uint32_t elapsed_ms;        // 已耗时
    uint32_t estimated_remaining_ms; // 预估剩余时间
} ota_progress_t;
```

### 4.4 `ota_error_t`

OTA 错误信息。

```c
typedef struct {
    ota_error_code_t code;      // 错误码
    char message[256];          // 错误描述
    ota_state_t failed_state;   // 失败时的状态
    uint32_t timestamp;         // 错误时间戳
} ota_error_t;
```

---

## 5. 配置选项

### 5.1 menuconfig 配置

通过 `idf.py menuconfig` 配置：

```
Component config → OTA Service
├── [ ] Enable OTA Service
├── (80) HTTP Server Port
├── (30) Health Check Timeout (seconds)
├── (3) Watchdog Rollback Threshold
├── (5) Panic Rollback Threshold
├── (20) Minimum Battery Percent
└── [ ] Enable Certificate Pinning
```

### 5.2 编译时配置

`ota_config.h`:

```c
#define OTA_HEALTH_CHECK_TIMEOUT_MS   30000  // 健康检查超时
#define OTA_WATCHDOG_THRESHOLD        3      // Watchdog 阈值
#define OTA_PANIC_THRESHOLD           5      // Panic 阈值
#define OTA_MIN_BATTERY_PERCENT       20     // 最低电量
#define OTA_MAX_RETRIES               3      // 最大重试次数
#define OTA_DOWNLOAD_BUFFER_SIZE      4096   // 下载缓冲区
```

---

## 6. 错误码

### 6.1 网络错误 (0x0100-0x01FF)

| 错误码 | 名称 | 说明 |
|--------|------|------|
| 0x0100 | `OTA_ERR_WIFI_DISCONNECT` | WiFi 断开 |
| 0x0101 | `OTA_ERR_HTTP_TIMEOUT` | HTTP 超时 |
| 0x0102 | `OTA_ERR_HTTP_ERROR` | HTTP 错误 (4xx/5xx) |
| 0x0103 | `OTA_ERR_DNS_FAILED` | DNS 解析失败 |
| 0x0104 | `OTA_ERR_CONNECTION_REFUSED` | 连接被拒绝 |

### 6.2 下载错误 (0x0200-0x02FF)

| 错误码 | 名称 | 说明 |
|--------|------|------|
| 0x0200 | `OTA_ERR_DOWNLOAD_INCOMPLETE` | 下载不完整 |
| 0x0201 | `OTA_ERR_SIZE_MISMATCH` | 大小不匹配 |
| 0x0202 | `OTA_ERR_WRITE_FAILED` | Flash 写入失败 |
| 0x0203 | `OTA_ERR_NO_SPACE` | 分区空间不足 |

### 6.3 验证错误 (0x0300-0x03FF)

| 错误码 | 名称 | 说明 |
|--------|------|------|
| 0x0300 | `OTA_ERR_RSA_VERIFY_FAILED` | RSA 签名验证失败 |
| 0x0301 | `OTA_ERR_SHA256_MISMATCH` | SHA256 不匹配 |
| 0x0302 | `OTA_ERR_INVALID_SIGNATURE` | 无效签名 |

### 6.4 系统错误 (0x0600-0x06FF)

| 错误码 | 名称 | 说明 |
|--------|------|------|
| 0x0600 | `OTA_ERR_LOW_BATTERY` | 电量不足 |
| 0x0601 | `OTA_ERR_ALREADY_UPDATING` | 已在升级中 |
| 0x0602 | `OTA_ERR_USER_CANCEL` | 用户取消 |
| 0x0603 | `OTA_ERR_INTERNAL` | 内部错误 |

---

## 7. 使用示例

### 7.1 完整升级流程

```c
#include "ota_service.h"
#include "esp_log.h"

static const char *TAG = "main";

void on_progress(const ota_progress_t *progress)
{
    ESP_LOGI(TAG, "OTA 进度: %d%%", progress->progress_percent);
}

void on_result(bool success, const char *version, const char *error)
{
    if (success) {
        ESP_LOGI(TAG, "升级成功: %s", version);
    } else {
        ESP_LOGE(TAG, "升级失败: %s", error);
    }
}

void app_main(void)
{
    // 初始化
    nvs_flash_init();
    wifi_manager_init();
    ota_service_init();

    // 注册回调
    ota_service_register_progress_callback(on_progress);
    ota_service_register_result_callback(on_result);

    // 检查更新
    ota_update_info_t info;
    if (ota_service_check_update(&info) == ESP_OK && info.version[0] != '\0') {
        ESP_LOGI(TAG, "发现新版本: %s", info.version);

        // 开始升级
        ota_service_start_upgrade(&info);
    }

    // 主循环
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### 7.2 MQTT 命令处理

```c
#include "mqtt_client.h"
#include "ota_service.h"

void mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    if (strcmp(event->topic, "robotbuddy/ota/command") == 0) {
        // 处理 OTA 命令
        ota_service_handle_mqtt_command(event->data);
    }
}
```

---

## 8. 常见问题

### Q1: 升级失败如何恢复？

**A:** OTA 服务支持自动回滚。如果新固件运行异常（watchdog 重启 ≥3 次），系统会自动回滚到上一个稳定版本。如果所有 OTA 分区都损坏，会回退到 factory 分区。

### Q2: 电量不足时能否升级？

**A:** 电量 <20% 时会拒绝升级，保护刷写安全。建议在充电时升级。

### Q3: 如何确保固件安全？

**A:** OTA 服务使用多重安全机制：
1. **HTTPS 传输**：加密传输固件
2. **TLS 证书锁定**：防止中间人攻击
3. **RSA-2048 签名验证**：验证固件来源
4. **SHA256 完整性校验**：确保固件完整

### Q4: 下载中断后能否续传？

**A:** 支持。下载中断时会保存断点信息到 NVS，WiFi 重连后自动从断点继续下载。

### Q5: 如何查看升级进度？

**A:** 有两种方式：
1. **回调函数**：注册 `ota_progress_callback_t` 实时接收进度
2. **查询接口**：调用 `ota_service_get_progress()` 主动查询

---

## 附录

### A. 分区表配置

```csv
# partitions.csv
# Name,      Type,   SubType,  Offset,     Size,       Flags
nvs,         data,   nvs,      0x9000,     0x6000,
otadata,     data,   ota,      0xf000,     0x2000,
phy_init,    data,   phy,      0x11000,    0x1000,
factory,     app,    factory,  0x20000,    0x200000,
ota_0,       app,    ota_0,    0x220000,   0x200000,
ota_1,       app,    ota_1,    0x420000,   0x200000,
spiffs,      data,   spiffs,   0x620000,   0x400000,
```

### B. RSA 签名生成

```bash
# 生成 RSA 密钥对
openssl genrsa -out ota_private_key.pem 2048
openssl rsa -in ota_private_key.pem -pubout -out ota_public_key.pem

# 对固件签名
openssl dgst -sha256 -sign ota_private_key.pem -out firmware.sig firmware.bin
```

### C. 相关文档

- [OTA 需求分析](../requirement/ota-upgrade-requirements.md)
- [OTA 架构设计](../architecture/ota-upgrade-architecture.md)
- [OTA 测试计划](../testing/ota-upgrade-test-plan.md)

---

> **文档版本:** 1.0
> **最后更新:** 2026-07-19
> **维护者:** RobotBuddy Team
