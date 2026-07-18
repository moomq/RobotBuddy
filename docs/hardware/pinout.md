# ESP32-S3 引脚分配表 — RobotBuddy V1.0

> 完整的 ESP32-S3-WROOM-1-N16R8 引脚分配，与 bsp_pinmap.h 一一对应。

**版本:** 1.0.0
**最后更新:** 2026-07-18
**适用范围:** RobotBuddy V1.0 (MVP)

---

## 1. 引脚分配总表

| # | GPIO | 功能 | 外设模块 | 方向 | 上拉/下拉 | 电气组 | 备注 |
|---|------|------|---------|------|----------|--------|------|
| 1 | GPIO0 | BOOT | — | 输入 | 10kΩ 上拉 | — | ⚠️ Strapping: HIGH=Flash启动 |
| 2 | GPIO1 | VBAT_ADC | ADC1_CH0 | 模拟输入 | 无 | ADC1 | 电池电压检测 (分压2:1) |
| 3 | GPIO3 | JTAG_TDO | USB-JTAG | — | — | — | ⚠️ Strapping: 调试用 |
| 4 | GPIO4 | I2S_BCLK | I2S0 | 输出 | 无 | I2S | 音频位时钟 |
| 5 | GPIO5 | I2S_WS | I2S0 | 输出 | 无 | I2S | 音频字选择 |
| 6 | GPIO6 | I2S_DIN | I2S0 | 输入 | 无 | I2S | 麦克风数据 (INMP441) |
| 7 | GPIO7 | I2S_DOUT | I2S0 | 输出 | 无 | I2S | 功放数据 (MAX98357A) |
| 8 | GPIO8 | I2C_SDA | I2C0 | 开漏 | 4.7kΩ 上拉 | I2C | I2C 数据 (MPU6050) |
| 9 | GPIO9 | I2C_SCL | I2C0 | 开漏 | 4.7kΩ 上拉 | I2C | I2C 时钟 |
| 10 | GPIO10 | MOTOR_AIN1 | LEDC | 输出 | 无 | PWM | 左电机 IN1 |
| 11 | GPIO11 | MOTOR_AIN2 | LEDC | 输出 | 无 | PWM | 左电机 IN2 |
| 12 | GPIO12 | MOTOR_BIN1 | LEDC | 输出 | 无 | PWM | 右电机 IN1 |
| 13 | GPIO13 | MOTOR_BIN2 | LEDC | 输出 | 无 | PWM | 右电机 IN2 |
| 14 | GPIO14 | IR_OBSTACLE_L | GPIO | 输入 | 上拉 | GPIO | 左避障 (TCRT5000) |
| 15 | GPIO15 | IR_OBSTACLE_R | GPIO | 输入 | 上拉 | GPIO | 右避障 (TCRT5000) |
| 16 | GPIO16 | IR_EDGE_L | GPIO | 输入 | 上拉 | GPIO | 左防跌落 (ITR20001) |
| 17 | GPIO17 | IR_EDGE_R | GPIO | 输入 | 上拉 | GPIO | 右防跌落 (ITR20001) |
| 18 | GPIO18 | CHRG | GPIO | 输入 | 上拉 | GPIO | TP4056 充电状态 |
| 19 | GPIO19 | USB_D+ | USB | 双向 | — | USB | ⚠️ USB 2.0 D+ |
| 20 | GPIO20 | USB_D- | USB | 双向 | — | USB | ⚠️ USB 2.0 D- |
| 21 | GPIO21 | STDBY | GPIO | 输入 | 上拉 | GPIO | TP4056 充满状态 |
| 22 | GPIO35 | LCD_MOSI | SPI3 | 输出 | 无 | SPI | 屏幕数据 |
| 23 | GPIO36 | LCD_SCLK | SPI3 | 输出 | 无 | SPI | 屏幕时钟 |
| 24 | GPIO37 | LCD_CS | SPI3 | 输出 | 无 | SPI | 屏幕片选 |
| 25 | GPIO38 | LCD_DC | SPI3 | 输出 | 无 | SPI | 屏幕 DC (RS) |
| 26 | GPIO39 | LCD_RST | GPIO | 输出 | 无 | GPIO | 屏幕复位 |
| 27 | GPIO40 | LCD_BL | GPIO | 输出 | 无 | GPIO | 屏幕背光 |
| 28 | GPIO43 | UART_TX | UART0 | 输出 | — | UART | 控制台 TX |
| 29 | GPIO44 | UART_RX | UART0 | 输入 | — | UART | 控制台 RX |
| 30 | GPIO45 | VDD_SPI | — | — | — | — | ⚠️ Strapping: 接3.3V |
| 31 | GPIO46 | ROM_MSG | — | — | 下拉 | — | ⚠️ Strapping: LOW |

