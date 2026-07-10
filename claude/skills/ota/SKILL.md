# OTA Skill — RobotBuddy

## Role

RobotBuddy OTA 固件升级专家，负责固件分区策略、HTTPS 安全下载、签名验证、自动回滚和差分升级的端到端管理。

## Domain

ESP-IDF OTA API（esp_https_ota）、分区表管理（partitions.csv）、固件签名（RSA-2048）、SHA256 校验、TLS 证书锁定（Certificate Pinning）、安全启动集成（Secure Boot V2+）、差分/增量更新（Delta OTA）、自动回滚策略。

## Goal

实现安全、可靠的远程固件升级系统，支持自动回滚和差分升级，确保 RobotBuddy 在任何网络和供电条件下都能安全完成固件更新。

## Inputs

- 分区表设计（FLASH 容量 16MB，factory/ota_0/ota_1 布局）
- OTA 服务器 URL 和签名证书（RSA-2048 公钥）
- 安全启动配置（Secure Boot V2，可选）
- MQTT OTA 命令 Topic（`robotbuddy/{device_id}/ota/command`）
- Wi-Fi 连接状态和电源状态

## Outputs

- `firmware/system/ota_service/ota_manager.c` — OTA 管理器（状态机 + 流程编排）
- `firmware/system/ota_service/ota_download.c` — HTTPS 固件下载（断点续传 + 进度回调）
- `firmware/system/ota_service/ota_verify.c` — RSA-2048 签名验证 + SHA256 校验
- `firmware/system/ota_service/ota_partition.c` — 分区表管理（查询/切换/标记）
- `firmware/system/ota_service/ota_rollback.c` — 自动/手动回滚逻辑
- `firmware/system/ota_service/ota_security.c` — 证书锁定 + 安全启动集成
- `firmware/system/ota_service/ota_delta.c` — 差分升级（V2+ 可选）
- `docs/firmware/ota-management.md` — OTA 升级管理文档

## OTA Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        OTA System Architecture                   │
│                                                                  │
│  ┌──────────────┐                                                │
│  │ Cloud OTA    │   HTTPS (TLS 1.2+)                              │
│  │ Server       │   URL: https://ota.robotbuddy.local/            │
│  │              │                                                │
│  │  ┌────────┐  │                                                │
│  │  │ Firmware│  │   ① Check Update (HTTP GET /ota/check)         │
│  │  │  .bin   │──┼───────────────────────────────────────┐        │
│  │  ├────────┤  │                                        │        │
│  │  │ Version│  │   ② Download (HTTPS GET, Range支持)     │        │
│  │  │  .json │──┼───────────────────────────────────┐    │        │
│  │  ├────────┤  │                                    │    │        │
│  │  │ Sign   │  │   ③ Verify Signature (RSA-2048)    │    │        │
│  │  │  .sig  │──┼──────────────────────────────┐     │    │        │
│  │  └────────┘  │                               │     │    │        │
│  └──────────────┘                               │     │    │        │
│                                                  │     │    │        │
│  ┌───────────────────────────────────────────────┼─────┼────┼──┐   │
│  │                    ESP32-S3 Flash (16MB)      │     │    │  │   │
│  │                                               │     │    │  │   │
│  │  ┌─────────────────────────────────────────┐  │     │    │  │   │
│  │  │          Partition Table                 │  │     │    │  │   │
│  │  │  ┌──────────┬──────────┬──────────┐     │  │     │    │  │   │
│  │  │  │ factory  │  ota_0   │  ota_1   │     │  │     │    │  │   │
│  │  │  │  (2MB)   │  (2MB)   │  (2MB)   │     │  │     │    │  │   │
│  │  │  └──────────┴──────────┴──────────┘     │  │     │    │  │   │
│  │  │  ┌──────────┐  ┌──────────────────┐     │  │     │    │  │   │
│  │  │  │ otadata  │  │ spiffs (4MB)     │     │  │     │    │  │   │
│  │  │  │  (8KB)   │  │ 表情/音频资源    │     │  │     │    │  │   │
│  │  │  └──────────┘  └──────────────────┘     │  │     │    │  │   │
│  │  └─────────────────────────────────────────┘  │     │    │  │   │
│  │                                                │     │    │  │   │
│  │  ┌──────────────────────────────────────────┐  │     │    │  │   │
│  │  │        OTA Manager (State Machine)       │  │     │    │  │   │
│  │  │                                          │  │     │    │  │   │
│  │  │  ④ Flash Write ─► ota_0 / ota_1         │◄─┘     │    │  │   │
│  │  │  ⑤ Boot Partition Switch                │        │    │  │   │
│  │  │  ⑥ Reboot ─► New Firmware                │        │    │  │   │
│  │  │  ⑦ Health Check (30s timeout)            │        │    │  │   │
│  │  │  ⑧ Commit (SUCCESS) / Rollback (FAIL)   │        │    │  │   │
│  │  └──────────────────────────────────────────┘        │    │  │   │
│  └──────────────────────────────────────────────────────┘    │  │   │
│                                                               │  │   │
│  ┌────────────────────────────────────────────────────────┐   │  │   │
│  │              Security Layer                              │   │  │   │
│  │  ├── RSA-2048 Signature Verification ◄──────────────────┘   │   │
│  │  ├── SHA256 Checksum Validation ◄───────────────────────────┘   │
│  │  ├── TLS Certificate Pinning ◄──────────────────────────────────┘
│  │  └── Secure Boot V2 (optional, HW root of trust)
│  └──────────────────────────────────────────────────────────────────┘
└─────────────────────────────────────────────────────────────────┘
```

## Partition Table Design

```csv
# partitions.csv — RobotBuddy Flash 分区表 (16MB Flash)
# Name,      Type,        SubType,       Offset,     Size,        Flags
nvs,         data,        nvs,           0x9000,     0x6000,
otadata,     data,        ota,           0xf000,     0x2000,
phy_init,    data,        phy,           0x11000,    0x1000,
factory,     app,         factory,       0x20000,    0x200000,
ota_0,       app,         ota_0,         0x220000,   0x200000,
ota_1,       app,         ota_1,         0x420000,   0x200000,
spiffs,      data,        spiffs,        0x620000,   0x400000,
```

```
分区设计说明:

