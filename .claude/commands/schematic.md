# /schematic — KiCad 原理图设计工作流

## 用途

通过 KiCad MCP 工具创建或修改 RobotBuddy 的原理图，实现从空白到 ERC 通过的完整原理图设计流程。

## 适用场景

- 从零开始创建 RobotBuddy 原理图
- 新增电路子模块（如新增传感器、显示模组）
- 修改已有电路（引脚调整、网络变更）
- 原理图审查前的 ERC 自动修复
- 为已有原理图补充 JLCPCB BOM 属性

## 工作流

```
第 1 阶段：项目准备
├── 1.1 确认硬件需求 → 调用 requirement skill
├── 1.2 确认硬件架构 → 调用 architecture skill
├── 1.3 搜索可用符号库（KiCad 标准库 + JLCPCB 库）
├── 1.4 缺失符号 → 创建自定义符号 + 封装
├── 1.5 注册项目本地库
└── 1.6 输出：可用的符号库清单

第 2 阶段：层次化设计
├── 2.1 确定页划分（Power / MCU / Display / Audio / Sensors / Motors / Connectivity）
├── 2.2 创建子图文件 + 父图 Sheet 引用
├── 2.3 设置 Sheet Pin（子图接口）
├── 2.4 设置 Hierarchical Label（子图内部标签）
└── 2.5 输出：层次化结构图

第 3 阶段：逐页放置与连线
├── 3.1 批量放置元件 → batch_add_and_connect
├── 3.2 连线补全 → add_schematic_wire
├── 3.3 电源符号放置（GND / VCC / 3V3 / 5V）
├── 3.4 PWR_FLAG 放置（电源网络必须）
├── 3.5 未连接引脚标记 → batch_add_no_connects
├── 3.6 自动标注 → annotate_schematic
├── 3.7 字段自动调整 → autoplace_schematic_fields
└── 3.8 输出：完成连线的原理图页

第 4 阶段：BOM 属性补充
├── 4.1 为每个元件添加 LCSC / MPN / Manufacturer 属性
├── 4.2 自动填充 Datasheet URL → enrich_datasheets
└── 4.3 输出：含完整 BOM 信息的原理图

第 5 阶段：ERC 检查与修复
├── 5.1 运行 ERC
├── 5.2 修复悬空导线 → find_orphaned_wires
├── 5.3 修复重叠元素 → find_overlapping_elements
├── 5.4 修复穿越元件的导线 → find_wires_crossing_symbols
├── 5.5 修复浮空标签 → list_floating_labels
├── 5.6 网格对齐 → snap_to_grid
├── 5.7 重新运行 ERC → 确认 0 error
└── 5.8 输出：ERC 通过报告

第 6 阶段：生产准备
├── 6.1 导出 PDF 原理图（归档）
├── 6.2 导出网表
├── 6.3 生成 BOM
└── 6.4 输出：原理图 PDF + 网表 + BOM
```

## 使用示例

### 创建一个新模块

```
/schematic 在 Power 页添加电池电量检测电路：
- ADC1_CH0 (GPIO1) 接电阻分压
- R1: 100k 上拉至 VBAT
- R2: 100k 下拉至 GND
- C1: 100nF 到 GND (滤波)
```

### 修改已有电路

```
/schematic 修改 Display 页：
- 将 ST7789 的 CS 引脚从 GPIO37 改为 GPIO41
- 添加背光控制 MOSFET 电路
```

### 审查现有原理图

```
/schematic 对 robotbuddy.kicad_sch 执行完整审查：
- 运行 ERC，修复所有警告
- 检查所有网络连接完整性
- 补充缺失的 BOM 属性
```

## 常用 MCP 工具速查

| 任务 | 工具 | 说明 |
|------|------|------|
| 大量放置元件+连线 | `batch_add_and_connect` | 最快方式，一次完成 |
| 单个修改元件 | `edit_schematic_component` | 改封装/值/属性 |
| 连线 | `add_schematic_wire` | 点对点走线 |
| 连接网络 | `batch_connect` | 批量为引脚贴网络标签 |
| 未连接引脚 | `batch_add_no_connects` | 批量标记 NC |
| 查看引脚坐标 | `get_schematic_pin_locations` | 连线前获取精确坐标 |
| ERC 检查 | `run_erc` | 电气规则检查 |
| 字段调整 | `autoplace_schematic_fields` | 自动排布 Ref/Value 位置 |
| 可视化验证 | `get_schematic_view` | 导出原理图 PNG 查看 |

## 前置条件

- KiCad MCP 环境可用（已通过环境检查）
- 硬件需求文档已准备
- ESP32-S3 引脚分配表已确认

## 输出

- `.kicad_sch` 原理图文件（ERC 0 error）
- `.kicad_sym` 自定义符号库
- 原理图 PDF 归档文件
- 网表文件

## 注意事项

- **批量优先** — 能用 `batch_add_and_connect` 就不用逐个 `add_schematic_component`
- **ERC 不过不进 PCB** — ERC 有 error 时不去做 PCB 布局
- **PWR_FLAG 不要忘** — 每个电源网络必须有 PWR_FLAG
- **网格对齐** — 连完线后跑一次 `snap_to_grid`，防止假连接
- **可视化验证** — 每做完一页，导出 PNG 目视确认
