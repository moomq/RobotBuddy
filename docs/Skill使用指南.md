# RobotBuddy Skill 使用指南

> 本文档详细说明 RobotBuddy 项目的 Claude Code Skills 和 Commands 体系的使用方法。

**版本:** 1.0
**最后更新:** 2026-07-05
**适用范围:** RobotBuddy 全生命周期开发

---

## 目录

1. [体系概述](#1-体系概述)
2. [Commands（工作流命令）](#2-commands工作流命令)
3. [Skills（专业 Agent）](#3-skills专业-agent)
4. [Master 编排流程](#4-master-编排流程)
5. [场景化使用示例](#5-场景化使用示例)
6. [Skills 调用关系图](#6-skills-调用关系图)
7. [Templates 使用说明](#7-templates-使用说明)
8. [Standards & Checklists 使用说明](#8-standards--checklists-使用说明)
9. [最佳实践](#9-最佳实践)
10. [常见问题](#10-常见问题)

---

## 1. 体系概述

### 1.1 设计理念

RobotBuddy 的 `.claude/` 体系采用 **Command → Skill → Template** 三层架构：

```
┌──────────────────────────────────────────────────────┐
│                   Commands (工作流)                    │
│   定义"做什么"的端到端流程，串联多个 Skills              │
│   /feature  /hardware-bringup  /firmware  /review     │
│   /release  /debug                                    │
├──────────────────────────────────────────────────────┤
│                   Skills (专业能力)                    │
│   定义"怎么做"的专业知识和执行规范                       │
│   master  requirement  architecture  hardware-driver  │
│   audio-pipeline  display-engine  motion-control      │
│   freertos-system  cloud-communication  ide-integration│
│   coding  review  testing  document                   │
├──────────────────────────────────────────────────────┤
│              Templates / Standards / Checklists       │
│   定义"做成什么样"的输出模板和质量标准                   │
│   hardware-driver.md  component-bom.md  api-protocol.md│
│   embedded-coding.md  pre-commit.md                   │
└──────────────────────────────────────────────────────┘
```

### 1.2 使用方式

所有 Commands 和 Skills 均通过 **Claude Code 对话界面** 调用，无需额外安装工具：

- **Command**: 直接在对话中输入 `/<command-name>`，Claude 将按工作流定义自动执行
- **Skill**: 在对话中描述需求时，Claude 会自动匹配相关 Skill 并加载其专业知识
- **手动调用**: 可以明确指定 "调用 xxx skill 来..." 触发特定 Skill

### 1.3 与 embedded-async-framework-skills 的关系

本体系参考了 `embedded-async-framework-skills` 的 10 阶段流程模型，在此基础上针对 RobotBuddy 项目做了以下增强：

| 对比维度 | 参考框架 | RobotBuddy 体系 |
|----------|---------|-----------------|
| 流程阶段 | 10 个通用阶段 | 14 个机器人专属 Skills |
| 硬件覆盖 | 抽象 HAL 层 | ESP32-S3 具体外设（ST7789/INMP441/DRV8833...） |
| 软件架构 | 通用分层 | FreeRTOS Task + EventBus 具体实现 |
| 领域知识 | 无 | 音频管道/表情引擎/运动控制/云端通信/IDE集成 |
| 输出模板 | architecture + review | 3 个模板 + 编码标准 + 提交检查清单 |

---

## 2. Commands（工作流命令）

### 2.1 `/feature` — 功能开发全流程

**用途:** 端到端完成一个 RobotBuddy 功能开发

**触发方式:**
```
/feature
请帮我实现"编译状态 LED 闪烁提醒"功能
```

**执行流程:**

| 阶段 | 调用的 Skill | 产出物 |
|------|-------------|--------|
| ① 需求 | `requirement` | 功能需求分析文档 |
| ② 架构 | `architecture` + `freertos-system` | 任务设计 + 事件消息定义 |
| ③ 编码 | `coding` + 领域 Skills | 可编译的 C 代码 |
| ④ 测试 | `testing` | 测试报告 |
| ⑤ 审查 | `review` | 审查报告 |
| ⑥ 文档 | `document` | 更新后的项目文档 |

**预计耗时:** 30-60 分钟（简单功能）/ 2-4 小时（复杂功能）

**适用场景:**
- 新增表情动画（如 wink 眨眼）
- 新增语音交互命令（如 "帮我查 git log"）
- 新增传感器数据处理逻辑
- 新增云端 API 调用

**不适用场景:**
- 单行 Bug 修复 → 直接用 `/debug`
- 纯代码审查 → 直接用 `/review`
- 仅更新文档 → 直接调用 `document` skill

---

### 2.2 `/hardware-bringup` — 硬件模块调通

**用途:** 为新硬件组件开发驱动并验证

**触发方式:**
```
/hardware-bringup
请帮我调通 VL53L1X TOF 激光测距传感器，接到 I2C0
```

**执行流程:**

| 阶段 | 关键活动 | 检查点 |
|------|---------|--------|
| ① datasheet 分析 | 确认接口/电压/时序/寄存器 | 引脚分配表 |
| ② 驱动开发 | `hardware-driver` skill 生成驱动框架 | 编译通过 |
| ③ 硬件连接 | 焊接/跳线 + 万用表检测 | 无短路 |
| ④ 驱动验证 | 最小测试→全功能→边界 | WHO_AM_I 回读正确 |
| ⑤ 集成 | 注册到 FreeRTOS 服务层 | BOM 更新 |

**预计耗时:** 1-2 小时（常见传感器）/ 半天（复杂模组）

**前置条件:**
- 目标硬件模组在手
- ESP32-S3 开发板可用
- 万用表必备，示波器推荐

---

### 2.3 `/firmware` — 固件模块开发

**用途:** 开发 FreeRTOS Task 或 Service 模块

**触发方式:**
```
/firmware
请帮我实现番茄钟 Task，25min 专注 + 5min 休息
```

**执行流程:**

| 阶段 | 关键活动 | 验证方法 |
|------|---------|---------|
| ① 模块设计 | Task 优先级/栈/消息定义 | 架构审查 |
| ② 编码 | `coding` skill 指导实现 | 编译通过 |
| ③ 构建烧录 | `idf.py build flash monitor` | 启动日志正常 |
| ④ 集成测试 | 任务间通信 + 内存分析 | 栈水位 ≥ 512B |
| ⑤ 审查 | `review` skill | Critical = 0 |

**FreeRTOS 约束速查:**

```
优先级: 音频(7-8) > 显示(5-6) > 运动(3-4) > 业务(1-2) > 监控(0)
栈建议: 音频 8KB / 云端 12KB / 显示 4KB / 简单控制 2KB / 监控 1KB
ISR: 仅 FromISR API, ≤ 10μs, 禁止 printf
```

---

### 2.4 `/review` — 代码与架构审查

**用途:** 多维度的固件代码质量审查

**触发方式:**
```
/review
请审查 components/services/audio_manager/ 下的最新改动
```

**审查维度与权重:**

| 维度 | 权重 | 不通过的后果 |
|------|------|------------|
| 编译通过 | 必须 | 无法合入 |
| 内存安全 | 必须 | 可能 Crash |
| 并发安全 | 必须 | 竞态/死锁 |
| 实时性 | 高 | 音频丢帧/动画卡顿 |
| 错误处理 | 中 | 异常无响应 |
| 日志完整 | 中 | 难调试 |
| 功耗合理 | 低 | 续航缩短 |

**输出格式:**
```
🔴 Critical (阻塞合入) — 必须修复
🟡 Warning (强烈建议) — PR 批准前修复
🔵 Suggestion (可选优化) — 后续迭代修复
```

---

### 2.5 `/release` — 固件发布

**用途:** 将固件从开发状态发布为正式版本

**触发方式:**
```
/release
请发布 V1.1.0，新增了代码片段滚动显示功能
```

**执行流程:**

| 阶段 | 关键操作 |
|------|---------|
| ① 发布前检查 | `/review` 全维度 + 24h 压力测试 |
| ② 版本管理 | SemVer 版本号 + CHANGELOG |
| ③ 构建 | Release build + SHA256 + 签名 |
| ④ 文档 | Release Notes 生成 |
| ⑤ 归档 | Git tag + OTA 服务器上传 |

**版本规则:**
- PATCH (1.0.x): Bug 修复
- MINOR (1.x.0): 新功能
- MAJOR (x.0.0): 架构重构/不兼容变更

---

### 2.6 `/debug` — 固件调试

**用途:** 系统化诊断 ESP32-S3 固件异常

**触发方式:**
```
/debug
机器人运行 2 小时后自动重启，请帮我诊断
```

**诊断分类与工具:**

| 问题类型 | 诊断方法 | 工具 |
|---------|---------|------|
| Panic / Crash | Backtrace 解析 | `addr2line` + `espcoredump` |
| 内存泄漏 | Heap tracing | `heap_caps_get_info` |
| 栈溢出 | 水位监控 | `uxTaskGetStackHighWaterMark` |
| WiFi 断连 | 事件日志 + RSSI | `esp_wifi_sta_get_rssi` |
| 音频丢帧 | I2S DMA buffer | 逻辑分析仪 |
| 显示异常 | SPI 时序 | 示波器 |

**常见错误速查:**
```
Guru Meditation: LoadProhibited → NULL/已释放指针
Stack overflow in task xxx    → 栈太小，2x 扩容
heap_caps_alloc failed        → 内存泄漏/不足
I2S: buffer underrun          → DMA buffer 太小
Watchdog trigger              → Task 死循环/阻塞
```

---

## 3. Skills（专业 Agent）

### 3.1 Skills 分类

#### 流程编排类

| Skill | 角色 | 被谁调用 |
|-------|------|---------|
| **master** | 首席编排器 | 复杂多阶段任务时自动激活 |

#### 设计类

| Skill | 角色 | 输入 → 输出 |
|-------|------|------------|
| **requirement** | 需求分析专家 | 用户故事 → `docs/requirement/<name>.md` |
| **architecture** | 系统架构师 | 需求文档 → `docs/architecture/<name>.md` |

#### 硬件域

| Skill | 角色 | 覆盖的硬件 |
|-------|------|-----------|
| **hardware-driver** | 外设驱动专家 | ST7789/INMP441/MAX98357A/DRV8833/MPU6050/VL53L0X |
| **audio-pipeline** | 音频管道专家 | I2S 采集→VAD→云端 ASR→TTS→I2S 播放 |
| **display-engine** | 表情引擎专家 | ST7789/GC9A01 + 11 种表情状态机 |
| **motion-control** | 运动控制专家 | N20 电机+DRV8833+MPU6050+PID+桌面安全 |

#### 系统域

| Skill | 角色 | 覆盖的子系统 |
|-------|------|------------|
| **freertos-system** | FreeRTOS 系统专家 | Task 管理+EventBus+SysMon+电源管理 |
| **cloud-communication** | 云端通信专家 | WiFi+HTTP/WS+MQTT+OTA+TLS |
| **ide-integration** | IDE 集成专家 | VS Code 插件+Git Webhook+CI/CD |

#### 质量域

| Skill | 角色 | 覆盖的质量维度 |
|-------|------|--------------|
| **coding** | 高级嵌入式工程师 | 编码规范+内存管理+并发安全 |
| **review** | 代码审查专家 | 7 维审查（内存/并发/实时/错误/功耗/维护） |
| **testing** | 测试专家 | 4 级测试（单元→HIL→集成→验收） |
| **document** | 文档工程师 | 需求/架构/API/用户 全系列文档 |

---

### 3.2 master Skill — 编排器

**何时激活:**
- 执行 `/feature` 等复杂 Command 时自动激活
- 用户明确说 "请用 master skill 安排开发流程" 时
- 跨多个 Skill 的大型功能开发

**编排规则（哪些阶段可跳过）:**

| 任务类型 | 跳过的 Skills | 必须的 Skills |
|----------|-------------|--------------|
| 纯软件功能（新表情） | hardware-driver, motion-control | requirement→architecture→coding→testing→review |
| 新传感器接入 | audio, display, cloud | requirement→architecture→hardware-driver→coding→testing |
| 音频功能 | motion, display | requirement→architecture→audio→coding→testing |
| 云端 AI 功能 | hardware-driver, motion | requirement→architecture→cloud→coding→testing |
| Bug 修复 | requirement, architecture | coding→testing→review |
| 架构重构 | hardware-driver | requirement→architecture→coding→testing→review |

**ESP32-S3 约束（所有开发必须遵守）:**
```
Flash:   16 MB → 固件分区 ≤ 8 MB
PSRAM:   8 MB  → 动态分配上限 ~6 MB
DRAM:    512 KB → 栈+静态+堆 共享
IRAM:    ~400KB → ISR 代码+关键路径
Clocks:  240 MHz CPU, max 40 MHz SPI/I2S
WiFi:    2.4 GHz only
```

---

### 3.3 requirement Skill — 需求分析

**调用方式:**
```
请用 requirement skill 分析"番茄钟提醒功能"
```

**分析 6 维度:**

```
功能描述 → 触发条件 → 期望结果
硬件依赖 → 涉及什么硬件？能力是否满足？
Task 影响 → 新建 Task？影响哪些现有 Task？
云/网络依赖 → 需要 AI？需要 WiFi？离线降级？
异常处理 → WiFi 断开？硬件故障？内存不足？
验收条件 → 怎么验证？性能指标？边界条件？
```

**RobotBuddy 核心约束（需求必须满足的硬指标）:**
```
响应延迟: 语音 < 1.5s, 动画 < 50ms, 电机 < 10ms
表情帧率: ≥ 30 FPS
音频质量: 16kHz / 16bit / 单声道
离线模式: 本地唤醒词 + 预设表情 + 基础移动
电池: 活跃 ≥ 4h, 待机 ≥ 24h
```

---

### 3.4 architecture Skill — 架构设计

**调用方式:**
```
请用 architecture skill 设计"云端异步消息推送"的架构方案
```

**设计框架（四层架构）:**
```
Application Layer (Tasks)
     ↓
Service Layer (Managers)
     ↓
Framework Layer (EventBus / StateMachine)
     ↓
Driver Layer (SPI / I2C / I2S / PWM / ...)
     ↓
BSP (Board Support Package)
```

**Task 设计模板:**
```c
TASK_NAME        "my_task"
TASK_STACK_SIZE  4096 bytes
TASK_PRIORITY    3 (0=lowest, 8=highest)
TASK_PERIOD_MS   50 (0=event-driven)
TASK_CORE_ID     0 (PRO_CPU) or 1 (APP_CPU)
```

**通信机制选择:**
```
短消息 (< 256B)     → FreeRTOS Queue
多事件同步          → Event Group
音频流              → Ring Buffer
大数据 (> 1KB)      → Stream Buffer
轻量级通知          → Task Notification
```

---

### 3.5 hardware-driver Skill — 外设驱动

**调用方式:**
```
请用 hardware-driver skill 为 MPU6050 IMU 生成驱动代码
```

**RobotBuddy 驱动清单:**
```
ST7789 LCD        SPI (VSPI)    40MHz, Mode 0, 240×240
INMP441 Mic       I2S           16kHz, 16bit, DMA 4096
MAX98357A Amp     I2S           16kHz, 16bit, 3W
DRV8833 Motor     PWM×2         100Hz-50kHz, 双通道
MPU6050 IMU       I2C (I2C0)    400kHz, 6轴
VL53L0X TOF       I2C (I2C0)    400kHz, 2m range
TCRT5000 IR       ADC/GPIO      阈值式数字输出
TP4056 Charger    GPIO          CHRG/STDBY 状态
Battery ADC       ADC           分压 2:1
```

**驱动代码规范:**
- 所有函数返回 `esp_err_t`
- 线程安全：用 `SemaphoreHandle_t` 保护
- DMA buffer 必须 4 字节对齐 + cache sync
- 设备句柄为不透明指针 (`xxx_ctx_t`)
- 驱动初始化幂等（多次调用安全）

---

### 3.6 audio-pipeline Skill — 音频管道

**调用方式:**
```
请用 audio-pipeline skill 优化音频采集延迟
```

**全链路架构:**
```
麦克风 → I2S DMA → Ring Buffer → VAD (WebRTC) → Opus Enc → WebSocket → Cloud
                                                                          ↓
扬声器 ← MAX98357A ← I2S DMA ← Ring Buffer ← PCM Dec ← WebSocket ← Cloud TTS
```

**性能指标:**
```
采集延迟:    < 10ms
ASR 延迟:    < 500ms
LLM 延迟:    < 500ms
TTS 首字:    < 300ms
端到端:      < 1.5s (用户说完→开始播放回复)
音频丢帧率:  0%
播放卡顿:    < 1次/10min
```

---

### 3.7 display-engine Skill — 表情引擎

**调用方式:**
```
请用 display-engine skill 添加"收到新邮件"的 EXCITED 表情动画
```

**11 种表情状态与触发场景:**

| 表情 | 触发场景 | 眼睛特征 | 虹膜颜色 |
|------|---------|---------|---------|
| IDLE | 无交互 | 微动、眨眼 | 青色 |
| LISTENING | 语音输入中 | 放大、波纹 | 青色 |
| THINKING | LLM 处理 | 左右移动、加载圈 | 蓝色 |
| ANSWERING | TTS 播放 | 嘴部随音动 | 青色 |
| HAPPY | 编译成功 ✅ | 弯月眼 ^_^ | 绿色 |
| ERROR | 编译失败 ❌ | 怒眼 ×_× | 红色 |
| WARNING | 警告 ⚠️ | 闪烁 | 黄色 |
| CONFUSED | 没理解 ❓ | 歪头 ?_? | 青色 |
| FOCUS | 番茄钟 | 微闭 | 暗蓝 |
| SLEEP | 超时休眠 | 闭眼 | 黑色 |
| EXCITED | 新通知 | 跳动 ✨ | 金色 |

**渲染参数:**
```c
eye_radius, eye_spacing   — 眼睛大小和间距
iris_radius, iris_x/y     — 虹膜大小和视线方向
pupil_radius              — 瞳孔大小
eyelid_top/bottom         — 眼皮位置 (0=睁, 1=闭)
iris_color                — 虹膜 RGB565
highlight_enabled         — 高光点开关
```

---

### 3.8 motion-control Skill — 运动控制

**调用方式:**
```
请用 motion-control skill 校准 N20 电机的 PID 参数
```

**运动控制管道:**
```
Behavior Mgr (100Hz) → Motion Planner → PID ×2 → PWM ×2 → DRV8833 → N20 ×2
                                            ↑
                        IMU Fusion (Yaw) ← MPU6050
                        Edge IR ← ITR20001 → Emergency Stop
```

**编程场景的运动行为:**
```
编译中     → 原地微动 + 转圈
编译成功   → 原地旋转一圈庆祝
编译失败   → 后退一步
新消息     → 前进一小步
番茄钟提醒 → 移动到用户面前 + 摇摆
空闲       → 随机漫步
```

---

### 3.9 freertos-system Skill — 系统架构

**调用方式:**
```
请用 freertos-system skill 审计当前所有 Task 的栈使用情况
```

**核心组件:**
```
Task Registry    — 集中管理所有 Task 配置
EventBus         — 中心化事件发布/订阅
SysMon           — 运行时健康监测 (栈/堆/CPU/看门狗)
Power Manager    — 电源模式切换 (Active/Light/Deep Sleep)
```

**SysMon 报告内容:**
- 每个 Task 栈水位
- DRAM/PSRAM 空闲量
- 双核 CPU 使用率
- 各 Task 运行时间统计
- 系统运行时长
- Watchdog 复位次数

---

### 3.10 cloud-communication Skill — 云端通信

**调用方式:**
```
请用 cloud-communication skill 实现断线重连的指数退避逻辑
```

**通信栈:**
```
AI Dialog / Build Status / Git Notify / OTA
        ↓           ↓           ↓       ↓
    REST API    WebSocket     MQTT    HTTPS
        ↓           ↓           ↓       ↓
              TLS / mbedTLS
                    ↓
              WiFi Manager (连接/断线/重连/信号)
```

**Cloud API Endpoints:**
```
POST /v1/chat              — AI 对话
WS   /v1/asr/stream        — 语音识别流
WS   /v1/tts/stream        — 语音合成流
PUT  /v1/build/status      — 编译状态
PUT  /v1/git/status        — Git 状态
GET  /v1/ota/check         — OTA 检查
```

**MQTT Topics:**
```
robotbuddy/{id}/build/status     PC → Robot (QoS 1)
robotbuddy/{id}/git/status       PC → Robot (QoS 1)
robotbuddy/{id}/notification     PC/Cloud → Robot (QoS 1)
robotbuddy/{id}/ota/command      Cloud → Robot (QoS 2)
robotbuddy/{id}/status           Robot → Cloud (QoS 0, 60s)
robotbuddy/{id}/heartbeat        Robot → Cloud (QoS 0, 10s)
```

---

### 3.11 ide-integration Skill — IDE 集成

**调用方式:**
```
请用 ide-integration skill 设计 VS Code 编译状态推送的消息格式
```

**集成架构:**
```
VS Code Plugin → Local Bridge (ws://localhost:9527) → MQTT → RobotBuddy
Git Webhook    →┘                                   CI/CD  →┘
```

**版本规划:**
```
V1.0 — 无集成（手动）
V2.0 — VS Code 编译状态推送
V2.1 — Git 状态通知
V2.2 — CI/CD 通知
V2.3 — 番茄钟联动
V3.0 — 终端命令联动
```

---

### 3.12 coding Skill — 编码规范

**调用方式:**
```
请用 coding skill 审查这段代码的内存安全性
```

**核心规范要点:**
- 返回类型：所有可失败函数 → `esp_err_t`
- 内存：DRAM > PSRAM 选择策略，必须检查 malloc 返回值
- 并发：共享资源必须有互斥锁，ISR 仅使用 FromISR API
- 日志：TAG 统一，按级别使用 ESP_LOGE/W/I/D/V
- 禁止：delay 阻塞、while(1) 忙等、ISR 中 printf、递归、不检查返回值

**13 条强制禁止项:** 详见 `standards/embedded-coding.md`

---

### 3.13 review Skill — 代码审查

**调用方式:**
```
请用 review skill 审查 PR #42 的改动
```

**7 维审查:**
```
1. 编译与静态分析 — 0 error, 0 warning
2. 内存安全性     — malloc/free 配对, 无泄漏, 栈安全
3. 并发安全性     — 互斥锁正确, 无死锁, ISR 安全
4. 实时性         — ISR < 10μs, 音频不丢帧, 30FPS
5. 错误处理       — 所有返回值检查, 降级路径
6. 功耗管理       — 无忙等, WiFi 休眠, 屏幕低功耗
7. 代码可维护性   — 命名清晰, 无魔法数字, 函数 ≤ 50行
```

---

### 3.14 testing Skill — 测试体系

**调用方式:**
```
请用 testing skill 为 audio_manager 模块生成 HIL 测试用例
```

**4 级测试金字塔:**
```
L4: 用户验收 (发布前, 真实使用 1 周)
    ↑
L3: 系统集成 (发布前, 24h 压力测试)
    ↑
L2: HIL 硬件在环 (每次 PR, 真实 ESP32)
    ↑
L1: 单元测试 (每次 Commit, PC/QEMU + CMock)
```

---

### 3.15 document Skill — 文档生成

**调用方式:**
```
请用 document skill 为 I2S 音频模块生成 API 参考文档
```

**文档清单:**
```
docs/
├── 桌面机器人设计需求.md      ← 产品 PRD (Source of Truth)
├── architecture/               ← 架构文档
├── hardware/                   ← BOM + 原理图 + 引脚表
├── firmware/                   ← 构建/烧录/API 参考
├── cloud/                      ← API 规范 + OTA 指南
├── ide-plugins/                ← VS Code + Bridge 配置
├── testing/                    ← 测试计划 + 报告
├── user/                       ← 快速入门 + 语音命令 + FAQ
└── design/                     ← 3D 模型 + 外观规格
```

---

## 4. Master 编排流程

### 4.1 标准开发流程

```
                     ┌──────────────┐
                     │   用户需求    │
                     └──────┬───────┘
                            ↓
              ┌─────────────────────────┐
              │  1. requirement Skill   │ → 需求分析文档
              └────────────┬────────────┘
                           ↓
              ┌─────────────────────────┐
              │  2. architecture Skill  │ → 架构设计文档
              └────────────┬────────────┘
                           ↓
              ┌─────────────────────────┐
              │  3. 领域 Skills（按需）  │
              │  hardware-driver        │
              │  audio-pipeline         │ → 模块设计 + 代码
              │  display-engine         │
              │  motion-control         │
              │  cloud-communication    │
              │  ide-integration        │
              └────────────┬────────────┘
                           ↓
              ┌─────────────────────────┐
              │  4. freertos-system     │ → 系统集成
              └────────────┬────────────┘
                           ↓
              ┌─────────────────────────┐
              │  5. coding Skill        │ → 实现代码
              └────────────┬────────────┘
                           ↓
              ┌─────────────────────────┐
              │  6. testing Skill       │ → 测试报告
              └────────────┬────────────┘
                           ↓
              ┌─────────────────────────┐
              │  7. review Skill        │ → 审查报告
              └────────────┬────────────┘
                           ↓
              ┌─────────────────────────┐
              │  8. document Skill      │ → 更新文档
              └─────────────────────────┘
```

### 4.2 跳过规则

不是每个阶段都必须执行。master skill 的调度规则：

| 任务类型 | 必须的 Skills | 跳过的 Skills |
|----------|-------------|--------------|
| **纯软件功能**（新表情/新对话逻辑） | requirement → architecture → coding → testing → review | hardware-driver, motion-control |
| **新传感器** | requirement → architecture → hardware-driver → coding → testing | audio, display, cloud |
| **音频相关** | requirement → architecture → audio → coding → testing | motion, display |
| **显示相关** | requirement → architecture → display → coding → testing | motion, audio |
| **云端功能** | requirement → architecture → cloud → coding → testing | hardware-driver, motion |
| **Bug 修复** | coding → testing → review | requirement, architecture |
| **架构重构** | requirement → architecture → coding → testing → review | hardware-driver |

---

## 5. 场景化使用示例

### 5.1 场景：新增一个表情动画

**用户输入:**
```
/feature
我想给 RobotBuddy 新增一个 "收到 GitHub Star" 的 CELEBRATE 庆祝表情：
眼睛变成金色星星，原地跳两下
```

**执行过程:**

1. Claude 自动调用 `master` skill 识别任务类型 → 纯软件功能
2. 调用 `requirement` skill 分析：
   - 触发条件：MQTT 收到 `github/star` 消息
   - 硬件依赖：仅屏幕+电机
   - 验收条件：表情动画清晰、运动安全
3. 调用 `architecture` skill 设计：
   - 新增 `EMOTION_CELEBRATE` 枚举值
   - 事件总线新增 `EVENT_GITHUB_STAR`
   - 行为管理器关联表情+运动
4. 调用 `display-engine` skill 实现：
   - 设计金色星星眼睛帧序列
   - 注册到表情状态机
5. 调用 `motion-control` skill 实现：
   - "原地跳两下"的安全运动序列
6. 调用 `coding` skill 编码实现
7. 调用 `testing` skill 验证
8. 调用 `review` skill 审查
9. 输出完整代码 + 测试报告

---

### 5.2 场景：新增 TOF 传感器

**用户输入:**
```
/hardware-bringup
请帮我调通 VL53L1X TOF 激光测距传感器
连接到 I2C0 (SDA=GPIO21, SCL=GPIO22)
```

**执行过程:**

1. 读取 VL53L1X datasheet → 提取寄存器信息
2. 调用 `hardware-driver` skill → 生成驱动框架：
   - `drivers/sensor/vl53l1x.h` + `vl53l1x.c`
   - 初始化序列：上电→读 WHO_AM_I→配置 Range→开始测距
3. 硬件验证：
   - 万用表确认 VCC/GND 无误
   - 上电后 I2C scan 发现 0x29 地址
   - 回读 WHO_AM_I 寄存器 = 0xEA 确认 ✅
4. 功能测试：
   - 10cm/50cm/100cm 测距精度 < 5%
5. 集成：注册到 sensor_manager，发布 SENSOR_TOF_DATA 事件

---

### 5.3 场景：诊断内存泄漏

**用户输入:**
```
/debug
机器人运行 2 小时后 PSRAM free 从 6MB 降到 500KB，然后重启
```

**执行过程:**

1. 收集信息：
   - 串口日志中的 heap 状态
   - 启用 `CONFIG_HEAP_TRACING=y` 重新编译
2. 复现测试：
   - 运行 30min，dump heap trace
   - 发现 `audio_stream_handler.c:156` 的 malloc 未配对 free
3. 定位根因：
   - TTS WebSocket 每次接收音频块时 `malloc` 新 buffer
   - 播放完成后未 `free`
4. 修复验证：
   - 添加对应的 `free()` 调用
   - 24h 压力测试 → PSRAM 稳定在 5.8MB+

---

### 5.4 场景：发布新版本

**用户输入:**
```
/release
发布 V1.1.0
新增功能：
- 代码片段滚动显示
- 番茄钟 25+5 模式
修复：
- WiFi 断线后不自动重连 (issue #23)
- 眨眼动画偶尔卡住 (issue #31)
```

**执行过程:**

1. 发布前检查：
   - `/review` 全维度 → 0 Critical
   - 24h 压力测试 → 通过
2. 版本管理：
   - `app_version.h`: `"1.0.0"` → `"1.1.0"`
3. CHANGELOG:
   ```markdown
   ## [1.1.0] - 2026-07-05
   ### Added
   - 代码片段滚动显示功能
   - 番茄钟 25+5 专注模式
   ### Fixed
   - WiFi 断线自动重连 (#23)
   - 眨眼动画冻结问题 (#31)
   ```
4. 构建：
   - `idf.py build` → `firmware_v1.1.0.bin`
   - SHA256: `a1b2c3...`
5. 归档：
   - `git tag v1.1.0 && git push --tags`
   - 上传 OTA 固件到服务器

---

## 6. Skills 调用关系图

### 6.1 依赖关系

```
master (编排器)
  ├── requirement ──────────────────────┐
  ├── architecture ─────────────────────┤
  ├── hardware-driver ──────────────────┤
  │     └── 使用: SPI/I2C/I2S/PWM/GPIO  │
  ├── audio-pipeline ───────────────────┤
  │     └── 依赖: hardware-driver (I2S) │
  ├── display-engine ───────────────────┤
  │     ├── 依赖: hardware-driver (SPI) │
  │     └── 协作: freertos-system       │
  ├── motion-control ───────────────────┤
  │     └── 依赖: hardware-driver (PWM) │
  ├── freertos-system ──────────────────┤
  │     └── 被多个 Skills 依赖          │
  ├── cloud-communication ──────────────┤
  │     ├── 依赖: freertos-system       │
  │     └── 协作: audio-pipeline        │
  ├── ide-integration ──────────────────┤
  │     └── 依赖: cloud-communication   │
  ├── coding ───────────────────────────┤
  │     └── 使用所有 Skills 的输出      │
  ├── testing ──────────────────────────┤
  ├── review ───────────────────────────┤
  └── document ─────────────────────────┘
```

### 6.2 Commands → Skills 映射

```
/feature           → master → requirement → architecture → [domain skills] → coding → testing → review → document
/hardware-bringup  → hardware-driver → testing
/firmware          → architecture → freertos-system → coding → testing → review
/review            → review
/release           → review → document
/debug             → (各种诊断方法，按问题类型选择)
```

---

## 7. Templates 使用说明

### 7.1 `hardware-driver.md` — 驱动文档模板

**何时使用:**
- 每完成一个硬件驱动开发后
- 新成员接手驱动维护时参考

**填写要点:**
- 引脚分配表务必与实际原理图一致
- 初始化序列要按步骤列出（调试时对照检查）
- API 参考用 Doxygen 格式，可从代码注释自动提取
- 已知问题 section 保持更新

### 7.2 `component-bom.md` — 物料清单模板

**何时使用:**
- V1.0 原型验证阶段
- 每次 BOM 变更（替换元件、新增传感器）
- 成本核算

**填写要点:**
- 采购链接保持有效（或用型号替代）
- 替代件/备选型号记录清楚差异
- 批次总成本定期更新

### 7.3 `api-protocol.md` — API 协议模板

**何时使用:**
- 新增云端 API 端点
- 修改现有 API 参数/响应格式
- 安全审计

**填写要点:**
- 每个端点提供 Request/Response 完整 JSON 示例
- 错误码列表保持和实际 API 一致
- MQTT Topic 统一管理，避免命名冲突

---

## 8. Standards & Checklists 使用说明

### 8.1 `embedded-coding.md` — 编码标准

**执行方式:**
- `coding` skill 内化为编程规范
- `review` skill 作为审查基准
- CI 中配置 `clang-format` + `cppcheck` 自动检查

**13 条强制禁止项速查:**
```
❌ delay()/_delay_ms()    → vTaskDelay()
❌ while(1); 忙等         → Queue/Event/Semaphore
❌ 递归函数               → 迭代
❌ 可变参数函数           → 结构体+数组
❌ malloc 不检查返回值    → 检查 NULL
❌ 动态分配无 free        → RAII 模式
❌ ISR 中 printf/日志     → ESP_EARLY_LOGx
❌ ISR 中非 FromISR API   → FromISR 版本
❌ 释放后使用指针         → 立即置 NULL
❌ 函数超过 50 行         → 拆分
❌ 超过 5 个参数          → 结构体封装
❌ 魔法数字               → 命名常量
❌ 注释掉的旧代码         → 删除
```

### 8.2 `pre-commit.md` — 提交检查清单

**三级检查体系:**

| 级别 | 耗时 | 触发时机 | 内容 |
|------|------|---------|------|
| **快速** | < 5min | 每次 commit | 编译通过 + 命名规范 + 无调试残留 |
| **完整** | ≥ 15min | 每个 PR | 内存/并发/错误/实时/硬件/测试/文档 |
| **发布前** | ≥ 24h | 每个版本 | 完整检查 + 24h 压力 + 全部功能 + 功耗 + OTA |

**快速检查中最重要的 5 项（必须过）:**
1. `idf.py build` 0 error, 0 warning
2. 无 TODO/FIXME 遗留（除非有 Issue 号）
3. 无注释掉的旧代码
4. Commit message 格式正确
5. 无意外提交的临时文件

---

## 9. 最佳实践

### 9.1 开发前：先问 master

在开始任何开发前，先描述你的需求，让 `master` skill 判断该走哪些阶段：

```
我要给 RobotBuddy 增加一个功能：[功能描述]
请用 master skill 告诉我需要哪些步骤
```

这能避免走进死胡同——比如直接写代码却发现架构设计有缺陷。

### 9.2 硬件先行，软件跟进

RobotBuddy 是嵌入式项目，硬件稳定性是软件的前提：

- 新传感器 → 先用 `hardware-bringup` 调通驱动
- 引脚冲突 → 对照 BOM 表确认 GPIO 空闲
- 时序问题 → 示波器确认再优化代码

### 9.3 小步提交，频繁审查

- 每个 commit 做一次快速检查（`pre-commit.md` 快速级）
- 每个 PR 做一次完整检查 + `/review`
- 发现 Critical 问题立即修复，不积累

### 9.4 文档即代码

- 修改 API → 同步更新 `api-protocol.md`
- 新增驱动 → 套用 `hardware-driver.md` 模板
- 改变引脚 → 更新 BOM 和原理图
- Commit message 写清楚"为什么"改

### 9.5 测试驱动调试

遇到问题时的优先级：
1. 先跑已有测试 → 确认哪个模块坏了
2. 缩小范围 → 单独测试可疑模块
3. 加日志 → 定位具体代码行
4. 修 + 加新测试 → 防止回归

---

## 10. 常见问题

### Q1: Skills 和 Commands 有什么区别？

- **Commands**（`/feature`、`/review` 等）是**工作流**——定义了从 A 到 B 的完整步骤序列
- **Skills** 是**专业能力**——每个 Skill 是一个领域的专家，由 Command 或用户按需调用

打个比方：Commands 是菜谱（红烧肉的步骤），Skills 是厨师（切菜的、掌勺的）。

### Q2: 我能跳过某些 Skills 直接写代码吗？

可以。简单任务不需要走全流程。例如：
- 改一个常量值 → 直接改，快速检查即可
- 修复一个小 Bug → `/debug` 诊断 → 修 → 测试
- 加一个日志 → 直接改

判断标准：如果改动涉及**架构决策**或**多个模块交互**，请走完整流程。

### Q3: 如何知道我的 Task 栈大小是否安全？

在 `freertos-system` skill 的 sysmon 中有栈水位监控。运行一段时间后：
```c
UBaseType_t high_water = uxTaskGetStackHighWaterMark(NULL);
// 剩余 ≥ 512 bytes → 安全
// 剩余 < 256 bytes → 立即增大 2x
```

### Q4: 我的 WebSocket 音频流卡顿怎么办？

用 `audio-pipeline` skill 的诊断流程：
1. 检查 I2S DMA buffer → 是否 ≥ 4096？
2. 检查 WiFi RSSI → 是否 < -70 dBm？
3. 检查音频 Task CPU 使用率 → 是否被其他 Task 抢占？
4. 检查 PSRAM 带宽 → cache sync 是否正确？

### Q5: OTA 升级失败怎么回滚？

RobotBuddy 有自动回滚机制：
- 启动后 3 次 watchdog 重启 → 自动回滚
- 也可以手动触发: 调用 `cloud-communication` skill 的 `ota_rollback()`

### Q6: 想加一个新的 Skill 怎么操作？

1. 在 `.claude/skills/<name>/` 下创建 `SKILL.md`
2. 参考现有 Skill 的结构：Role → Domain → Goal → Inputs → Outputs → Rules → Checklist
3. 如果涉及新流程，在 `master` skill 的编排规则中添加
4. 如果有新的 Command 需要它，在对应 command.md 中引用

---

## 附录 A: 文件结构速查

```
.claude/
├── commands/
│   ├── feature.md              ← /feature — 功能开发全流程
│   ├── hardware-bringup.md     ← /hardware-bringup — 硬件调通
│   ├── firmware.md             ← /firmware — 固件开发
│   ├── review.md               ← /review — 代码审查
│   ├── release.md              ← /release — 版本发布
│   └── debug.md                ← /debug — 固件调试
├── skills/
│   ├── master/SKILL.md         ← 编排器
│   ├── requirement/SKILL.md    ← 需求分析
│   ├── architecture/SKILL.md   ← 架构设计
│   ├── hardware-driver/SKILL.md ← 外设驱动
│   ├── audio-pipeline/SKILL.md ← 音频管道
│   ├── display-engine/SKILL.md ← 表情引擎
│   ├── motion-control/SKILL.md ← 运动控制
│   ├── freertos-system/SKILL.md ← FreeRTOS系统
│   ├── cloud-communication/SKILL.md ← 云端通信
│   ├── ide-integration/SKILL.md ← IDE集成
│   ├── coding/SKILL.md         ← 编码实现
│   ├── review/SKILL.md         ← 代码审查
│   ├── testing/SKILL.md        ← 测试验证
│   └── document/SKILL.md       ← 文档生成
├── templates/
│   ├── hardware-driver.md      ← 驱动文档模板
│   ├── component-bom.md        ← BOM 模板
│   └── api-protocol.md         ← API 协议模板
├── standards/
│   └── embedded-coding.md      ← 编码标准
├── checklists/
│   └── pre-commit.md           ← 提交检查清单
└── settings.local.json         ← 本地权限配置
```

## 附录 B: 快速命令参考

| 我想做什么 | 使用的命令/Skill |
|-----------|----------------|
| 开发一个新功能 | `/feature` |
| 调通一个新硬件 | `/hardware-bringup` |
| 写一个 FreeRTOS 模块 | `/firmware` |
| 审查代码质量 | `/review` |
| 发布新版本 | `/release` |
| 诊断 Bug | `/debug` |
| 分析需求 | `requirement` skill |
| 设计架构 | `architecture` skill |
| 写驱动代码 | `hardware-driver` skill |
| 优化音频 | `audio-pipeline` skill |
| 设计表情 | `display-engine` skill |
| 调电机 PID | `motion-control` skill |
| 检查系统健康 | `freertos-system` skill |
| 配网/OTA | `cloud-communication` skill |
| VS Code 集成 | `ide-integration` skill |
| 写代码 | `coding` skill |
| 写测试 | `testing` skill |
| 写文档 | `document` skill |
| 查看编码规范 | `standards/embedded-coding.md` |
| 提交前检查 | `checklists/pre-commit.md` |
| 填 BOM 表 | `templates/component-bom.md` |
| 定义 API | `templates/api-protocol.md` |
| 写驱动文档 | `templates/hardware-driver.md` |

---

> **文档版本:** 1.0
> **适用范围:** RobotBuddy 全生命周期开发
> **维护者:** 项目团队
> **相关文档:** `桌面机器人设计需求.md` (产品 PRD), `embedded-async-framework-skills` (参考框架)
