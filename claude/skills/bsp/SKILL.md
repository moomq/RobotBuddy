# BSP Skill — RobotBuddy

## Role

RobotBuddy BSP 板级支持包专家，负责 ESP32-S3 引脚矩阵规划、时钟树配置、启动序列管理、外设初始化顺序、电源域控制、Strapping 引脚管理、eFuse 管理及工厂产线自检。

## Domain

ESP32-S3 pin matrix, clock tree configuration, boot sequence, peripheral initialization order, power domain control, strapping pins, eFuse management, factory test。

## Goal

提供完整的 RobotBuddy PCB 板级软件支撑，确保硬件平台稳定可靠，所有外设正确初始化，引脚无冲突，启动流程可追溯，产线可自动化检验。

## Inputs

- RobotBuddy 原理图（当前引脚分配方案）
- ESP32-S3 技术参考手册 (Technical Reference Manual)
- ESP32-S3 数据手册 (Datasheet) — Pin Overview, Strapping, eFuse
- ESP-IDF 启动流程文档 (Bootloader / app_main)
- 各外设驱动的配置需求（SPI / I2C / I2S / PWM / ADC）
- `/pin-check` 命令的引脚冲突检测规则

## Outputs

- `firmware/bsp/board_init.c` — 板级初始化（统一入口）
- `firmware/bsp/board_init.h` — 板级初始化接口与阶段定义
- `firmware/bsp/board_pinmap.h` — 引脚宏定义（完整 GPIO 映射表）
- `firmware/bsp/board_clock.c` — 时钟树配置
- `firmware/bsp/board_clock.h` — 时钟配置接口
- `firmware/bsp/board_self_test.c` — 工厂产线自检程序
- `firmware/bsp/board_self_test.h` — 自检接口与 JSON 输出格式
- `firmware/bsp/board_efuse.c` — eFuse 管理
- `firmware/bsp/board_efuse.h` — eFuse 安全编程接口
- `docs/hardware/bsp.md` — BSP 文档（引脚分配表 + 时钟树 + 启动流程）

## BSP Components

```
┌──────────────────────────────────────────────────────────────────┐
│                   RobotBuddy Board Support Package                │
│                                                                   │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐      │
│  │ Clock    │   │ Pin      │   │ Boot     │   │ Init     │       │
│  │ Tree     │──▶│ Matrix   │──▶│ Config   │──▶│ Sequence │       │
│  │ 配置     │   │ 映射     │   │ 启动配置 │   │ 初始化   │       │
│  └──────────┘   └──────────┘   └──────────┘   └──────────┘      │
│                                                       │          │
│                                                       ▼          │
│                                              ┌──────────────┐   │
│                                              │ Factory Test │   │
│                                              │ 工厂产线自检 │   │
│                                              └──────────────┘   │
│                                                                   │
│  ┌──────────────────────────────────────────────────────────────┐ │
│  │ 支撑模块                                                      │ │
│  │ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────────────┐ │ │
│  │ │Strapping│ │ eFuse   │ │ Power   │ │ Peripheral Init     │ │ │
│  │ │Pin 管理 │ │ 管理    │ │ Domain  │ │ Order (阶段化)      │ │ │
│  │ └─────────┘ └─────────┘ └─────────┘ └─────────────────────┘ │ │
│  └──────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────┘
```

## Pin Matrix

### 完整 GPIO 分配表

