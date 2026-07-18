# RobotBuddy 原理图 ERC 错误修复指南

**日期**: 2026-07-18
**ERC 检查结果**: 143 errors, 99 warnings

## 一、ERC 错误汇总

### 1.1 错误类型统计

| 错误类型 | 数量 | 严重性 | 说明 |
|---------|------|--------|------|
| Label not connected | 77 | Error | 全局标签未连接到网络 |
| Pin not connected | 36 | Error | 层次图纸引脚未连接 |
| Input Power pin not driven | 16 | Error | 电源输入引脚未驱动 |
| Input pin not driven | 14 | Error | 输入引脚未驱动 |

### 1.2 警告类型统计

| 警告类型 | 数量 | 说明 |
|---------|------|------|
| 符号与库存在差异 | 22 | 符号定义与库不一致（可忽略） |
| Label connected to only one pin | 18 | 标签只连接一个引脚（正常） |
| Local and global labels have same name | 17 | 本地和全局标签重名（需修复） |
| 封装未找到 | 5 | 自定义封装路径问题（可忽略） |

---

## 二、修复步骤

### 2.1 打开项目

```bash
# Windows
E:\kicad\bin\kicad.exe "F:\04 code\RobotBuddy\hardware\RobotBuddy.kicad_pro"
```

或在 KiCad 中打开项目文件：
- `F:\04 code\RobotBuddy\hardware\RobotBuddy.kicad_pro`

### 2.2 顶层原理图修复（优先级最高）

**问题**: 子图的 Sheet Pin 未连接，导致信号无法在子图间传递

**修复方法**:

1. **打开顶层原理图** `RobotBuddy.kicad_sch`

2. **连接电源网络**（使用全局标签）:

   在顶层原理图中，使用 "Place → Global Label" 添加以下全局标签，并连接到对应子图的 Sheet Pin：

   | 全局标签名 | 连接的 Sheet Pin |
   |-----------|-----------------|
   | `VBUS` | Power.VBUS (input) |
   | `USB_DP` | Power.USB_DP, MCU.USB_DP |
   | `USB_DN` | Power.USB_DN, MCU.USB_DN |
   | `+5V` | Power.+5V, MCU.+3V3, Motors.+5V, Audio.+5V |
   | `+3V3` | Power.+3V3, MCU.+3V3, Display.+3V3, Audio.+3V3, Sensors.+3V3 |
   | `GND` | 所有子图的 GND 引脚（需要手动连线） |

3. **连接信号网络**（使用全局标签）:

   | 全局标签名 | 连接的 Sheet Pin |
   |-----------|-----------------|
   | `VBAT_ADC` | Power.VBAT_ADC, MCU.VBAT_ADC |
   | `CHRG` | Power.CHRG, MCU.CHRG |
   | `STDBY` | Power.STDBY, MCU.STDBY |
   | `I2C_SDA` | MCU.I2C_SDA, Sensors.I2C_SDA |
   | `I2C_SCL` | MCU.I2C_SCL, Sensors.I2C_SCL |
   | `I2S_BCLK` | MCU.I2S_BCLK, Audio.I2S_BCLK |
   | `I2S_WS` | MCU.I2S_WS, Audio.I2S_WS |
   | `I2S_DIN` | MCU.I2S_DIN, Audio.I2S_DIN |
   | `I2S_DOUT` | MCU.I2S_DOUT, Audio.I2S_DOUT |
   | `LCD_SCLK` | MCU.LCD_SCLK, Display.LCD_SCLK |
   | `LCD_MOSI` | MCU.LCD_MOSI, Display.LCD_MOSI |
   | `LCD_CS` | MCU.LCD_CS, Display.LCD_CS |
   | `LCD_DC` | MCU.LCD_DC, Display.LCD_DC |
   | `LCD_RST` | MCU.LCD_RST, Display.LCD_RST |
   | `LCD_BL` | MCU.LCD_BL, Display.LCD_BL |
   | `MOTOR_AIN1` | MCU.MOTOR_AIN1, Motors.MOTOR_AIN1 |
   | `MOTOR_AIN2` | MCU.MOTOR_AIN2, Motors.MOTOR_AIN2 |
   | `MOTOR_BIN1` | MCU.MOTOR_BIN1, Motors.MOTOR_BIN1 |
   | `MOTOR_BIN2` | MCU.MOTOR_BIN2, Motors.MOTOR_BIN2 |
   | `IR_OBSTACLE_L` | MCU.IR_OBSTACLE_L, Sensors.IR_OBSTACLE_L |
   | `IR_OBSTACLE_R` | MCU.IR_OBSTACLE_R, Sensors.IR_OBSTACLE_R |
   | `IR_EDGE_L` | MCU.IR_EDGE_L, Sensors.IR_EDGE_L |
   | `IR_EDGE_R` | MCU.IR_EDGE_R, Sensors.IR_EDGE_R |