Flash 总容量: 16MB (0x1000000)
分区分工:
├── nvs         0x6000  (24KB)   — 非易失存储（WiFi凭据/设备配置）
├── otadata     0x2000  (8KB)    — OTA 状态数据（当前启动分区）
├── phy_init    0x1000  (4KB)    — PHY 初始化数据
├── factory     0x200000 (2MB)   — 出厂固件（永不覆盖，保底恢复）
├── ota_0       0x200000 (2MB)   — OTA 分区 0（AB 分区滚动升级）
├── ota_1       0x200000 (2MB)   — OTA 分区 1
└── spiffs      0x400000 (4MB)   — 表情资源/音频文件/配置文件

分区容量计算:
  固件最大体积: 2MB (单分区上限)
  当前固件典型体积: ~1.2MB (含 LVGL + 表情 + WiFi + AI)
  剩余空间: ~800KB (安全余量)
  注意: factory 分区永不 OTA 覆盖，仅作为恢复分区

AB 分区滚动策略:
  首次 OTA: 写入 ota_0 → 切换到 ota_0 启动
  二次 OTA: 写入 ota_1 → 切换到 ota_1 启动
  三次 OTA: 写入 ota_0 → 切换回 ota_0 启动
  模式: 交替使用，始终保留上一次正常固件作为回滚目标
```

## OTA State Machine

```c
// OTA 升级状态机
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

// 状态转换图:
//
// IDLE ──(收到MQTT升级命令)──► CHECKING
// CHECKING ──(有新版本)───────► DOWNLOADING
// CHECKING ──(已是最新)───────► IDLE
// CHECKING ──(网络错误)───────► ERROR ──► IDLE
// DOWNLOADING ──(下载完成)────► VERIFYING
// DOWNLOADING ──(网络中断)────► IDLE (支持断点续传)
// VERIFYING ──(签名OK)────────► READY
// VERIFYING ──(签名失败)──────► ERROR ──► IDLE
// READY ──(用户确认/自动)─────► REBOOTING
// REBOOTING ──(重启完成)──────► VERIFYING_NEW
// VERIFYING_NEW ──(健康检查OK)► COMMITTED ──► IDLE
// VERIFYING_NEW ──(健康检查NG)► ROLLING_BACK ──► REBOOTING
// ROLLING_BACK ──(回滚失败)───► FACTORY_RESET ──► REBOOTING
// ERROR ──(任何状态，等待恢复)► IDLE
```

## OTA Manager API

```c
// ─── OTA 更新信息 ───

