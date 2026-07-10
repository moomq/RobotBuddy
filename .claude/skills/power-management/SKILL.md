# Power Management Skill — RobotBuddy

## Role

RobotBuddy 电源管理专家，负责电池监测、充电管理、功耗优化和电源状态机。

## Domain

18650 锂电池管理、TP4056 充电 IC、MT3608 升压、ESP32-S3 电源模式（Active / Light Sleep / Deep Sleep）、ADC 电量检测、功耗优化策略。

## Goal

实现安全可靠的电源管理系统，确保 RobotBuddy 在不同电量和工作状态下智能调配功耗，最大化续航时间。

## Inputs

- 电源硬件规格（TP4056 + MT3608 + 18650 电池）
- 各模块功耗数据（WiFi / 屏幕 / 电机 / 音频）
- FreeRTOS 任务架构和电源模式需求

## Outputs

- `firmware/system/power_mgr/power_manager.c` — 电源管理器
- `firmware/system/power_mgr/battery_monitor.c` — 电池监测
- `firmware/system/power_mgr/charger_handler.c` — 充电管理
- `firmware/system/power_mgr/sleep_manager.c` — 休眠管理
- `docs/firmware/power-management.md` — 电源管理文档

## Power Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Power Management System                  │
│                                                              │
│  ┌──────────────┐   ┌──────────────┐   ┌────────────────┐  │
│  │ Battery      │   │ Charger      │   │ Power State    │  │
│  │ Monitor      │   │ Handler      │   │ Machine        │  │
│  │ (ADC 定时采样)│   │ (TP4056 状态)│   │ (Active/Sleep) │  │
│  └──────┬───────┘   └──────┬───────┘   └───────┬────────┘  │
│         │                  │                    │            │
│  ┌──────↓──────────────────↓────────────────────↓────────┐  │
│  │              Power Manager (决策中心)                    │  │
│  │                                                         │  │
│  │  输入: 电量%、充电状态、用户活动、时间                      │  │
│  │  输出: 电源模式切换、模块使能/禁用、告警                    │  │
│  └─────────────────────────┬───────────────────────────────┘  │
│                             │                                 │
│  ┌──────────────────────────↓──────────────────────────────┐ │
│  │              受控模块                                     │ │
│  │  WiFi │ Display │ Motor │ Audio │ LED │ Sensor Polling  │ │
│  └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

## Battery Monitor

```c
// 电池参数
#define BATTERY_FULL_MV         4200    // 满电电压 (mV)
#define BATTERY_EMPTY_MV        3000    // 截止电压 (mV)
#define BATTERY_WARNING_MV      3400    // 低电量警告 (mV)
#define BATTERY_CRITICAL_MV     3200    // 严重低电量 (mV)
#define BATTERY_ADC_SAMPLES     16      // ADC 采样次数（取平均）
#define BATTERY_SAMPLE_PERIOD_MS 30000  // 采样周期 30s

// 电量百分比计算（基于放电曲线）
typedef struct {
    uint32_t voltage_mv;       // 当前电压 (mV)
    uint8_t percentage;        // 电量百分比 (0-100%)
    bool is_charging;          // 是否充电中
    bool is_low_battery;       // 是否低电量
    bool is_critical;          // 是否严重低电量
    uint32_t estimate_runtime_min; // 预估剩余运行时间 (分钟)
} battery_status_t;

// 电池监测 API
esp_err_t battery_monitor_init(void);
esp_err_t battery_get_status(battery_status_t *status);
uint8_t battery_get_percentage(void);
bool battery_is_charging(void);

// 电压→百分比 查找表（18650 放电曲线近似）
static const uint16_t s_discharge_curve[][2] = {
    {4200, 100}, {4100, 90}, {4000, 80}, {3900, 70},
    {3800, 60}, {3700, 50}, {3600, 40}, {3500, 30},
    {3400, 20}, {3300, 10}, {3200, 5},  {3000, 0},
};
```

## Charger Handler (TP4056)

```c
// TP4056 状态引脚
// CHRG (充电中): LOW = 充电中, HIGH = 充电完成/未接入
// STDBY (待机):  LOW = 充电完成, HIGH = 充电中/未接入

typedef enum {
    CHARGER_STATE_IDLE,         // 未接入电源
    CHARGER_STATE_CHARGING,     // 充电中
    CHARGER_STATE_FULL,         // 充电完成
    CHARGER_STATE_ERROR,        // 异常（两个引脚都 LOW）
} charger_state_t;

typedef struct {
    charger_state_t state;
    uint32_t charge_start_ms;   // 充电开始时间
    uint32_t charge_duration_ms;// 充电持续时间
} charger_status_t;

esp_err_t charger_handler_init(gpio_num_t pin_chrg, gpio_num_t pin_stdbY);
esp_err_t charger_get_status(charger_status_t *status);

// 充电事件 → 表情联动
// CHARGING → 显示充电中表情（闪电图标 + 电量百分比动画）
// FULL → 显示满电表情（绿色笑脸）
```

## Power State Machine

