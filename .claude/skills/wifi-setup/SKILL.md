# WiFi Setup Skill — RobotBuddy

## Role

RobotBuddy WiFi 配网与管理专家，负责多种配网方式（SmartConfig / BLE / SoftAP / WPS）、NVS 凭证存储、WiFi 电源管理、RSSI 信号监测和多网络自动切换。

## Domain

ESP32-S3 WiFi 配网方法（SmartConfig、BLE Provisioning、SoftAP、WPS）、NVS 加密存储、WiFi 电源管理（Modem-sleep / Light-sleep）、RSSI 监测与漫游、ESP-IDF v5.x WiFi 驱动与 ESP-NETIF、FreeRTOS 事件组与 Queue。

## Goal

构建稳定、安全的 WiFi 连接管理系统，支持多种配网方式、自动重连、RSSI 感知的网络切换和直观的配网状态可视化。

## Inputs

- 硬件约束（ESP32-S3 2.4GHz only, 板载天线 / IPEX 可选）
- 配网场景需求（首次开机、重置后、新网络环境）
- 配网 UX 规范（表情反馈、LED 状态灯、屏幕指示）
- 安全需求（凭证加密存储、配网通道安全）

## Outputs

- `firmware/services/wifi/wifi_manager.c` — WiFi 连接管理器（初始化、连接、重连、断开）
- `firmware/services/wifi/wifi_provisioning.c` — 配网服务（SmartConfig / BLE / SoftAP / WPS）
- `firmware/services/wifi/wifi_credential.c` — 凭证存储管理（NVS 加密读写、凭证迁移）
- `firmware/services/wifi/wifi_monitor.c` — WiFi 信号监测（RSSI 采样、信号等级评估）
- `firmware/services/wifi/wifi_state_machine.c` — WiFi 状态机（状态转换、事件分发）
- `docs/firmware/wifi-setup.md` — WiFi 配网与管理文档

## WiFi Provisioning Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                    WiFi Provisioning System                        │
│                                                                   │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │                    配网入口 (Provisioning Entry)               │ │
│  │                                                               │ │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐    │ │
│  │  │SmartConfig│  │   BLE    │  │  SoftAP  │  │   WPS    │    │ │
│  │  │ (ESP-Touch│  │Provision │  │  Captive │  │  Button  │    │ │
│  │  │ AirKiss)  │  │ (GATT)   │  │  Portal  │  │   Push   │    │ │
│  │  └─────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘    │ │
│  │        │             │             │             │            │ │
│  └────────┼─────────────┼─────────────┼─────────────┼────────────┘ │
│           │             │             │             │               │
│  ┌────────↓─────────────↓─────────────↓─────────────↓────────────┐ │
│  │                Provisioning Manager (配网决策)                   │ │
│  │                                                                │ │
│  │  ├── 方式选择: 根据配网方式优先级和用户选择                          │ │
│  │  ├── 超时管理: 每种方式独立超时（SmartConfig 120s / BLE 60s）     │ │
│  │  ├── 状态上报: 配网中 / 成功 / 失败 → 表情 + LED + 屏幕           │ │
│  │  └── 安全验证: 仅接受加密配网数据                                 │ │
│  └──────────────────────────┬─────────────────────────────────────┘ │
│                             │                                       │
│  ┌──────────────────────────↓─────────────────────────────────────┐ │
│  │                   Credential Storage (NVS)                       │ │
│  │                                                                  │ │
│  │  ┌──────────────────────┐   ┌──────────────────────┐           │ │
│  │  │ 当前网络凭证            │   │ 历史网络凭证 (最多5组)  │           │ │
│  │  │ ssid + password       │   │ ssid + password      │           │ │
│  │  │ (AES-256 加密存储)      │   │ + last_rssi + priority│           │ │
│  │  └──────────────────────┘   └──────────────────────┘           │ │
│  └──────────────────────────┬─────────────────────────────────────┘ │
│                             │                                       │
│  ┌──────────────────────────↓─────────────────────────────────────┐ │
│  │                     WiFi Manager (运行态)                         │ │
│  │                                                                  │ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │ │
│  │  │ WiFi State   │  │ RSSI Monitor │  │ Auto Roaming │          │ │
│  │  │ Machine      │  │ (定期采样)    │  │ (多网络切换)  │          │ │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘          │ │
│  │         │                 │                 │                    │ │
│  │  ┌──────↓─────────────────↓─────────────────↓──────────────┐    │ │
│  │  │               WiFi Event Dispatcher                       │    │ │
│  │  │  事件 → 上层 (Cloud / Behavior / Power / Display)         │    │ │
│  │  └──────────────────────────────────────────────────────────┘    │ │
│  └──────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────┘
```

## WiFi Manager API

```c
// =============================================
// wifi_manager.h — WiFi 连接管理器
// =============================================

