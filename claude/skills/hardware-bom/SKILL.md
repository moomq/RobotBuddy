# Hardware BOM Skill — RobotBuddy

## Role

RobotBuddy BOM 物料管理专家，负责元件选型、成本优化、替代料分析和生产物料表生成。

## Domain

电子元器件选型、JLCPCB 贴片服务、BOM 成本优化、LCSC 元件数据库、供应链管理、PCB 可制造性。

## Goal

为 RobotBuddy 提供最优化的元件选型和完整物料清单，确保成本可控、交期可预期、质量可追溯。

## Inputs

- RobotBuddy 原理图 (KiCad `.kicad_sch`)
- 目标成本预算
- 生产数量级别（原型 5pcs / 小批量 100pcs / 量产 1kpcs+）
- 元件约束（贴片优先、Basic 库优先）

## Outputs

- `hardware/output/robotbuddy_bom.csv` — 完整 BOM 表
- `hardware/output/robotbuddy_bom_cost.md` — 成本分析报告
- `hardware/output/robotbuddy_bom_alternatives.md` — 替代料建议

## BOM 工作流

```
┌──────────────────────────────────────────────────────────────────┐
│                    BOM 物料管理工作流                              │
│                                                                   │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐      │
│  │ Step 1   │   │ Step 2   │   │ Step 3   │   │ Step 4   │      │
│  │ 提取     │──▶│ 选型     │──▶│ 成本     │──▶│ 输出     │      │
│  │ BOM      │   │ 优化     │   │ 分析     │   │ 产线     │      │
│  └──────────┘   └──────────┘   └──────────┘   └──────────┘      │
│       │              │              │              │              │
│       ▼              ▼              ▼              ▼              │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐      │
│  │•导出 KiCad│  │•JLCPCB搜索│  │•单价汇总 │   │•CSV/JSON │      │
│  │ BOM      │   │•Basic优先 │  │•贴片费   │   │•备料清单 │      │
│  │•提取 LCSC│   │•替代料   │   │•扩展料费 │   │•成本报告 │      │
│  │•分类元件 │   │•库存检查 │   │•焊接建议 │   │•替代方案 │      │
│  └──────────┘   └──────────┘   └──────────┘   └──────────┘      │
└──────────────────────────────────────────────────────────────────┘
```

## Step 1: 提取 BOM

### 从原理图导出 BOM

```python
# 从原理图导出结构化 BOM
export_sch_bom(
    schematicPath="hardware/robotbuddy.kicad_sch",
    outputPath="hardware/output/robotbuddy_bom_raw.csv",
    fields="Reference,Value,Footprint,LCSC,MPN,Manufacturer",
    groupBy="Value,Footprint",
    excludeDnp=True,
    includeExcludedFromBom=False
)
```

### BOM 解析与分类

```markdown
元件分类：
├── 核心 IC（MCU、Sensor、Motor Driver）→ 选型核心指标
├── 阻容感（R/C/L）                    → 封装统一、Basic 优先
├── 连接器（排针、Type-C、电池座）      → 机械配合、3D 验证
├── 开关/按键/指示灯                   → 手感、亮度、寿命
├── PCB 板                             → 层数、厚度、颜色、工艺
└── 结构件（螺丝、铜柱、外壳）          → 公差、强度
```

## Step 2: 选型优化

### JLCPCB 元件数据库搜索

JLCPCB 元件分三级：

| 类型 | 名称 | 贴片费 | 说明 |
|------|------|--------|------|
| **Basic** | 基础库 | **免费** | ~30 万种，优先使用 |
| **Preferred Extended** | 优惠扩展库 | **$3/种** | 额外 ~25 万种 |
| **Standard Extended** | 标准扩展库 | **$3/种** | 全部可贴 |

### 搜索策略

```python
# 1. 先搜索是否有完全匹配
search_jlcpcb_parts(query="10k 0603 1%", package="0603", library_type="Basic")

# 2. 无 Basic 则搜 Extended + 选最优价
search_jlcpcb_parts(query="10k 0603 1%", package="0603", library_type="Extended")

# 3. 查看元件详情
get_jlcpcb_part(lcsc_number="C25804")

# 4. 寻找替代料
suggest_jlcpcb_alternatives(lcsc_number="C25804", limit=5)
```

### 选型原则

```
优先级排序：
1. Basic 库可贴片 (免贴片费)
2. Preferred Extended (贴片费 $3)
3. 库存充足 (>1000pcs)
4. 价格合理
5. 符合规格要求
```

### 阻容感标准化策略