```c
// ============================================================
// board_pinmap.h — RobotBuddy GPIO 完整分配表
// ============================================================

// ─── SPI (VSPI_HOST) — ST7789 屏幕 ──────────────────────────
#define PIN_LCD_SCLK    GPIO_NUM_36     // SPI 时钟
#define PIN_LCD_MOSI    GPIO_NUM_35     // SPI 数据输出
#define PIN_LCD_DC      GPIO_NUM_38     // 数据/命令选择
#define PIN_LCD_RST     GPIO_NUM_39     // 硬件复位
#define PIN_LCD_CS      GPIO_NUM_37     // 片选
#define PIN_LCD_BL      GPIO_NUM_40     // 背光控制 (PWM/LEDC)

// ─── I2S0 — 音频 ────────────────────────────────────────────
#define PIN_I2S_BCLK    GPIO_NUM_4      // I2S 位时钟
#define PIN_I2S_WS      GPIO_NUM_5      // I2S 字选 (L/R)
#define PIN_I2S_DIN     GPIO_NUM_6      // I2S 数据输入 (INMP441 麦克风)
#define PIN_I2S_DOUT    GPIO_NUM_7      // I2S 数据输出 (MAX98357A 功放)

// ─── I2C0 — 传感器总线 ──────────────────────────────────────
#define PIN_I2C_SDA     GPIO_NUM_8      // I2C 数据线
#define PIN_I2C_SCL     GPIO_NUM_9      // I2C 时钟线

// ─── PWM (LEDC) — 电机驱动 (DRV8833) ────────────────────────
#define PIN_MOTOR_AIN1  GPIO_NUM_10     // 左电机 IN1
#define PIN_MOTOR_AIN2  GPIO_NUM_11     // 左电机 IN2
#define PIN_MOTOR_BIN1  GPIO_NUM_12     // 右电机 IN1
#define PIN_MOTOR_BIN2  GPIO_NUM_13     // 右电机 IN2

// ─── GPIO 数字输入 — 红外传感器 & 充电检测 ──────────────────
#define PIN_TCRT_L      GPIO_NUM_14     // 左避障传感器 (TCRT5000)
#define PIN_TCRT_R      GPIO_NUM_15     // 右避障传感器 (TCRT5000)
#define PIN_EDGE_L      GPIO_NUM_16     // 左边缘检测 (ITR20001)
#define PIN_EDGE_R      GPIO_NUM_17     // 右边缘检测 (ITR20001)
#define PIN_CHRG        GPIO_NUM_18     // TP4056 充电状态 (LOW=充电中)
#define PIN_STDBY       GPIO_NUM_21     // TP4056 充电完成 (LOW=充满)

// ─── ADC — 电池电压检测 ─────────────────────────────────────
#define PIN_VBAT        GPIO_NUM_1      // ADC1_CH0, 电阻分压 2:1

// ─── 保留引脚 (不可分配) ────────────────────────────────────
// GPIO 19, 20 — USB D+/D- (USB-JTAG 调试接口)
// GPIO 26-32  — Flash/PSRAM (已连接 SPI Flash)
// GPIO 43, 44 — UART0 TX/RX (默认 USB-JTAG Serial, 可复用但调试时不建议)
// GPIO 0, 3, 45, 46 — Strapping 引脚 (启动时影响 boot 行为)

// ─── Strapping 引脚状态 ─────────────────────────────────────
#define PIN_STRAP_BOOT  GPIO_NUM_0      // Boot mode (HIGH=Flash, LOW=Download)
#define PIN_STRAP_TDO   GPIO_NUM_3      // JTAG TDO signal
#define PIN_STRAP_VDD   GPIO_NUM_45     // VDD_SPI voltage select
#define PIN_STRAP_LOG   GPIO_NUM_46     // ROM messages enable
```

### 引脚分配汇总表

| GPIO | 功能 | 接口类型 | 备注 |
|------|------|---------|------|
| 0 | BOOT | Strapping | 启动模式选择，上拉为 Flash Boot |
| 1 | VBAT | ADC1_CH0 | 电池电压检测，WiFi 安全 |
| 3 | — | Strapping | JTAG TDO，未使用但保留 |
| 4 | I2S BCLK | I2S0 | 音频位时钟 |
| 5 | I2S WS | I2S0 | 音频字选信号 |
| 6 | I2S DIN | I2S0 | INMP441 麦克风数据输入 |
| 7 | I2S DOUT | I2S0 | MAX98357A 功放数据输出 |
| 8 | I2C SDA | I2C0 | 传感器总线数据线 |
| 9 | I2C SCL | I2C0 | 传感器总线时钟线 |
| 10 | MOTOR AIN1 | LEDC PWM | 左电机 IN1 |
| 11 | MOTOR AIN2 | LEDC PWM | 左电机 IN2 |
| 12 | MOTOR BIN1 | LEDC PWM | 右电机 IN1 |
| 13 | MOTOR BIN2 | LEDC PWM | 右电机 IN2 |
| 14 | TCRT_L | GPIO In | 左避障传感器 |
| 15 | TCRT_R | GPIO In | 右避障传感器 |
| 16 | EDGE_L | GPIO In | 左边缘检测 |
| 17 | EDGE_R | GPIO In | 右边缘检测 |
| 18 | CHRG | GPIO In | TP4056 充电中检测 |
| 19 | USB D- | 保留 | USB-JTAG 调试 |
| 20 | USB D+ | 保留 | USB-JTAG 调试 |
| 21 | STDBY | GPIO In | TP4056 充电完成检测 |
| 26-32 | Flash | 保留 | SPI Flash/PSRAM |
| 35 | LCD MOSI | VSPI | ST7789 数据 |
| 36 | LCD SCLK | VSPI | ST7789 时钟 |
| 37 | LCD CS | VSPI | ST7789 片选 |
| 38 | LCD DC | VSPI | ST7789 命令/数据 |
| 39 | LCD RST | GPIO Out | ST7789 硬件复位 |
| 40 | LCD BL | LEDC PWM | ST7789 背光 |
| 43 | UART0 TX | 保留 | 默认 Debug Serial |
| 44 | UART0 RX | 保留 | 默认 Debug Serial |
| 45 | VDD_SPI | Strapping | 电压选择，保留 |
| 46 | ROM_LOG | Strapping | ROM 日志控制，保留 |

