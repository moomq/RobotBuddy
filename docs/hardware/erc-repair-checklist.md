# RobotBuddy ERC 修复检查清单

**项目**: RobotBuddy
**日期**: 2026-07-18
**状态**: 子图层次标签悬空，需要手动修复

---

## 📊 错误统计

| 子图 | 错误数 | 主要问题 |
|------|--------|---------|
| Power | 8 | 电源信号层次标签悬空 |
| MCU | 26 | 大量信号层次标签悬空 |
| Display | 7 | LCD 信号层次标签悬空 |
| Audio | 6 | I2S 信号层次标签悬空 |
| Sensors | 7 | I2C/IR 信号层次标签悬空 |
| Motors | 5 | 电机控制信号层次标签悬空 |

**总计**: 59 个层次标签需要检查和修复

---

## 🔧 修复步骤概览

### 修复顺序（推荐）

```
1. Power 子图  → 电源基础
2. MCU 子图    → 核心控制
3. Audio 子图  → I2S 音频
4. Display 子图 → SPI 显示
5. Sensors 子图 → I2C 传感器
6. Motors 子图  → 电机驱动
```

### 每个子图的修复流程

```
1. 打开子图
2. 定位悬空的层次标签
3. 检查标签是否连接到网络
4. 连接到对应的元件引脚
5. 保存并关闭
6. 运行 ERC 检查验证
```

---

## 📋 详细检查清单

### 1️⃣ Power 子图 (`power.kicad_sch`)

#### 需要检查的层次标签

| 标签名 | 位置 (mm) | 类型 | 连接目标 | 状态 |
|--------|----------|------|---------|------|
| **VBUS** | (15, 30) | input | USB-C 连接器 VBUS 引脚 | ⬜ 待修复 |
| **USB_DP** | (10, 50) | bidirectional | USB-C D+ 引脚 | ⬜ 待修复 |
| **USB_DN** | (10, 55) | bidirectional | USB-C D- 引脚 | ⬜ 待修复 |
| **+5V** | (170, 80) | output | MT3608 输出端 | ⬜ 待修复 |
| **+3V3** | (170, 90) | output | AMS1117-3.3V 输出端 | ⬜ 待修复 |
| **VBAT_ADC** | (170, 40) | output | 电池分压电阻网络 | ⬜ 待修复 |
| **CHRG** | (170, 50) | output | TP4056 CHRG 引脚 | ⬜ 待修复 |
| **STDBY** | (170, 55) | output | TP4056 STDBY 引脚 | ⬜ 待修复 |

#### 额外需要添加

- [ ] 添加 `PWR_FLAG` 到 VBUS 网络
- [ ] 添加 `PWR_FLAG` 到 +5V 网络
- [ ] 添加 `PWR_FLAG` 到 +3V3 网络
- [ ] 添加 `PWR_FLAG` 到 GND 网络
- [ ] 确保所有 GND 引脚连接到 GND 电源符号

---

### 2️⃣ MCU 子图 (`mcu.kicad_sch`)

#### 需要检查的层次标签

