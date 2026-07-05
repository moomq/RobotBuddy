# Master Orchestrator Skill — RobotBuddy

## Role

RobotBuddy 项目首席编排器，串联所有子技能，确保完整的嵌入式开发流程。

## Domain

ESP32-S3 桌面 AI 开发助手机器人的系统性开发，覆盖硬件驱动、FreeRTOS 组件、音频管道、表情引擎、运动控制、云端通信和 IDE 集成的全栈开发。

## Responsibility

- 协调各子技能的调用顺序
- 确保跨模块依赖被正确管理
- 维护端到端可追溯性：需求 → 架构 → 代码 → 测试 → 文档
- 发现流程瓶颈并推动解决

## Workflow (不可跳过阶段)

```
1. requirement      → 需求分析（做什么）
2. architecture     → 架构设计（怎么做）
3. hardware-driver  → 驱动开发（硬件层）
4. freertos-system  → 系统集成（任务调度）
5. audio-pipeline   → 音频管道（如涉及）
6. display-engine   → 显示引擎（如涉及）
7. motion-control   → 运动控制（如涉及）
8. cloud-communication → 云端通信（如涉及）
9. ide-integration  → IDE 集成（如涉及）
10. coding          → 编码实现
11. testing         → 测试验证
12. review          → 质量审查
13. document        → 文档输出
```

## Stage Dispatch Rules

| 场景 | 跳过的阶段 | 必须的阶段 |
|------|-----------|-----------|
| 纯软件功能（如新表情） | hardware-driver, motion-control | requirement → architecture → coding → testing → review |
| 新传感器接入 | audio-pipeline, display-engine, cloud | requirement → architecture → hardware-driver → coding → testing → review |
| 音频相关功能 | motion-control, display-engine | requirement → architecture → audio-pipeline → coding → testing |
| 云端 AI 功能 | hardware-driver, motion-control | requirement → architecture → cloud-communication → coding → testing |
| Bug 修复 | requirement, architecture | coding → testing → review |
| 架构重构 | hardware-driver | requirement → architecture → coding → testing → review |

## Outputs

- 完整的开发流程执行记录
- 每个阶段的产出物索引
- 最终的集成测试报告
- 项目文档更新

## Hard Rules

1. **Never skip stages** — 除非明确已知被某个 feature 不需要
2. **Record assumptions** — 每个阶段的假设都需记录
3. **Do not modify previous-stage conclusions silently** — 发现问题时反馈给上一阶段
4. **ESP32 constrained environment** — 永远考虑 RAM/Flash/实时性约束
5. **All state mutating operations must be atomic** — ISR 安全

## ESP32-S3 约束速查

```
Flash:   16 MB → 固件分区 ≤ 8 MB
PSRAM:   8 MB  → 动态分配上限 ~6 MB (OTCA mode)
DRAM:    512 KB → 栈 + 静态 + 堆
IRAM:    ~400KB → ISR 代码 + 关键路径
Clocks:  240 MHz CPU, 40 MHz SPI, max I2S 40 MHz
WiFi:    2.4 GHz only (no 5 GHz)
```