// WiFi 配置
typedef struct {
    char ssid[33];
    char password[65];
    wifi_auth_mode_t auth_mode;     // WPA2 / WPA3 / Open
    uint8_t retry_max;              // 最大重连次数 (默认 5)
    uint32_t retry_base_ms;         // 重连基础间隔 (默认 1000ms)
    bool enable_power_save;         // 是否开启 WiFi 省电
    wifi_ps_type_t ps_mode;         // WIFI_PS_MIN_MODEM / WIFI_PS_MAX_MODEM
} wifi_conn_config_t;

// WiFi 连接状态 (详见下方状态机)
typedef enum {
    WIFI_MGR_STATE_UNINITIALIZED,
    WIFI_MGR_STATE_DISCONNECTED,
    WIFI_MGR_STATE_CONNECTING,
    WIFI_MGR_STATE_CONNECTED,
    WIFI_MGR_STATE_RECONNECTING,
    WIFI_MGR_STATE_PROVISIONING,
    WIFI_MGR_STATE_ERROR,
} wifi_mgr_state_t;

// WiFi 事件 (上报给上层)
typedef enum {
    WIFI_EVENT_CONNECTED,
    WIFI_EVENT_DISCONNECTED,
    WIFI_EVENT_CONNECTION_FAILED,
    WIFI_EVENT_RSSI_LOW,            // RSSI < -70 dBm
    WIFI_EVENT_RSSI_CRITICAL,       // RSSI < -85 dBm
    WIFI_EVENT_PROVISION_DONE,      // 配网成功
    WIFI_EVENT_PROVISION_TIMEOUT,   // 配网超时
    WIFI_EVENT_IP_ACQUIRED,         // 获取到 IP 地址
    WIFI_EVENT_IP_LOST,             // IP 地址丢失
} wifi_event_id_t;

// 事件回调类型
typedef void (*wifi_event_callback_t)(wifi_event_id_t event, void *event_data, void *user_data);

// ── 初始化与生命周期 ──
esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_deinit(void);

// ── 连接管理 ──
esp_err_t wifi_manager_connect(const wifi_conn_config_t *config);
esp_err_t wifi_manager_disconnect(void);

// ── 状态查询 ──
wifi_mgr_state_t wifi_manager_get_state(void);
int8_t wifi_manager_get_rssi(void);         // 返回 dBm
bool wifi_manager_is_connected(void);
esp_err_t wifi_manager_get_ip(char *ip_str, size_t len);

// ── 事件注册 ──
esp_err_t wifi_manager_register_callback(wifi_event_callback_t cb, void *user_data);
esp_err_t wifi_manager_unregister_callback(wifi_event_callback_t cb);

// ── 省电 ──
esp_err_t wifi_manager_set_ps_mode(wifi_ps_type_t mode);
```

## WiFi Provisioning API

```c
// =============================================
// wifi_provisioning.h — 配网服务
// =============================================

// 配网方式
typedef enum {
    PROV_METHOD_SMARTCONFIG,        // ESP-Touch v2 / AirKiss
    PROV_METHOD_BLE,                // BLE GATT Provisioning
    PROV_METHOD_SOFTAP,             // SoftAP + Captive Portal
    PROV_METHOD_WPS,                // WPS Push Button / PIN
    PROV_METHOD_AUTO,               // 自动选择最优方式
} prov_method_t;

// 配网状态
typedef enum {
    PROV_STATE_IDLE,
    PROV_STATE_STARTING,            // 启动配网服务
    PROV_STATE_LISTENING,           // 等待配网数据
    PROV_STATE_RECEIVED,            // 收到凭证
    PROV_STATE_CONNECTING,          // 尝试连接目标 AP
    PROV_STATE_SUCCESS,             // 配网成功
    PROV_STATE_TIMEOUT,             // 配网超时
    PROV_STATE_FAILED,              // 配网失败
    PROV_STATE_CANCELLED,           // 用户取消
} prov_state_t;

// 配网配置
typedef struct {
    prov_method_t preferred_method;     // 首选配网方式
    uint32_t timeout_seconds;           // 配网超时时间 (秒)
    char softap_ssid[33];               // SoftAP 模式时广播的 SSID
    char softap_password[65];           // SoftAP 密码 (最少 8 位)
    char ble_device_name[32];           // BLE 广播名称
    bool show_qr_code;                  // 是否在屏幕上显示二维码 (SoftAP)
    bool auto_start_on_no_creds;        // 无凭证时自动进入配网模式
} prov_config_t;

// ── 配网核心 API ──
esp_err_t wifi_provisioning_init(const prov_config_t *config);

