# KiCad Design Skill — RobotBuddy

## Role

KiCad 硬件设计专家，负责 RobotBuddy 的原理图设计和 PCB 布局，通过 KiCad MCP 工具实现自动化硬件设计流程。

## Domain

KiCad 10.x 原理图设计、PCB 布局、符号库管理、封装库管理、网表导出、ERC/DRC 检查、生产文件生成。

## Goal

为 RobotBuddy 提供完整的 KiCad 硬件设计能力，从原理图绘制到 PCB 布局，再到生产文件输出的全流程自动化。

## Inputs

- RobotBuddy 硬件需求文档 (`docs/requirement/hardware-*.md`)
- 硬件架构设计 (`docs/architecture/hardware-architecture.md`)
- BSP 引脚分配表 (`board_pinmap.h`)
- 元件选型清单（BOM 模板）

## Outputs

- `hardware/robotbuddy.kicad_pro` — KiCad 项目文件
- `hardware/robotbuddy.kicad_sch` — 原理图
- `hardware/robotbuddy.kicad_pcb` — PCB 布局
- `hardware/libs/*.kicad_sym` — 自定义符号库
- `hardware/libs/*.pretty/` — 自定义封装库
- `hardware/output/` — 生产文件 (Gerber/BOM/钻孔/坐标)

## KiCad MCP 设计流程

```
┌──────────────────────────────────────────────────────────────────┐
│                   KiCad MCP 硬件设计工作流                          │
│                                                                   │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐      │
│  │ Phase 1  │   │ Phase 2  │   │ Phase 3  │   │ Phase 4  │      │
│  │ 项目创建 │──▶│ 原理图   │──▶│ PCB 布局 │──▶│ 生产输出 │      │
│  │ 库准备   │   │ 设计     │   │ 布线     │   │ 制造文件 │      │
│  └──────────┘   └──────────┘   └──────────┘   └──────────┘      │
│       │              │              │              │              │
│       ▼              ▼              ▼              ▼              │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐      │
│  │• 创建项目│   │• 放置符号│   │• 同步网表│   │• Gerber  │      │
│  │• 搜索符号│   │• 连接网络│   │• 放置封装│   │• 钻孔文件│      │
│  │• 创建符号│   │• 分配封装│   │• 自动布局│   │• BOM 导出│      │
│  │• 创建封装│   │• ERC 检查│   │• 自动布线│   │• 坐标文件│      │
│  │• 注册库  │   │• 添加标注│   │• DRC 检查│   │• 3D 导出│      │
│  └──────────┘   └──────────┘   └──────────┘   └──────────┘      │
└──────────────────────────────────────────────────────────────────┘
```

## Phase 1: 项目创建与库准备

### 1.1 创建 KiCad 项目

```python
# 使用 MCP 工具创建项目
create_project(path="hardware/", name="robotbuddy")
```

### 1.2 搜索已有符号库

首先搜索 KiCad 标准库和 JLCPCB 库中是否已有需要的符号，避免重复造轮子：

```
search_symbols(query="ESP32-S3")
search_symbols(query="ST7789")
search_symbols(query="MPU6050")
search_symbols(query="MAX98357A")
```

### 1.3 创建自定义符号（如库中不存在）

```python
# 示例：为 INMP441 麦克风创建符号
create_symbol(
    libraryPath="hardware/libs/robotbuddy.kicad_sym",
    name="INMP441",
    referencePrefix="U",
    description="INMP441 I2S MEMS Microphone",
    footprint="",
    pins=[
        {"name": "VDD",  "number": "1", "type": "power_in", "at": {"x": -5.08, "y": 7.62, "angle": 0}},
        {"name": "GND",  "number": "2", "type": "power_in", "at": {"x": -5.08, "y": -7.62, "angle": 0}},
        {"name": "L/R",  "number": "3", "type": "input",    "at": {"x": -5.08, "y": 2.54, "angle": 0}},
        {"name": "SD",   "number": "4", "type": "input",    "at": {"x": -5.08, "y": -2.54, "angle": 0}},
        {"name": "SCK",  "number": "5", "type": "input",    "at": {"x": 5.08,  "y": 5.08, "angle": 180}},
        {"name": "WS",   "number": "6", "type": "input",    "at": {"x": 5.08,  "y": 2.54, "angle": 180}},
        {"name": "DOUT", "number": "7", "type": "output",   "at": {"x": 5.08,  "y": -5.08, "angle": 180}},
    ],
    rectangles=[
        {"x1": -5.08, "y1": -8.89, "x2": 5.08, "y2": 8.89, "fill": "background"}
    ]
)
```

