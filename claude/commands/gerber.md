# /gerber — 生产文件生成与验证

## 用途

从 RobotBuddy 的 KiCad PCB 生成完整的制造文件包（Gerber、钻孔、BOM、坐标），并进行投板前的最终验证。

## 适用场景

- PCB 设计完成，准备投板（JLCPCB / PCBWay 等）
- 生产文件归档
- PCB 版本发布前的生产文件包打包
- 设计变更后重新生成生产文件
- 对已有 Gerber 做可制造性复核

## 工作流

```
第 1 阶段：PCB 最终检查
├── 1.1 确认 DRC 通过 → run_drc (0 error, 0 warning)
├── 1.2 Courtyard 碰撞检查 → check_courtyard_overlaps
├── 1.3 板框尺寸确认 → get_board_extents
├── 1.4 PCB 2D 预览 → get_board_2d_view
├── 1.5 ERC 复查 → run_erc
└── 1.6 输出：PCB 最终状态确认

第 2 阶段：Gerber 文件生成
├── 2.1 导出 Gerber → export_gerbers
│   ├── 层选择: F.Cu, B.Cu, F.Paste, B.Paste
│   ├── 层选择: F.SilkS, B.SilkS, F.Mask, B.Mask
│   └── 层选择: Edge.Cuts, F.Fab, B.Fab
├── 2.2 导出钻孔文件 → export_drill
│   ├── 格式: Excellon
│   ├── 分离 PTH / NPTH
│   └── 生成钻孔图 PDF
└── 2.3 输出：Gerber 包 + 钻孔包

第 3 阶段：BOM + 坐标文件
├── 3.1 导出 BOM → export_bom (CSV + HTML)
├── 3.2 导出贴片坐标 → export_pos (CSV, mm)
├── 3.3 检查 BOM 与贴片坐标一致性（元件数量匹配）
└── 3.4 输出：BOM + CPL 文件

第 4 阶段：3D 与文档
├── 4.1 导出 3D STEP → export_3d
├── 4.2 导出 PCB PDF → export_pcb_pdf
├── 4.3 导出 PCB SVG → export_pcb_svg
└── 4.4 输出：3D 模型 + PCB 文档

第 5 阶段：投板检查清单
├── 5.1 文件完整性检查（所有必需文件存在）
├── 5.2 Gerber 查看器目视检查（可用在线工具）
├── 5.3 BOM 元件库存确认（JLCPCB 物料就绪）
├── 5.4 阻抗匹配确认（如适用）
├── 5.5 拼板检查（如需拼板）
└── 5.6 输出：投板就绪确认
```

## 输出文件结构

```
hardware/output/
├── gerber/
│   ├── robotbuddy-F_Cu.gtl          ← 顶层铜
│   ├── robotbuddy-B_Cu.gbl          ← 底层铜
│   ├── robotbuddy-F_SilkS.gto       ← 顶层丝印
│   ├── robotbuddy-B_SilkS.gbo       ← 底层丝印
│   ├── robotbuddy-F_Mask.gts        ← 顶层阻焊
│   ├── robotbuddy-B_Mask.gbs        ← 底层阻焊
│   ├── robotbuddy-F_Paste.gtp       ← 顶层钢网
│   ├── robotbuddy-B_Paste.gbp       ← 底层钢网
│   ├── robotbuddy-Edge_Cuts.gm1     ← 板框
│   └── robotbuddy-F_Fab.gtl         ← 顶层装配
│
├── drill/
│   ├── robotbuddy-PTH.drl           ← 金属化孔
│   ├── robotbuddy-NPTH.drl          ← 非金属化孔
│   └── robotbuddy-drill_map.pdf     ← 钻孔图
│
├── bom/
│   ├── robotbuddy_bom.csv           ← BOM 清单
│   └── robotbuddy_bom.html          ← BOM 网页版
│
├── pos/
│   └── robotbuddy_pos.csv           ← 贴片坐标 (CPL)
│
├── 3d/
│   └── robotbuddy_3d.step           ← 3D 模型
│
├── doc/
│   ├── robotbuddy_pcb.pdf           ← PCB 图纸
│   └── robotbuddy_pcb.svg           ← PCB 矢量图
│
└── robotbuddy_v1.0_production.zip   ← 生产文件打包
```

## 使用示例

### 完整生产文件生成

```
/gerber 为 robotbuddy V1.0 生成完整生产文件：
- 目标厂商: JLCPCB
- 工艺: 2 层, 1oz, 1.6mm, HASL 无铅, 绿色阻焊
- 打包输出到 hardware/output/
```

### 设计变更后重新生成

```
/gerber 只重新生成 Gerber 和钻孔（BOM 不变）：
- 更新了顶层布线
- 刷新 Gerber 和钻孔文件
```

### 投板前验证

```
/gerber 投板前验证检查：
- 检查所有文件完整性
- 生成预览 PDF 供人工确认
- 在线 Gerber 查看器 URL
```

## JLCPCB 投板参数参考

| 参数 | 选项 | 备注 |
|------|------|------|
| 层数 | 2 层 | 简单双面板 |
| 板厚 | 1.6mm | 标准 |
| 尺寸 | 80×60mm | ≤100×100mm 特价 |
| 铜厚 | 1oz | 标准 |
| 阻焊颜色 | 绿色 | 免费色 |
| 丝印颜色 | 白色 | 默认 |
| 表面处理 | HASL 无铅 | 原型首选 |
| 金手指 | 无 | — |
| 飞针测试 | 全测 | 推荐 |
| 数量 | 5 pcs | 原型起步 |

## 常用 MCP 工具速查

| 任务 | 工具 |
|------|------|
| 导出 Gerber | `export_gerbers` |
| 导出钻孔 | `export_drill` |
| 导出 BOM | `export_bom` / `export_sch_bom` |
| 导出坐标 | `export_pos` |
| 导出 3D | `export_3d` / `export_3d_cli` |
| 导出 PCB PDF | `export_pcb_pdf` |
| 导出 PCB SVG | `export_pcb_svg` |
| 导出 DXF | `export_pcb_dxf` |
| DRC 检查 | `run_drc` |
| 2D 预览 | `get_board_2d_view` |
| Board 信息 | `get_board_info` |
| Board 尺寸 | `get_board_extents` |

## 前置条件

- PCB 布局和布线已完成
- DRC 已通过
- 原理图 ERC 已通过

## 注意事项

- **DRC 未过不生产** — DRC 有 error 时禁止生成生产文件
- **文件命名规范** — 使用 JLCPCB 认可的 Protel 扩展名
- **钻孔分 PTH/NPTH** — 安装孔是 NPTH，信号过孔是 PTH
- **BOM 与坐标一致** — BOM 行数 = CPL 行数（考虑 DNP）
- **在线验证** — 生成后用 JLCPCB 在线 Gerber Viewer 检查一遍
- **版本号标注** — PCB 丝印上必须有版本号
- **压缩打包** — 投板时打包为一个 ZIP，命名含日期和版本
- **存档备份** — 每个版本的生产文件包都要 Git 归档或独立备份