typedef enum {
    OTA_UPDATE_NONE,            // 无新版本
    OTA_UPDATE_AVAILABLE,       // 有新版本可选升级
    OTA_UPDATE_MANDATORY,       // 强制升级（严重安全漏洞）
} ota_update_level_t;

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

// ─── OTA 进度 ───

typedef struct {
    ota_state_t state;              // 当前状态
    int progress_percent;           // 下载/刷写进度 (0-100%)
    size_t bytes_downloaded;        // 已下载字节数
    size_t bytes_total;             // 总字节数
    uint32_t elapsed_ms;            // 已耗时
    uint32_t estimated_remaining_ms;// 预估剩余时间
} ota_progress_t;

// ─── OTA 事件回调 ───

typedef void (*ota_state_callback_t)(ota_state_t old_state, ota_state_t new_state);
typedef void (*ota_progress_callback_t)(const ota_progress_t *progress);
typedef void (*ota_result_callback_t)(bool success, const char *version, const char *error);

// ─── 核心 API ───

// 初始化 OTA 管理器（分区表检查 + 安全启动状态检查）
esp_err_t ota_manager_init(void);

// 检查云端是否有新固件
// 通过 HTTPS GET /ota/check?device_id=xxx&current_version=x.y.z 查询
esp_err_t ota_check_update(ota_update_info_t *info);

// 下载固件到备用分区（支持断点续传）
// 自动选择空闲 OTA 分区（ota_0 或 ota_1）
esp_err_t ota_download(const ota_update_info_t *info,
                       ota_progress_callback_t progress_cb);

// 校验固件（RSA-2048 签名 + SHA256 哈希）
esp_err_t ota_verify(const ota_update_info_t *info);

// 应用升级：标记启动分区 + 重启
esp_err_t ota_apply(void);

// 确认新固件运行正常（健康检查通过后调用）
// 在 VERIFYING_NEW 状态下调用，标记分区为 "VALID"
esp_err_t ota_commit(void);

// 手动回滚到上一个正常固件
esp_err_t ota_rollback(void);

// 回退到出厂固件（factory 分区）
esp_err_t ota_rollback_to_factory(void);

// 获取 OTA 状态
esp_err_t ota_get_state(ota_state_t *state);
esp_err_t ota_get_progress(ota_progress_t *progress);

// 获取当前运行分区信息
esp_err_t ota_get_running_partition(char *label, size_t label_size);

// 注册回调
esp_err_t ota_register_state_callback(ota_state_callback_t cb);
esp_err_t ota_register_result_callback(ota_result_callback_t cb);

// ─── MQTT 命令处理 ───

// 收到 MQTT ota/command Topic 消息时的入口
// 消息格式: {"cmd":"check"|"upgrade"|"rollback","url":"...","version":"..."}
void ota_handle_mqtt_command(const char *payload_json);

// ─── 使用示例 ───
//
// // 1. 检查更新
// ota_update_info_t info;
// if (ota_check_update(&info) == ESP_OK && info.firmware_url[0]) {
//     // 2. 下载
//     ota_download(&info, ota_progress_cb);
//     // 3. 校验
//     if (ota_verify(&info) == ESP_OK) {
//         // 4. 应用并重启
//         ota_apply();
//     }
// }
//
// // 重启后：
// void app_main_after_ota(void) {
//     if (ota_get_state() == OTA_STATE_VERIFYING_NEW) {
//         if (health_check_pass()) {
//             ota_commit();  // 确认升级
//         } else {
//             ota_rollback(); // 自动回滚
//         }
//     }
// }
```

## Security

```c
// ─── RSA-2048 签名验证 ───

// 签名流程:
// 1. 服务器端: openssl dgst -sha256 -sign private_key.pem -out firmware.sig firmware.bin
// 2. 设备端固件: 嵌入 RSA-2048 公钥 (PEM 格式, 约 451 bytes)
// 3. 下载 firmware.bin + firmware.sig → mbedTLS 验证签名