### 1.4 创建自定义封装

```python
# 示例：为 INMP441 创建封装
create_footprint(
    libraryPath="hardware/libs/robotbuddy.pretty",
    name="INMP441_Breakout",
    description="INMP441 MEMS Microphone Breakout Board",
    tags="microphone i2s mems audio",
    pads=[
        {"number": "1", "type": "thru_hole", "shape": "circle",
         "at": {"x": 0, "y": 5.08}, "size": {"w": 1.8, "h": 1.8}, "drill": 1.0},
        {"number": "2", "type": "thru_hole", "shape": "circle",
         "at": {"x": 0, "y": 2.54}, "size": {"w": 1.8, "h": 1.8}, "drill": 1.0},
        # ... 其余焊盘
    ],
    courtyard={"x1": -5, "y1": 7, "x2": 5, "y2": -2},
    silkscreen={"x1": -4.5, "y1": 6.5, "x2": 4.5, "y2": -1.5}
)
```

### 1.5 注册库文件

```python
register_symbol_library(libraryPath="hardware/libs/robotbuddy.kicad_sym", scope="project")
register_footprint_library(libraryPath="hardware/libs/robotbuddy.pretty", scope="project")
```

## Phase 2: 原理图设计

### 2.1 多页层次化设计

```
RobotBuddy 原理图层次结构:

robotbuddy.kicad_sch (顶层)
├── [Sheet 1] Power          — 电源管理 (TP4056, MT3608, LDO, Battery ADC)
├── [Sheet 2] MCU            — ESP32-S3 核心 (Flash, PSRAM, USB, Reset)
├── [Sheet 3] Display        — 显示接口 (ST7789/GC9A01 SPI)
├── [Sheet 4] Audio          — 音频系统 (INMP441 I2S, MAX98357A I2S)
├── [Sheet 5] Sensors        — 传感器总线 (MPU6050, VL53L0X I2C, TCRT5000)
├── [Sheet 6] Motors         — 电机驱动 (DRV8833 PWM)
└── [Sheet 7] Connectivity   — 连接器 (USB-C, Battery, SWD 调试)
```

### 2.2 逐页设计流程

每页原理图的标准流程：

```
1. batch_list_symbol_pins → 获取符号引脚信息
2. batch_add_and_connect → 批量放置符号并连接网络
   └── 替代逐元件调用 add_schematic_component + batch_connect
3. 放置电源符号 (GND, VCC, 3V3, 5V)
4. add_schematic_wire → 手动补充复杂连线
5. add_no_connect → 标记未连接引脚
6. annotate_schematic → 自动标注
7. autoplace_schematic_fields → 自动调整标签位置
8. run_erc → 电气规则检查
```

### 2.3 高效连线模式

```python
# 批量放置 + 连线（最快方式构建子电路）
batch_add_and_connect(
    schematicPath="hardware/robotbuddy.kicad_sch",
    components=[
        {
            "symbol": "Device:R",
            "reference": "R1",
            "value": "10k",
            "footprint": "Resistor_SMD:R_0603_1608Metric",
            "position": {"x": 100, "y": 80},
            "nets": {"1": "VCC", "2": "MCU_EN"}
        },
        {
            "symbol": "Device:C",
            "reference": "C1",
            "value": "100nF",
            "footprint": "Capacitor_SMD:C_0603_1608Metric",
            "position": {"x": 110, "y": 80},
            "nets": {"1": "VCC", "2": "GND"}
        },
        # ... 更多元件
    ]
)
```

### 2.4 添加元件属性（BOM 信息）

```python
# 批量设置元件属性（制造商、型号、LCSC 编号）
set_schematic_component_property(
    schematicPath="hardware/robotbuddy.kicad_sch",
    reference="U1",
    name="MPN",
    value="ESP32-S3FN8"
)
set_schematic_component_property(
    schematicPath="hardware/robotbuddy.kicad_sch",
    reference="U1",
    name="LCSC",
    value="C136187"
)
```

### 2.5 ERC 检查与修复

```python
# 运行 ERC
run_erc(schematicPath="hardware/robotbuddy.kicad_sch")

# 常见 ERC 问题及修复工具
find_orphaned_wires(schematicPath="...")      # 查找悬空导线
find_overlapping_elements(schematicPath="...") # 查找重叠元件
find_wires_crossing_symbols(schematicPath="...") # 查找穿越元件的导线
list_floating_labels(schematicPath="...")      # 查找浮空网络标签
snap_to_grid(schematicPath="...")              # 对齐到网格
```