### 引脚使用统计

```
已分配:    21 / 48 (43.8%)
保留:      12 / 48 (25.0%)
Strapping:  4 / 48  (8.3%)
可用:      11 / 48 (22.9%)
可用 GPIO: 2, 3, 22, 23, 33, 34, 41, 42, 45, 46, 47, 48
(其中 3, 45, 46 为 Strapping，复用需谨慎)
```

## Clock Tree Configuration

```c
// ============================================================
// board_clock.h — RobotBuddy 时钟树配置
// ============================================================

// 时钟源
// XTAL (外部晶振): 40 MHz
// PLL (锁相环):    480 MHz (40MHz × 12)
// RTC 时钟:       32.768 kHz (可选外部晶振，或内部 150 kHz)

// CPU 时钟
// 来源: PLL 480MHz
// 分频: ÷2
// 结果: CPU_CLK = 240 MHz (max for ESP32-S3)
#define CPU_FREQ_MHZ            240

// APB 总线时钟
// 来源: CPU_CLK 240MHz
// 分频: ÷3
// 结果: APB_CLK = 80 MHz (max for APB peripherals)
#define APB_FREQ_MHZ            80

// SPI 外设时钟 (VSPI — ST7789 屏幕)
// 来源: APB_CLK 80MHz
// 分频: ÷2
// 结果: SPI_CLK = 40 MHz (max stable for ST7789)
#define SPI_CLK_MHZ             40

// I2C 外设时钟
// 来源: APB_CLK 80MHz 或 XTAL 40MHz
// 分频: 配置为 ÷2 得到 40MHz (来源 APB) 或直接从 XTAL 40MHz
// 实际 I2C_SCL: 100 kHz (标准模式) 或 400 kHz (快速模式)
#define I2C_CLK_MHZ             40          // I2C 外设源时钟
#define I2C_SCL_FREQ_HZ         400000      // I2C 总线 SCL 频率 (400kHz Fast)

// I2S 音频时钟
// 来源: PLL_AUDIO (独立 PLL for audio)
// PLL_AUDIO: 从 XTAL 40MHz 倍频，专为音频采样率优化
// 采样率: 16 kHz (语音品质)
// 位深:   16 bit
// 声道:   1 (Mono)
// BCLK = 采样率 × 位深 × 声道 = 16000 × 16 × 1 = 256 kHz
// MCLK = 采样率 × 256 = 4.096 MHz (典型音频 MCLK)
#define AUDIO_SAMPLE_RATE_HZ    16000
#define AUDIO_BIT_DEPTH         16
#define AUDIO_CHANNELS          1
#define AUDIO_BCLK_HZ           (AUDIO_SAMPLE_RATE_HZ * AUDIO_BIT_DEPTH * AUDIO_CHANNELS)  // 256 kHz
#define AUDIO_MCLK_HZ           (AUDIO_SAMPLE_RATE_HZ * 256)                               // 4.096 MHz

// LEDC PWM 时钟 (电机 + 背光)
// 来源: APB_CLK 80MHz 或 XTAL 40MHz
// 电机 PWM 频率: 1 kHz (降低电磁噪声)
// 背光 PWM 频率: 5 kHz (避免人眼可见闪烁)
#define LEDC_SOURCE_CLK_MHZ     80          // LEDC 时钟源
#define PWM_MOTOR_FREQ_HZ       1000        // 电机 PWM 频率
#define PWM_BACKLIGHT_FREQ_HZ   5000        // 背光 PWM 频率

// ADC 时钟
// 来源: RTC 时钟域
// ADC 采样率: 约 100 kSPS (ESP32-S3 max)
// ADC 采样周期: 软件定时器 30s (电池采样)
#define ADC_SAMPLE_INTERVAL_MS  30000       // 电池 ADC 采样间隔

// 时钟树初始化 API
typedef struct {
    uint32_t cpu_freq_mhz;          // CPU 主频 (MHz), default 240
    uint32_t apb_freq_mhz;          // APB 总线频率 (MHz), default 80
    bool use_external_32k;          // 是否使用外部 32.768kHz 晶振
    bool enable_pll_audio;          // 是否启用 PLL_AUDIO
} clock_config_t;

esp_err_t board_clock_init(const clock_config_t *cfg);
void board_clock_print_tree(void);  // 打印当前时钟树状态
```

### 时钟树 ASCII 图