// 启动配网 (阻塞当前 Task，或通过回调异步通知结果)
typedef void (*prov_result_cb_t)(prov_state_t state, const wifi_conn_config_t *config, void *user_data);
esp_err_t wifi_provisioning_start(prov_method_t method, prov_result_cb_t callback, void *user_data);

// 停止配网
esp_err_t wifi_provisioning_stop(void);

// 获取当前配网状态
prov_state_t wifi_provisioning_get_state(void);

// ── 各配网方式初始化 ──
esp_err_t prov_smartconfig_init(void);
esp_err_t prov_smartconfig_start(uint32_t timeout_sec);
esp_err_t prov_smartconfig_stop(void);

esp_err_t prov_ble_init(const char *device_name);
esp_err_t prov_ble_start(uint32_t timeout_sec);
esp_err_t prov_ble_stop(void);

esp_err_t prov_softap_init(const char *ssid, const char *password);
esp_err_t prov_softap_start(uint32_t timeout_sec);
esp_err_t prov_softap_stop(void);

esp_err_t prov_wps_init(void);
esp_err_t prov_wps_start(uint32_t timeout_sec);
esp_err_t prov_wps_stop(void);
```

## Credential Storage

```c
// =============================================
// wifi_credential.h — NVS 凭证存储
// =============================================

#define WIFI_CRED_MAX_COUNT     5       // 最多保存 5 组历史网络
#define WIFI_SSID_MAX_LEN       32
#define WIFI_PASS_MAX_LEN       64

// 单条凭证
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN + 1];
    char password[WIFI_PASS_MAX_LEN + 1];
    wifi_auth_mode_t auth_mode;
    int8_t last_rssi;                   // 上次连接的信号强度
    uint32_t last_connected_time;       // 上次连接时间戳
    uint8_t priority;                   // 优先级 0-255 (数值越大优先级越高)
} wifi_credential_t;

// ── 凭证 CRUD ──

// 保存凭证 (AES-256 加密写入 NVS)
esp_err_t wifi_cred_save(const wifi_credential_t *cred);

// 加载指定 SSID 的凭证
esp_err_t wifi_cred_load(const char *ssid, wifi_credential_t *cred);

// 加载当前活跃的网络凭证 (最后成功连接的那个)
esp_err_t wifi_cred_load_active(wifi_credential_t *cred);

// 加载所有历史凭证
esp_err_t wifi_cred_load_all(wifi_credential_t *creds, uint8_t *count);

// 删除指定凭证
esp_err_t wifi_cred_delete(const char *ssid);

// 删除所有凭证 (恢复出厂设置时调用)
esp_err_t wifi_cred_erase_all(void);

// 设置活跃网络 (标记为优先连接)
esp_err_t wifi_cred_set_active(const char *ssid);

// ── 凭证自动选择 ──

// 扫描周围 AP，在已保存凭证中选择信号最强且可用的网络
// 返回: ESP_OK 且 cred 填充推荐连接的凭证, ESP_ERR_NOT_FOUND 无可用网络
esp_err_t wifi_cred_auto_select(wifi_credential_t *cred);

// ── 凭证迁移 ──

// 从旧版本格式迁移凭证 (NVS 分区结构调整时)
esp_err_t wifi_cred_migrate_if_needed(void);

// 凭证校验 (SSID 长度、密码复杂度等)
bool wifi_cred_validate(const wifi_credential_t *cred);
```

## 配网方式对比表

```
┌─────────────┬──────────────┬──────────────┬──────────────┬──────────────┐
│   特性       │ SmartConfig  │     BLE      │   SoftAP     │     WPS      │
│             │ (ESP-Touch)  │ (Provision)  │ (Captive)    │  (Button)    │
├─────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ 配网设备    │ 手机 App     │ 手机 App     │ 手机浏览器   │ 路由器按钮   │
│             │ (ESP-Touch)  │ (BLE GATT)   │ (任何设备)   │ (物理操作)   │
├─────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ 配网速度    │ 10-30s       │ 5-15s        │ 10-30s       │ 30-120s      │
├─────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ 成功率      │ 85-95%       │ 95-99%       │ 95-99%       │ 90-95%       │
├─────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ 用户操作    │ 输入密码+广播│ 蓝牙扫描+连接│ 连接热点+网页│ 按路由WPS键  │
│ 复杂度      │ 中等         │ 中等         │ 中等         │ 简单         │
├─────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ 手机端依赖  │ ESP-Touch App│ 任意 BLE App │ 浏览器即可   │ 无            │
│             │ (需安装)     │ (系统自带)   │ (系统自带)   │              │
├─────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ 路由器兼容性│ 部分路由器   │ 无依赖       │ 无依赖       │ 需支持 WPS    │
│             │ 不兼容       │              │              │              │
├─────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ 安全性      │ 中等         │ 高           │ 中等         │ 中等         │
│             │ (数据加密)   │ (GATT加密)   │ (HTTPS可配)  │ (PIN可暴力)  │
├─────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ 额外硬件    │ 无           │ BLE 天线     │ 无           │ 无            │
│             │              │ (ESP32-S3内置)│             │               │
├─────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ 多设备配网  │ 不支持       │ 支持         │ 支持         │ 不支持        │
│ (同时)      │              │              │              │              │
├─────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ 配网后      │ WiFi STA     │ WiFi STA     │ WiFi STA     │ WiFi STA     │
│ 模式切换    │ 自动恢复     │ 自动恢复     │ 需手动切换   │ 自动恢复     │
├─────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ RobotBuddy  │ 首选方式     │ 备选方式     │ 调试/维护    │ 可选方式     │
│ 优先级      │ (设备数量少) │ (可靠性高)   │ (无App时)    │ (需路由支持) │
└─────────────┴──────────────┴──────────────┴──────────────┴──────────────┘