| 标签名 | 位置 (mm) | 类型 | 连接目标 | 状态 |
|--------|----------|------|---------|------|
| **+3V3** | (10, 50) | input | ESP32-S3 3V3 电源引脚 | ⬜ 待修复 |
| **+5V** | (10, 55) | input | （注意：ESP32-S3 使用 3.3V，检查设计） | ⬜ 待修复 |
| **+3V3** | (10, 60) | input | ESP32-S3 3V3 电源引脚（重复？） | ⬜ 待修复 |
| **VBAT_ADC** | (10, 65) | input | ESP32-S3 ADC 引脚（如 GPIO1） | ⬜ 待修复 |
| **CHRG** | (10, 70) | input | ESP32-S3 GPIO 引脚 | ⬜ 待修复 |
| **STDBY** | (10, 75) | input | ESP32-S3 GPIO 引脚 | ⬜ 待修复 |
| **USB_DP** | (10, 80) | bidirectional | ESP32-S3 USB D+ 引脚 | ⬜ 待修复 |
| **USB_DN** | (10, 85) | bidirectional | ESP32-S3 USB D- 引脚 | ⬜ 待修复 |
| **I2S_BCLK** | (170, 60) | output | ESP32-S3 I2S BCLK 引脚 | ⬜ 待修复 |
| **I2S_WS** | (170, 65) | output | ESP32-S3 I2S WS 引脚 | ⬜ 待修复 |
| **I2S_DIN** | (170, 70) | input | ESP32-S3 I2S DIN 引脚 | ⬜ 待修复 |
| **I2S_DOUT** | (170, 75) | output | ESP32-S3 I2S DOUT 引脚 | ⬜ 待修复 |
| **I2C_SDA** | (170, 80) | bidirectional | ESP32-S3 I2C SDA 引脚 | ⬜ 待修复 |
| **I2C_SCL** | (170, 85) | output | ESP32-S3 I2C SCL 引脚 | ⬜ 待修复 |
| **LCD_MOSI** | (170, 90) | output | ESP32-S3 SPI MOSI 引脚 | ⬜ 待修复 |
| **LCD_SCLK** | (170, 95) | output | ESP32-S3 SPI SCLK 引脚 | ⬜ 待修复 |
| **LCD_CS** | (170, 100) | output | ESP32-S3 GPIO（CS） | ⬜ 待修复 |
| **LCD_DC** | (170, 105) | output | ESP32-S3 GPIO（DC） | ⬜ 待修复 |
| **LCD_RST** | (170, 110) | output | ESP32-S3 GPIO（RST） | ⬜ 待修复 |
| **LCD_BL** | (170, 115) | output | ESP32-S3 GPIO（背光） | ⬜ 待修复 |
| **MOTOR_AIN1** | (170, 120) | output | ESP32-S3 GPIO | ⬜ 待修复 |
| **MOTOR_AIN2** | (170, 125) | output | ESP32-S3 GPIO | ⬜ 待修复 |
| **MOTOR_BIN1** | (170, 130) | output | ESP32-S3 GPIO | ⬜ 待修复 |
| **MOTOR_BIN2** | (170, 135) | output | ESP32-S3 GPIO | ⬜ 待修复 |
| **IR_OBSTACLE_L** | (170, 140) | input | ESP32-S3 GPIO | ⬜ 待修复 |
| **IR_OBSTACLE_R** | (170, 145) | input | ESP32-S3 GPIO | ⬜ 待修复 |
| **IR_EDGE_L** | (170, 150) | input | ESP32-S3 GPIO | ⬜ 待修复 |
| **IR_EDGE_R** | (170, 155) | input | ESP32-S3 GPIO | ⬜ 待修复 |

#### ⚠️ MCU 子图特别注意事项

**问题**: 有多个层次标签在相同坐标，表示引脚位置重叠

- **位置 (10, 50)**: +3V3
- **位置 (10, 55)**: +5V（设计可能有误，ESP32-S3 不应直接连接 5V）
- **位置 (10, 60)**: +3V3（重复定义）

**建议操作**:
1. 检查电源设计是否正确
2. 如果 +5V 用于其他目的，重新命名或连接
3. 删除重复的 +3V3 定义
4. 重新排列层次标签位置，避免重叠

#### ESP32-S3 引脚分配建议

