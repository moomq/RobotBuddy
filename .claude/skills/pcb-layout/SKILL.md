# PCB Layout Skill — RobotBuddy

## Role

PCB 布局布线专家，负责 RobotBuddy PCB 的元件布局、走线优化、信号完整性、电磁兼容性 (EMC) 和热管理设计。

## Domain

PCB 布局布线、EMC/EMI 设计、信号完整性 (SI)、电源完整性 (PI)、热管理、可制造性设计 (DFM)、KiCad 高级 PCB 功能。

## Goal

为 RobotBuddy 设计稳定可靠、可量产、成本优化的 PCB 布局，确保所有模块正常工作和整机 EMC 合规。

## Inputs

- RobotBuddy 原理图 (含网表)
- 机械约束 (外壳 3D 模型、安装孔位)
- BSP 引脚分配表
- 生产规格 (层数、板厚、铜厚、表面处理)

## Outputs

- `hardware/robotbuddy.kicad_pcb` — 完成布局布线的 PCB
- `hardware/output/` — 生产文件 (Gerber/钻孔/BOM/坐标)
- `docs/hardware/pcb-design-report.md` — PCB 设计报告

## PCB 布局流程

```
┌──────────────────────────────────────────────────────────────────┐
│                    PCB Layout 设计流程                             │
│                                                                   │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐      │
│  │ Step 1   │   │ Step 2   │   │ Step 3   │   │ Step 4   │      │
│  │ 规划     │──▶│ 布局     │──▶│ 布线     │──▶│ 验证     │      │
│  │ 层叠     │   │ 分区     │   │ 优化     │   │ 签核     │      │
│  └──────────┘   └──────────┘   └──────────┘   └──────────┘      │
│       │              │              │              │              │
│       ▼              ▼              ▼              ▼              │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐      │
│  │• 板框尺寸│   │• 功能分区│   │• 电源走线│   │• DRC     │      │
│  │• 层叠结构│   │• 关键定位│   │• 信号走线│   │• 3D 碰撞 │      │
│  │• DFM 规则│   │• 去耦电容│   │• 地平面  │   │• 信号检查│      │
│  │• 安装孔  │   │• 自动布局│   │• 自动布线│   │• 热仿真  │      │
│  └──────────┘   └──────────┘   └──────────┘   └──────────┘      │
└──────────────────────────────────────────────────────────────────┘
```

## Step 1: PCB 规划

### 机械规格

| 参数 | 值 |
|------|-----|
| 板面尺寸 | 80×60mm |
| 板厚 | 1.6mm (标准) |
| 层数 | 2 层 (Top/Bottom) |
| 铜厚 | 1oz (35μm) |
| 最小线宽/间距 | 6/6mil (0.15mm) |
| 最小过孔 | 0.3/0.6mm (内径/外径) |
| 表面处理 | HASL 有铅/无铅 (原型)，ENIG (量产) |
| 阻焊颜色 | 绿色/黑色 |
| 丝印颜色 | 白色 |

### 层叠结构

```
┌─────────────────────────────────────┐
│  Top Layer (F.Cu)                   │ ← 信号走线 + 元件 + GND 铜皮
│  ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─  │
│  Prepreg (FR-4)                     │ ← 介电常数 εr ≈ 4.6
│  ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─  │
│  Bottom Layer (B.Cu)                │ ← GND 平面 + 少量信号走线
└─────────────────────────────────────┘
```

### MCP 设置

```python
# 创建 PCB 板框
add_board_outline(
    shape="rounded_rectangle",
    params={"width": 80, "height": 60, "cornerRadius": 3, "x": -5, "y": -5, "unit": "mm"}
)

# 设计规则
set_design_rules(
    clearance=0.2,
    trackWidth=0.3,
    viaDiameter=0.8,
    viaDrill=0.4,
    minTrackWidth=0.15,
    minViaDiameter=0.6,
    minViaDrill=0.3,
    courtyardClearance=0.25
)

# 4 个 M3 安装孔
for pos in [(3,3), (72,3), (3,52), (72,52)]:
    add_mounting_hole(
        position={"x": pos[0], "y": pos[1], "unit": "mm"},
        diameter=3.2,
        padDiameter=6.0
    )
```

### 元件密度评估