推荐策略:
  1. 首次配网 → SmartConfig (优先) → BLE (备选) → SoftAP (兜底)
  2. 用户触发重配 → 上次成功的方式
  3. 工厂模式 → SoftAP + Captive Portal (无需 App)
  4. WPS 作为最后手段 (仅当路由器明确支持 WPS 且有物理按键可触达)
```

## WiFi 状态机

```c
// =============================================
// wifi_state_machine.h — WiFi 状态机
// =============================================

// 完整 WiFi 状态 (包含配网 + 连接 + 重连)
typedef enum {
    // ── 初始态 ──
    WIFI_SM_INIT,                   // 初始化中 (netif + driver init)

    // ── 无凭证态 ──
    WIFI_SM_NO_CREDENTIALS,         // 无保存的网络凭证 → 需要配网

    // ── 配网态 ──
    WIFI_SM_PROV_STARTING,          // 启动配网服务
    WIFI_SM_PROV_LISTENING,         // 配网监听中 (等待手机发送凭证)
    WIFI_SM_PROV_RECEIVED,          // 收到凭证，开始连接目标 AP

    // ── 连接态 ──
    WIFI_SM_SCANNING,               // 扫描周围 AP
    WIFI_SM_CONNECTING,             // 连接中 (DHCP 进行中)
    WIFI_SM_CONNECTED,              // 已连接 (IP 获取成功)

    // ── 断线重整 ──
    WIFI_SM_DISCONNECTED,           // 已断开连接
    WIFI_SM_RECONNECTING,           // 重连中 (指数退避)
    WIFI_SM_RECONNECT_FAILED,       // 重连失败 (已达最大重试次数)

    // ── 异常态 ──
    WIFI_SM_ERROR,                  // 一般错误
    WIFI_SM_FATAL_ERROR,            // 致命错误 (需重启)
} wifi_sm_state_t;

// ── 状态转换图 ──
//
//                         ┌─────────────────────┐
//                         │    WIFI_SM_INIT      │
//                         └──────────┬──────────┘
//                                    │ init ok
//                                    ↓
//                      ┌─────────────────────────┐
//            ┌────────┤  有凭证?   无凭证?        ├────────┐
//            │        └─────────────────────────┘        │
//            │ yes                                       │ no
//            ↓                                           ↓
//  ┌──────────────────┐                    ┌──────────────────────────┐
//  │ WIFI_SM_SCANNING │                    │ WIFI_SM_NO_CREDENTIALS   │
//  └────────┬─────────┘                    └────────────┬─────────────┘
//           │ 找到最佳 AP                                 │ 用户触发配网
//           ↓                                             ↓
//  ┌──────────────────┐                    ┌──────────────────────────┐
//  │WIFI_SM_CONNECTING│                    │ WIFI_SM_PROV_STARTING    │
//  └────────┬─────────┘                    └────────────┬─────────────┘
//           │                                             │
//      ┌────┴────┐                                        ↓
//      │         │                           ┌──────────────────────────┐
//   成功 ↓    失败 ↓                         │ WIFI_SM_PROV_LISTENING   │
//  ┌────┴──┐ ┌───┴────────────┐             └────────────┬─────────────┘
//  │CONNECTED│ │WIFI_SM_ERROR  │                          │ 收到凭证
//  └───┬─────┘ └────────────────┘                          ↓
//      │                                     ┌──────────────────────────┐
//      ├── 断开 ──→ DISCONNECTED             │ WIFI_SM_PROV_RECEIVED    │
//      │              │                      └────────────┬─────────────┘
//      │              ↓                                    │
//      │      ┌──────────────────────┐                     ↓
//      │      │WIFI_SM_RECONNECTING  │←────────────────────┘
//      │      └──────────┬───────────┘         (进入连接流程)
//      │                 │
//      │       ┌─────────┴─────────┐
//      │       │                   │
//      │    成功↓               失败↓ (超过最大重试)
//      │  CONNECTED      RECONNECT_FAILED
//      │                     │
//      │           ┌─────────┴─────────┐
//      │           │ 切换到备选网络?     │
//      │           │ 或等待用户操作      │
//      │           └───────────────────┘
//      │
//      └── RSSI 过低 ──→ 触发漫游 (SCANNING → CONNECTING 到更强 AP)
//

