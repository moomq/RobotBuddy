# RobotBuddy 原理图自动连线方案

## 当前问题

- ✅ 全局标签已存在（17个）
- ❌ Sheet Pin 未连接到全局标签
- ❌ 导致 143 个 ERC 错误

## 自动修复策略

我将为您生成完整的 S-expression 连线代码，可以直接插入到原理图文件中。

### 需要连接的网络分组

1. **电源网络** (4个)
   - VBUS, +5V, +3V3, GND

2. **USB数据线** (2个)
   - USB_DP, USB_DN

3. **充电状态** (3个)
   - CHRG, STDBY, VBAT_ADC

4. **I2C总线** (2个)
   - I2C_SDA, I2C_SCL

5. **I2S音频** (4个)
   - I2S_BCLK, I2S_WS, I2S_DIN, I2S_DOUT

6. **LCD显示** (6个)
   - LCD_MOSI, LCD_SCLK, LCD_CS, LCD_DC, LCD_RST, LCD_BL

7. **电机控制** (4个)
   - MOTOR_AIN1, MOTOR_AIN2, MOTOR_BIN1, MOTOR_BIN2

8. **红外传感器** (4个)
   - IR_OBSTACLE_L, IR_OBSTACLE_R, IR_EDGE_L, IR_EDGE_R

**总计**: 29 个网络需要连接

## 修复方法

### 方案 A：在 KiCad 中使用 Python 控制台（推荐）

在 KiCad 原理图编辑器中：

1. 打开 `Tools → Scripting Console`
2. 运行我生成的 Python 脚本
3. 自动完成所有连线

### 方案 B：我直接修改原理图文件

我可以直接编辑 `.kicad_sch` 文件，添加连线代码。

---

**您希望我执行哪个方案？**

请告诉我，我会立即帮您自动修复！
