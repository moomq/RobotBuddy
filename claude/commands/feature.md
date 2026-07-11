# /feature — RobotBuddy 功能开发全流程

## 用途

为 RobotBuddy 桌面机器人完成一个完整功能的端到端开发，从需求分析到代码合入。

## 适用场景

- 新增一个表情/动作/语音交互功能
- 新增一个传感器驱动模块
- 新增一个云端 AI 交互能力
- 新增一个 VS Code / Git 集成功能

## 工作流

```
第 1 阶段：需求分析
├── 1.1 阅读 docs/桌面机器人设计需求.md 确认功能边界
├── 1.2 调用 requirement skill → 拆解功能需求、明确验收条件
└── 1.3 输出：功能需求分析（含场景、接口、异常处理）

第 2 阶段：架构设计
├── 2.1 调用 architecture skill → FreeRTOS 任务划分、模块接口设计
├── 2.2 调用 freertos-system skill → 事件总线消息定义、优先级分配
├── 2.3 输出：技术方案（任务图、消息流、接口契约）

第 3 阶段：编码实现
├── 3.1 调用 coding skill → 按模块实现 C/C++ 代码
├── 3.2 涉及硬件时调用 hardware-driver skill
├── 3.3 涉及音频时调用 audio-pipeline skill
├── 3.4 涉及显示时调用 display-engine skill
├── 3.5 涉及运动时调用 motion-control skill
├── 3.6 涉及联网时调用 cloud-communication skill
├── 3.7 输出：可编译的固件代码

第 4 阶段：测试
├── 4.1 调用 testing skill → 单元测试 + 硬件在环测试
├── 4.2 验证 FreeRTOS 任务栈/堆使用
└── 4.3 输出：测试报告

第 5 阶段：审查
├── 5.1 调用 review skill → 代码审查 + 架构审查
├── 5.2 对照 standards/embedded-coding.md 检查
├── 5.3 对照 checklists/pre-commit.md 检查
└── 5.4 输出：审查报告 + 修改建议

第 6 阶段：文档
├── 6.1 调用 document skill → 更新 API 文档、更新需求文档
└── 6.2 输出：更新后的项目文档
```

## 前置条件

- 需求文档已确认（docs/桌面机器人设计需求.md）
- 开发环境已搭建（ESP-IDF v5.x、VS Code + ESP32 插件）

## 输出

- 需求分析文档
- 技术方案文档
- 实现代码
- 测试报告
- 审查报告
- 更新后的项目文档

## 注意事项

- ESP32-S3 PSRAM 有限（8MB），需关注内存占用
- FreeRTOS 任务栈分配需合理，防止溢出
- ISR 中不能使用阻塞 API
- 所有云端通信需考虑 WiFi 断线重连
- 文档中禁用PlantUML绘制UML图，一律使用mermaid。