```
                     XTAL 40 MHz
                         │
            ┌────────────┼────────────┐
            │            │            │
            ▼            ▼            ▼
     ┌──────────┐  ┌──────────┐  ┌──────────┐
     │   PLL    │  │PLL_AUDIO │  │ RTC 时钟 │
     │ 480 MHz  │  │          │  │ 32.768kHz│
     └────┬─────┘  └────┬─────┘  └────┬─────┘
          │             │             │
     ┌────┼────┐        │             ▼
     │    │    │        │        ADC (RTC Domain)
     ▼    ▼    ▼        │        电池电压检测
   CPU  APB  SPI        │        30s 间隔采样
  240M  80M  40M        │
          │             │
    ┌─────┼─────┐       │
    │     │     │       │
    ▼     ▼     ▼       ▼
  I2C   PWM  LEDC     I2S
  40M   1kHz  5kHz     BCLK 256kHz
  →400k            MCLK 4.096MHz
  (SCL)
```

## Boot Sequence

```c
// ============================================================
// RobotBuddy 启动序列
// ============================================================

/*
 * 阶段 0: ROM Boot (硬件固化)
 * ├── 读取 eFuse 配置 → 确定 Flash 电压、安全启动策略
 * ├── 检测 Strapping 引脚 (GPIO0, GPIO45, GPIO46)
 * │   ├── GPIO0 = HIGH → Flash Boot (正常运行)
 * │   └── GPIO0 = LOW  → Download Boot (固件烧录模式)
 * ├── 从 Flash 加载 2nd Stage Bootloader
 * └── 跳转到 Bootloader
 *
 * 阶段 1: 2nd Stage Bootloader (ESP-IDF)
 * ├── 初始化 SPI Flash (基于 eFuse 配置的电压和模式)
 * ├── 校验分区表 (partition table)
 * ├── 加载 app 镜像 (factory / ota_0 / ota_1)
 * ├── 校验镜像签名 (安全启动)
 * └── 跳转到 app_main()
 *
 * 阶段 2: app_main() → board_init() (用户代码)
 * ├── board_init() — 板级初始化 (见下方详细流程)
 * └── xTaskCreate(...) — 启动应用 Task
 */

// ============================================================
// board_init() 详细初始化流程
// ============================================================

esp_err_t board_init(void) {
    esp_err_t ret;

    // ── PHASE_CRITICAL: 关键阶段（必须最先初始化）────────────
    // 1. GPIO 子系统 (配置引脚方向、上下拉)
    ret = board_gpio_init();
    ESP_ERROR_CHECK(ret);

    // 2. 时钟树配置 (设置 CPU/APB/外设时钟)
    ret = board_clock_init(&(clock_config_t){.cpu_freq_mhz = 240});
    ESP_ERROR_CHECK(ret);

    // ── PHASE_BUS: 总线阶段（通信接口初始化）──────────────────
    // 3. I2C 总线初始化 (传感器链路)
    ret = board_i2c_init();
    ESP_ERROR_CHECK(ret);

    // 4. SPI 总线初始化 (显示链路)
    ret = board_spi_init();
    ESP_ERROR_CHECK(ret);

    // 5. I2S 初始化 (音频链路)
    ret = board_i2s_init();
    ESP_ERROR_CHECK(ret);

    // ── PHASE_PERIPHERAL: 外设阶段（具体设备初始化）───────────
    // 6. PWM / LEDC 初始化 (电机 + 背光)
    ret = board_pwm_init();
    ESP_ERROR_CHECK(ret);

    // 7. ADC 初始化 (电池检测)
    ret = board_adc_init();
    ESP_ERROR_CHECK(ret);

    // ── PHASE_SYSTEM: 系统阶段（平台服务初始化）───────────────
    // 8. NVS 初始化 (非易失存储)
    ret = board_nvs_init();
    ESP_ERROR_CHECK(ret);

    // ── PHASE_NETWORK: 网络阶段 ──────────────────────────────
    // 9. WiFi 初始化 (由 WiFi Manager Task 异步完成)
    //    board_init 仅创建 WiFi Task，不阻塞等待连接

    // ── PHASE_APP: 应用阶段（高层外设与 Task 启动）───────────
    // 10. Display 初始化 (ST7789)
    ret = board_display_init();
    ESP_ERROR_CHECK(ret);

    // 11. Audio 初始化 (INMP441 + MAX98357A)
    ret = board_audio_init();
    ESP_ERROR_CHECK(ret);

    // 12. Sensor 初始化 (MPU6050 + TCRT5000 + ITR20001)
    ret = board_sensor_init();
    ESP_ERROR_CHECK(ret);

    // 13. 启动应用 Tasks (音频 / 显示 / 行为 / 通信等)
    ret = board_app_tasks_start();
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "board_init() complete — all peripherals initialized");
    return ESP_OK;
}
```

### 初始化阶段宏定义