// ── 状态机 API ──

// 状态机初始化
esp_err_t wifi_sm_init(void);

// 获取当前状态
wifi_sm_state_t wifi_sm_get_state(void);

// 状态名称 (用于日志和屏幕显示)
const char *wifi_sm_state_to_string(wifi_sm_state_t state);

// 触发状态转换 (通常由内部事件自动触发，也支持外部强制跳转)
esp_err_t wifi_sm_transition(wifi_sm_state_t target_state);

// 状态变更回调
typedef void (*wifi_sm_state_cb_t)(wifi_sm_state_t old_state, wifi_sm_state_t new_state);
esp_err_t wifi_sm_register_callback(wifi_sm_state_cb_t cb);

// ── 重连策略 ──

// 指数退避重连
// retry  1: 1s
// retry  2: 2s
// retry  3: 4s
// retry  4: 8s
// retry  5: 16s
// retry  6+: 30s (封顶)
//
// 如果保存了多个网络凭证，重试 3 次当前网络失败后，
// 自动尝试切换到下一个信号最好的已保存网络

#define WIFI_RECONNECT_BASE_MS      1000    // 基础间隔
#define WIFI_RECONNECT_MAX_MS       30000   // 最大间隔
#define WIFI_RECONNECT_MAX_RETRY    5       // 每个网络最大重试
#define WIFI_SWITCH_NETWORK_AFTER   3       // 重试 N 次后切换网络
```

## RSSI Monitor

```c
// =============================================
// wifi_monitor.h — RSSI 信号监测
// =============================================

#define RSSI_SAMPLE_PERIOD_MS       5000    // RSSI 采样周期 (5s)
#define RSSI_SAMPLE_WINDOW          6       // 滑动窗口大小 (30s 有效数据)

// 信号等级
typedef enum {
    RSSI_EXCELLENT,     // ≥ -50 dBm   (满格)
    RSSI_GOOD,          // -50 ~ -60   (3格)
    RSSI_FAIR,          // -60 ~ -70   (2格)
    RSSI_WEAK,          // -70 ~ -80   (1格)
    RSSI_CRITICAL,      // < -80       (断线风险)
} rssi_level_t;

typedef struct {
    int8_t current_rssi;            // 当前 RSSI (dBm)
    int8_t avg_rssi;                // 滑动窗口平均 RSSI
    int8_t min_rssi;                // 窗口内最小 RSSI
    int8_t max_rssi;                // 窗口内最大 RSSI
    rssi_level_t level;             // 信号等级
    uint8_t signal_bars;            // 信号格数 (0-4)
    bool is_degrading;              // 信号是否在持续衰减
    int8_t rssi_trend;              // 信号趋势 (dBm/分钟, 正=变好, 负=变差)
} wifi_rssi_report_t;

// RSSI 监测 API
esp_err_t wifi_monitor_init(void);
esp_err_t wifi_monitor_start(void);
esp_err_t wifi_monitor_stop(void);
esp_err_t wifi_monitor_get_report(wifi_rssi_report_t *report);

// 弱信号回调
// 当 RSSI < -70 dBm 持续 30s → 触发弱信号事件
// 当 RSSI < -85 dBm 持续 10s → 触发严重弱信号事件 (建议切换网络)
typedef void (*rssi_alert_cb_t)(rssi_level_t level, int8_t rssi, void *user_data);
esp_err_t wifi_monitor_register_alert(rssi_alert_cb_t cb, void *user_data);

// ── 自动漫游 ──
//
// 条件: 当前 RSSI < -75 dBm 持续 60s
// 动作: 扫描周围 AP → 在已保存凭证中选择信号最强的 → 自动切换
// 保护: 30s 内不重复漫游
//       连接中的网络至少维持 30s 再切换
```

## 配网 UX 流程

```c
// =============================================
// 配网时的用户交互设计
// =============================================

