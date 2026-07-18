# /pcb-layout — PCB 布局布线工作流

## 用途

通过 KiCad MCP 工具执行 RobotBuddy PCB 的布局布线和设计验证，从网表同步到 DRC 通过的完整 PCB 设计流程。

## 适用场景

- 从原理图网表同步到 PCB 开始布局
- 新增元件后的 PCB 布局调整
- 自动布线 + 手动精细化
- DRC 检查与修复
- 生产文件输出前的最终验证

## 工作流

```
第 1 阶段：准备与同步
├── 1.1 确认原理图 ERC 已通过
├── 1.2 网表同步 → sync_schematic_to_board
├── 1.3 设置板框 → add_board_outline
├── 1.4 添加安装孔 → add_mounting_hole ×4
├── 1.5 配置设计规则 → set_design_rules
├── 1.6 创建网络类 → add_net_class (Power / Signal)
└── 1.7 输出：有网表和封装的空白 PCB

第 2 阶段：元件布局
├── 2.1 手动放置连接器（边沿固定）
├── 2.2 手动放置 MCU（中心偏左）
├── 2.3 手动放置大元件（电感、电池座、喇叭）
├── 2.4 自动布局其余元件 → suggest_placement (dry run)
├── 2.5 审核自动布局结果
├── 2.6 微调布局（对齐/旋转/交换元件）
├── 2.7 Courtyard 碰撞检查 → check_courtyard_overlaps
├── 2.8 3D 可视确认
└── 2.9 输出：完成布局的 PCB

第 3 阶段：布线
├── 3.1 关键信号手动布线（USB 差分 > SPI CLK > I2S）
├── 3.2 电源网络布线（VBAT/5V/3V3 → 宽走线）
├── 3.3 自动布线其余网络 → autoroute
├── 3.4 自动布线结果审核
├── 3.5 补充/优化走线
├── 3.6 添加 GND 铜皮
├── 3.7 GND 缝合过孔
└── 3.8 输出：完成布线的 PCB

第 4 阶段：验证
├── 4.1 DRC 检查 → run_drc → 0 error
├── 4.2 Courtyard 最终检查
├── 4.3 3D 干涉检查
├── 4.4 板框检查（尺寸/安装孔/连接器位置）
├── 4.5 生成 PCB 2D 预览图
└── 4.6 输出：DRC 通过报告

第 5 阶段：生产输出
├── 5.1 导出 Gerber → export_gerbers
├── 5.2 导出钻孔 → export_drill
├── 5.3 导出 BOM → export_bom
├── 5.4 导出坐标 → export_pos
├── 5.5 导出 3D → export_3d
├── 5.6 导出 PCB PDF/SVG（文档用）
└── 5.7 输出：完整生产文件包
```

## 使用示例

### 首次布局

```
/pcb-layout 对 robotbuddy 执行初始布局：
- 板框 80×60mm, 4 个 M3 孔
- 连接器 J1(USB-C), J2(LCD FPC), J3(电池), J4(电机) 固定边沿
- MCU U1 居中，天线朝右悬空
- 其余元件自动布局
```

### 布线调整

```
/pcb-layout 布线优化：
- USB 差分信号手动布线 (J1→U1, 90Ω, 等长)
- 电源网络 0.8mm 走线
- 其余网络自动布线 5 次取最优
```

### DRC 修复

```
/pcb-layout 修复 DRC 违规：
- 查看当前 DRC 报告
- 逐项修复违规
- 重新验证
```

### 生产文件导出

```
/pcb-layout 导出生产文件：
- 输出到 hardware/output/
- Gerber + 钻孔 + BOM + 坐标
```

## 设计规则参考

| 参数 | 2 层板 | 4 层板 |
|------|--------|--------|
| 最小线宽 | 0.15mm (6mil) | 0.127mm (5mil) |
| 最小间距 | 0.15mm (6mil) | 0.127mm (5mil) |
| 最小过孔 | 0.3/0.6mm | 0.2/0.45mm |
| 板厚 | 1.6mm | 1.6mm |
| 铜厚 | 1oz | 1oz (内层 0.5oz) |

## 走线宽度速查

```
1oz 铜厚, 外层:
- 信号: 0.3mm (≤1A)
- 电源分支: 0.5mm (≤1.5A)
- 电源主干: 0.8mm (≤2.5A)
- 电机供电: 1.0mm (≤3A)
- VBAT 主路: 1.5mm (≤4A)
```

## 前置条件

- 原理图 ERC 通过
- KiCad MCP 环境可用
- (可选) Freerouting 已配置（用于自动布线）
- 外壳 3D 模型（用于干涉检查）

## 输出

- `.kicad_pcb` — PCB 布局文件
- `output/gerber/` — Gerber 制造文件
- `output/drill/` — 钻孔文件
- `output/robotbuddy_bom.csv` — BOM 物料清单
- `output/robotbuddy_pos.csv` — 贴片坐标文件
- `output/robotbuddy_3d.step` — 3D 模型

## 注意事项

- **DRC 零错误原则** — 单个 DRC error 也不能放过
- **手动关键信号** — 高速/差分/模拟信号先手动布线
- **自动布局仅作参考** — 连接器和大元件手动定，其余才自动
- **GND 铜皮最后加** — 所有布线完成后再添加铜皮
- **天线区域无铜** — ESP32-S3 模组天线区域禁止铺铜
- **3D 验证是必须的** — 导出 STEP 检查与外壳的干涉