```
RobotBuddy V1.0 预估元件数: ~60

功能分区面积占比:
┌──────────────────────┬─────┬──────┬──────┐
│ 功能区               │ 元件│ 面积%│ 位置 │
├──────────────────────┼─────┼──────┼──────┤
│ MCU + Flash + 晶振   │  ~8 │  15% │ 中部 │
│ 电源 (TP4056+MT3608) │ ~12 │  20% │ 左上 │
│ 显示接口 (FPC)       │  ~4 │   8% │ 下中 │
│ 音频 (INMP441+AMP)   │  ~8 │  15% │ 右上 │
│ 传感器 (I2C 总线)    │  ~6 │  10% │ 右中 │
│ 电机驱动 (DRV8833)   │  ~8 │  12% │ 左下 │
│ 红外传感器接口       │  ~6 │  10% │ 下边 │
│ 连接器 (USB+电池)    │  ~4 │   8% │ 左边 │
│ 调试接口             │  ~2 │   2% │ 任意 │
├──────────────────────┼─────┼──────┼──────┤
│ 合计                 │ ~60 │ 100% │      │
└──────────────────────┴─────┴──────┴──────┘
```

## Step 2: 元件布局

### 2.1 功能分区原则

```
         ← 80mm →
    ┌─────────────────────────────┐
    │  ┌─电源区───────────────┐   │
    │  │ TP4056  MT3608       │   │  ↑
    │  │ 电池  LDO  滤波电容  │   │  │
    │  └──────────────────────┘   │  │
    │  ┌─连接器──────────────┐   │  │
    │  │ USB-C 电池座 开关   │   │ 60mm
    │  └──────────────────────┘   │  │
    │  ┌─MCU区─────────────────┐  │  │
    │  │ ESP32-S3  Flash       │  │  │
    │  │ 晶振 去耦电容         │  │  │
    │  └────────────────────────┘  │  │
    │  ┌─传感器──┐ ┌─音频区──┐    │  │
    │  │MPU6050  │ │INMP441  │    │  │
    │  │VL53L0X  │ │MAX98357A│    │  ↓
    │  └─────────┘ └─────────┘    │
    └─────────────────────────────┘
```

### 2.2 关键元件定位规则

```
优先级排列（从高到低）：

1. 连接器（边沿固定）
   - USB-C: 左边中部，距边 2mm
   - 电池座: 左上角
   - LCD FPC: 下边中部
   - 电机连接器: 左下角

2. MCU（中心偏左）
   - ESP32-S3 模组居中偏左，天线区域悬空
   - 晶振紧贴 MCU (≤5mm)
   - 去耦电容紧贴电源引脚 (≤3mm)

3. 电源模块（左上）
   - 大电流路径: 电池→MT3608→LDO→负载
   - 电感远离信号线

4. 音频（右上，远离电源区）
   - I2S 信号远离 DC-DC 电感
   - MIC 有拾音孔位置约束

5. 传感器（右中，远离电机 PWM）
   - I2C 上拉电阻靠近总线
   - MPU6050 需要放置方向标记（丝印）
```

### 2.3 自动布局

```python
# 整板自动布局（干运行，先检查不应用）
suggest_placement(
    locked=["J1", "J2", "J3", "J4"],     # 连接器固定
    rotate=True,
    spread=True,
    align=True,
    power_weight=3.0,
    decoupling_boost=2.0,
    iterations=200,
    apply=False                            # 先检查结果
)

# 审核后应用
check_courtyard_overlaps(positions=result)  # 确认无碰撞
suggest_placement(..., apply=True)          # 确认无误后应用
```

### 2.4 去耦电容布局

```python
# 去耦电容黄金法则：
# 1. 每对 VDD-GND 一个 100nF X7R 电容
# 2. 电容尽量靠近 IC 电源引脚 (≤3mm)
# 3. 电容 GND 端直接打过孔到地平面

# ESP32-S3 去耦要求
# VDD3P3 (3 对): 3× 100nF + 1× 10μF
# VDD_SPI: 1× 100nF
# VDDA: 1× 100nF + 1× 10μF
```

## Step 3: 走线策略

### 3.1 信号分类