```c
// ============================================================
// 初始化阶段定义 (用于条件编译和阶段跳过)
// ============================================================
typedef enum {
    INIT_PHASE_CRITICAL     = 0,    // GPIO / Clock — 必须先成功
    INIT_PHASE_BUS          = 1,    // I2C / SPI / I2S — 通信总线
    INIT_PHASE_PERIPHERAL   = 2,    // Display / Sensors / Motors — 具体设备
    INIT_PHASE_SYSTEM       = 3,    // NVS / Event Loop — 平台服务
    INIT_PHASE_NETWORK      = 4,    // WiFi — 网络连接
    INIT_PHASE_APP          = 5,    // Tasks / Services — 应用逻辑
    INIT_PHASE_MAX
} init_phase_t;

// 阶段状态
typedef struct {
    init_phase_t phase;
    bool succeeded;
    uint32_t duration_ms;       // 该阶段耗时
    const char *error_msg;      // 失败时填充错误信息
} init_phase_status_t;

// API: 获取各阶段初始化状态
esp_err_t board_get_init_status(init_phase_status_t *status, size_t *count);
```

## 外设初始化顺序 & 依赖关系

```c
// ============================================================
// 外设初始化顺序与依赖关系
// ============================================================

/*
 * 依赖 DAG (有向无环图):
 *
 *   PHASE_CRITICAL (GPIO / Clock)
 *         │
 *         ▼
 *   PHASE_BUS (I2C / SPI / I2S)
 *         │
 *    ┌────┼────┐
 *    │    │    │
 *    ▼    ▼    ▼
 *  PWM  ADC   (I2C 就绪)
 *    │    │      │
 *    │    │      ▼
 *    │    │   Sensors (传感器需 I2C + GPIO)
 *    │    │
 *    ▼    ▼
 *  NVS (存储服务, 需要 APB 总线就绪)
 *    │
 *    ▼
 *  WiFi (网络, 独立初始化, 不阻塞)
 *    │
 *    ▼
 *  Display (SPI 就绪)
 *    │
 *    ▼
 *  Audio (I2S 就绪, 可能在 Display 之后启动以避免 I2S 总线竞争)
 *    │
 *    ▼
 *  PHASE_APP (All Tasks start)
 *
 * 关键约束:
 * 1. I2C 必须在 Sensors 之前初始化 (Sensors 依赖 I2C 总线)
 * 2. SPI 必须在 Display 之前初始化
 * 3. I2S 必须在 Audio 之前初始化
 * 4. NVS 必须在 WiFi 之前初始化 (WiFi 存储配网信息)
 * 5. GPIO 是所有外设的基础，必须在 PHASE_CRITICAL 最先完成
 * 6. PWM 和 ADC 无强依赖关系，可并行初始化
 * 7. WiFi 初始化不在 board_init 中阻塞，由专门的 WiFi Task 异步完成
 */

// 阶段初始化 API (支持按阶段编译)
esp_err_t board_init_phase_critical(void);      // GPIO + Clock
esp_err_t board_init_phase_bus(void);           // I2C + SPI + I2S
esp_err_t board_init_phase_peripheral(void);    // PWM + ADC
esp_err_t board_init_phase_system(void);        // NVS
esp_err_t board_init_phase_network(void);       // WiFi (async)
esp_err_t board_init_phase_app(void);           // Display + Audio + Sensors + Tasks

// 完整板级初始化 (依次调用所有阶段)
esp_err_t board_init_all(void);
```

## Strapping 引脚管理

```c
// ============================================================
// Strapping 引脚分析与配置
// ============================================================

/*
 * ESP32-S3 Strapping 引脚在芯片复位释放时被采样，决定启动行为。
 * 启动后这些引脚可以重新配置为 GPIO 使用，但需确保：
 * 1. 上电瞬间外部电路提供的电平符合预期启动模式
 * 2. 外部上拉/下拉电阻与 Strapping 要求一致
 *
 * GPIO 0  — Boot Mode
 *   HIGH (默认内部上拉): Flash Boot — 从 SPI Flash 启动固件
 *   LOW:                 Download Boot — 进入固件下载模式
 *   建议: 外部 10kΩ 上拉到 3.3V，并通过按键接地（用作多功能按键）
 *
 * GPIO 3  — JTAG TDO
 *   默认 JTAG 信号，启动后可使用。一般不用作 GPIO。
 *   建议: 保持浮空或用于 JTAG 调试
 *
 * GPIO 45 — VDD_SPI Voltage
 *   0 (默认内部下拉): VDD_SPI = 3.3V (标准 Flash)
 *   1:                VDD_SPI = 1.8V (低压 Flash)
 *   建议: 保持默认下拉，不接外部电路
 *
 * GPIO 46 — ROM Messages
 *   0 (默认内部下拉): ROM 日志输出使能
 *   1:                ROM 日志输出禁用
 *   RobotBuddy 使用 USB-JTAG 串口，ROM 日志不影响
 *   建议: 保持默认状态
 */

// Strapping 引脚检查 API
esp_err_t board_check_strapping_pins(void);

// 检查逻辑:
//   GPIO 0:  读取电平 → HIGH = Flash Boot (OK), LOW = Download Boot (WARN)
//   GPIO 3:  无特殊检查
//   GPIO 45: 读取电平 → 0 = 3.3V (OK), 1 = 1.8V (检查 Flash 型号)
//   GPIO 46: 无特殊检查

// Strapping 引脚电压时序要求
// ────────────────────────────────
// t_strap_setup:  ≥ 1 μs  — 复位释放前 Strapping 信号稳定时间
// t_strap_hold:   ≥ 0 ns  — 复位释放后 Strapping 信号保持时间
// 确保外部电路在芯片复位期间满足以上时序
```