```markdown
## RobotBuddy 阻容感标准封装

| 类型 | 封装 | 功率/电压 | 推荐品牌 | 说明 |
|------|------|----------|---------|------|
| 电阻 | 0603 | 1/10W | Yageo | 通用阻值 10Ω-100kΩ |
| 电阻 | 0805 | 1/8W | Yageo | 大功率/电机电流检测 |
| 电容(C0G) | 0603 | 50V | Murata/Samsung | 高频去耦 (1pF-1nF) |
| 电容(X7R) | 0603 | 25V | Murata/Samsung | 去耦 (1nF-10μF) |
| 电容(X5R) | 0805 | 16V | Murata/Samsung | 大容量 (10μF-47μF) |
| 磁珠 | 0603 | 100-600Ω | TDK | EMI 抑制 |
| 电感 | CD54 | 饱和电流 | — | 电机/电源滤波 |

## 标准阻容值表

推荐使用 E12/E24 系列标准值:

电阻 (Ω): 10, 12, 15, 18, 22, 27, 33, 39, 47, 56, 68, 82
          100, 120, 150, 180, 220, 270, 330, 390, 470, 560, 680, 820
          1k, 1.2k, 1.5k, 1.8k, 2.2k, 2.7k, 3.3k, 3.9k, 4.7k, 5.6k, 6.8k, 8.2k
          10k, 12k, 15k, 18k, 22k, 27k, 33k, 39k, 47k, 56k, 68k, 82k
          100k, 120k, 150k, 180k, 220k, 270k, 330k, 390k, 470k, 560k, 680k, 820k
          1M

电容 (典型): 10pF, 22pF, 33pF, 100pF, 1nF, 10nF, 22nF, 33nF, 47nF, 100nF,
             220nF, 470nF, 1μF, 2.2μF, 4.7μF, 10μF, 22μF, 47μF, 100μF
```

## Step 3: 成本分析

### 贴片费用计算

```markdown
## JLCPCB 贴片成本公式

总成本 = PCB 成本 + 元件成本 + 贴片费 + 焊接费 + 运费

1. PCB 成本 (100×100mm 以内, 2 层, 5pcs): $2/pcb (特价)
2. 元件成本: 单价 × 数量 × (1 + 损耗率 5%)
3. 贴片费:
   - Basic 料: $0
   - Extended 料: $3/种
4. 焊接费: 延展费率 $0.0017/焊点
5. 运费: 约 $7-15 (按地区不同)
```

### RobotBuddy 成本估算示例

```markdown
## RobotBuddy V1.0 成本估算 (5pcs 原型)

### 核心元件
| 参考 | 元件 | LCSC | 单价 | 数量 | 小计 |
|------|------|------|------|------|------|
| U1 | ESP32-S3FN8 | C136187 | $2.80 | 5 | $14.00 |
| U2 | MPU6050 | C24112 | $1.50 | 5 | $7.50 |
| U3 | DRV8833 | C39147 | $0.85 | 5 | $4.25 |
| U4 | TP4056 | C16581 | $0.12 | 5 | $0.60 |
| U5 | MT3608 | C84818 | $0.15 | 5 | $0.75 |
| J1 | LCD FPC 8P | C180206 | $0.08 | 5 | $0.40 |
| — | ST7789 LCD | C51782 | $2.50 | 5 | $12.50 |

### 分类汇总
| 类别 | 种类数 | Extended 数 | 元件费 | 贴片费 |
|------|--------|------------|--------|--------|
| 核心 IC | 7 | 1 | $40.00 | $3 |
| 阻容感 | 18 | 3 | $2.50 | $9 |
| 连接器 | 4 | 1 | $3.20 | $3 |
| 开关/LED | 3 | 0 | $0.60 | $0 |
| **合计** | **32** | **5** | **$46.30** | **$15** |

总成本: $46.30 (元件) + $15 (贴片费) + $10 (PCB) + $10 (运费) = **$81.30 / 5pcs**
单套: **$16.26**
```

### 降本建议

| 措施 | 节省 | 影响 |
|------|------|------|
| 阻容感全用 Basic | -$9 贴片费 | 部分非标值需调整 |
| 减少扩展料 <5 种 | -$15 贴片费 | 需调整选型 |
| 拼板 2in1 | -$5 PCB | 需要 V-Cut 或邮票孔 |
| 1.6mm → 1.0mm | -$2 PCB | 结构强度略降 |
| 单面贴片 | -$8 贴片费 | 布线难度增加 |
| 下单前检查是否为优惠期 | 可达 -$20 | 关注 JLCPCB 活动 |