| 功能 | 建议引脚 | 说明 |
|------|---------|------|
| I2C_SDA | GPIO8 | I2C 数据 |
| I2C_SCL | GPIO9 | I2C 时钟 |
| I2S_BCLK | GPIO14 | I2S 位时钟 |
| I2S_WS | GPIO15 | I2S 字选择 |
| I2S_DIN | GPIO16 | I2S 数据输入 |
| I2S_DOUT | GPIO17 | I2S 数据输出 |
| SPI_MOSI | GPIO35 | SPI 数据 |
| SPI_SCLK | GPIO36 | SPI 时钟 |
| LCD_CS | GPIO37 | 片选 |
| LCD_DC | GPIO38 | 数据/命令 |
| LCD_RST | GPIO39 | 复位 |
| LCD_BL | GPIO40 | 背光控制 |
| MOTOR_AIN1 | GPIO41 | 电机 A 相 1 |
| MOTOR_AIN2 | GPIO42 | 电机 A 相 2 |
| MOTOR_BIN1 | GPIO45 | 电机 B 相 1 |
| MOTOR_BIN2 | GPIO46 | 电机 B 相 2 |
| IR_OBSTACLE_L | GPIO47 | 左障碍传感器 |
| IR_OBSTACLE_R | GPIO48 | 右障碍传感器 |
| IR_EDGE_L | GPIO5 | 左边缘传感器 |
| IR_EDGE_R | GPIO6 | 右边缘传感器 |
| CHRG | GPIO4 | 充电状态 |
| STDBY | GPIO7 | 待机状态 |
| VBAT_ADC | GPIO1 | 电池电压 ADC |

---

### 3️⃣ Audio 子图 (`audio.kicad_sch`)

#### 需要检查的层次标签

| 标签名 | 位置 (mm) | 类型 | 连接目标 | 状态 |
|--------|----------|------|---------|------|
| **+3V3** | (10, 50) | input | INMP441 VDD, MAX98357A Vin | ⬜ 待修复 |
| **+5V** | (10, 55) | input | （检查是否需要） | ⬜ 待修复 |
| **I2S_BCLK** | (10, 65) | input | INMP441 SCK, MAX98357A BCLK | ⬜ 待修复 |
| **I2S_WS** | (10, 70) | input | INMP441 WS, MAX98357A LRC | ⬜ 待修复 |
| **I2S_DOUT** | (10, 75) | input | MAX98357A DIN | ⬜ 待修复 |
| **I2S_DIN** | (170, 65) | output | INMP441 DOUT | ⬜ 待修复 |

#### 元件连接指南

**INMP441 麦克风**:
- VDD → +3V3
- GND → GND
- L/R → GND (左声道) 或 VDD (右声道)
- SD (DOUT) → I2S_DIN 层次标签
- SCK → I2S_BCLK 层次标签
- WS → I2S_WS 层次标签

**MAX98357A 放大器**:
- Vin → +3V3 或 +5V
- GND → GND
- BCLK → I2S_BCLK 层次标签
- LRC → I2S_WS 层次标签
- DIN → I2S_DOUT 层次标签

---

### 4️⃣ Display 子图 (`display.kicad_sch`)

#### 需要检查的层次标签

| 标签名 | 位置 (mm) | 类型 | 连接目标 | 状态 |
|--------|----------|------|---------|------|
| **+3V3** | (10, 50) | input | ST7789 VCC | ⬜ 待修复 |
| **LCD_MOSI** | (10, 60) | input | ST7789 SDA (MOSI) | ⬜ 待修复 |
| **LCD_SCLK** | (10, 65) | input | ST7789 SCL (SCLK) | ⬜ 待修复 |
| **LCD_CS** | (10, 70) | input | ST7789 CS | ⬜ 待修复 |
| **LCD_DC** | (10, 75) | input | ST7789 DC (RS) | ⬜ 待修复 |
| **LCD_RST** | (10, 80) | input | ST7789 RST | ⬜ 待修复 |
| **LCD_BL** | (10, 85) | input | 背光控制（GPIO 或 PWM） | ⬜ 待修复 |

#### ST7789 连接指南

