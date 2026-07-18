# KiCad ERC 错误修复最终指南

**项目**: RobotBuddy
**日期**: 2026-07-18
**状态**: 需要 KiCad GUI 手动修复

---

## 🔴 问题诊断

### ERC 报告摘要

| 错误类型 | 数量 | 说明 |
|---------|------|------|
| **Label not connected** | 77 | 顶层全局标签悬空，未连接到网络 |
| **Pin not connected** | 36 | Sheet Pin 未连接到导线 |
| **Hier label mismatch** | - | Sheet Pin 与子图层次标签不匹配 |

### 根本原因

1. **顶层原理图**:
   - 有全局标签 (`global_label`)
   - 有 Sheet Pin 定义
   - **但没有导线连接 Sheet Pin**

2. **子图**:
   - 使用层次标签 (`hierarchical_label`)
   - 层次标签需要通过顶层的 Sheet Pin 传递信号

3. **设计模式混合**:
   - 同时使用了层次标签和全局标签
   - 两者未正确关联

---

## ✅ 推荐解决方案

### 方案一：在 KiCad GUI 中手动连接（最可靠）

#### 步骤 1：打开项目

```bash
# Windows
E:\kicad\bin\kicad.exe "F:\04 code\RobotBuddy\hardware\RobotBuddy.kicad_pro"
```

或在 KiCad 中打开 `F:\04 code\RobotBuddy\hardware\RobotBuddy.kicad_pro`

#### 步骤 2：打开顶层原理图

双击 `RobotBuddy.kicad_sch` 打开顶层原理图

#### 步骤 3：连接 Sheet Pin

**重要**: 删除顶层的所有 `global_label`（它们会干扰连接）

然后使用导线连接同名的 Sheet Pin：

| 网络 | 连接源 | 连接目标 |
|------|--------|----------|
| **+5V** | Power.+5V | MCU.+3V3, Motors.+5V, Audio.+5V |
| **+3V3** | Power.+3V3 | MCU.+3V3, Display.+3V3, Audio.+3V3, Sensors.+3V3 |
| **USB_DP** | Power.USB_DP | MCU.USB_DP |
| **USB_DN** | Power.USB_DN | MCU.USB_DN |
| **VBAT_ADC** | Power.VBAT_ADC | MCU.VBAT_ADC |
| **CHRG** | Power.CHRG | MCU.CHRG |
| **STDBY** | Power.STDBY | MCU.STDBY |
| **I2S_BCLK** | MCU.I2S_BCLK | Audio.I2S_BCLK |
| **I2S_WS** | MCU.I2S_WS | Audio.I2S_WS |
| **I2S_DIN** | MCU.I2S_DIN | Audio.I2S_DIN |
| **I2S_DOUT** | MCU.I2S_DOUT | Audio.I2S_DOUT |
| **I2C_SDA** | MCU.I2C_SDA | Sensors.I2C_SDA |
| **I2C_SCL** | MCU.I2C_SCL | Sensors.I2C_SCL |
| **LCD_MOSI** | MCU.LCD_MOSI | Display.LCD_MOSI |
| **LCD_SCLK** | MCU.LCD_SCLK | Display.LCD_SCLK |
| **LCD_CS** | MCU.LCD_CS | Display.LCD_CS |
| **LCD_DC** | MCU.LCD_DC | Display.LCD_DC |
| **LCD_RST** | MCU.LCD_RST | Display.LCD_RST |
| **LCD_BL** | MCU.LCD_BL | Display.LCD_BL |
| **MOTOR_AIN1** | MCU.MOTOR_AIN1 | Motors.MOTOR_AIN1 |
| **MOTOR_AIN2** | MCU.MOTOR_AIN2 | Motors.MOTOR_AIN2 |
| **MOTOR_BIN1** | MCU.MOTOR_BIN1 | Motors.MOTOR_BIN1 |
| **MOTOR_BIN2** | MCU.MOTOR_BIN2 | Motors.MOTOR_BIN2 |
| **IR_OBSTACLE_L** | Sensors.IR_OBSTACLE_L | MCU.IR_OBSTACLE_L |
| **IR_OBSTACLE_R** | Sensors.IR_OBSTACLE_R | MCU.IR_OBSTACLE_R |
| **IR_EDGE_L** | Sensors.IR_EDGE_L | MCU.IR_EDGE_L |
| **IR_EDGE_R** | Sensors.IR_EDGE_R | MCU.IR_EDGE_R |