## eFuse 管理

```c
// ============================================================
// eFuse 管理 — 安全启动和硬件配置
// ============================================================

/*
 * ESP32-S3 eFuse 是一次性可编程存储器，用于:
 * 1. Flash 电压配置 (VDD_SPI 3.3V / 1.8V)
 * 2. 安全启动密钥 (Secure Boot)
 * 3. Flash 加密密钥 (Flash Encryption)
 * 4. MAC 地址
 * 5. 禁用某些调试接口 (JTAG / ROM Download)
 *
 * ⚠️  警告: eFuse 烧录不可逆！错误的 eFuse 设置可能导致芯片永久锁定。
 */

// eFuse 检查 API
typedef struct {
    bool flash_voltage_3v3;             // VDD_SPI 是否为 3.3V
    bool secure_boot_enabled;           // 是否启用安全启动
    bool flash_encryption_enabled;      // 是否启用 Flash 加密
    bool jtag_disabled;                 // JTAG 是否禁用
    bool rom_download_disabled;         // ROM Download 模式是否禁用
    uint8_t mac[6];                     // MAC 地址
    uint32_t wafer_info;                // 晶圆信息
    uint32_t package_type;              // 封装类型
} board_efuse_status_t;

esp_err_t board_efuse_read_status(board_efuse_status_t *status);
esp_err_t board_efuse_print_summary(void);

// 安全编程接口 (仅在开发阶段使用，量产时需谨慎)
typedef enum {
    EFUSE_SECURE_BOOT_KEY,              // 安全启动密钥
    EFUSE_FLASH_ENCRYPTION_KEY,         // Flash 加密密钥
    EFUSE_DISABLE_JTAG,                 // 禁用 JTAG
    EFUSE_DISABLE_ROM_DOWNLOAD,         // 禁用 ROM Download
} board_efuse_program_target_t;

// 仅在确认无误后调用，需要 erase 整片 Flash 重新烧录
esp_err_t board_efuse_program_security_keys(void);

// 量产建议:
// 1. 开发阶段: 不烧 eFuse，保持 JTAG 和 ROM Download 可用
// 2. 测试阶段: 烧写 MAC 地址 (如果需要自定义)
// 3. 量产阶段: 烧写安全启动密钥 + 禁用 ROM Download
// 4. 每次烧写前必须导出 eFuse 摘要并由操作员确认
```

## 工厂产线自检