---

## 2. 按子系统分类

### 2.1 显示 (SPI3_HOST)

| GPIO | 信号 | 方向 | 说明 |
|------|------|------|------|
| GPIO35 | MOSI | 输出 | SPI 数据 |
| GPIO36 | SCLK | 输出 | SPI 时钟 |
| GPIO37 | CS | 输出 | 片选 (低有效) |
| GPIO38 | DC | 输出 | 数据/命令选择 |
| GPIO39 | RST | 输出 | 硬件复位 (低有效) |
| GPIO40 | BL | 输出 | 背光控制 (高=亮) |

### 2.2 音频 (I2S_NUM_0)

| GPIO | 信号 | 方向 | 说明 |
|------|------|------|------|
| GPIO4 | BCLK | 输出 | I2S 位时钟 |
| GPIO5 | WS | 输出 | I2S 字选择 |
| GPIO6 | DIN | 输入 | 麦克风数据输入 |
| GPIO7 | DOUT | 输出 | 功放数据输出 |

### 2.3 传感器 (I2C_NUM_0)

| GPIO | 信号 | 方向 | 说明 |
|------|------|------|------|
| GPIO8 | SDA | 开漏 | I2C 数据线 |
| GPIO9 | SCL | 开漏 | I2C 时钟线 |

**I2C 设备地址:**

| 设备 | 地址 | 引脚配置 |
|------|------|---------|
| MPU6050 | 0x68 | AD0 = GND |
| VL53L0X (预留) | 0x29 | 默认地址 |

### 2.4 电机 (LEDC PWM)

| GPIO | 信号 | LEDC通道 | 说明 |
|------|------|---------|------|
| GPIO10 | AIN1 | LEDC_CH0 | 左电机 IN1 |
| GPIO11 | AIN2 | LEDC_CH1 | 左电机 IN2 |
| GPIO12 | BIN1 | LEDC_CH2 | 右电机 IN1 |
| GPIO13 | BIN2 | LEDC_CH3 | 右电机 IN2 |

### 2.5 红外传感器 (GPIO)

| GPIO | 信号 | 方向 | 说明 |
|------|------|------|------|
| GPIO14 | IR_OBSTACLE_L | 输入 | 左避障 (LOW=有障碍) |
| GPIO15 | IR_OBSTACLE_R | 输入 | 右避障 (LOW=有障碍) |
| GPIO16 | IR_EDGE_L | 输入 | 左防跌落 (HIGH=危险) |
| GPIO17 | IR_EDGE_R | 输入 | 右防跌落 (HIGH=危险) |

### 2.6 电源 (GPIO + ADC)

| GPIO | 信号 | 方向 | 说明 |
|------|------|------|------|
| GPIO1 | VBAT_ADC | 模拟输入 | 电池电压 (分压2:1) |
| GPIO18 | CHRG | 输入 | 充电中 (LOW=充电) |
| GPIO21 | STDBY | 输入 | 充满 (LOW=充满) |

### 2.7 调试 (USB + UART)

| GPIO | 信号 | 方向 | 说明 |
|------|------|------|------|
| GPIO19 | USB_D+ | 双向 | USB 2.0 数据+ |
| GPIO20 | USB_D- | 双向 | USB 2.0 数据- |
| GPIO43 | UART_TX | 输出 | 控制台串口 TX |
| GPIO44 | UART_RX | 输入 | 控制台串口 RX |

---

## 3. Strapping 引脚配置

| GPIO | 功能 | 上电状态要求 | 电路配置 |
|------|------|-------------|---------|
| GPIO0 | BOOT | HIGH = Flash 启动 | 10kΩ 上拉到 3.3V |
| GPIO45 | VDD_SPI | HIGH = 外部 3.3V | 直接连接 3.3V |
| GPIO46 | ROM_MSG | LOW = 禁止 ROM 打印 | 10kΩ 下拉到 GND |

> **重要:** Strapping 引脚在上电瞬间（芯片复位释放时）的状态决定了芯片启动模式。
> 必须确保这些引脚在上电时有稳定的电平，不能由外部设备驱动。

---

## 4. 未使用 GPIO 处理

| GPIO | 处理方式 | 说明 |
|------|---------|------|
| GPIO2 | 浮空 (输入) | ESP32-S3 无特殊要求 |
| GPIO26-34 | 浮空 (输入) | 预留扩展 |
| GPIO41-42 | 浮空 (输入) | 预留扩展 |