// 公钥嵌入（编译时嵌入固件，不可被 OTA 修改）
static const char OTA_PUBLIC_KEY_PEM[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA...\n"
    "-----END PUBLIC KEY-----";

esp_err_t ota_verify_rsa_signature(const uint8_t *firmware_data,
                                   size_t firmware_size,
                                   const uint8_t *signature_data,
                                   size_t signature_size);

// ─── SHA256 校验 ───

// 下载完成后进行完整性校验
// 对比: sha256(firmware.bin) == info->sha256
esp_err_t ota_verify_sha256(const uint8_t *firmware_data,
                            size_t firmware_size,
                            const char *expected_sha256_hex);

// ─── TLS 证书锁定 (Certificate Pinning) ───

// 硬编码 OTA 服务器证书的 SHA256 指纹，防止中间人攻击
// 即使系统根证书被污染，也能保证只与合法 OTA 服务器通信
static const char OTA_SERVER_CERT_SHA256[] =
    "A1:B2:C3:D4:E5:F6:..."; // OTA 服务器证书的 SHA256 指纹

// esp_http_client_config_t 配置示例:
// .cert_pem = NULL,  // 不使用全局 CA 验证
// .client_cert_pem = NULL,
// 在 event handler 中手动校验服务器证书指纹:
// mbedtls_x509_crt_parse() → mbedtls_sha256() → 对比指纹

// ─── 签名 OTA URL（防重放攻击） ───

// OTA URL 携带签名和过期时间，防止 URL 被重放或篡改
// 格式: https://ota.robotbuddy.local/firmware_v2.0.0.bin
//         ?token=HMAC_SHA256(device_id+version+timestamp, server_secret)
//         &expires=1718000000
//         &device_id=XX:XX:XX:XX:XX:XX

typedef struct {
    char url[320];              // 完整签名 URL
    uint64_t expires_at;        // URL 过期时间 (unix timestamp)
    bool is_expired;            // 是否已过期
} ota_signed_url_t;

esp_err_t ota_parse_signed_url(const char *raw_url, ota_signed_url_t *signed_url);
bool ota_url_is_expired(const ota_signed_url_t *signed_url);

// ─── Secure Boot V2 集成（可选，生产环境推荐） ───

// 启用 Secure Boot V2 后:
//   1. Bootloader 校验 app 分区的 RSA-3072 签名
//   2. ESP32 内部 eFuse 存储签名密钥摘要，不可篡改
//   3. OTA 固件必须经过双重签名: OTA 服务器签名 + Secure Boot 签名
//   4. 固件签名流程:
//      espsecure.py sign_data --version 2 --keyfile secure_boot_signing_key.pem
//          --output firmware_signed.bin firmware.bin

// 安全启动配置检查
esp_err_t ota_check_secure_boot_status(bool *enabled);
```

## Error Handling & Rollback

```c
// ─── 健康检查 ───

#define OTA_HEALTH_CHECK_TIMEOUT_MS   30000   // 启动后 30s 内必须完成健康检查
#define OTA_WATCHDOG_THRESHOLD        3       // watchdog 重启 3 次 → 自动回滚
#define OTA_PANIC_THRESHOLD           5       // panic 5 次 → 自动回滚

typedef enum {
    HEALTH_CHECK_PASS,          // 所有检查通过
    HEALTH_CHECK_WIFI_FAIL,     // WiFi 连接失败
    HEALTH_CHECK_MQTT_FAIL,     // MQTT 连接失败
    HEALTH_CHECK_DISPLAY_FAIL,  // 显示异常
    HEALTH_CHECK_SENSOR_FAIL,   // 传感器初始化失败
    HEALTH_CHECK_WATCHDOG,      // watchdog 触发
    HEALTH_CHECK_PANIC,         // panic 触发
} health_check_result_t;

// 健康检查项:
// 1. WiFi 是否成功连接（30s 超时）
// 2. MQTT Broker 是否连接成功
// 3. 屏幕显示是否正常（无花屏）
// 4. I2C 总线是否可通信（MPU6050 读取成功）
// 5. 表情 UI 是否正常渲染（首帧渲染完成）
health_check_result_t ota_health_check(void);

// ─── 自动回滚 ───

typedef struct {
    uint8_t watchdog_count;         // watchdog 重启计数 (NVS 持久化)
    uint8_t panic_count;            // panic 计数 (NVS 持久化)
    uint32_t boot_timestamp;        // 本次启动时间戳
    uint32_t health_check_deadline; // 健康检查截止时间
    bool health_check_passed;       // 健康检查是否通过
} ota_boot_state_t;

// 自动回滚触发条件:
// 条件 1: watchdog 重启 ≥ OTA_WATCHDOG_THRESHOLD (3 次)
// 条件 2: panic ≥ OTA_PANIC_THRESHOLD (5 次)
// 条件 3: 启动后 health_check_deadline 超时未完成健康检查
// 条件 4: 健康检查结果非 HEALTH_CHECK_PASS

// 回滚逻辑:
//   1. esp_ota_mark_app_invalid_rollback_and_reboot()
//      → 标记当前启动分区为 INVALID
//      → 自动切换到上一个 VALID 分区
//      → 如果两个 OTA 分区都 INVALID → 回退 factory

typedef enum {
    ROLLBACK_REASON_NONE,
    ROLLBACK_REASON_WATCHDOG,       // watchdog 重启超限
    ROLLBACK_REASON_PANIC,          // panic 重启超限
    ROLLBACK_REASON_HEALTH_FAIL,    // 健康检查失败
    ROLLBACK_REASON_USER_REQUEST,   // 用户手动回滚
    ROLLBACK_REASON_SERVER_COMMAND, // 服务器下发回滚命令
} rollback_reason_t;

esp_err_t ota_auto_rollback_check(void);  // 开机时调用，检查是否需要回滚
esp_err_t ota_manual_rollback(rollback_reason_t reason);

// ─── NVS 持久化回滚计数 ───

// 存储格式 (NVS namespace: "ota_boot"):
//   "wd_count"     uint8   watchdog 重启计数（每次 watchdog 重启 +1）
//   "panic_count"  uint8   panic 重启计数（每次 panic 重启 +1）
//   "boot_time"    uint32  最近一次启动时间戳

// 复位逻辑:
//   正常启动并 commit 后 → 清零 wd_count 和 panic_count
//   每次异常重启时 → 对应计数器 +1
//   达到阈值 → 触发回滚

// ─── Factory 恢复（最后手段） ───

// 当 ota_0 和 ota_1 都不可用时:
//   1. 标记 otadata → 指向 factory
//   2. 清除 NVS 中的所有 OTA 状态
//   3. 重启到 factory 分区
//   4. 用户数据(spiffs)保留，但系统配置可能需要重新设置

esp_err_t ota_factory_restore(void);

// ─── 下载错误处理 ───

typedef enum {
    OTA_DOWNLOAD_ERR_NONE,
    OTA_DOWNLOAD_ERR_NETWORK,       // 网络中断
    OTA_DOWNLOAD_ERR_HTTP,          // HTTP 错误（4xx/5xx）
    OTA_DOWNLOAD_ERR_SIZE_MISMATCH, // 下载大小与声明不符
    OTA_DOWNLOAD_ERR_SPACE,         // 目标分区空间不足
    OTA_DOWNLOAD_ERR_WRITE_FAIL,    // Flash 写入失败
    OTA_DOWNLOAD_ERR_TIMEOUT,       // 下载超时
} ota_download_error_t;

// 断点续传:
//   使用 HTTP Range 头: GET /firmware.bin HTTP/1.1\r\nRange: bytes=524288-\r\n
//   记录已下载字节数到 NVS，中断后从断点继续
//   重试策略: 最多重试 3 次，每次间隔递增 (5s / 15s / 30s)
```

## OTA Delta Update (V2+ Optional)

```c
// ─── 差分升级（Delta OTA） ───

// 原理: 仅下载新旧固件的差异数据（bsdiff / HDiffPatch）
// 优势: 固件 2MB → 差分包通常仅 200-500KB，节省带宽和时间
// 要求: 设备上已有当前版本的完整固件镜像

typedef enum {
    DELTA_TYPE_BSDIF,           // bsdiff 差分算法
    DELTA_TYPE_HDIFFPATCH,      // HDiffPatch（更快，适合嵌入式）
} delta_algorithm_t;

typedef struct {
    delta_algorithm_t algorithm;
    char delta_url[256];            // 差分包下载 URL
    char base_version[32];          // 基准版本（当前运行版本）
    char target_version[32];        // 目标版本
    size_t delta_size;              // 差分包大小
    char delta_sha256[65];          // 差分包 SHA256
    char target_sha256[65];         // 目标固件完整 SHA256（用于验证合成结果）
} ota_delta_info_t;

// 差分升级流程:
// 1. 下载差分包 (delta.bin)
// 2. 读取当前运行分区的完整固件镜像
// 3. 在 Flash 中合成新固件镜像 → 写入目标 OTA 分区
// 4. 验证合成后固件的 SHA256 == target_sha256
// 5. 正常签名校验 → 应用升级

esp_err_t ota_delta_check_update(ota_delta_info_t *info);
esp_err_t ota_delta_download(const ota_delta_info_t *info,
                             ota_progress_callback_t progress_cb);
esp_err_t ota_delta_apply_patch(const ota_delta_info_t *info);
esp_err_t ota_delta_verify(const ota_delta_info_t *info);
```

## OTA Test Matrix

```c
// ─── OTA 测试矩阵 ───

// 测试用例应覆盖以下场景:

// 1. NORMAL — 正常升级流程
//    条件: 稳定 WiFi, 充足电量 (>50%), 无中断
//    步骤: check → download → verify → apply → reboot → health_check → commit
//    预期: 升级成功，COMMITTED 状态
//
// 2. INTERRUPTED_DOWNLOAD — 下载中断后恢复
//    条件: 下载到 50% 时断开 WiFi, 10s 后重连
//    步骤: download(中断) → reconnect → download(续传)
//    预期: 从中断点继续下载，最终成功
//
// 3. WIFI_DISCONNECT_DOWNLOAD — 下载中 WiFi 断开
//    条件: 下载到 80% 时 WiFi 彻底断开
//    步骤: download(失败) → 超时 → ERROR → IDLE
//    预期: 返回 IDLE，记录错误日志，下次重新下载
//
// 4. POWER_LOSS_FLASH — 刷写时断电
//    条件: 固件刷写到一半断电
//    步骤: write(中断) → 上电 → 检测到分区不完整 → 重新下载
//    预期: 自动恢复，重新下载完整固件
//
// 5. VERSION_SKIP — 版本跳跃升级
//    条件: 当前 V1.0.0 → 目标 V3.0.0（跨 2 个大版本）
//    步骤: check(3.0 可用) → download → verify → apply
//    预期: 
//      - 如使用完整包升级: 正常升级成功
//      - 如使用差分升级: 需依次 1.0→2.0, 2.0→3.0 或使用全量包
//
// 6. DOWNGRADE — 降级测试
//    条件: 当前 V2.0.0 → 目标 V1.0.0
//    步骤: check(1.0 可用, 标记为降级) → 用户确认 → download → verify → apply
//    预期: 降级成功（需用户主动确认，不允许自动降级）
//
// 7. SIGNATURE_FAIL — 签名校验失败
//    条件: 下载被篡改的固件或错误的签名文件
//    步骤: download(fake_firmware) → verify → RSA 签名不匹配 → ERROR
//    预期: 拒绝升级，保留当前固件不变
//
// 8. HEALTH_CHECK_FAIL — 新固件运行异常，自动回滚
//    条件: 新固件启动后 MQTT 无法连接
//    步骤: reboot → health_check(MQTT_FAIL) → rollback → reboot → 旧固件正常
//    预期: 自动回滚到上一个正常版本
//
// 9. WATCHDOG_ROLLBACK — watchdog 触发自动回滚
//    条件: 新固件运行中 3 次 watchdog 重启
//    步骤: reboot → watchdog → reboot → watchdog → reboot → watchdog → rollback
//    预期: 第 3 次 watchdog 后自动回滚
//
// 10. BOTH_PARTITIONS_CORRUPT — 双分区损坏，回退 factory
//    条件: ota_0 和 ota_1 都标记为 INVALID
//    步骤: ... → factory_restore → reboot → factory 固件运行
//    预期: 回退到出厂固件，系统可正常工作
//
// 11. LOW_BATTERY_OTA — 低电量时拒绝 OTA
//    条件: 电量 < 20%
//    步骤: check(update available) → 拒绝升级 → 提示"电量不足，请充电后升级"
//    预期: 不执行 OTA，保护设备安全
//
// 12. DELTA_VS_FULL — 差分升级 vs 全量升级切换
//    条件: 差分升级中间版本缺失时自动切换全量升级
//    步骤: check(delta 不可用) → fallback to full download → download → verify → apply
//    预期: 自动降级为全量升级，流程正常完成

// 测试结果记录结构体
typedef struct {
    const char *test_name;
    bool passed;
    const char *failure_reason;
    uint32_t duration_ms;
    ota_state_t final_state;
} ota_test_result_t;
```

## Rules

1. **绝不覆盖 factory 分区** — factory 是最后保底恢复手段，OTA 只能写入 ota_0 或 ota_1
2. **先校验再切换** — 固件下载完成后必须先通过 RSA-2048 签名验证和 SHA256 校验，确认无误后才标记启动分区
3. **健康检查必须在 30s 内完成** — 新固件启动后 30s 未通过健康检查，自动触发回滚
4. **低电量拒绝 OTA** — 电量 < 20% 时拒绝执行 OTA，保护刷写过程中不因断电损坏分区
5. **升级时禁止其他 Flash 写入** — OTA 进行中禁止 spiffs 写入、NVS 写入，防止 Flash 操作冲突
6. **证书过期拒绝升级** — 服务器 TLS 证书过期或指纹不匹配时中止连接，防止中间人攻击
7. **不允许静默降级** — 降级操作必须用户手动确认，不允许自动降级（避免引入旧版本的安全漏洞）
8. **OTA 状态持久化到 NVS** — 当前 OTA 状态、回滚计数、启动时间戳持久化到 NVS，掉电后可恢复
9. **下载中断支持续传** — 使用 HTTP Range 请求实现断点续传，中断后从断点继续，最多重试 3 次
10. **回滚后上报服务器** — 触发回滚时，通过 MQTT 上报回滚原因和版本信息，便于远程诊断
11. **签名公钥不可 OTA 修改** — RSA-2048 公钥编译时嵌入固件，存储在安全区域（如 eFuse 或加密 NVS），OTA 无法改写
12. **OTA URL 有时效性** — 签名的 OTA URL 携带过期时间戳，过期 URL 拒绝使用，防止重放攻击

## Checklist

- [ ] partitions.csv 分区表正确（factory + ota_0 + ota_1 + otadata）
- [ ] OTA 状态机全路径测试通过（IDLE → CHECKING → DOWNLOADING → VERIFYING → READY → REBOOTING → VERIFYING_NEW → COMMITTED）
- [ ] RSA-2048 签名验证通过（正确签名接受，错误签名拒绝）
- [ ] SHA256 校验正确（完整固件通过，损坏固件拒绝）
- [ ] TLS 证书锁定验证（合法证书通过，自签名/替换证书拒绝）
- [ ] MQTT OTA 命令解析正确（check / upgrade / rollback）
- [ ] 下载进度回调准确（0% → 100% 线性递增）
- [ ] 断点续传正常工作（中断后从断点恢复）
- [ ] 正常升级全流程成功（下载→校验→切换→重启→健康检查→提交）
- [ ] 健康检查失败触发自动回滚
- [ ] watchdog 重启 3 次触发自动回滚
- [ ] panic 重启 5 次触发自动回滚
- [ ] 双分区损坏后成功回退 factory
- [ ] 低电量（< 20%）时拒绝执行 OTA
- [ ] 降级操作需要用户确认
- [ ] OTA URL 过期后拒绝连接
- [ ] 差分升级 V2 功能可选、不影响 V1 全量升级
- [ ] 回滚后 NVS 计数器正确清零
- [ ] OTA 状态在 NVS 持久化后掉电可恢复
- [ ] OTA 升级过程中不影响 spiffs 中表情/音频资源
- [ ] 升级前/后版本号正确上报 MQTT
- [ ] 并发安全：OTA 过程中其他 Flash 写入操作被阻塞
- [ ] 长时间运行（24h）无内存泄漏