// ── 表情反馈 ──
//
// 配网状态                         表情            屏幕文字            LED
// ────────────────────────────────────────────────────────────────────────
// 无凭证 (首次开机)                大眼睛+好奇      "Hi! 让我连上WiFi"  蓝色呼吸
// PROV_STARTING                   眯眼+思考        "正在准备配网..."    蓝色闪烁
// PROV_LISTENING (SmartConfig)    眨眼+期待        "请用App发送WiFi密码" 蓝色快闪
// PROV_LISTENING (BLE)            眨眼+期待        "请打开蓝牙连接我"   蓝色快闪
// PROV_LISTENING (SoftAP)         眨眼+QR码        "连接热点完成配置"   蓝色快闪
// PROV_RECEIVED                   开心+确认        "收到! 正在连接..."  蓝色→绿色渐变
// PROV_CONNECTING                 专注+旋转        "连接WiFi中..."      绿色闪烁
// PROV_SUCCESS                    大笑+庆祝        "连上啦! (信号格)"  绿色常亮
// PROV_TIMEOUT                    失望+嘟嘴        "配网超时, 重试?"    红色闪烁
// PROV_FAILED                     沮丧+问号        "配网失败, 检查密码" 红色常亮
// PROV_CANCELLED                  中性+眨眼        "配网已取消"         蓝色常亮

// ── 配网超时策略 ──
//
// SmartConfig: 120s 超时  → 提示失败 → 询问是否切换到 BLE
// BLE:          60s 超时  → 提示失败 → 询问是否切换到 SoftAP
// SoftAP:      180s 超时  → 提示失败 → 询问是否重试 或 WPS
// WPS:         120s 超时  → 提示失败 → 回到配网方式选择
//
// 总计配网流程不超过 5 分钟 (防止电池消耗)
// 超过 5 分钟后进入省电模式，等待用户手动触发重新配网

// ── LED 状态灯协议 ──
//
// LED 颜色          闪烁模式       含义
// ──────────────────────────────────────────────
// 蓝色              常亮          已连接 (空闲)
// 蓝色              慢闪 (1Hz)    SmartConfig 监听中
// 蓝色              快闪 (3Hz)    BLE 广播中
// 青色              慢闪 (1Hz)    SoftAP 模式
// 绿色              常亮          连接成功
// 绿色              慢闪 (1Hz)    WiFi 已连接 (PS 模式)
// 黄色              慢闪 (1Hz)    重连中
// 红色              快闪 (3Hz)    连接失败 / 超时
// 白色              常亮          恢复出厂 / 重置中

// ── 屏幕 UI 元素 ──
//
// WiFi 图标 (状态栏右上角):
//   ├── 无凭证:   灰色 WiFi 图标 + "?"
//   ├── 配网中:   蓝色 WiFi 图标 + 动画点
//   ├── 已连接:   信号格数图标 (0-4 格, 基于 RSSI)
//   ├── 重连中:   黄色 WiFi 图标 + 旋转动画
//   └── 错误:     红色 WiFi 图标 + "!"
//
// IP 地址显示 (设置页面):
//   192.168.1.100
//   SSID: MyHomeWiFi
//   RSSI: -55 dBm ▂▄▆█

// ── 语音反馈 (可选, 配网成功/失败时) ──
//
// 成功: "WiFi 已连接" (TTS)
// 超时: "配网超时，请重试" (TTS)
// 失败: "网络连接失败，请检查密码" (TTS)
//
// 语音仅在非静音模式下播放
// 配网过程中禁用语音采集 (避免干扰)
```

## WiFi Power Management

```c
// =============================================
// WiFi 电源管理策略
// =============================================

// ESP32-S3 WiFi 省电模式
//
// WIFI_PS_NONE          — 不休眠 (最低延迟, 最高功耗 ~170mA)
// WIFI_PS_MIN_MODEM     — Modem-sleep (CPU 运行, WiFi 休眠, DTIM 间隔唤醒)
//                         典型功耗 ~15mA, 延迟增加 100-300ms
// WIFI_PS_MAX_MODEM     — 深度 Modem-sleep (更长 DTIM 间隔)
//                         典型功耗 ~5mA, 延迟增加 500ms-2s

// 省电策略 — 根据电源模式自动调整
//
// Power Mode          WiFi PS Mode        Beacon Interval  适用场景
// ──────────────────────────────────────────────────────────────────
// ACTIVE              WIFI_PS_NONE        DTIM 1            实时通信/配网
// POWER_SAVE          WIFI_PS_MIN_MODEM   DTIM 3            MQTT 保持
// LIGHT_SLEEP         WIFI_PS_MAX_MODEM   DTIM 5            仅接收通知
// DEEP_SLEEP          WiFi OFF            —                 完全断网
//
// 配网期间强制 WIFI_PS_NONE (确保配网成功率)

// ── WiFi 功耗统计 ──
//
// 2.4GHz WiFi 典型功耗 (ESP32-S3, 3.3V):
//
// 模式              TX 峰值    RX        平均 (DTIM 1)  平均 (DTIM 3)
// ──────────────────────────────────────────────────────────────────
// Active            310mA      110mA      120mA          80mA
// Modem-sleep       —          —          15mA           8mA
// Max Modem-sleep   —          —          5mA            3mA
// WiFi OFF          —          —          ~0.5mA         ~0.5mA
//
// 配网期间 (SmartConfig):
//   平均 150mA (持续监听 UDP 广播)
//   建议: 配网期间保持 USB 供电, 或确保电量 > 50%

