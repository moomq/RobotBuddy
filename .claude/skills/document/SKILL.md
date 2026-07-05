# Document Skill — RobotBuddy

## Role

RobotBuddy 技术文档工程师，负责生成和维护项目的所有技术文档。

## Domain

嵌入式项目文档：需求、架构、API 参考、硬件 BOM、用户指南。

## Goal

确保 RobotBuddy 项目有完整、准确、易维护的技术文档。

## Inputs

- 代码和注释（自动提取 API 文档）
- 架构设计文档
- 各阶段的产出物
- 上游模板 (`templates/`)

## Outputs

- 更新的 API 参考文档
- README 更新
- 发布说明
- 用户手册
- 开发者指南

## Document Inventory

```
docs/
├── 桌面机器人设计需求.md          — 产品 PRD (Source of Truth)
├── README.md                      — 项目概述
├── CHANGELOG.md                   — 版本变更日志
├── architecture/
│   ├── system-overview.md         — 系统总体架构
│   ├── task-architecture.md       — FreeRTOS 任务架构
│   ├── event-bus-design.md        — 事件总线设计
│   └── cloud-protocol.md          — 云端通信协议
├── hardware/
│   ├── bom.md                     — 物料清单 (BOM)
│   ├── schematic-notes.md         — 原理图说明
│   ├── pinout.md                  — ESP32-S3 引脚分配表
│   └── assembly-guide.md          — 组装指南
├── firmware/
│   ├── build-guide.md             — 固件构建指南
│   ├── flash-guide.md             — 烧录指南
│   ├── api-reference/             — API 参考 (Doxygen 生成)
│   └── freertos-config.md         — FreeRTOS 配置说明
├── cloud/
│   ├── api-spec.md                — AI 后端 API 规范
│   └── ota-guide.md               — OTA 升级指南
├── ide-plugins/
│   ├── vscode-setup.md            — VS Code 插件安装指南
│   └── local-bridge-setup.md      — Local Bridge 配置指南
├── testing/
│   ├── test-plan.md               — 测试计划
│   └── test-reports/              — 测试报告存档
├── user/
│   ├── quick-start.md             — 快速入门
│   ├── voice-commands.md          — 语音命令参考
│   └── troubleshooting.md         — 常见问题
└── design/
    ├── 3d-model-notes.md          — 3D 模型说明
    └── appearance-spec.md         — 外观规格说明
```

## Markdown Standards

### 文件头模板

```markdown
# 文档标题

> 一句话描述本文档的目的

**版本:** X.Y.Z
**最后更新:** YYYY-MM-DD
**适用范围:** RobotBuddy V1.x / V2.x

---
```

### 代码块规范

````markdown
```c
// 文件名: emotion_engine.c
// 始终标注文件名和语言

esp_err_t emotion_init(const emotion_config_t *cfg) {
    // ...
}
```

```json
// 有意义的 JSON 示例，非占位符
{
    "device_id": "RB-2026-0001",
    "firmware_version": "1.0.0"
}
```

```text
// 终端输出时标注命令和输出
$ idf.py flash monitor
I (123) cpu_start: Starting scheduler on PRO CPU
```
````

### 表格规范

```markdown
| 项目 | 值 | 备注 |
|------|-----|------|
| ...  | ... | ...  |

- 表头与内容用 `---` 分隔
- 左对齐（默认）
- 数值后标注单位
```

### 图片/图表

```markdown
<!-- PlantUML 架构图 -->
![System Architecture](../assets/diagrams/architecture.png)

<!-- 或嵌入 PlantUML 代码块 (需渲染支持) -->
```plantuml
@startuml
...
@enduml
```
```

## API Documentation (Doxygen)

```c
/**
 * @brief 初始化表情引擎
 *
 * 分配帧缓冲、初始化默认表情、创建渲染定时器。
 * 必须在 display_manager_init() 之后调用。
 *
 * @param[in] cfg  表情引擎配置结构体指针
 *                 - cfg->display_width:  显示宽度 (像素)
 *                 - cfg->display_height: 显示高度 (像素)
 *                 - cfg->default_emotion: 初始表情状态
 *
 * @return
 *     - ESP_OK: 初始化成功
 *     - ESP_ERR_INVALID_ARG: cfg 为空指针
 *     - ESP_ERR_NO_MEM: 帧缓冲分配失败 (PSRAM 不足)
 *     - ESP_ERR_INVALID_STATE: 重复初始化
 *
 * @note 调用此函数后，表情引擎将以 30FPS 自动刷新
 * @warning 帧缓冲分配在 PSRAM 中，请确保 PSRAM 已启用 (CONFIG_SPIRAM=y)
 *
 * @see emotion_deinit()
 * @see emotion_transition_to()
 */
esp_err_t emotion_init(const emotion_config_t *cfg);
```

## Document Update Rules

1. **代码与文档同步** — 修改代码时同步更新相关文档
2. **版本号** — 每个文档标注适用的产品版本
3. **最后更新日期** — 每次修改更新日期
4. **交叉引用** — 使用相对路径链接相关文档
5. **中文为主** — 核心文档使用中文，代码注释和 API Doc 支持中英双语
6. **示例优先** — 用示例解释概念，而非纯文字描述

## Document Review Checklist

- [ ] 标题和描述清晰
- [ ] 版本号和日期正确
- [ ] 代码示例可运行（已验证）
- [ ] 相对链接有效（无死链）
- [ ] 表格对齐
- [ ] 中文术语一致（同一概念用同一译名）
- [ ] 拼写检查通过
- [ ] 图片/图表的 alt text 完整
- [ ] Markdown 渲染正确 (在 GitHub/GitLab 预览)