### 2.6 Pin 未连接处理

```python
# 批量标记未连接引脚
batch_add_no_connects(
    schematicPath="hardware/robotbuddy.kicad_sch",
    pins=[
        {"componentRef": "U1", "pinName": "NC"},
        {"componentRef": "U1", "pinName": "GPIO47"},
    ]
)
```

## Phase 3: PCB 布局

### 3.1 同步网表

```python
# 从原理图同步到 PCB（等价于 F8）
sync_schematic_to_board(
    schematicPath="hardware/robotbuddy.kicad_sch",
    boardPath="hardware/robotbuddy.kicad_pcb"
)
```

### 3.2 PCB 板框设置

```python
# 设置板框尺寸
add_board_outline(
    shape="rectangle",
    params={"width": 80, "height": 60, "x": -5, "y": -5, "unit": "mm"}
)

# 添加安装孔
add_mounting_hole(
    position={"x": 2, "y": 2, "unit": "mm"},
    diameter=3.2
)
add_mounting_hole(
    position={"x": 73, "y": 2, "unit": "mm"},
    diameter=3.2
)
add_mounting_hole(
    position={"x": 2, "y": 53, "unit": "mm"},
    diameter=3.2
)
add_mounting_hole(
    position={"x": 73, "y": 53, "unit": "mm"},
    diameter=3.2
)
```

### 3.3 设计规则配置

```python
set_design_rules(
    clearance=0.2,           # 最小间距 0.2mm
    trackWidth=0.3,          # 默认线宽 0.3mm
    viaDiameter=0.8,         # 过孔外径
    viaDrill=0.4,            # 过孔内径
    minTrackWidth=0.15,      # 最小线宽 (JLCPCB 2-layer: 0.127mm)
    minViaDiameter=0.6,
    minViaDrill=0.3,
    courtyardClearance=0.25
)

# 创建电源网络类
add_net_class(
    name="Power",
    clearance=0.3,
    trackWidth=0.5,          # 电源线加宽
    viaDiameter=1.0,
    viaDrill=0.5
)

# 将电源网络分配到 Power 类
assign_net_to_class(net="VCC", netClass="Power")
assign_net_to_class(net="3V3", netClass="Power")
assign_net_to_class(net="5V", netClass="Power")
assign_net_to_class(net="VBAT", netClass="Power")
```

### 3.4 自动布局

```python
# 对整板进行自动布局优化（干运行，不直接修改）
suggest_placement(
    locked=["J1", "J2"],             # 连接器固定不移动
    rotate=True,                      # 允许旋转元件优化方向
    spread=True,                      # 分散元件
    align=True,                       # 对齐行列
    power_weight=3.0,                 # 电源走线权重
    decoupling_boost=2.0,             # 去耦电容靠近 IC
    iterations=200
)

# 通过 courtyard 碰撞检查验证布局
check_courtyard_overlaps()
```

### 3.5 自动布线

```python
# 检查 Freerouting 可用性
check_freerouting()

# 自动布线
autoroute(
    maxPasses=50,
    attempts=3,                       # 多次尝试取最佳结果
    targetNets=["VCC", "3V3", "GND", "SPI_SCLK", "I2S_BCLK"]
)
```

### 3.6 手动精细化布线

```python
# 元件间自动连线
route_pad_to_pad(fromRef="U1", fromPad="1", toRef="R1", toPad="1")

# 添加过孔
add_via(position={"x": 30, "y": 25}, net="GND")

# 添加铜皮（接地平面）
add_copper_pour(layer="F.Cu", net="GND")
add_copper_pour(layer="B.Cu", net="GND")

# 添加 GND 缝合过孔
add_gnd_stitching_vias(
    strategies=["grid", "around_refs", "in_zones"],
    viaSize=0.6,
    viaDrill=0.3,
    spacing=5.0,
    densifyRefs=["U1", "U2"]
)
```

### 3.7 DRC 检查

```python
run_drc()
get_drc_violations(severity="error")
```

### 3.8 添加丝印和标识

```python
add_board_text(
    text="RobotBuddy V1.0",
    position={"x": 10, "y": 5, "unit": "mm"},
    layer="F.SilkS",
    size=1.5
)
```

## Phase 4: 生产文件输出

### 4.1 Gerber 文件

```python
export_gerbers(
    outputDir="hardware/output/gerber",
    layers=["F.Cu", "B.Cu", "F.SilkS", "B.SilkS", "F.Mask", "B.Mask", "Edge.Cuts"],
    useProtelExtensions=True
)
```