// ── WiFi 电源管理 API ──
esp_err_t wifi_power_set_mode(wifi_ps_type_t mode);
wifi_ps_type_t wifi_power_get_mode(void);

// 监听电源模式切换, 自动调整 WiFi 省电等级
// (在 power_manager 回调中调用)
esp_err_t wifi_power_on_system_power_change(power_mode_t new_mode);

// 配网期间临时禁用省电
esp_err_t wifi_power_disable_for_provisioning(void);
esp_err_t wifi_power_restore_after_provisioning(void);
```

## 多网络管理 (Multi-Network)

```c
// =============================================
// 多网络自动切换
// =============================================

// 使用场景:
//   用户可能在家、办公室、工作室等不同环境使用 RobotBuddy
//   系统应自动识别环境并连接到对应的最优网络

#define WIFI_NETWORK_MAX_SAVED      5       // 最多保存 5 个网络

typedef enum {
    NETWORK_PRIORITY_MANUAL,        // 手动指定优先级
    NETWORK_PRIORITY_LAST_USED,     // 最后使用的网络优先
    NETWORK_PRIORITY_RSSI,          // 信号最强优先
    NETWORK_PRIORITY_CUSTOM,        // 用户自定义排序
} network_priority_mode_t;

// 网络切换决策:
//
// 1. 当前连接 RSSI < -75 dBm 持续 60s
// 2. 扫描周围 AP
// 3. 在已保存的凭证中查找是否有更强的 AP 可用
// 4. 如果有且 RSSI 差值 > 10 dBm → 自动切换
// 5. 如果无 → 保持当前连接 (不变比断线好)
//
// 切换保护:
//   - 连接新网络前先确保能获取 IP
//   - 新连接稳定 10s 后才断开旧连接
//   - 30s 内不重复切换
//   - 切换期间暂停云端通信 (缓冲或丢弃)

// ── 网络评分 ──
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN + 1];
    int8_t rssi;
    bool is_saved;                  // 是否有凭证
    bool is_current;                // 是否是当前连接
    uint8_t score;                  // 综合评分 (0-100)
    // 评分公式: RSSI 权重 60% + 历史连接次数 25% + 手动优先级 15%
} wifi_network_score_t;

esp_err_t wifi_network_scan_and_score(wifi_network_score_t *networks, uint8_t *count);
esp_err_t wifi_network_switch_to(const char *ssid);
```

## 安全

```c
// =============================================
// WiFi 凭证安全存储
// =============================================

// ── NVS 加密存储 ──
//
// ESP32-S3 支持 Flash Encryption + NVS Encryption:
//
// 1. Flash Encryption (硬件层):
//    - 使用 eFuse 存储 256-bit AES 密钥
//    - 所有 Flash 内容 (包括 NVS 分区) 自动加解密
//    - 启用后无法读取原始 Flash 内容
//    - 生产固件必须启用
//
// 2. NVS Encryption (软件层):
//    - 在 Flash Encryption 基础上额外加密 NVS 分区
//    - 使用 nvs_flash_init() + nvs_sec_cfg_t
//    - 密钥来源: eFuse 或 HMAC 派生
//    - 所有 WiFi 凭证存储在这个加密分区
//
// 推荐方案:
//   生产: Flash Encryption (eFuse) + NVS Encryption
//   开发: NVS Encryption only (方便调试)
//
// 凭证在内存中:
//   - 仅在连接时解密到 RAM
//   - 连接完成后立即从 RAM 中清除 (memset 0)
//   - 不在日志中打印密码 (仅打印 SSID)

// ── 凭证加密存储 API ──
typedef struct {
    uint8_t key[32];                // NVS 加密密钥 (256-bit)
    bool flash_encryption_enabled;  // Flash 加密是否已启用
} wifi_security_config_t;

// 初始化加密存储
esp_err_t wifi_security_init(const wifi_security_config_t *config);

// 安全写入凭证 (自动 AES-256 加密)
esp_err_t wifi_cred_save_secure(const wifi_credential_t *cred);

// 安全读取凭证 (自动 AES-256 解密, 返回后 caller 负责清零)
esp_err_t wifi_cred_load_secure(const char *ssid, wifi_credential_t *cred);

// 安全删除 (覆盖 NVS 区域后再删除)
esp_err_t wifi_cred_delete_secure(const char *ssid);

// 安全擦除所有凭证 (NVS 区域填 0 后删除 key)
esp_err_t wifi_cred_erase_all_secure(void);

