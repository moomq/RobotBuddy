# IDE Integration Skill — RobotBuddy

## Role

RobotBuddy IDE 集成专家，负责将机器人连接到开发者的编程工作流（VS Code、Git、CI/CD）。

## Domain

VS Code Extension API、Git Webhook、CI/CD Pipeline、MQTT/HTTP Bridge、本地 WebSocket 服务。

## Goal

让 RobotBuddy 成为开发者桌面上真正的"编程伙伴"——实时感知代码状态、主动反馈。

## Inputs

- VS Code Extension 开发文档
- Git Webhook 事件类型
- CI/CD 平台 API (GitHub Actions / GitLab CI)
- MQTT Broker 配置

## Outputs

- `ide-plugins/vscode-extension/` — VS Code 插件
- `ide-plugins/git-webhook/` — Git Webhook 监听服务
- `ide-plugins/local-bridge/` — 本地桥接服务（PC ↔ RobotBuddy）
- `docs/ide-integration.md` — IDE 集成文档

## Integration Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Developer Desktop                        │
│                                                              │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐               │
│  │ VS Code  │   │ Terminal │   │ Git      │               │
│  │ Plugin   │   │ Watcher  │   │ Hook     │               │
│  └────┬─────┘   └────┬─────┘   └────┬─────┘               │
│       │               │               │                     │
│       └───────────────┼───────────────┘                     │
│                       │                                     │
│              ┌────────↓────────┐                            │
│              │  Local Bridge   │  ← WebSocket Server        │
│              │  (ws://localhost │    (Python/Node.js)        │
│              │   :9527)        │                            │
│              └────────┬────────┘                            │
│                       │                                     │
├───────────────────────┼─────────────────────────────────────┤
│                       │ WiFi / MQTT                         │
├───────────────────────┼─────────────────────────────────────┤
│                       │                                     │
│              ┌────────↓────────┐                            │
│              │   RobotBuddy    │                            │
│              │   ESP32-S3      │                            │
│              └─────────────────┘                            │
└─────────────────────────────────────────────────────────────┘
```

## VS Code Extension

```typescript
// vscode-extension/src/extension.ts

// 监听的事件类型:
// 1. 编译开始/结束/失败
// 2. 文件保存
// 3. 测试运行/通过/失败
// 4. 断点触发
// 5. 终端命令执行

interface BuildEvent {
    type: 'build_start' | 'build_success' | 'build_failed';
    project: string;
    errors?: number;
    warnings?: number;
    timestamp: number;
    duration_ms?: number;
}

interface TestEvent {
    type: 'test_run' | 'test_pass' | 'test_fail';
    passed: number;
    failed: number;
    skipped: number;
    duration_ms: number;
}

interface SaveEvent {
    type: 'file_save';
    filename: string;
    language: string;
    line_count: number;
}

// 通过本地 WebSocket 发送到 Local Bridge
// Local Bridge 再通过 MQTT 转发给 RobotBuddy
```

### VS Code 插件功能列表

| 功能 | 触发条件 | 机器人反应 |
|------|---------|-----------|
| 编译开始 | `npm run build` / `cargo build` | 眼睛→FOCUS + 转圈动画 |
| 编译成功 | Build exit 0 | 眼睛→HAPPY ✅ + 原地转圈 |
| 编译失败 | Build exit ≠ 0 | 眼睛→ERROR ❌ + 错误计数显示 |
| 测试通过 | All tests pass | 眼睛→HAPPY + "测试全通过" |
| 测试失败 | Test failures | 眼睛→WARNING + 失败数量 |
| 文件保存 | Save event | 眼睛→微动（确认收到） |
| Git commit | Post-commit hook | 眼睛→HAPPY + commit 计数 |
| 长时间编码 | 连续 25min 活动 | 番茄钟提醒 |

## Git Webhook

```json
// GitHub Webhook → Local Bridge → MQTT → RobotBuddy

// 监听事件:
{
  "push": {         // 代码推送
    "commits": [...],
    "ref": "refs/heads/main"
  },
  "pull_request": { // PR 创建/合并
    "action": "opened|closed|merged",
    "title": "...",
    "user": "..."
  },
  "issues": {       // Issue 创建/关闭
    "action": "opened|closed",
    "title": "..."
  },
  "check_run": {    // CI 检查完成
    "status": "completed",
    "conclusion": "success|failure"
  }
}
```

## Local Bridge Service

```python
# local-bridge/bridge.py
# 运行在开发者 PC 上，桥接 VS Code / Git / CI → MQTT → RobotBuddy

