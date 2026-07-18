# RobotBuddy 硬件开发指南

> 硬件开发 Skills & Commands 完整使用指南
>
> 适用版本: RobotBuddy V1.0+
> 更新日期: 2026-07-18

---

## 目录

1. [概述](#1-概述)
2. [环境准备](#2-环境准备)
3. [Skills 体系](#3-skills-体系)
4. [Commands 体系](#4-commands-体系)
5. [硬件开发完整流程](#5-硬件开发完整流程)
6. [工作流速查](#6-工作流速查)
7. [常见场景](#7-常见场景)
8. [最佳实践](#8-最佳实践)
9. [故障排除](#9-故障排除)

---

## 1. 概述

### 1.1 什么是硬件开发 Skills & Commands

RobotBuddy 的硬件开发体系包含 **3 个 Skills**（专业知识技能）和 **3 个 Commands**（工作流命令），依托 **KiCad MCP** 工具实现自动化硬件设计：

```
┌─────────────────────────────────────────────────────────────┐
│                  RobotBuddy 硬件开发体系                       │
│                                                              │
│                    ┌──────────────┐                          │
│                    │  Master Skill │  ← 顶层编排             │
│                    └──────┬───────┘                          │
│                           │                                  │
│       ┌───────────────────┼───────────────────┐              │
│       │                   │                   │              │
│       ▼                   ▼                   ▼              │
│  ┌──────────┐       ┌──────────┐       ┌──────────┐         │
│  │kicad-    │       │hardware- │       │pcb-      │         │
│  │design    │       │bom       │       │layout    │         │
│  │ 原理图+  │       │ 物料管理 │       │ 布局布线 │         │
│  │ PCB设计  │       │ 成本优化 │       │ 信号完整性│        │
│  └────┬─────┘       └────┬─────┘       └────┬─────┘         │
│       │                  │                  │                │
│  ┌────┴─────┐       ┌────┴─────┐       ┌────┴─────┐         │
│  │/schematic│       │  /gerber │       │/pcb-     │         │
│  │ 原理图   │       │  生产文件│       │layout    │         │
│  │ 工作流   │       │  工作流  │       │ PCB工作流│         │
│  └──────────┘       └──────────┘       └──────────┘         │
│                                                              │
│  现有支持:                         新增 (本次):              │
│  ├─ /hardware-bringup 硬件调通     ├─ kicad-design skill     │
│  ├─ /pin-check 引脚检查            ├─ hardware-bom skill     │
│  ├─ bsp skill 板级支持             ├─ pcb-layout skill       │
│  └─ hardware-driver skill 驱动     ├─ /schematic command     │
│                                    ├─ /pcb-layout command    │
│                                    └─ /gerber command        │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 与现有体系的关系

| 现有 | 新增 | 关系 |
|------|------|------|
| `hardware-driver` skill | `kicad-design` skill | 驱动开发 vs 硬件设计 |
| `bsp` skill | `pcb-layout` skill | 软件 BSP vs 硬件 PCB |
| `/hardware-bringup` command | `/schematic` command | 硬件调通 vs 原理图设计 |
| `/pin-check` command | `/pcb-layout` command | 引脚冲突检查 vs 物理布局布线 |

---

## 2. 环境准备

### 2.1 必需组件

| 组件 | 版本要求 | 用途 |
|------|---------|------|
| **KiCad** | 10.0+ | PCB 设计主程序 |
| **KiCad MCP** | 最新 | 自动化 PCB 设计接口 |
| **ESP-IDF** | 5.x | 固件开发（配合硬件调试） |

### 2.2 可选组件

| 组件 | 用途 |
|------|------|
| **Java 21+** + **Freerouting** | PCB 自动布线 |
| **JLCPCB 元件数据库** | 离线元件搜索和 BOM 优化 |

### 2.3 环境验证

在开始硬件设计前，验证环境：

```
# 检查 KiCad MCP 状态
→ 验证 backend 正常
→ 验证 KiCad 进程在运行
→ 验证工具列表完整（应 >100 个工具）

# 可选检查
→ check_freerouting 验证自动布线
→ download_jlcpcb_database 下载元件库
```

### 2.4 初始化硬件项目

```
1. create_project → 创建 KiCad 项目
2. register_symbol_library → 注册自定义符号库
3. register_footprint_library → 注册自定义封装库
4. 设置项目变量和设计规则
```

---

## 3. Skills 体系

### 3.1 Skills 总览

```
调用方式: 在对话中自然提及相关任务，或使用 Skill 工具显式调用

  调用示例:
    "帮我设计 RobotBuddy 的电源部分原理图" → 自动调用 kicad-design
    "这个 BOM 的成本能优化吗"           → 自动调用 hardware-bom
    "帮我布局这块 PCB"                  → 自动调用 pcb-layout
```

### 3.2 kicad-design — 原理图与 PCB 设计

**职责**: KiCad 项目创建、符号/封装库管理、原理图设计、PCB 同步

**核心能力**:
- 项目创建与库管理
- 搜索/创建符号和封装
- 层次化原理图设计
- 网络连接与标注
- ERC 检查与修复
- 原理图→PCB 网表同步

**关键 MCP 工具**:

| 工具 | 用途 | 频率 |
|------|------|------|
| `batch_add_and_connect` | 批量放置元件+连线 | ★★★★★ |
| `add_schematic_wire` | 补充复杂连线 | ★★★★ |
| `run_erc` | 电气规则检查 | ★★★★★ |
| `create_symbol` | 创建自定义符号 | ★★★ |
| `create_footprint` | 创建自定义封装 | ★★★ |
| `search_symbols` | 搜索已有符号 | ★★★★ |

**输出物**:
- `.kicad_sch` 原理图文件
- `.kicad_sym` 自定义符号库
- 原理图 PDF
- ERC 通过报告

### 3.3 hardware-bom — BOM 物料管理

**职责**: 元件选型、成本优化、JLCPCB 贴片服务适配、生产物料表生成

**核心能力**:
- 从原理图提取 BOM
- JLCPCB 元件搜索与比价
- Basic/Extended 库优化
- 替代料推荐
- 成本分析与降本建议
- Datasheet URL 自动填充

**关键 MCP 工具**:

| 工具 | 用途 | 频率 |
|------|------|------|
| `search_jlcpcb_parts` | 搜索 JLCPCB 元件 | ★★★★★ |
| `get_jlcpcb_part` | 查看元件详情 | ★★★★ |
| `suggest_jlcpcb_alternatives` | 找替代料 | ★★★ |
| `export_sch_bom` | 导出 BOM | ★★★★★ |
| `set_schematic_component_property` | 添加 LCSC 属性 | ★★★★ |
| `enrich_datasheets` | 自动填充数据手册 | ★★★ |

**输出物**:
- `robotbuddy_bom.csv` — 完整 BOM
- `robotbuddy_bom_cost.md` — 成本分析
- `robotbuddy_bom_alternatives.md` — 替代料表

### 3.4 pcb-layout — PCB 布局布线

**职责**: 元件布局优化、走线策略、信号完整性、EMC 设计、DFM 检查

**核心能力**:
- 功能分区布局
- 自动+手动混合布线
- 电源网络优化
- 地平面设计
- 信号完整性分析
- DRC 检查与修复
- 3D 干涉检查

**关键 MCP 工具**:

| 工具 | 用途 | 频率 |
|------|------|------|
| `sync_schematic_to_board` | 网表同步 | ★★★★★ |
| `suggest_placement` | 自动布局优化 | ★★★★ |
| `route_pad_to_pad` | 元件间布线 | ★★★★★ |
| `autoroute` | 自动布线 | ★★★★ |
| `run_drc` | 设计规则检查 | ★★★★★ |
| `add_gnd_stitching_vias` | GND 缝合过孔 | ★★★ |
| `check_courtyard_overlaps` | 3D 碰撞检查 | ★★★★ |

**输出物**:
- `.kicad_pcb` — 完成布局布线的 PCB
- DRC 通过报告
- 3D 模型 (STEP)

---

## 4. Commands 体系

### 4.1 调用方式

```
直接输入命令名即可触发:

  /schematic  <任务描述>
  /pcb-layout <任务描述>
  /gerber <任务描述>
```

### 4.2 `/schematic` — 原理图设计工作流

**用途**: 创建或修改 KiCad 原理图的 6 阶段完整流程

| 阶段 | 内容 | 关键操作 |
|------|------|---------|
| 1. 项目准备 | 库搜索、符号创建、库注册 | `search_symbols`, `create_symbol` |
| 2. 层次化设计 | 页划分、子图创建 | `create_hierarchical_subsheet` |
| 3. 放置与连线 | 元件放置、网络连接 | `batch_add_and_connect`, `add_schematic_wire` |
| 4. BOM 属性 | LCSC/MPN 属性填充 | `set_schematic_component_property` |
| 5. ERC 修复 | 错误检测与修复 | `run_erc`, `find_orphaned_wires` |
| 6. 生产准备 | PDF 导出、网表生成 | `export_sch_pdf`, `generate_netlist` |

**使用示例**:

```
/schematic 创建 RobotBuddy 的电源管理子图：
- 电池输入 → TP4056 充电 → MT3608 升压 5V → LDO 3.3V
- 电池电压检测 (ADC1_CH0)
- 充电状态指示 (CHRG/STDBY 引脚)
```

### 4.3 `/pcb-layout` — PCB 布局布线工作流

**用途**: 从网表同步到 DRC 通过的 5 阶段完整 PCB 设计流程

| 阶段 | 内容 | 关键操作 |
|------|------|---------|
| 1. 准备与同步 | 网表同步、板框、DR | `sync_schematic_to_board`, `set_design_rules` |
| 2. 元件布局 | 手动定位 + 自动优化 | `suggest_placement` |
| 3. 布线 | 关键信号 + 自动布线 | `route_pad_to_pad`, `autoroute` |
| 4. 验证 | DRC + 3D 干涉 | `run_drc`, `check_courtyard_overlaps` |
| 5. 生产输出 | Gerber/BOM/坐标导出 | `export_gerbers`, `export_pos` |

**使用示例**:

```
/pcb-layout 初始布局 RobotBuddy PCB：
- 板框 80×60mm, 1.6mm 厚, 2 层, 1oz
- 4 个 M3 安装孔, 连接器边沿固定
- MCU 居中, 天线区悬空
```

### 4.4 `/gerber` — 生产文件生成

**用途**: 生成完整制造文件包并进行投板前验证

| 阶段 | 内容 | 关键操作 |
|------|------|---------|
| 1. 最终检查 | DRC+ERC 复查、2D 预览 | `get_board_2d_view` |
| 2. Gerber | 各层导出 | `export_gerbers` |
| 3. 钻孔 | PTH/NPTH 分离 | `export_drill` |
| 4. BOM+坐标 | 物料和贴片数据 | `export_bom`, `export_pos` |
| 5. 3D+文档 | STEP/PDF/SVG | `export_3d` |
| 6. 投板检查 | 完整性验证 | 打包 ZIP |

**使用示例**:

```
/gerber 生成 RobotBuddy V1.0 生产文件：
- 目标: JLCPCB 2 层, 1oz, HASL, 绿色
- 打包输出到 hardware/output/
```

---

## 5. 硬件开发完整流程

### 5.1 端到端流程

```
                        硬件开发完整流程
                        
  ┌──────────────────────────────────────────────────────────┐
  │ 软件侧                            硬件侧                  │
  │                                                          │
  │ 1. 需求分析                      2. 元件选型             │
  │    requirement skill ──────────▶ hardware-bom skill      │
  │    (功能定义)                      (BOM 初版)            │
  │                                                          │
  │ 3. 原理图设计                    4. BOM 优化             │
  │    kicad-design skill ──────────▶ hardware-bom skill     │
  │    /schematic command              (成本分析)            │
  │                                                          │
  │ 5. PCB 布局                      6. 设计验证             │
  │    pcb-layout skill ────────────▶ DRC + 3D              │
  │    /pcb-layout command                                      │
  │                                                          │
  │ 7. 生产文件                                               │
  │    /gerber command ─────────────▶ 投板                   │
  │                                                          │
  │ 8. BSP + 驱动开发                 9. 硬件调试             │
  │    bsp skill +                     /hardware-bringup     │
  │    hardware-driver skill           command               │
  │                                                          │
  │ 10. 固件集成                     11. 量产 BOM            │
  │     firmware build ─────────────▶ hardware-bom skill     │
  └──────────────────────────────────────────────────────────┘
```

### 5.2 各阶段分工

| 阶段 | 负责 Skill/Command | 产出物 |
|------|-------------------|--------|
| 需求分析 | `requirement` skill | 硬件需求文档 |
| 元件选型 | `hardware-bom` skill | 初版 BOM |
| 原理图 | `kicad-design` + `/schematic` | `.kicad_sch` (ERC OK) |
| BOM 定稿 | `hardware-bom` skill | 终版 BOM + 成本报告 |
| PCB 布局 | `pcb-layout` + `/pcb-layout` | `.kicad_pcb` (DRC OK) |
| 生产输出 | `/gerber` | Gerber + 钻孔 + BOM + 坐标 |
| 驱动开发 | `hardware-driver` skill | 驱动代码 |
| BSP 集成 | `bsp` skill | 板级初始化 + 自检 |
| 硬件调通 | `/hardware-bringup` | 硬件测试报告 |

---

## 6. 工作流速查

### 6.1 快速参考卡

```
┌─────────────────────────────────────────────────────────────────┐
│                    RobotBuddy 硬件开发速查卡                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  📐 原理图设计                                                   │
│  ─────────────                                                  │
│  新建项目:  "用 kicad-design 创建 RobotBuddy 的 KiCad 项目"       │
│  画原理图:  /schematic <描述你的电路>                             │
│  加 BOM 属性: "给原理图所有元件加上 LCSC 编号"                   │
│  检查原理图: /schematic 执行 ERC 检查并修复                       │
│                                                                  │
│  📦 BOM 管理                                                    │
│  ─────────────                                                  │
│  搜索元件:  "在 JLCPCB 上搜索 10k 0603 电阻 Basic 库"            │
│  看成本:    "分析 RobotBuddy BOM 成本并给出降本建议"              │
│  找替代料:  "C25804 库存不足，有什么可以替代的"                   │
│  出 BOM:    /gerber 导出 BOM                                     │
│                                                                  │
│  🖥️ PCB 设计                                                    │
│  ─────────────                                                  │
│  开始布局:  /pcb-layout 从原理图同步并开始布局                    │
│  自动布线:  /pcb-layout 执行自动布线                              │
│  检查 DRC:  /pcb-layout 运行 DRC 并修复                          │
│  手动布线:  "把 U1 的 GPIO36 连到 J2 的 SCLK, 线宽 0.3mm"       │
│                                                                  │
│  🏭 生产输出                                                    │
│  ─────────────                                                  │
│  生成文件:  /gerber 生成完整生产文件                              │
│  投板检查:  /gerber 执行投板前最终验证                            │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 6.2 常用 MCP 工具速查

| 我想... | 用这个工具 | 示例 |
|---------|-----------|------|
| 一次放很多元件+连线 | `batch_add_and_connect` | 放置 MCU 外围电路 |
| 改元件的封装 | `edit_schematic_component` | C1 从 0603 改 0805 |
| 连接元件引脚 | `batch_connect` | MCU GPIO → LED |
| 画一条连线 | `add_schematic_wire` | 复杂跨页连线 |
| 看引脚在哪 | `get_schematic_pin_locations` | 布线前确认坐标 |
| 自动标号 | `annotate_schematic` | R?→R1, R2... |
| 检查错误 | `run_erc` | 原理图完成后 |
| 看原理图长啥样 | `get_schematic_view` | 导出 PNG 查看 |
| 网表同步到 PCB | `sync_schematic_to_board` | 原理图→PCB |
| 自动摆元件 | `suggest_placement` | 自动布局 |
| 两个元件之间走线 | `route_pad_to_pad` | U1.1→R1.1 |
| 自动走完所有线 | `autoroute` | ⚡ 需要 Java |
| 检查 PCB 错误 | `run_drc` | PCB 完成后 |
| 加 GND 铜皮 | `add_copper_pour` | 整板铺地 |
| 出制造文件 | `export_gerbers` | 准备投板 |
| 元件多少钱 | `search_jlcpcb_parts` | 选型时查价 |

---

## 7. 常见场景

### 7.1 新增一个传感器模块

```
场景: 要在 RobotBuddy 上增加 VL53L0X ToF 传感器

完整步骤:

1. 需求确认
   "用 requirement skill 分析 VL53L0X 接入需求"

2. 引脚分配
   "用 bsp skill 为 VL53L0X 分配 I2C 地址和 GPIO"
   (VL53L0X 可共享现有 I2C0 总线，需额外 XSHUT 引脚)

3. 选型搜索
   "在 JLCPCB 搜索 VL53L0X 模块和必要外围元件"
   → hardware-bom skill 自动搜索

4. 原理图修改
   /schematic 在 Sensors 子图添加 VL53L0X：
   - I2C 挂到现有 I2C0 总线 (SDA=GPIO8, SCL=GPIO9)
   - XSHUT 接 GPIO_XX
   - 添加 2.2kΩ I2C 上拉, 100nF 去耦

5. BOM 更新
   "更新 BOM，加入 VL53L0X 相关元件"

6. PCB 调整
   /pcb-layout 在传感器区域放置 VL53L0X
   → 重新布局 + 补走线

7. 生产文件
   /gerber 生成新的生产文件

8. 驱动开发
   "用 hardware-driver skill 开发 VL53L0X 驱动"
```

### 7.2 改版 PCB（修复设计缺陷）

```
场景: ST7789 屏幕初始化不稳定，怀疑 SPI 走线过长

诊断+修复流程:

1. 引脚检查
   /pin-check 确认 SPI 引脚分配无冲突

2. PCB 审查
   /pcb-layout 检查 SPI 走线：
   - 测量 SCLK 走线长度
   - 检查是否跨分割平面
   - 查看是否有邻近干扰源

3. 布线优化
   "缩短 SPI SCLK 走线，加串阻 22Ω，包地处理"

4. 重新验证
   /pcb-layout 运行 DRC，确认 0 error

5. 生产文件
   /gerber 生成 V1.1 生产文件（改版）
```

### 7.3 成本优化（降本）

```
场景: RobotBuddy 单套 BOM 成本 $18，目标降到 $12

优化流程:

1. 成本分析
   "分析 RobotBuddy BOM 成本构成，找出 TOP10 高价元件"
   → hardware-bom skill

2. 逐项优化
   "对 TOP10 高成本元件逐一找替代料"
   ├── MCU: ESP32-S3FN8 → ESP32-S3R8 (PSRAM 不同)
   ├── LCD: 品牌屏 → 兼容屏
   ├── 连接器: 进口 → 国产替代
   └── 阻容感: Extended → Basic 库

3. 贴片费优化
   "统计 Extended 料种类，合并到 ≤10 种"

4. 更新 BOM
   "生成优化后的 BOM 和成本对比报告"

5. 更新原理图
   /schematic 更新元件封装和值（与 BOM 一致）
```

### 7.4 新项目快速启动

```
场景: 从零开始创建 RobotBuddy 的 KiCad 项目

一键启动:

1. "用 kicad-design skill 创建 RobotBuddy KiCad 项目"
   → 创建项目 + 注册库 + 搜索可用符号

2. "搜索 RobotBuddy 需要的所有核心元件符号"
   → ESP32-S3, ST7789, MPU6050, DRV8833, INMP441, MAX98357A, TP4056

3. /schematic 按层次化结构创建 7 个子图

4. "为缺失的元件创建自定义符号和封装"

5. "给所有元件添加 JLCPCB 属性"
```

---

## 8. 最佳实践

### 8.1 设计原则

| 原则 | 说明 | 检查方法 |
|------|------|---------|
| **先搜索再创建** | 标准库或 JLCPCB 库可能已有符号 | `search_symbols` |
| **批量优于逐个** | 使用 `batch_*` 工具减少 MCP 调用 | 对比效率 |
| **ERC 不过不进 PCB** | 原理图错误不带到 PCB | `run_erc` |
| **DRC 零容忍** | 任何 DRC error 必须修复 | `run_drc` |
| **Basic 优先** | 阻容感全用 JLCPCB Basic 库 | BOM Library Type |
| **信号分类布线** | 高速/功率/模拟走线策略不同 | PCB 规则 |
| **3D 验证必须** | 外壳干涉检查不能少 | `export_3d` |

### 8.2 效率技巧

```
1. 并行操作
   - 多个独立子图可以同时编辑
   - BOM 属性填充可以在原理图画完后批量执行

2. 模板复用
   - 保存常见的子电路为独立子图（如电源模块）
   - 项目间可复制自定义符号库

3. 检查点保存
   - 每个阶段完成后导出 PDF 归档
   - 重要版本 Git tag

4. 自动化检查
   - 每次原理图修改后运行 ERC
   - 每次 PCB 修改后运行 DRC
   - 生成文件前跑完整的检查清单
```

### 8.3 文件管理

```
hardware/
├── robotbuddy.kicad_pro          ← KiCad 项目
├── robotbuddy.kicad_sch          ← 顶层原理图
├── robotbuddy.kicad_pcb          ← PCB 布局
├── sheets/                       ← 子图目录
│   ├── power.kicad_sch
│   ├── mcu.kicad_sch
│   ├── display.kicad_sch
│   ├── audio.kicad_sch
│   ├── sensors.kicad_sch
│   ├── motors.kicad_sch
│   └── connectivity.kicad_sch
├── libs/                         ← 自定义库
│   ├── robotbuddy.kicad_sym      ← 符号库
│   └── robotbuddy.pretty/        ← 封装库
├── models/                       ← 3D 模型
│   └── robotbuddy.3dshapes/
├── output/                       ← 生产文件
│   ├── gerber/
│   ├── drill/
│   ├── bom/
│   ├── pos/
│   ├── 3d/
│   └── doc/
├── datasheets/                   ← 数据手册归档
└── docs/                         ← 设计文档
    ├── hardware-architecture.md
    ├── hardware-schematic.md
    ├── pcb-design-report.md
    └── bom-cost-analysis.md
```

---

## 9. 故障排除

### 9.1 常见问题

| 问题 | 原因 | 解决 |
|------|------|------|
| 符号找不到 | 库未注册或搜索词不匹配 | `register_symbol_library` + `list_symbol_libraries` |
| ERC 误报 "not connected" | 网格不对齐 | `snap_to_grid` 2.54mm |
| 自动布局元件挤一起 | spread/align 参数不够 | 增加 `iterations` + `spread=True` |
| 自动布线失败 | Freerouting 未装 | `check_freerouting` → 安装 Java |
| DRC 间距违规 | 封装不匹配或铜皮间距 | 检查 Pad→Copper clearance |
| BOM 行数与坐标不匹配 | 有元件在 BOM 中排除但仍在 PCB 上 | 检查 `exclude_from_bom` |

### 9.2 ERC 错误速修

| ERC 错误类型 | 工具 | 操作 |
|-------------|------|------|
| Pin not connected | `batch_add_no_connects` | 标记未使用引脚 |
| Wire end unconnected | `find_orphaned_wires` | 删除/补连悬空导线 |
| Conflicting net names | `get_schematic_view` | 检查标签与连线冲突 |
| Power pin not driven | `add_schematic_net_label` | 添加 PWR_FLAG |
| No connection to net | `list_floating_labels` | 修复浮空标签 |

### 9.3 DRC 错误速修

| DRC 错误类型 | 工具 | 操作 |
|-------------|------|------|
| Clearance violation | `route_pad_to_pad` | 重走并加大间距 |
| Track too thin | `modify_trace` | 加宽走线 |
| Via too small | `set_design_rules` | 调整过孔规则 |
| Courtyard overlap | `move_component` | 移动元件 |
| Unconnected items | `route_pad_to_pad` | 补全走线 |

### 9.4 快速诊断命令

```
# 查看当前 KiCad MCP 状态
get_backend_state

# 查看当前 PCB 状态
get_board_info

# 查看设计规则
get_design_rules

# 查看 DRC 违规
get_drc_violations(severity="all")

# 查看 PCB 层列表
get_layer_list

# 2D 预览 PCB
get_board_2d_view(layers=["F.Cu","B.Cu","Edge.Cuts","F.SilkS"])

# 3D 预览 PCB
export_3d(outputPath="preview.step", format="STEP")
```

---

## 附录 A: 文件清单

### 新增 Skills（3个）

| Skill | 文件 |
|-------|------|
| `kicad-design` | `.claude/skills/kicad-design/SKILL.md` |
| `hardware-bom` | `.claude/skills/hardware-bom/SKILL.md` |
| `pcb-layout` | `.claude/skills/pcb-layout/SKILL.md` |

### 新增 Commands（3个）

| Command | 文件 |
|---------|------|
| `/schematic` | `.claude/commands/schematic.md` |
| `/pcb-layout` | `.claude/commands/pcb-layout.md` |
| `/gerber` | `.claude/commands/gerber.md` |

### 现有相关文件（不变）

| 文件 | 说明 |
|------|------|
| `.claude/skills/hardware-driver/SKILL.md` | 硬件驱动开发 |
| `.claude/skills/bsp/SKILL.md` | 板级支持包 |
| `.claude/commands/pin-check.md` | 引脚冲突检查 |
| `.claude/commands/hardware-bringup.md` | 硬件模块调通 |

---

## 附录 B: 术语表

| 术语 | 全称 | 说明 |
|------|------|------|
| BOM | Bill of Materials | 物料清单 |
| CPL | Component Placement List | 贴片坐标文件 |
| DFM | Design for Manufacturing | 可制造性设计 |
| DRC | Design Rule Check | 设计规则检查 |
| ERC | Electrical Rules Check | 电气规则检查 |
| HASL | Hot Air Solder Leveling | 热风整平焊料（表面处理） |
| ENIG | Electroless Nickel Immersion Gold | 化学镍金（表面处理） |
| JLCPCB | — | 嘉立创 PCB 制造/贴片服务 |
| LCSC | — | 立创商城（元件采购） |
| PTH | Plated Through Hole | 金属化通孔 |
| NPTH | Non-Plated Through Hole | 非金属化通孔 |
| MCU | Microcontroller Unit | 微控制器 (ESP32-S3) |
| MPN | Manufacturer Part Number | 制造商型号 |

---

> 本文档随 RobotBuddy 项目迭代更新。如有疑问或改进建议，请在项目仓库提 Issue。