// ── 配网通道安全 ──
//
// BLE Provisioning:
//   - Security Level 1: 椭圆曲线 Diffie-Hellman (ECDH) 密钥交换
//   - Security Level 2: ECDH + Proof of Possession (PoP)
//   - RobotBuddy 建议使用 Security Level 1 (PoP 增加用户操作复杂度)
//
// SoftAP:
//   - WPA2-PSK 加密 WiFi 信道
//   - Captive Portal 建议使用 HTTPS (自签名证书)
//   - HTTP → HTTPS 强制跳转
//
// SmartConfig:
//   - ESP-Touch v2 支持 AES 加密
//   - AirKiss 数据包已加密
//   - 注意: SmartConfig 数据通过 UDP 广播, 理论上同信道设备可嗅探
//
// WPS:
//   - WPS PIN 模式已知安全漏洞 (暴力破解), 不推荐
//   - WPS Push Button 模式较安全 (物理访问前提)
//   - RobotBuddy 仅支持 WPS Push Button

// ── 凭证安全规则 ──
//
// 1. 不做: 明文存储密码
// 2. 不做: 日志/串口输出密码
// 3. 不做: HTTP 传输凭证 (配网时必须 HTTPS 或加密信道)
// 4. 不做: 将凭证暴露给调试接口
// 5. 必须: 恢复出厂设置时安全擦除所有凭证
// 6. 必须: 内存中的凭证使用后立即清零
// 7. 必须: 生产固件启用 Flash Encryption
// 8. 必须: 配网通道选择加密方式 (BLE ECDH 或 SoftAP WPA2)
```

## Rules

1. **配网安全** — 所有配网方式必须使用加密通道 (SmartConfig AES / BLE ECDH / SoftAP WPA2+HTTPS)
2. **凭证加密存储** — WiFi 密码必须 AES-256 加密后存入 NVS，绝不明文存储
3. **自动重连** — WiFi 断线自动重连，使用指数退避 (1s, 2s, 4s, 8s, 16s, 封顶 30s)
4. **多网络切换** — 当前连接 RSSI < -75 dBm 持续 60s 时，自动扫描并切换到信号更强的已保存网络
5. **配网超时** — 每种配网方式有独立超时，总配网流程不超过 5 分钟，防止电池耗尽
6. **配网模式功耗** — 配网期间强制 WIFI_PS_NONE，建议 USB 供电或电量 > 50%
7. **UX 反馈** — 每个配网状态变化必须同步更新表情、LED 和屏幕显示
8. **安全擦除** — 恢复出厂设置时需覆盖擦除 NVS 凭证区 (先填 0 再删除 key)
9. **内存安全** — 凭证加载到 RAM 后，连接完成立即清零内存 (memset 0x00)
10. **日志安全** — 日志中不得输出 WiFi 密码，仅输出 SSID 和连接状态
11. **漫游保护** — 30s 内不重复切换网络，新连接稳定 10s 后才断开旧连接
12. **依赖声明** — WiFi Manager 依赖 power-management skill (电源模式切换联动) 和 cloud-communication skill (连接状态上报)

## Checklist

- [ ] SmartConfig 配网流程正常 (ESP-Touch App → 收到凭证 → 连接成功)
- [ ] BLE Provisioning 配网流程正常 (手机 App GATT 连接 → 发送凭证 → 连接成功)
- [ ] SoftAP Captive Portal 配网流程正常 (连接热点 → 网页输入 SSID/密码 → 连接成功)
- [ ] WPS Push Button 配网流程正常 (按下路由器 WPS 键 → 自动连接)
- [ ] WiFi 凭证 AES-256 加密存储验证 (读取 NVS 原始数据不可见明文密码)
- [ ] 首次开机无凭证时自动进入配网模式
- [ ] WiFi 断线后自动重连 (指数退避)
- [ ] 多网络凭证自动选择（在已保存网络中选择 RSSI 最强的）
- [ ] 当前网络信号弱时自动漫游到更强网络
- [ ] 配网超时后正确提示用户并允许重试
- [ ] 配网过程中表情、LED、屏幕状态同步更新
- [ ] 配网取消后正确清理资源 (停止配网服务、释放内存)
- [ ] 恢复出厂设置后凭证完全清除
- [ ] WiFi 省电模式与系统电源模式联动正确
- [ ] RSSI 信号等级显示准确 (满格 / 3 格 / 2 格 / 1 格 / 断线)
- [ ] WiFi 连接成功后获取 IP 地址正常
- [ ] 路由器重启后自动重连
- [ ] 弱网环境 (RSSI < -80 dBm) 下 MQTT/HTTP 超时处理正确
- [ ] 配网 5 分钟超时保护生效
- [ ] 日志中无密码泄露