#### 步骤 4：处理 GND 网络

所有子图的 GND 引脚需要连接到 GND 电源符号：

1. 放置 GND 符号：`Place → Power Symbol` → 搜索 `GND`
2. 将所有子图的 GND 引脚连接到 GND 符号

#### 步骤 5：修复 MCU 引脚重叠问题

**问题**: MCU 有多个引脚在坐标 (210, 80)，导致重叠

**解决**:
1. 打开 `mcu.kicad_sch`
2. 重新排列引脚位置，避免重叠
3. 或者使用总线连接

#### 步骤 6：在 Power 子图添加 PWR_FLAG

打开 `power.kicad_sch`，添加 `PWR_FLAG` 符号：

| 网络 | 位置建议 | 符号库 |
|------|---------|--------|
| VBUS | 靠近 USB-C 连接器 | power:PWR_FLAG |
| +5V | MT3608 输出端 | power:PWR_FLAG |
| +3V3 | AMS1117 输出端 | power:PWR_FLAG |
| GND | 电源入口处 | power:PWR_FLAG |

#### 步骤 7：运行 ERC 检查

1. 在顶层原理图中：`Tools → Electrical Rules Checker`
2. 点击 `Run`
3. 检查结果是否为 **0 errors**

---

### 方案二：使用全局标签替代层次标签（需重新设计）

#### 原理

- 将所有子图的 `hierarchical_label` 改为 `global_label`
- 移除顶层 Sheet Pin 定义
- 相同名称的全局标签自动连接

#### 步骤

1. **在每个子图中**:
   - 打开子图
   - 选择所有层次标签
   - 删除层次标签
   - 添加全局标签（同名）
   - 保存

2. **在顶层原理图中**:
   - 打开顶层
   - 删除所有 Sheet Pin（保留 Sheet 本身）
   - 删除所有全局标签
   - 保存

3. **运行 ERC 检查**

**注意**: 此方法需要重新设计顶层原理图结构

---

## ⚠️ 已知问题

### MCU 引脚重叠

- **位置**: (210, 80)
- **影响引脚**: LCD_RST, IR_EDGE_R, LCD_DC, LCD_BL, MOTOR_BIN1, IR_OBSTACLE_L, IR_OBSTACLE_R, IR_EDGE_L, MOTOR_AIN1, MOTOR_AIN2, MOTOR_BIN2
- **原因**: 原理图设计时未调整引脚位置
- **解决**: 在 `mcu.kicad_sch` 中重新排列引脚位置

---

## 📁 备份文件位置

| 备份类型 | 目录 |
|---------|------|
| 顶层原理图 | `.erc_fix_backups/RobotBuddy.kicad_sch.*` |
| 子图原始版本 | `.label_fix_backups/*.kicad_sch.*` |
| 历史版本 | `.history/` |

---

## 🎯 完成标准

修复完成后应达到：

- [ ] ERC 检查: **0 errors**
- [ ] Warnings: < 20（仅保留可忽略警告）
- [ ] 所有 Sheet Pin 已连接
- [ ] 电源网络有 PWR_FLAG
- [ ] 无引脚重叠

---

## 📚 参考资料

- KiCad 文档: https://docs.kicad.org/
- 层次化设计: https://docs.kicad.org/6.0/en/eeschema/eeschema.html#hierarchical-schematics
- ERC 检查: https://docs.kicad.org/6.0/en/eeschema/eeschema.html#electrical-rules-check

---

**作者**: Claude Code Assistant
**生成时间**: 2026-07-18
**工具版本**: KiCad 10.0.4