| 类型 | 信号 | 走线要求 |
|------|------|---------|
| **高速数字** | SPI (40MHz) | 线宽 0.3mm, 源端串阻 22Ω, 参考地 |
| **模拟敏感** | I2S BCLK/WS/DATA, MIC | 远离 PWM, 包地, 不跨分割 |
| **功率** | VBAT, 5V, 3V3, Motor | 线宽 ≥0.8mm, 多边形填充 |
| **标准数字** | I2C (400kHz), GPIO, UART | 线宽 0.3mm, 常规间距 |
| **差分** | USB D+/D- (90Ω) | 等长 ±0.5mm, 等间距, 参考地 |
| **敏感** | 模拟输入 (VBAT ADC) | 远离干扰源, Kelvin 采样 |

### 3.2 关键信号布线清单

```python
# 1. USB 差分对（优先手动布线）
# 等长约束: 差分对长度差 < 0.5mm
# 阻抗: 90Ω 差分 (USB 2.0 HS)
route_differential_pair(
    positivePad={"reference": "J1", "pad": "D+"},
    negativePad={"reference": "J1", "pad": "D-"},
    layer="F.Cu",
    width=0.3,
    gap=0.15
)

# 2. SPI 高速信号 (ST7789 屏幕)
# 时钟线最短，MOSI 次之，等长目标 5mm 内
route_pad_to_pad(fromRef="U1", fromPad="GPIO36", toRef="J2", toPad="SCLK",
                  layer="F.Cu", width=0.3)

# 3. I2S 音频信号 (抖动敏感)
# 远离 PWM 电机信号，可包地屏蔽
route_pad_to_pad(fromRef="U1", fromPad="GPIO4", toRef="U3", toPad="BCLK",
                  layer="B.Cu", width=0.3)  # 底层走避开上层电机 PWM

# 4. 大电流走线
# 电机: 2A peak → 线宽 ≥1.0mm
route_pad_to_pad(fromRef="U1", fromPad="GPIO10", toRef="U4", toPad="AIN1",
                  layer="F.Cu", width=1.0)
```

### 3.3 走线宽度速查表

```
铜厚 1oz (35μm) 时温升 10°C 的载流能力:

  线宽    最大电流 (外层)    适用场景
  ─────  ────────────────    ────────────
  0.15mm  0.5A               常规信号
  0.3mm   1.0A               通用数字信号
  0.5mm   1.5A               电源分支
  0.8mm   2.5A               3V3 / 5V 主干
  1.0mm   3.0A               电机供电
  1.5mm   4.0A               VBAT 电池
  2.0mm   5.0A               大电流主路

过孔 (0.3/0.6mm) 载流:
  1 个过孔 ≈ 0.8A
  大电流路径需要多个并联过孔
```

### 3.4 自动布线

```python
# 检查 Freerouting 依赖
check_freerouting()

# 自动布线 (多次迭代取最优)
autoroute(
    boardPath="hardware/robotbuddy.kicad_pcb",
    attempts=5,                          # 5 次尝试
    maxPasses=50,
    targetNets=["VCC", "3V3", "GND", "I2S_BCLK"],
    passSchedule=[50, 60, 65, 70, 75, 80, 85, 90, 55, 95]
)
```

### 3.5 地平面设计

```python
# 顶层和底层 GND 铜皮
add_copper_pour(layer="F.Cu", net="GND", clearance=0.3)
add_copper_pour(layer="B.Cu", net="GND", clearance=0.3)

# GND 缝合过孔（减小回路面积、改善 EMI）
add_gnd_stitching_vias(
    strategies=["grid", "around_refs", "in_zones"],
    viaSize=0.6,
    viaDrill=0.3,
    spacing=5.0,
    densifyRefs=["U1", "U2", "U3"],
    edgeMargin=1.0
)
```

## Step 4: 设计验证

### 4.1 DRC 检查

```python
run_drc()
violations = get_drc_violations(severity="error")

# 必须 0 错误才能签核
```

### 4.2 3D 干涉检查

```python
check_courtyard_overlaps(include_boundary=True)

# 导出 3D STEP 供机械设计检查
export_3d(
    outputPath="hardware/output/robotbuddy_3d.step",
    format="STEP",
    includeComponents=True,
    includeCopper=True
)
```

### 4.3 关键检查清单