| ST7789 引脚 | 连接到 |
|------------|--------|
| VCC | +3V3 层次标签 |
| GND | GND 电源符号 |
| SCL (SCLK) | LCD_SCLK 层次标签 |
| SDA (MOSI) | LCD_MOSI 层次标签 |
| RES (RST) | LCD_RST 层次标签 |
| DC (RS) | LCD_DC 层次标签 |
| CS | LCD_CS 层次标签 |
| BLK (背光) | LCD_BL 层次标签 |

---

### 5️⃣ Sensors 子图 (`sensors.kicad_sch`)

#### 需要检查的层次标签

| 标签名 | 位置 (mm) | 类型 | 连接目标 | 状态 |
|--------|----------|------|---------|------|
| **+3V3** | (10, 50) | input | MPU6050 VCC, VL53L0X VCC, TCRT5000 VCC | ⬜ 待修复 |
| **I2C_SDA** | (10, 60) | bidirectional | MPU6050 SDA, VL53L0X SDA | ⬜ 待修复 |
| **I2C_SCL** | (10, 65) | input | MPU6050 SCL, VL53L0X SCL | ⬜ 待修复 |
| **IR_OBSTACLE_L** | (170, 50) | output | 左 TCRT5000 输出 | ⬜ 待修复 |
| **IR_OBSTACLE_R** | (170, 55) | output | 右 TCRT5000 输出 | ⬜ 待修复 |
| **IR_EDGE_L** | (170, 60) | output | 左边缘传感器输出 | ⬜ 待修复 |
| **IR_EDGE_R** | (170, 65) | output | 右边缘传感器输出 | ⬜ 待修复 |

#### I2C 上拉电阻

**重要**: I2C 总线需要上拉电阻

- SDA: 4.7kΩ 上拉到 +3V3
- SCL: 4.7kΩ 上拉到 +3V3

#### 元件连接指南

**MPU6050 陀螺仪/加速度计**:
- VCC → +3V3 层次标签
- GND → GND 电源符号
- SCL → I2C_SCL 层次标签
- SDA → I2C_SDA 层次标签
- AD0 → GND (地址 0x68) 或 VCC (地址 0x69)
- INT → （可选，中断输出）

**VL53L0X 激光测距**:
- VCC → +3V3 层次标签
- GND → GND 电源符号
- SCL → I2C_SCL 层次标签
- SDA → I2C_SDA 层次标签
- XSHUT → （可选，关断控制）

**TCRT5000 红外传感器** (x4):
- VCC (阳极) → +3V3 或 +5V
- GND (阴极) → GND
- 输出 → 对应的 IR_OBSTACLE_* 或 IR_EDGE_* 层次标签

---

### 6️⃣ Motors 子图 (`motors.kicad_sch`)

#### 需要检查的层次标签

| 标签名 | 位置 (mm) | 类型 | 连接目标 | 状态 |
|--------|----------|------|---------|------|
| **+5V** | (10, 50) | input | DRV8833 VM (电机电源) | ⬜ 待修复 |
| **MOTOR_AIN1** | (10, 60) | input | DRV8833 AIN1 | ⬜ 待修复 |
| **MOTOR_AIN2** | (10, 65) | input | DRV8833 AIN2 | ⬜ 待修复 |
| **MOTOR_BIN1** | (10, 70) | input | DRV8833 BIN1 | ⬜ 待修复 |
| **MOTOR_BIN2** | (10, 75) | input | DRV8833 BIN2 | ⬜ 待修复 |

#### DRV8833 连接指南

| DRV8833 引脚 | 连接到 |
|-------------|--------|
| VM (电机电源) | +5V 层次标签 |
| VCC (逻辑电源) | +3V3 或 +5V |
| GND | GND 电源符号 |
| AIN1 | MOTOR_AIN1 层次标签 |
| AIN2 | MOTOR_AIN2 层次标签 |
| BIN1 | MOTOR_BIN1 层次标签 |
| BIN2 | MOTOR_BIN2 层次标签 |
| AOUT1 | 电机 A 正极 |
| AOUT2 | 电机 A 负极 |
| BOUT1 | 电机 B 正极 |
| BOUT2 | 电机 B 负极 |
| nFAULT | （可选，故障指示） |
| nSLEEP | VCC (使能) 或 GPIO 控制 |