```c
// ============================================================
// Factory Self-Test — 工厂产线自动化自检
// ============================================================

/*
 * 自检流程:
 * 上电 → 初始化 BSP → 执行自检序列 → 输出 JSON 结果 → 判断 PASS/FAIL
 *
 * 每个自检项独立运行，失败不影响后续检查 (best-effort)。
 */

typedef enum {
    SELF_TEST_PASS = 0,
    SELF_TEST_FAIL,
    SELF_TEST_SKIP,     // 模块未使能或不可用
} self_test_result_t;

typedef struct {
    const char *test_name;
    self_test_result_t result;
    uint32_t duration_ms;
    const char *detail;     // 通过时的补充信息
    const char *error;      // 失败时的错误描述
} self_test_item_t;

typedef struct {
    self_test_item_t items[12];     // 自检项列表
    size_t item_count;
    uint32_t total_duration_ms;
    uint8_t pass_count;
    uint8_t fail_count;
    uint8_t skip_count;
    bool overall_pass;              // 全部 PASS 或 SKIP → true
} self_test_report_t;

// ============================================================
// 自检执行
// ============================================================

esp_err_t board_self_test_run(self_test_report_t *report);

// 自检序列:
//
//  1. I2C Bus Scan
//     扫描 I2C0 总线 (地址 0x03-0x77)
//     PASS: 检测到 MPU6050 (0x68) 应答
//     PASS: 如有 VL53L0X (0x29) 也应答
//     FAIL: I2C 总线无任何设备应答
//
//  2. SPI Display Test Pattern
//     初始化 ST7789 → 发送测试图案 (红/绿/蓝/白全屏)
//     PASS: 无 SPI 通信错误
//     FAIL: SPI 超时或 ST7789 无应答
//
//  3. I2S Audio Loopback Test
//     发送测试 Tone (1kHz) → I2S_DOUT
//     (可选: 用电容耦合(loopback 线)将 DOUT 接回 DIN)
//     PASS: I2S DMA 正常启动，无缓冲区错误
//     FAIL: I2S 初始化失败或 DMA 错误
//
//  4. PWM Motor Test
//     对 AIN1/AIN2/BIN1/BIN2 依次输出 50% 占空比 100ms
//     PASS: LEDC 通道配置成功，无硬件错误
//     FAIL: LEDC 配置失败
//
//  5. Sensor WHO_AM_I Check
//     MPU6050: 读取 Register 0x75 → 期望值 0x68
//     VL53L0X (可选): 读取 Model ID Register → 期望值 0xEE
//     PASS: 所有已连接传感器的 ID 匹配
//     FAIL: 传感器 ID 不匹配或无应答
//
//  6. IR Sensor Digital Check
//     TCRT_L / TCRT_R / EDGE_L / EDGE_R: 读取 GPIO 电平
//     PASS: GPIO 读取成功 (电平不判定, 受环境光影响)
//     FAIL: GPIO 读取异常
//
//  7. Charger Status Check
//     CHRG (GPIO18): 读取电平
//     STDBY (GPIO21): 读取电平
//     PASS: 两个引脚逻辑状态互斥 (不可同时为 LOW)
//     FAIL: 两个引脚都为 LOW (异常状态)
//
//  8. Battery ADC Check
//     读取 ADC1_CH0 (GPIO1) 32 次取均值
//     PASS: 电压在合理范围 (2.5V - 4.5V)
//     FAIL: 电压超出范围或 ADC 读取失败
//
//  9. NVS Read/Write Test
//     写入测试键值 → 读取验证 → 删除
//     PASS: 读写一致
//     FAIL: NVS 分区错误或读写不一致
//
// 10. WiFi Scan
//     初始化 WiFi → 扫描附近 AP → 获取 RSSI
//     PASS: 至少扫描到 1 个 AP 或 WiFi 初始化成功
//     FAIL: WiFi 初始化失败
//
// 11. Display Backlight Test
//     LEDC 控制背光 GPIO40: 0% → 50% → 100% → 0% (每档 200ms)
//     PASS: LEDC 配置成功
//     FAIL: LEDC 配置失败
//
// 12. GPIO Loopback Test (如硬件支持)
//     GPIO 输出 → GPIO 输入 测试
//     SKIP (默认): 需要额外硬件飞线
```

### 自检结果 JSON 输出格式

```json
{
  "device": "RobotBuddy",
  "fw_version": "1.0.0",
  "hw_revision": "REV_B",
  "serial": "RB-2026-00001",
  "timestamp": 1720600000,
  "overall": "PASS",
  "duration_ms": 5230,
  "pass": 11,
  "fail": 0,
  "skip": 1,
  "items": [
    {"name": "i2c_scan",       "result": "PASS", "ms": 120,  "detail": "Found: 0x68 (MPU6050), 0x29 (VL53L0X)"},
    {"name": "spi_display",    "result": "PASS", "ms": 850,  "detail": "ST7789 test pattern OK"},
    {"name": "i2s_audio",      "result": "PASS", "ms": 500,  "detail": "I2S DMA OK, 1kHz tone sent"},
    {"name": "pwm_motor",      "result": "PASS", "ms": 420,  "detail": "4 channels configured"},
    {"name": "sensor_whoami",  "result": "PASS", "ms": 200,  "detail": "MPU6050: 0x68, VL53L0X: 0xEE"},
    {"name": "ir_sensor",      "result": "PASS", "ms": 30,   "detail": "TCRT_L:1 TCRT_R:1 EDGE_L:1 EDGE_R:0"},
    {"name": "charger",        "result": "PASS", "ms": 5,    "detail": "CHRG:1 STDBY:0 (battery powered)"},
    {"name": "battery_adc",    "result": "PASS", "ms": 1100, "detail": "VBAT: 3.87V (85%)"},
    {"name": "nvs_rw",         "result": "PASS", "ms": 15,   "detail": "R/W consistent"},
    {"name": "wifi_scan",      "result": "PASS", "ms": 1950, "detail": "5 APs found, RSSI max: -42dBm"},
    {"name": "backlight",      "result": "PASS", "ms": 620,  "detail": "LEDC GPIO40 OK"},
    {"name": "gpio_loopback",  "result": "SKIP", "ms": 0,    "detail": "No loopback wiring detected"}
  ]
}
```