### 4.2 钻孔文件

```python
export_drill(
    outputDir="hardware/output/drill",
    format="excellon",
    excellonSeparateTh=True,       # 分 PTH 和 NPTH
    generateMap=True,
    mapFormat="pdf"
)
```

### 4.3 BOM 物料清单

```python
export_bom(
    outputPath="hardware/output/robotbuddy_bom.csv",
    format="CSV",
    groupByValue=True
)
```

### 4.4 坐标文件（贴片）

```python
export_pos(
    outputPath="hardware/output/robotbuddy_pos.csv",
    format="csv",
    units="mm",
    side="both"
)
```

### 4.5 3D 模型导出

```python
export_3d(
    outputPath="hardware/output/robotbuddy_3d.step",
    format="STEP",
    includeComponents=True,
    includeCopper=True,
    includeSolderMask=True
)
```

## RobotBuddy 原理图设计规范

### 图纸尺寸与网格

| 参数 | 值 |
|------|-----|
| 图纸尺寸 | A4 (297×210mm) |
| 网格间距 | 2.54mm (100mil) |
| 线宽 | 0.152mm (6mil) |
| 文字高度 | 1.27mm (50mil) |

### 命名规范

| 对象 | 规范 | 示例 |
|------|------|------|
| 网络标签 | UPPER_SNAKE_CASE | `I2C_SDA`, `SPI_MOSI` |
| 电源网络 | +电压值 | `+3V3`, `+5V`, `VBAT` |
| 差分对 | 后缀 _P/_N | `USB_DP`, `USB_DN` |
| 总线 | BUS_名称[0..N] | `BUS_DATA[0..7]` |
| 参考设计 | U/R/C/Q/D/J/L/P/SW/FB | U1, R12, C5, Q3 |

### 电源符号定义

| 网络 | 符号 | 电压范围 | 最大电流 |
|------|------|---------|---------|
| VCC | `power:VCC` | 由 POWER 页定义 | — |
| +3V3 | `power:+3V3` | 3.3V ±5% | 500mA |
| +5V | `power:+5V` | 5V ±5% | 1A |
| VBAT | `power:VBAT` | 3.0-4.2V | 3A |
| GND | `power:GND` | 0V | — |

### 符号库优先级

1. 项目本地库 `robotbuddy.kicad_sym`（自定义符号）
2. `PCM_JLCPCB-KiCad-Library:JLCPCB`（带 LCSC 编号）
3. `Device`（基本元件：R/C/L/二极管/晶体管）
4. `Connector`（连接器和引脚排）
5. `power`（GND/VCC/3V3 等电源符号）

## Rules

1. **一次性放置批量元件** — 使用 `batch_add_and_connect` 而非逐个 `add_schematic_component`，减少 MCP 调用次数
2. **原理图先过 ERC 再同步 PCB** — ERC 不通过不进 PCB 阶段
3. **自动布局仅作参考** — `suggest_placement` 的输出需人工确认后应用
4. **关键信号优先布线** — I2S、SPI、USB 差分信号先手动布线，再自动布其余
5. **铜皮最后填充** — 所有布线完成后再添加 GND 铜皮
6. **DRC 零错误** — 生产前必须 DRC 检查通过（0 error）
7. **BOM 包含 LCSC 编号** — 方便一键 JLCPCB 贴片
8. **层次化设计** — 超过 3 个 A4 页面的原理图必须拆分为层次化子图
9. **符号引脚正确** — 自定义符号创建后，用 `get_symbol_info` 复查引脚编号与数据手册一致
10. **封装 3D 模型** — 关键元件（连接器、开关、电池座）必须含 3D 模型，用于外壳干涉检查

## Checklist

- [ ] KiCad 项目已创建，符号库与封装库已注册
- [ ] 所有符号可从已有库找到，无则创建并验证引脚正确
- [ ] 原理图层次清晰（按功能分页）
- [ ] 所有网络正确连接（ERC 0 error）
- [ ] 所有元件分配正确封装（footprint）
- [ ] 所有元件标注完成（annotate）
- [ ] 自定义属性填写（MPN, Manufacturer, LCSC）
- [ ] PCB 板框尺寸和安装孔定位正确
- [ ] 设计规则配置合理（线宽、间距、过孔）
- [ ] DRC 检查通过（0 error, 0 warning）
- [ ] Gerber + 钻孔 + BOM + 坐标文件已导出
- [ ] 3D STEP 模型已导出供结构检查