4. **连线方法**:
   - 使用 "Add Wire" 工具（快捷键 `W`）
   - 点击 Sheet Pin 开始连线
   - 到达目标位置后点击完成
   - 双击连线添加全局标签（快捷键 `Ctrl+L`）

### 2.3 Power 子图修复

**问题**: 电源输入引脚未驱动（缺少 PWR_FLAG）

**修复方法**:

1. **打开 Power 子图** `power.kicad_sch`

2. **添加 PWR_FLAG 符号**（从 `power` 库）:

   | 网络 | 位置建议 | PWR_FLAG 参考编号 |
   |------|---------|------------------|
   | `VBUS` | 靠近 USB-C 连接器 | PWR2 |
   | `+5V` | MT3608 输出端 | PWR3 |
   | `+3V3` | AMS1117 输出端 | PWR4 |
   | `GND` | 电源入口处 | PWR5 |

3. **添加方法**:
   - "Place → Symbol" (快捷键 `A`)
   - 搜索 `power:PWR_FLAG`
   - 放置符号
   - 连接引脚到对应电源网络

### 2.4 其他子图修复

#### Display 子图

**问题**: LCD 输入引脚未驱动

**修复**: 这些引脚已在顶层原理图中通过全局标签连接，无需单独修复。

#### Audio 子图

**问题**: I2S 信号未连接

**修复**: 已在顶层原理图中处理。

#### Motors 子图

**问题**: 控制信号未连接

**修复**: 已在顶层原理图中处理。

#### Sensors 子图

**问题**: I2C 和 IR 信号未连接

**修复**: 已在顶层原理图中处理。

---

## 三、验证步骤

### 3.1 运行 ERC

修复完成后，在 KiCad 中：

1. 打开顶层原理图
2. 点击 "Tools → Electrical Rules Checker" (快捷键 `T, E`)
3. 点击 "Run"
4. 检查结果是否为 0 errors

### 3.2 命令行验证

```bash
# Windows PowerShell
& "E:\kicad\bin\kicad-cli.exe" sch erc "F:\04 code\RobotBuddy\hardware\RobotBuddy.kicad_sch"
```

期望输出:
```
未发现违规项
已将 ERC 报告保存到文件 ...
```

---

## 四、常见问题

### Q1: 为什么需要 PWR_FLAG?

**A**: ERC 检查器需要知道电源网络的来源。PWR_FLAG 符号告诉 ERC 这个网络是电源输入点，避免了 "Input Power pin not driven" 错误。

### Q2: 全局标签 vs 层次标签

**A**:
- **全局标签 (Global Label)**: 在整个项目所有层级中可见，用于跨子图连接
- **层次标签 (Hierarchical Label)**: 仅在当前子图和父图之间可见，用于 Sheet Pin 连接

本项目应使用**全局标签**连接不同子图。

### Q3: 符号差异警告可以忽略吗？

**A**: 可以忽略。这些警告是因为自定义符号与 KiCad 标准库定义略有不同，不影响功能。如需消除警告，可以：
- 右键点击符号 → "Update Symbol from Library"
- 或重新创建符号

---

## 五、完成标准

修复完成后，应达到以下标准：

- [ ] ERC 检查: **0 errors**
- [ ] Warnings: < 20 (仅保留符号差异等可忽略警告)
- [ ] 所有子图的 Sheet Pin 已连接
- [ ] 电源网络有 PWR_FLAG
- [ ] 关键信号网络已连通

---

## 六、自动化脚本（可选）

如果需要使用脚本自动生成连线，可以使用以下 KiCad Python API:

```python
# 示例：自动连接 Sheet Pin
import pcbnew
from pcbnew import SCH_EDIT_FRAME

# 获取原理图框架
frame = SCH_EDIT_FRAME()

# 连接 Sheet Pin 到全局标签
# ... (需要编写完整的脚本)
```

**注意**: 由于 KiCad Python API 限制，手动修复是最可靠的方法。

---

## 七、后续工作

ERC 通过后，下一步：

1. **PCB 同步**: "Tools → Update PCB from Schematic" (F8)
2. **DRC 检查**: 在 PCB 编辑器中运行设计规则检查
3. **BOM 导出**: "File → BOM"
4. **Gerber 导出**: "File → Plot"

---

**生成时间**: 2026-07-18
**作者**: Claude Code Assistant
**工具**: KiCad 10.0.4