## Step 4: BOM 输出

### 各格式 BOM

```python
# CSV 格式（JLCPCB 标准格式）
export_bom(outputPath="hardware/output/robotbuddy_bom.csv", format="CSV")

# JSON 格式（自动导入 ERP）
export_bom(outputPath="hardware/output/robotbuddy_bom.json", format="JSON")

# HTML 格式（人工审核）
export_bom(outputPath="hardware/output/robotbuddy_bom.html", format="HTML")
```

### 标准 BOM 字段

```markdown
| 字段 | 说明 | 示例 |
|------|------|------|
| Reference | 参考设计号 | U1, R5, C12 |
| Value | 元件值/型号 | 10k, 100nF, ESP32-S3 |
| Footprint | KiCad 封装 | Resistor_SMD:R_0603 |
| LCSC | LCSC 编号 | C25804 |
| MPN | 制造商型号 | RC0603FR-0710KL |
| Manufacturer | 制造商 | Yageo |
| Qty | 数量 | 5 |
| Unit Price | 单价 (USD) | 0.002 |
| Extended Price | 总价 | 0.010 |
| Library Type | 库类型 | Basic / Extended |
| Stock | 库存 | 45230 |
| Datasheet | 数据手册 URL | https://... |
| Notes | 备注 | 可代用 C12345 |
```

### 元件库类型标注

在原理图中为元件添加 LCSC 属性，方便 BOM 直接导出：

```python
# 批量添加 JLCPCB 属性
set_schematic_component_property(
    schematicPath="hardware/robotbuddy.kicad_sch",
    reference="R1",
    name="LCSC",
    value="C25804",       # 10k 0603 1%
    hide=True
)
set_schematic_component_property(
    schematicPath="hardware/robotbuddy.kicad_sch",
    reference="R1",
    name="Manufacturer",
    value="Yageo",
    hide=True
)

# 自动填充 Datasheet URL
enrich_datasheets(schematic_path="hardware/robotbuddy.kicad_sch")
```

## 元件到货检查工作流

```markdown
## 收到 JLCPCB 贴片板后的检查清单

### 目视检查
- [ ] PCB 无刮伤、变形
- [ ] 丝印清晰、无偏移
- [ ] 焊点光亮、无桥接/虚焊
- [ ] 所有元件在位（对照 BOM 点料）

### 上电测试
- [ ] 3V3 电压正常 (3.14–3.47V)
- [ ] 5V 电压正常 (4.75–5.25V)
- [ ] 短路测试（VCC 对 GND 阻值 >1kΩ）
- [ ] 各模块 I2C 扫描通过
- [ ] 烧录测试固件 → 运行自检

### 不良品处理
- [ ] 拍照记录不良现象
- [ ] 与 BOM 核对是否用错料
- [ ] 与 Gerber 核对是否为 PCB 制造缺陷
- [ ] 联系 JLCPCB 售后（品质问题可索赔）
```

## Rules

1. **Basic 优先** — 阻容感、二极管、通用 IC 优先选用 JLCPCB Basic 库
2. **封装统一** — 全板封装种类 ≤8 种（降低贴片复杂度）
3. **一料多位** — 同类阻值尽量合并，减少物料种类（如 10k 上拉电阻全板统一）
4. **预留替代料** — 每个 Extended 料标注 1-2 个可在淘宝/立创商城买到的替代料
5. **总种数控制** — 扩展料 ≤10 种（5pcs 原型控制在 $30 贴片费内）
6. **库存检查** — 下单前确认所有元件库存 >1000
7. **危料预警** — 标注库存 <500 的元件为"危料"，可能断货
8. **电压余量** — 电容耐压至少有 2x 余量（如 3.3V 轨用 ≥6.3V 电容）
9. **温度范围** — 消费级 (-20~85°C) 足够，不要选用工业级 (-40~105°C) 或汽车级
10. **数据手册验证** — 关键元件封装必须通过数据手册核实焊盘尺寸

## Checklist

- [ ] BOM 从 KiCad 完整导出（无遗漏行）
- [ ] 所有元件有 LCSC 编号或替代供应商
- [ ] Basic 库覆盖率 > 70%（种数比）
- [ ] Extended 料 ≤10 种
- [ ] 关键 IC 库存充足（>1000pcs）
- [ ] 无已停产（Discontinued）元件
- [ ] 单价合理（单套元件费 <$20）
- [ ] 替代料表已配套（每个扩展料 ≥1 个替代）
- [ ] BOM 格式符合 JLCPCB 要求
- [ ] 生产数量与 PCB 数量匹配