```c
// 电源模式
typedef enum {
    POWER_MODE_ACTIVE,          // 全功能运行
    POWER_MODE_POWER_SAVE,      // 省电模式（降低屏幕亮度、WiFi 省电）
    POWER_MODE_LIGHT_SLEEP,     // 轻度休眠（WiFi 保持、屏幕关闭、电机禁用）
    POWER_MODE_DEEP_SLEEP,      // 深度休眠（仅 RTC + 唤醒源）
} power_mode_t;

// 模式切换条件
// ACTIVE → POWER_SAVE:    电量 < 20% 或用户手动切换
// POWER_SAVE → LIGHT_SLEEP: 连续 5min 无交互
// LIGHT_SLEEP → DEEP_SLEEP: 连续 30min 无交互 或 电量 < 5%
// DEEP_SLEEP → ACTIVE:     触摸传感器 / 唤醒词 / 定时器
// 任何模式 → ACTIVE:       用户主动交互（语音/触摸/手机APP）

// 各模式下的模块状态
typedef struct {
    bool wifi_enabled;
    bool display_enabled;
    bool motor_enabled;
    bool audio_capture_enabled;
    bool audio_playback_enabled;
    bool sensor_polling_enabled;
    uint8_t display_brightness; // 0-255
    wifi_ps_type_t wifi_ps_mode;
} power_mode_config_t;

static const power_mode_config_t POWER_CONFIGS[] = {
    // ACTIVE
    { true,  true,  true,  true,  true,  true,  255, WIFI_PS_NONE },
    // POWER_SAVE
    { true,  true,  false, true,  true,  true,  128, WIFI_PS_MIN_MODEM },
    // LIGHT_SLEEP
    { true,  false, false, false, true,  false, 0,   WIFI_PS_MIN_MODEM },
    // DEEP_SLEEP
    { false, false, false, false, false, false, 0,   WIFI_PS_NONE },
};

// 电源管理 API
esp_err_t power_manager_init(void);
esp_err_t power_set_mode(power_mode_t mode);
power_mode_t power_get_mode(void);

// 注册电源模式切换回调
typedef void (*power_mode_cb_t)(power_mode_t old_mode, power_mode_t new_mode);
esp_err_t power_register_callback(power_mode_cb_t callback);

// 强制唤醒（从任何休眠模式）
esp_err_t power_force_wakeup(void);
```

## Sleep Manager

```c
// Deep Sleep 唤醒源配置
typedef struct {
    gpio_num_t wake_pin;            // 触摸/按钮唤醒 GPIO
    uint64_t wake_timeout_sec;      // 定时唤醒周期（秒）
    bool enable_gpio_wakeup;
    bool enable_timer_wakeup;
} sleep_wakeup_config_t;

// 进入 Deep Sleep
esp_err_t sleep_enter_deep(const sleep_wakeup_config_t *cfg);

// Light Sleep（WiFi 保持连接）
esp_err_t sleep_enter_light(void);

// 从 Deep Sleep 恢复
bool sleep_is_wakeup_from_deep(void);
esp_err_t sleep_restore_after_deep(void);
```

## Power Budget

```
各模块典型功耗 (3.7V 电池):

模块             活跃功耗     休眠功耗     占比
─────────────────────────────────────────────
ESP32-S3 CPU    ~100mA       ~10μA       35%
WiFi (TX)       ~170mA       ~0.5mA      60%
WiFi (PS mode)  ~15mA        —           —
ST7789 显示     ~30mA        ~1mA        10%
MAX98357A 功放  ~50mA        ~1mA        17%
DRV8833 + 电机  ~300mA       ~0mA        (间歇)
MPU6050         ~4mA         ~8μA        1%
IR 传感器 ×4    ~20mA        ~0mA        7%
─────────────────────────────────────────────
总计 (活跃)      ~670mA      —
总计 (待机)      ~47mA       —
总计 (WiFi PS)   ~67mA       —

电池 2600mAh:
  活跃续航 ≈ 2600/670 ≈ 3.9h ✅ (目标 ≥ 4h)
  待机续航 ≈ 2600/47  ≈ 55h  ✅ (目标 ≥ 24h)
  WiFi PS  ≈ 2600/67  ≈ 39h  ✅
```

## Rules

1. **低电量保护** — 电量 < 20% 禁用电机，< 10% 进入省电模式，< 5% 进入深度休眠
2. **充电保护** — 充电时限制电机最大电流，防止 TP4056 过热
3. **平滑过渡** — 电源模式切换不可中断音频播放，需等待当前操作完成
4. **电量通知** — 低电量触发表情 WARNING + 语音提醒
5. **深度休眠恢复** — 唤醒后自动恢复 WiFi 连接和表情显示
6. **ADC 采样去抖** — 使用 16 次采样取中值，避免瞬间负载影响读数
7. **电压分压计算** — 分压电阻 2:1，ADC 读数 × 2 × (3.3/4095) × 校准系数
8. **NVS 持久化** — 电量低事件记录到 NVS，异常掉电后可恢复

## Checklist

- [ ] ADC 电量读数准确（万用表对比误差 < 5%）
- [ ] 电量百分比与实际放电曲线吻合
- [ ] TP4056 充电状态检测正确（CHRG/STDBY 引脚）
- [ ] 充电中表情和语音提示正常
- [ ] Active → Power Save → Light Sleep → Deep Sleep 状态切换正常
- [ ] Deep Sleep 唤醒后系统恢复正常运行
- [ ] 低电量时自动禁用电机
- [ ] WiFi 省电模式下 MQTT 消息不丢失
- [ ] 屏幕亮度调节无明显闪烁
- [ ] 24h 待机功耗测量 ≤ 50mA（WiFi PS 模式）
- [ ] 电源模式切换无死锁/崩溃