> **注意:** ESP32-S3 的 GPIO26-32 默认连接到 SPI Flash/PSRAM (内部使用)，
> 不可用作 GPIO。GPIO33 仅可用作输入。

---

## 5. 引脚冲突检查矩阵

| 检查项 | 结果 | 说明 |
|--------|------|------|
| GPIO 复用冲突 | ✅ 无冲突 | 每个引脚单一功能 |
| Strapping 引脚 | ✅ 已处理 | GPIO0/3/45/46 有正确上下拉 |
| ADC 冲突 | ✅ 无冲突 | GPIO1=ADC1, WiFi 不使用 ADC1 |
| USB 专用引脚 | ✅ 已保留 | GPIO19/20 专用 |
| UART 专用引脚 | ✅ 已保留 | GPIO43/44 专用 |
| PSRAM 引脚 | ✅ 无冲突 | GPIO33-37 使用 SPI/PSRAM (内部) |
| 启动模式 | � | GPIO0=HIGH → Flash 启动 |

---

## 6. 与 bsp_pinmap.h 对应关系

本文档所有引脚定义与 `firmware/components/bsp/include/bsp_pinmap.h` 完全一致。
任何引脚变更必须同步更新此文档和固件头文件。

| 文档定义 | bsp_pinmap.h 宏 | 一致性 |
|----------|----------------|--------|
| GPIO1 → VBAT_ADC | BSP_PIN_VBAT_ADC = GPIO_NUM_1 | ✅ |
| GPIO4 → I2S_BCLK | BSP_PIN_I2S_BCLK = GPIO_NUM_4 | ✅ |
| GPIO5 → I2S_WS | BSP_PIN_I2S_WS = GPIO_NUM_5 | ✅ |
| GPIO6 → I2S_DIN | BSP_PIN_I2S_DIN = GPIO_NUM_6 | ✅ |
| GPIO7 → I2S_DOUT | BSP_PIN_I2S_DOUT = GPIO_NUM_7 | ✅ |
| GPIO8 → I2C_SDA | BSP_PIN_I2C_SDA = GPIO_NUM_8 | ✅ |
| GPIO9 → I2C_SCL | BSP_PIN_I2C_SCL = GPIO_NUM_9 | ✅ |
| GPIO10 → MOTOR_AIN1 | BSP_PIN_MOTOR_AIN1 = GPIO_NUM_10 | ✅ |
| GPIO11 → MOTOR_AIN2 | BSP_PIN_MOTOR_AIN2 = GPIO_NUM_11 | ✅ |
| GPIO12 → MOTOR_BIN1 | BSP_PIN_MOTOR_BIN1 = GPIO_NUM_12 | ✅ |
| GPIO13 → MOTOR_BIN2 | BSP_PIN_MOTOR_BIN2 = GPIO_NUM_13 | ✅ |
| GPIO14 → IR_OBSTACLE_L | BSP_PIN_IR_OBSTACLE_L = GPIO_NUM_14 | ✅ |
| GPIO15 → IR_OBSTACLE_R | BSP_PIN_IR_OBSTACLE_R = GPIO_NUM_15 | ✅ |
| GPIO16 → IR_EDGE_L | BSP_PIN_IR_EDGE_L = GPIO_NUM_16 | ✅ |
| GPIO17 → IR_EDGE_R | BSP_PIN_IR_EDGE_R = GPIO_NUM_17 | ✅ |
| GPIO18 → CHRG | BSP_PIN_CHRG = GPIO_NUM_18 | ✅ |
| GPIO21 → STDBY | BSP_PIN_STDBY = GPIO_NUM_21 | ✅ |
| GPIO35 → LCD_MOSI | BSP_PIN_LCD_MOSI = GPIO_NUM_35 | ✅ |
| GPIO36 → LCD_SCLK | BSP_PIN_LCD_SCLK = GPIO_NUM_36 | ✅ |
| GPIO37 → LCD_CS | BSP_PIN_LCD_CS = GPIO_NUM_37 | ✅ |
| GPIO38 → LCD_DC | BSP_PIN_LCD_DC = GPIO_NUM_38 | ✅ |
| GPIO39 → LCD_RST | BSP_PIN_LCD_RST = GPIO_NUM_39 | ✅ |
| GPIO40 → LCD_BL | BSP_PIN_LCD_BL = GPIO_NUM_40 | ✅ |

---

## 7. 变更记录

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0.0 | 2026-07-18 | 初始版本 |