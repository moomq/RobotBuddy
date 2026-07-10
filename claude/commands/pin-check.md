# /pin-check — RobotBuddy GPIO 引脚冲突检查

## 用途

验证 RobotBuddy 的 ESP32-S3 GPIO 引脚分配，检测冲突、保留引脚误用和 Strapping 引脚风险。

## 适用场景

- 新增硬件模块时确认引脚可用
- PCB 设计前验证引脚分配完整性
- 排查引脚冲突导致的硬件异常
- 修改引脚分配后重新验证

## 工作流

```
第 1 阶段：读取引脚分配
├── 1.1 读取 docs/hardware/pinout.md（如存在）
├── 1.2 或读取代码中的 GPIO 定义 (#define PIN_XXX GPIO_NUM_XX)
├── 1.3 或读取 Kconfig 中的引脚配置
└── 1.4 输出：当前引脚分配表

第 2 阶段：冲突检测
├── 2.1 重复分配检测: 同一 GPIO 被多个模块使用
├── 2.2 保留引脚检测:
│   ├── GPIO 19/20 — USB D+/D- (USB-JTAG)
│   ├── GPIO 45/46 — PSRAM (部分模组)
│   ├── GPIO 26-32 — Flash/PSRAM (勿用)
│   └── GPIO 0, 2, 46 — Strapping 引脚
├── 2.3 Strapping 引脚风险:
│   ├── GPIO 0 — 启动模式选择 (HIGH=Flash, LOW=Download)
│   ├── GPIO 3 — JTAG 信号 (TDO)
│   ├── GPIO 45 — VDD_SPI 电压选择
│   └── GPIO 46 — ROM messages printing
├── 2.4 接口兼容性检测:
│   ├── SPI 引脚是否分配到 VSPI/HSPI
│   ├── I2C 引脚是否支持开漏
│   ├── I2S 引脚是否在支持的 GPIO 范围
│   ├── ADC 输入是否在 ADC1 (WiFi 使用时 ADC2 不可用)
│   └── PWM 输出是否支持 LEDC
└── 2.5 输出：冲突报告

第 3 阶段：建议修正
├── 3.1 为冲突引脚推荐替代 GPIO
├── 3.2 标注 Strapping 引脚的使用风险
├── 3.3 生成修正后的引脚分配表
└── 3.4 输出：修正建议

第 4 阶段：生成文档
├── 4.1 更新 docs/hardware/pinout.md
├── 4.2 生成引脚分配图（ASCII / 表格）
└── 4.3 输出：更新后的引脚文档
```

## ESP32-S3 GPIO 速查

```c
// GPIO 可用性分类
//
// ✅ 安全可用 (任意功能):
// GPIO 1-18, 21-23, 35-42
//
// ⚠️ Strapping 引脚 (上拉/下拉影响启动):
// GPIO 0  — Boot mode (HIGH=Flash boot, LOW=Download boot)
// GPIO 3  — JTAG TDO
// GPIO 45 — VDD_SPI 电压 (0=3.3V, 1=1.8V)
// GPIO 46 — ROM messages (0=enable, 1=disable)
//
// ❌ 禁止使用:
// GPIO 19, 20 — USB D+/D- (USB-JTAG 调试)
// GPIO 26-32 — 连接 Flash (已占用)
// GPIO 43, 44 — UART0 TX/RX (默认 USB-JTAG，可复用但调试时不可)
//
// ⚡ 限制使用 (WiFi 开启时 ADC2 不可用):
// GPIO 1-10  — ADC1 (安全，WiFi 开启时可用)
// GPIO 11-20 — ADC2 (WiFi 开启时不可用！)

// 推荐引脚分配方案 (RobotBuddy)
//
// SPI (ST7789 屏幕):
//   SCLK = GPIO 36, MOSI = GPIO 35, MISO = N/C
//   CS   = GPIO 37, DC   = GPIO 38, RST  = GPIO 39
//   BL   = GPIO 40 (背光控制)
//
// I2S (音频):
//   BCLK = GPIO 4,  WS = GPIO 5
//   DIN  = GPIO 6  (INMP441 麦克风)
//   DOUT = GPIO 7  (MAX98357A 功放)
//
// I2C (传感器):
//   SDA  = GPIO 8,  SCL = GPIO 9
//   (MPU6050, VL53L0X 共享 I2C0)
//
// PWM (电机):
//   AIN1 = GPIO 10, AIN2 = GPIO 11 (左电机)
//   BIN1 = GPIO 12, BIN2 = GPIO 13 (右电机)
//
// GPIO (红外/充电):
//   TCRT_L  = GPIO 14, TCRT_R  = GPIO 15 (避障)
//   EDGE_L  = GPIO 16, EDGE_R  = GPIO 17 (边缘)
//   CHRG    = GPIO 18, STDBY   = GPIO 21 (TP4056)
//
// ADC (电池):
//   VBAT    = GPIO 1 (ADC1_CH0, 电阻分压)
```

## 冲突检测输出格式

```markdown
## GPIO 冲突检测报告

### ❌ 冲突 (必须修复)
| GPIO | 模块 A | 模块 B | 说明 |
|------|--------|--------|------|
| GPIO 4 | I2S BCLK | IR 避障 | 引脚重复分配 |

### ⚠️ 风险 (建议修改)
| GPIO | 当前用途 | 风险 | 建议 |
|------|---------|------|------|
| GPIO 0 | 用户按钮 | Strapping: LOW=Download mode | 改用 GPIO 1 |
| GPIO 11 | 电机 PWM | ADC2: WiFi 时不可用 | 改用 GPIO 12 |

### ✅ 安全 (无问题)
| GPIO | 用途 | 接口 | 备注 |
|------|------|------|------|
| GPIO 1 | VBAT ADC | ADC1 | WiFi 安全 |
| GPIO 4 | I2S BCLK | I2S0 | — |
| ... | | | |

### 引脚使用统计
- 已分配: 18 / 48 (37.5%)
- 可用: 30
- 保留: 0 (无冲突)
```

## 注意事项

- ADC2 在 WiFi 开启时不可用，这是最常见的隐性冲突
- Strapping 引脚可以在启动后复用，但需确保上电时的电平正确
- ESP32-S3 的 GPIO 矩阵允许将大部分外设映射到任意 GPIO，但有最优分配
- I2S 和 I2C 的默认引脚使用可减少配置复杂度
- 建议在 PCB 设计阶段就完成引脚分配验证