```markdown
## PCB 签核检查清单

### 机械
- [ ] 板厚与外壳配合正确
- [ ] 安装孔位置与外壳对位
- [ ] 连接器伸出位置无误
- [ ] 按键/开关/指示灯位置可操作
- [ ] 天线区域无铜皮遮挡 (ESP32-S3)

### 电气
- [ ] DRC 0 error, 0 warning
- [ ] 电源网络走线宽度满足载流
- [ ] 去耦电容靠近 IC 电源引脚
- [ ] 晶振负载电容接地良好
- [ ] I2C 上拉电阻在位

### 信号完整性
- [ ] SPI 时钟走线最短
- [ ] I2S 音频信号远离 PWM
- [ ] USB 差分对等长等间距
- [ ] 天线匹配网络完整

### EMI/EMC
- [ ] 地平面完整无割裂
- [ ] GND 缝合过孔密度合理
- [ ] DC-DC 回路面积最小
- [ ] 晶振下无走线

### DFM (可制造性)
- [ ] 最小线宽 ≥6mil
- [ ] 最小钻孔 ≥0.3mm
- [ ] BGA/封装焊盘间距符合 PCB 厂工艺
- [ ] 丝印与阻焊开窗无干涉
- [ ] 元件之间有 3D 间距
- [ ] 无锐角走线
- [ ] 测试点可在背面对位

### 元件
- [ ] 极性元件丝印明确 (二极管/电容/IC)
- [ ] MPU6050 方向标记可见
- [ ] 所有封装与 BOM 一致
- [ ] 无缺料/DNP 元件影响测试
```

## 特殊设计指南

### ESP32-S3 PCB 注意事项

```
1. 天线区域 (模组 PCB 天线端)
   - 天线部分伸出板边或悬空，下方无铜
   - 天线周围 15mm 内避免金属
   - 天线面朝外，远离电池/电机

2. 晶振
   - 40MHz 晶振紧贴 ESP32-S3
   - 负载电容接地独立，不共用地过孔
   - 晶振下方禁止走线

3. 电源去耦
   - VDD3P3 三组独立去耦
   - 大电容 (10μF) 放 IC 近处，小电容 (100nF) 最靠近

4. Flash/PSRAM
   - VDD_SPI 引脚有独立去耦
   - 走线等长
```

### 音频 PCB 注意事项

```
1. 模拟与数字分区
   - MIC 的模拟信号（INMP441 DOUT）是数字 I2S，但 MIC 内部是模拟 MEMS
   - 数字 I2S 走线远离 DC-DC 电感

2. 功放散热
   - MAX98357A 3W 输出 → 需要一定铜皮面积散热
   - 功率地（PGND）与信号地（AGND）单点连接

3. 去耦
   - 功放供电 5V 需要大电容 (≥100μF) + 小电容并联
```

### 电机驱动 PCB 注意事项

```
1. 大电流走线
   - 电机电流: 1A + 堵转 2.5A peak
   - Motor VCC 走线用 2mm 宽或铜皮
   - 每个电机输出至少 2 个并联过孔

2. 续流二极管
   - DRV8833 内置同步整流，不需外部二极管
   - 但大容量电容靠近 Motor VCC 抑制反电势

3. 隔离
   - 电机地与信号地之间用磁珠或 0Ω 隔离（可选）
```

## Rules

1. **天线优先** — 先确定 ESP32-S3 天线位置和无铜区，再安排其他元件
2. **电源后信号** — 先走电源网络，再走信号网络
3. **包地屏蔽** — 敏感信号 (I2S BCLK, ADC 输入) 双面包地
4. **大电流路径最短** — 电池→MT3608→LDO→负载 路径最短、最宽
5. **DRC 零容忍** — 任何 DRC 错误都必须在布线阶段解决
6. **去耦电容就近** — ≤3mm 到 IC 电源引脚
7. **3D 验证必做** — 外壳 3D 模型与 PCB 3D 的干涉检查
8. **测试点预留** — 关键网络 (电源/总线/IO) 预留测试焊盘
9. **丝印清晰** — 元件方向 (Pin1 标记)、连接器功能标注、版本号
10. **DFM 合规** — 最小线宽/间距/钻孔符合所选 PCB 厂商工艺能力

## Checklist

- [ ] 板框尺寸与外壳匹配
- [ ] 安装孔位置正确
- [ ] 功能分区清晰
- [ ] 去耦电容就近放置
- [ ] 关键信号（USB Diff、SPI CLK、I2S BCLK）手动布线
- [ ] 大电流走线宽度足够
- [ ] GND 铜皮完整无割裂
- [ ] GND 缝合过孔密度合理
- [ ] DRC 0 error
- [ ] 3D 干涉检查通过
- [ ] 生产文件已导出验证