```c
// 自检结果序列化 API
esp_err_t board_self_test_to_json(const self_test_report_t *report, char *buf, size_t buf_size);
void board_self_test_print_json(const self_test_report_t *report);
```

## Board State Snapshot

```c
// ============================================================
// 板级状态快照 (用于调试和状态上报)
// ============================================================

typedef struct {
    // 时钟状态
    uint32_t cpu_freq_mhz;
    uint32_t apb_freq_mhz;

    // 引脚状态
    struct {
        bool i2c_initialized;
        bool spi_initialized;
        bool i2s_initialized;
        bool pwm_initialized;
        bool adc_initialized;
    } buses;

    // Strapping 状态
    struct {
        bool gpio0_high;        // Flash Boot mode
        bool gpio45_low;        // 3.3V SPI
    } strapping;

    // eFuse 状态
    board_efuse_status_t efuse;

    // 初始化耗时
    uint32_t init_total_ms;
    init_phase_status_t phase_status[INIT_PHASE_MAX];

    // 自检结果 (最近一次)
    bool self_test_passed;
    uint32_t last_self_test_time;
} board_state_snapshot_t;

esp_err_t board_get_state_snapshot(board_state_snapshot_t *snapshot);
void board_print_state_snapshot(const board_state_snapshot_t *snapshot);
```

## Rules

1. **引脚冲突零容忍** — 任何 GPIO 不可重复分配。修改引脚分配后必须运行 `/pin-check` 命令验证
2. **初始化顺序不可颠倒** — 严格遵循 PHASE_CRITICAL → BUS → PERIPHERAL → SYSTEM → NETWORK → APP 顺序
3. **每阶段失败即停** — 关键阶段 (PHASE_CRITICAL) 失败时立即中止，不继续后续初始化
4. **非关键阶段容错** — BUS/PERIPHERAL 阶段中单个外设失败不应阻止其他外设初始化，需记录错误状态
5. **Strapping 引脚启动后复用** — GPIO0/3/45/46 在启动完成后可重新配置，但需确保上电瞬间电平正确
6. **ADC2 不可用于 WiFi 使能场景** — VBAT 必须使用 ADC1 (GPIO1)，ADC2 在 WiFi 开启时读数不可靠
7. **eFuse 烧录需二次确认** — 产线 eFuse 编程操作必须有人工确认环节，不可自动执行
8. **自检必须幂等** — 自检程序可重复执行，不改变系统状态；执行前后系统状态一致
9. **自检结果持久化** — NVS 自检通过后，将结果和序列号写入 NVS，供后续产线追溯
10. **时钟切换需稳定等待** — PLL 切换后必须等待锁定信号 (PLL lock)，不可立即依赖新时钟
11. **SPI/I2S DMA 缓冲区对齐** — DMA buffer 必须 `__attribute__((aligned(32)))`，满足 ESP32-S3 cache line 对齐
12. **I2C 总线需上拉确认** — 初始化 I2C 前检查外部上拉电阻是否焊接 (通过扫描总线是否有设备应答间接验证)

## Checklist

- [ ] 所有 GPIO 分配无冲突（运行 `/pin-check` 通过）
- [ ] Strapping 引脚电平符合预期启动模式
- [ ] 时钟树各节点频率符合设计 (CPU 240MHz / APB 80MHz / SPI 40MHz)
- [ ] PLL_AUDIO 输出 4.096MHz MCLK (16kHz × 256)
- [ ] board_init() 启动耗时 < 2s (不含 WiFi 连接)
- [ ] 每个初始化阶段失败时正确打印错误日志
- [ ] I2C0 总线扫描检测到 MPU6050 (0x68)
- [ ] SPI 驱动 ST7789 正常显示测试图案
- [ ] I2S 音频输出 1kHz Tone 正常 (示波器确认波形)
- [ ] PWM 电机通道配置正确 (4 路 LEDC)
- [ ] ADC 电池电压读数准确 (与万用表对比误差 < 5%)
- [ ] NVS 读写测试通过
- [ ] 自检报告 JSON 格式正确且可被产线解析
- [ ] 所有自检项 PASS 或合理 SKIP (无虚假 FAIL)
- [ ] eFuse 状态可读取且与实际配置一致
- [ ] 引脚冲突检测报告无 ❌ 冲突项
- [ ] 保留引脚 (GPIO19/20/26-32/43/44) 未被分配
- [ ] Flash 电压配置 (VDD_SPI) 与实际硬件一致
- [ ] 量产固件 ROM Download 模式已禁用 (安全要求)