import asyncio
import websockets
import paho.mqtt.client as mqtt

class LocalBridge:
    """
    功能:
    1. WebSocket Server (ws://localhost:9527)
       - 接收 VS Code 插件推送的事件
    2. Git Hook Listener
       - 监控 .git/hooks/ 目录下的事件
    3. MQTT Client
       - 将事件转发到 RobotBuddy 的 MQTT Topic
    4. Device Discovery
       - mDNS / SSDP 自动发现局域网内的 RobotBuddy
    """

    async def handle_ws_message(self, message):
        event = json.loads(message)
        # 转换 + 转发到 MQTT
        topic = f"robotbuddy/{self.device_id}/{event['type']}"
        self.mqtt.publish(topic, json.dumps(event))
```

## CI/CD Integration

```yaml
# .github/workflows/notify-robotbuddy.yml
# CI/CD Pipeline → MQTT → RobotBuddy

name: Notify RobotBuddy
on:
  workflow_run:
    workflows: ["Build", "Test", "Deploy"]
    types: [completed]

jobs:
  notify:
    runs-on: ubuntu-latest
    steps:
      - name: Send to RobotBuddy
        uses: robotbuddy/notify-action@v1
        with:
          device_id: ${{ secrets.ROBOTBUDDY_ID }}
          mqtt_broker: ${{ secrets.MQTT_BROKER }}
          status: ${{ github.event.workflow_run.conclusion }}
```

## Protocol Buffer (推荐用于结构化消息)

```protobuf
// robotbuddy.proto
// IDE ↔ Local Bridge ↔ RobotBuddy 之间的消息协议

syntax = "proto3";

message BuildStatus {
    enum Status {
        UNKNOWN = 0;
        STARTED = 1;
        SUCCESS = 2;
        FAILED = 3;
    }
    Status status = 1;
    string project = 2;
    int32 errors = 3;
    int32 warnings = 4;
    int32 duration_ms = 5;
    string output_summary = 6;  // 错误摘要 (≤200 chars, 屏幕可显示)
}

message GitStatus {
    string branch = 1;
    int32 ahead = 2;
    int32 behind = 3;
    int32 changed_files = 4;
    int32 untracked_files = 5;
}

message Notification {
    enum Level {
        INFO = 0;
        SUCCESS = 1;
        WARNING = 2;
        ERROR = 3;
    }
    Level level = 1;
    string title = 2;
    string body = 3;
    string action_url = 4;  // 可操作的链接
}
```

## Version Roadmap

| 版本 | 功能 | 接口方式 |
|------|------|---------|
| V1.0 MVP | None (manual) | — |
| V2.0 | VS Code 编译状态 | Local Bridge + MQTT |
| V2.1 | Git 状态通知 | Git Webhook → Local Bridge |
| V2.2 | CI/CD 通知 | CI Webhook → MQTT |
| V2.3 | 番茄钟联动 | VS Code 活跃检测 |
| V3.0 | 终端命令联动 | Shell integration |
| V3.1 | 代码评审请求 | VS Code → Claude → RobotBuddy |

## Rules

1. Local Bridge 必须在开发者 PC 上运行（localhost 安全无 TLS）
2. MQTT QoS 至少为 1（确保消息送达机器人）
3. 错误摘要长度 ≤ 200 字符（屏幕可显示限制）
4. 消息频率限制：同一事件类型最少间隔 1s（防止刷屏）
5. Device ID 使用配置文件中的唯一标识符
6. 插件代码开源（MIT License），鼓励社区贡献

## Checklist

- [ ] VS Code 插件能正确检测编译事件
- [ ] Local Bridge WebSocket 连接稳定
- [ ] MQTT 消息能正确到达 RobotBuddy
- [ ] 编译成功/失败：机器人表情正确反应
- [ ] Git 推送：机器人通知正确
- [ ] 多项事件并发：消息队列不溢出
- [ ] 插件卸载后无残留进程/配置
- [ ] README 包含安装和配置指南