---

## 🔧 通用修复步骤

### 在 KiCad 中修复悬空层次标签

1. **打开子图文件**
   ```
   文件 → 打开 → 选择子图 .kicad_sch 文件
   ```

2. **定位层次标签**
   - 使用 ERC 报告中的坐标
   - 或使用 "查找" 功能 (Ctrl+F)

3. **检查标签连接**
   - 点击层次标签
   - 查看是否连接到导线
   - 未连接的标签会显示悬空

4. **连接到网络**
   - 使用 "添加导线" 工具 (快捷键 W)
   - 从层次标签的引脚画导线
   - 连接到对应的元件引脚

5. **验证连接**
   - 运行 ERC 检查 (Tools → Electrical Rules Checker)
   - 确认该层次标签错误消失

6. **保存文件**
   - Ctrl+S 保存

---

## ⚠️ 常见错误和解决方案

### 1. 层次标签方向错误

**症状**: 层次标签显示为输出但应该是输入

**解决**:
- 右键点击层次标签
- 选择 "Edit"
- 更改 "Shape" 属性：
  - `input` - 输入信号
  - `output` - 输出信号
  - `bidirectional` - 双向信号

### 2. 网络未命名

**症状**: 导线连接了但网络名称未定义

**解决**:
- 右键点击导线
- 选择 "Add Label" 或 "Add Global Label"
- 输入正确的网络名称

### 3. 引脚类型不匹配

**症状**: ERC 报告 "Pin type conflict"

**解决**:
- 检查元件引脚类型是否与层次标签匹配
- 例如：输入引脚不能连接到输出层次标签

### 4. 多个相同名称的标签

**症状**: ERC 报告 "Multiple labels with same name"

**解决**:
- 删除重复的层次标签
- 或重命名为不同的名称

---

## ✅ 完成标准

修复完成后，应达到以下标准：

- [ ] 所有子图 ERC 检查通过 (0 errors)
- [ ] 顶层原理图 ERC 检查通过 (0 errors)
- [ ] 所有层次标签连接到对应的元件引脚
- [ ] 电源网络有 PWR_FLAG 符号
- [ ] GND 网络正确连接
- [ ] 无引脚重叠问题
- [ ] 网络名称正确无误

---

## 📊 进度跟踪

| 子图 | 层次标签数 | 已修复 | 状态 |
|------|-----------|--------|------|
| Power | 8 | 0 | ⬜ 未开始 |
| MCU | 26 | 0 | ⬜ 未开始 |
| Audio | 6 | 0 | ⬜ 未开始 |
| Display | 7 | 0 | ⬜ 未开始 |
| Sensors | 7 | 0 | ⬜ 未开始 |
| Motors | 5 | 0 | ⬜ 未开始 |
| **总计** | **59** | **0** | ⬜ **未开始** |

---

## 📚 参考资料

### KiCad 快捷键

| 快捷键 | 功能 |
|--------|------|
| W | 添加导线 |
| L | 添加标签 |
| Ctrl+L | 添加全局标签 |
| Ctrl+H | 添加层次标签 |
| P | 放置元件 |
| Delete | 删除 |
| R | 旋转 |
| T, E | ERC 检查 |

### KiCad 文档

- 官方文档: https://docs.kicad.org/
- 层次化设计: https://docs.kicad.org/6.0/en/eeschema/eeschema.html#hierarchical-schematics
- ERC 检查: https://docs.kicad.org/6.0/en/eeschema/eeschema.html#electrical-rules-check

---

**生成时间**: 2026-07-18
**作者**: Claude Code Assistant
**工具版本**: KiCad 10.0.4
