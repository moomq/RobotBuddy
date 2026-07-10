# RobotBuddy Skill 使用指南

> 本文档详细说明 RobotBuddy 项目的 Claude Code Skills 和 Commands 体系的使用方法。

**版本:** 2.0
**最后更新:** 2026-07-10
**适用范围:** RobotBuddy 全生命周期开发 (V1.0 MVP → V2.0 增强 → V3.0 专业)

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
11. [版本-Skill 对照矩阵](#11-版本-skill-对照矩阵)
12. [任务决策树](#12-任务决策树)

---

## 1. 体系概述

### 1.1 设计理念

RobotBuddy 的 `.claude/` 体系采用 **Command → Skill → Template** 三层架构：

```
┌──────────────────────────────────────────────────────┐
│                   Commands (工作流)                    │
│   定义"做什么"的端到端流程，串联多个 Skills              │
│   /feature  /hardware-bringup  /firmware  /review     │
│   /release  /debug  /scaffold  /flash  /simulate      │
│   /status  /pin-check  /test  /clean  /deps           │
│   /calibrate  /log  /deploy                            │
├──────────────────────────────────────────────────────┤
│                   Skills (专业能力)                    │
│   定义"怎么做"的专业知识和执行规范                       │
│   【流程编排】master                                    │
│   【设计类】requirement  architecture                  │
│   【硬件域】hardware-driver  sensor-fusion  bsp        │
│          calibration  3d-structure                    │
│   【软件域】freertos-system  coding  lvgl-ui          │
│          display-engine  behavior-system              │
│   【音频/语音】audio-pipeline  voice                   │
│   【运动/电源】motion-control  power-management        │
│   【联网/AI】cloud-communication  wifi-setup           │
│          ai-dialog  ota  ide-integration              │
│   【质量域】testing  review  document                  │
│   【进阶】local-ai  benchmark                          │
├──────────────────────────────────────────────────────┤
│              Templates / Standards / Checklists       │
│   定义"做成什么样"的输出模板和质量标准                   │
│   Templates: hardware-driver, component-bom,          │
│     api-protocol, task-design, calibration-guide,     │
│     architecture-design, test-plan, state-machine     │
│   Standards: embedded-coding, python-coding,          │
│     api-design, protocol-buffer                      │
│   Checklists: pre-commit, hardware-test,              │
│     safety-checklist, release-checklist              │
└──────────────────────────────────────────────────────┘
```

### 1.2 使用方式

所有 Commands 和 Skills 均通过 **Claude Code 对话界面** 调用：

- **Command**: 直接在对话中输入 `/<command-name>`
- **Skill**: 描述需求时，Claude 会自动匹配相关 Skill 并加载其专业知识
- **手动调用**: 可以明确指定 "调用 xxx skill 来..." 触发特定 Skill

### 1.3 快速寻找帮助

| 我想做什么 | 使用的命令/Skill |
|-----------|----------------|
| 开发一个新功能 | `/feature` |
| 调通一个新硬件 | `/hardware-bringup` |
| 写一个 FreeRTOS 模块 | `/firmware` |
| 快速生成模块代码骨架 | `/scaffold` |
| 编译+烧录+监控 | `/flash` |
| PC 上模拟验证逻辑 | `/simulate` |
| 审查代码质量 | `/review` |
| 运行测试套件 | `/test` |
| 发布新版本 | `/release` |
| 远程部署固件 | `/deploy` |
| 诊断 Bug / Crash | `/debug` |
| 分析日志 / 导出 Core Dump | `/log` |
| 查看项目整体状态 | `/status` |
| 检查引脚冲突 | `/pin-check` |
| 传感器校准 | `/calibrate` |
| 清理构建/环境重置 | `/clean` |
| 检查开发环境依赖 | `/deps` |
| 设计系统架构 | `architecture` skill |
| 分析需求 | `requirement` skill |
| WiFi 配网/管理 | `wifi-setup` skill |
| 写驱动代码 | `hardware-driver` skill |
| 设计板级 BSP | `bsp` skill |
| 设计表情/UI | `display-engine` skill |
| 开发 LVGL UI 界面 | `lvgl-ui` skill |
| 调音频管道 | `audio-pipeline` skill |
| 端到端语音调试 | `voice` skill |
| 调电机 PID | `motion-control` skill |
| 设计行为系统 | `behavior-system` skill |
| 传感器数据融合 | `sensor-fusion` skill |
| 传感器校准 | `calibration` skill |
| 设计 AI 对话 | `ai-dialog` skill |
| 配置云端通信 | `cloud-communication` skill |
| OTA 升级部署 | `ota` skill |
| 电源管理/功耗优化 | `power-management` skill |
| 设计 3D 外壳 | `3d-structure` skill |
| VS Code 集成 | `ide-integration` skill |
| 写代码 | `coding` skill |
| 写测试 | `testing` skill |
| 写文档 | `document` skill |
| 查看编码规范 | `standards/embedded-coding.md` |
| 提交前检查 | `checklists/pre-commit.md` |

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

**涉及新硬件时额外调用:** `hardware-driver` → `sensor-fusion`（如需）→ `calibration`（如需）
**涉及表情/行为时额外调用:** `display-engine` → `behavior-system`
**涉及 AI 对话时额外调用:** `ai-dialog` → `voice`（如需端到端）
**涉及 UI 界面时额外调用:** `lvgl-ui`
**涉及 3D 结构变动时额外调用:** `3d-structure`

---

### 2.2 `/hardware-bringup` — 硬件模块调通

**用途:** 为新硬件组件开发驱动并验证

**触发方式:**
```
/hardware-bringup
请帮我调通 VL53L1X TOF 激光测距传感器，接到 I2C0
```

**执行流程:** ① datasheet 分析 → ② 驱动开发 → ③ 硬件连接 → ④ 驱动验证 → ⑤ 集成

---

### 2.3 `/firmware` — 固件模块开发

**用途:** 开发 FreeRTOS Task 或 Service 模块

**触发方式:**
```
/firmware
请帮我实现番茄钟 Task，25min 专注 + 5min 休息
```

**FreeRTOS 约束速查:**
```
优先级: 音频(7-8) > 显示(5-6) > 运动(3-4) > 业务(1-2) > 监控(0)
栈建议: 音频 8KB / 云端 12KB / 显示 4KB / 简单控制 2KB / 监控 1KB
ISR: 仅 FromISR API, ≤ 10μs, 禁止 printf
```

---

### 2.4 `/scaffold` — 模块脚手架生成

**用途:** 快速生成 RobotBuddy 固件模块的脚手架代码

**支持层级:** driver / service / app / system

**触发方式:**
```
/scaffold
请在 services 层创建一个 "notification_manager" 服务模块
```

**模板包括:** `.c` / `.h` / `CMakeLists.txt` / `Kconfig` / `README.md` / 测试骨架

---

### 2.5 `/flash` — 构建烧录监控

**用途:** 一键完成 RobotBuddy 固件的构建、烧录和串口监控

**触发方式:**
```
/flash
烧录到 COM3 并启动监控
```

**快捷命令:**
```bash
idf.py -p COM3 flash monitor     # 最常用
idf.py build                      # 仅构建
idf.py -p COM3 flash              # 仅烧录
idf.py -p COM3 monitor            # 仅监控
idf.py -p COM3 erase-flash flash monitor  # 完全擦除重烧
```

**烧录验证清单:**
- [ ] 编译 0 error, 0 warning
- [ ] 烧录成功（无超时/校验错误）
- [ ] 串口日志正常启动
- [ ] WiFi 连接成功
- [ ] 屏幕点亮
- [ ] 无 Panic / Watchdog 重启

---

### 2.6 `/simulate` — PC 端模拟运行

**用途:** 在无硬件的情况下，在 PC 上模拟运行 RobotBuddy 的软件逻辑

**触发方式:**
```
/simulate
请在 PC 上模拟 run 行为系统，注入编译成功事件，验证表情和运动联动
```

**模拟范围:** 表情引擎 / 行为系统 / 传感器融合 / 对话管理

**注意:** PC 模拟仅验证逻辑正确性，不替代硬件测试

---

### 2.7 `/review` — 代码与架构审查

**用途:** 多维度固件代码质量审查

**审查维度:** 编译 → 内存安全 → 并发安全 → 实时性 → 错误处理 → 功耗 → 可维护性

**输出格式:** 🔴Critical / 🟡Warning / 🔵Suggestion

---

### 2.8 `/test` — 一键测试 **[NEW]**

**用途:** 运行 RobotBuddy 的各级测试套件

**触发方式:**
```
/test                # 运行所有单元测试
/test --hil          # 运行硬件在环测试
/test --module audio # 仅测试音频模块
/test --stress       # 运行 24h 压力测试
```

**测试级别:**
```
L1: 单元测试 (每次 commit)    — idf.py build + ctest
L2: HIL 测试 (每次 PR)       — 真实 ESP32-S3 硬件
L3: 集成测试 (每周)           — 完整 RobotBuddy
L4: 压力测试 (发布前)         — ≥ 24h 连续运行
```

---

### 2.9 `/release` — 固件发布

**用途:** 将固件从开发状态推进到发布状态

**版本规则:** SemVer — MAJOR.MINOR.PATCH

---

### 2.10 `/deploy` — OTA 远程部署 **[NEW]**

**用途:** 将已构建的固件部署到目标 RobotBuddy 设备

**触发方式:**
```
/deploy                        # 部署到默认设备
/deploy --device RB-0003       # 部署到指定设备
/deploy --ota-url https://...  # 指定 OTA 服务器
/deploy --manual --port COM3   # 手动通过串口烧录
```

---

### 2.11 `/debug` — 固件调试

**用途:** 系统化诊断 ESP32-S3 固件异常

**诊断分类:** Panic/Crash → 内存泄漏 → 栈溢出 → WiFi 断连 → 音频丢帧 → 显示异常 → 电机异常

**常见错误速查:**
```
Guru Meditation: LoadProhibited → NULL/已释放指针
Stack overflow in task xxx    → 栈太小，2x 扩容
heap_caps_alloc failed        → 内存泄漏/不足
I2S: buffer underrun          → DMA buffer 太小
Watchdog trigger              → Task 死循环/阻塞
```

---

### 2.12 `/log` — 日志收集与分析 **[NEW]**

**用途:** 收集、过滤和分析 RobotBuddy 运行时日志

**触发方式:**
```
/log                     # 最近 1000 行
/log --errors            # 仅错误日志
/log --export report.txt # 导出完整日志
/log --coredump          # 解析最近一次 Core Dump
```

---

### 2.13 `/status` — 项目状态一览

**用途:** 快速查看项目当前状态（模块完成度、编译状态、硬件进度、待办事项）

**触发方式:**
```
/status
```

---

### 2.14 `/pin-check` — GPIO 引脚冲突检查

**用途:** 验证 ESP32-S3 GPIO 引脚分配，检测冲突

**ESP32-S3 GPIO 速查:**
```
✅ 安全可用: GPIO 1-18, 21-23, 35-42
⚠️ Strapping: GPIO 0, 3, 45, 46
❌ 禁止: GPIO 19, 20 (USB), GPIO 26-32 (Flash)
⚡ ADC2 (WiFi时不可用): GPIO 11-20
```

---

### 2.15 `/calibrate` — 传感器校准 **[NEW]**

**用途:** 执行传感器校准流程，确保数据准确

**触发方式:**
```
/calibrate              # 交互式引导校准所有传感器
/calibrate --imu        # 仅校准 IMU
/calibrate --edge       # 仅校准桌面边缘检测
/calibrate --battery    # 仅校准电池 ADC
/calibrate --factory    # 恢复出厂校准数据
```

---

### 2.16 `/clean` — 清理命令 **[NEW]**

**用途:** 清理构建产物、临时文件和重置环境

**触发方式:**
```
/clean                  # 清理构建产物 (idf.py fullclean)
/clean --all            # 完整清理：构建+临时文件+NVS擦除
/clean --nvs            # 仅擦除 NVS 分区（恢复出厂设置）
```

---

### 2.17 `/deps` — 依赖检查 **[NEW]**

**用途:** 检查开发环境依赖是否就绪

**触发方式:**
```
/deps
```

**检查项:**
- ESP-IDF 版本 (≥ v5.1)
- Python 3.8+ & pip packages
- xtensa-esp32s3 toolchain
- CMake ≥ 3.16
- USB 驱动 (CP210x / CH340)
- Git ≥ 2.30

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
| **sensor-fusion** | 传感器融合专家 | MPU6050+TCRT5000+ITR20001+VL53L0X 融合 |
| **bsp** | BSP 板级支持包 | 引脚矩阵/时钟树/启动序列/工厂自检 **[NEW]** |
| **calibration** | 传感器校准 | IMU/边缘/避障/ADC 校准流程 **[NEW]** |
| **3d-structure** | 3D 外壳设计 | 外壳建模/PCB布局/重心分析/组装指南 |

#### 软件域

| Skill | 角色 | 覆盖的子系统 |
|-------|------|------------|
| **freertos-system** | FreeRTOS 系统专家 | Task管理+EventBus+SysMon+电源管理 |
| **coding** | 高级嵌入式工程师 | 编码规范+内存管理+并发安全 |
| **lvgl-ui** | LVGL UI 开发 | ST7789+LVGL+Widgets+代码滚动/番茄钟 UI **[NEW]** |
| **display-engine** | 表情引擎专家 | ST7789/GC9A01 + 11种表情状态机 |
| **behavior-system** | 行为决策专家 | 多模态联动编排+场景调度+番茄钟 |

#### 音频/语音域

| Skill | 角色 | 覆盖的子系统 |
|-------|------|------------|
| **audio-pipeline** | 音频管道专家 | I2S采集→VAD→云端ASR→TTS→I2S播放 |
| **voice** | 语音交互系统 | 唤醒词→VAD→ASR→对话→TTS 端到端 **[NEW]** |

#### 运动/电源域

| Skill | 角色 | 覆盖的子系统 |
|-------|------|------------|
| **motion-control** | 运动控制专家 | N20电机+DRV8833+MPU6050+PID+桌面安全 |
| **power-management** | 电源管理 | 电池监测+充电管理+休眠策略+功耗优化 |

#### 网络/AI 域

| Skill | 角色 | 覆盖的子系统 |
|-------|------|------------|
| **cloud-communication** | 云端通信专家 | WiFi+HTTP/WS+MQTT+TLS |
| **wifi-setup** | WiFi 配网专家 | SmartConfig/BLE/SoftAP/NVS加密 **[NEW]** |
| **ai-dialog** | AI 对话引擎 | LLM路由+Prompt工程+上下文管理+意图分类 |
| **ota** | OTA 升级管理 | 分区管理+签名验证+回滚策略 **[NEW]** |
| **ide-integration** | IDE 集成专家 | VS Code插件+Git Webhook+CI/CD |

#### 质量域

| Skill | 角色 | 覆盖的质量维度 |
|-------|------|--------------|
| **testing** | 测试专家 | 4级测试（单元→HIL→集成→验收） |
| **review** | 代码审查专家 | 7维审查（内存/并发/实时/错误/功耗/维护） |
| **document** | 文档工程师 | 需求/架构/API/用户 全系列文档 |

#### 进阶域 (V2+)

| Skill | 角色 | 覆盖的子系统 |
|-------|------|------------|
| **local-ai** | 本地 AI 推理 | ESP-SR + TensorFlow Lite Micro **[P2]** |
| **benchmark** | 性能基准 | CPU/内存/延迟/FPS 系统性基准 **[P2]** |

---

### 3.2 master Skill — 编排器

**编排规则（包含新增 skills）:**

| 任务类型 | 必须的 Skills | 可选/按需的 Skills |
|----------|-------------|-------------------|
| 纯软件功能（新表情） | requirement→architecture→coding→testing→review | display-engine, behavior-system, lvgl-ui |
| 新传感器接入 | requirement→architecture→hardware-driver→coding→testing | sensor-fusion, calibration, bsp |
| 音频/语音功能 | requirement→architecture→audio-pipeline→coding→testing | voice, cloud-communication, ai-dialog |
| 显示/UI 功能 | requirement→architecture→display-engine→coding→testing | lvgl-ui, behavior-system |
| 云端 AI 功能 | requirement→architecture→cloud→ai-dialog→coding→testing | voice, ota, wifi-setup |
| 运动/控制功能 | requirement→architecture→motion-control→coding→testing | sensor-fusion, power-management |
| 电源/功耗功能 | requirement→architecture→power-management→coding→testing | freertos-system |
| WiFi/网络功能 | requirement→architecture→wifi-setup→coding→testing | cloud-communication, ota |
| 物理结构变更 | requirement→3d-structure→hardware-driver→coding→testing | bsp |
| Bug 修复 | coding→testing→review | (按需调用相关领域skill) |
| 架构重构 | requirement→architecture→coding→testing→review | freertos-system |
| 传感器校准 | calibration→testing | sensor-fusion |
| OTA 相关 | ota→testing | cloud-communication, wifi-setup |

---

### 3.3 各 Skill 详细说明

#### 🆕 wifi-setup — WiFi 配网 **[NEW]**

**调用方式:**
```
请用 wifi-setup skill 实现 SoftAP 配网的完整流程
```

**支持的配网方式:**
| 方式 | 优点 | 缺点 | 适用场景 |
|------|------|------|---------|
| SmartConfig | 一键配网 | 兼容性依赖路由器 | 家庭用户 |
| BLE | 可靠，跨平台 | 需要 APP | 手机 APP 配网 |
| SoftAP | 最可靠，无依赖 | 用户体验一般 | 调试/企业环境 |
| WPS | 简单 | 安全性差 | 不建议生产使用 |

**安全:**
- WiFi 凭证存储在 NVS 加密分区
- MQTT/HTTP Token 独立于 WiFi 密码
- 配网完成后关闭 SoftAP/BLE 广播

---

#### 🆕 lvgl-ui — LVGL UI 开发 **[NEW]**

**调用方式:**
```
请用 lvgl-ui skill 设计番茄钟计时器的 UI 页面
```

**RobotBuddy UI 页面清单:**
| 页面 | 用途 | V1.0 | V2.0 |
|------|------|------|------|
| 主表情页 | 眼睛动画 + 状态指示 | ✅ | ✅ |
| 代码滚动页 | 代码片段滚动显示 | — | ✅ |
| 番茄钟页 | 25+5 倒计时界面 | ✅ | ✅ |
| 设置菜单 | WiFi/音量/亮度 设置 | — | ✅ |
| 通知弹窗 | 编译结果/Git状态弹窗 | ✅ | ✅ |
| 充电动画 | 充电进度+电量显示 | ✅ | ✅ |

**性能约束:**
- LVGL buffer: 1/4 屏幕 (240×60) 节省内存
- 刷新率: 30 FPS (33ms/frame)
- 内存预算: LVGL heap ≤ 64KB

---

#### 🆕 calibration — 传感器校准 **[NEW]**

**调用方式:**
```
请用 calibration skill 校准 MPU6050 陀螺仪零偏
```

**校准矩阵:**

| 传感器 | 校准方法 | 耗时 | 频率建议 |
|--------|---------|------|---------|
| MPU6050 陀螺仪 | 静止采样 200 次取均值 | ~10s | 每次上电 |
| MPU6050 加速度计 | 6 面静止采样 | ~60s | 工厂一次 |
| ITR20001 边缘 | 桌面中心采样 100 次基线 | ~30s | 更换桌面 |
| TCRT5000 避障 | 不同距离采样拟合曲线 | ~60s | 工厂一次 |
| 电池 ADC | 2 点校准 (3.0V + 4.2V) | ~120s | 更换电池 |
| VL53L0X TOF | 已知距离偏移校准 | ~30s | 每月 |

---

#### 🆕 ota — OTA 升级管理 **[NEW]**

**调用方式:**
```
请用 ota skill 设计出厂固件 + 双 OTA 分区 + 自动回滚策略
```

**分区布局:**
```
nvs      (24KB)    — 设备配置
otadata  (8KB)     — OTA 状态
phy_init (4KB)     — PHY 初始化
factory  (2MB)     — 出厂固件（永不覆盖）
ota_0    (2MB)     — OTA 分区 0
ota_1    (2MB)     — OTA 分区 1
spiffs   (4MB)     — 表情/音频资源
```

**回滚策略:** 3 次 watchdog → 自动回滚 | 5 次 panic → 自动回滚 | 健康检查超时 → 回滚 | 回滚失败 → 恢复出厂固件

---

#### 🆕 voice — 语音交互系统 **[NEW]**

**调用方式:**
```
请用 voice skill 优化语音交互的全链路延迟
```

**全链路延迟预算:**
```
Mic → RingBuf:        < 10ms
VAD 检测:             < 30ms
唤醒词检测:            < 200ms
云端 ASR:              < 500ms
LLM 回复:              < 500ms
TTS 首字:              < 300ms
─────────────────────────────
端到端总计:            < 1.5s
```

**ASR 选型建议:**
| 服务 | 准确率 | 延迟 | 中文质量 | 价格 |
|------|--------|------|---------|------|
| Whisper (OpenAI) | 97% | 300ms | 优秀 | $$ |
| 讯飞语音 | 96% | 200ms | 最佳 | ¥ |
| 百度语音 | 95% | 250ms | 很好 | ¥ |

---

#### 🆕 bsp — 板级支持包 **[NEW]**

**调用方式:**
```
请用 bsp skill 审查 RobotBuddy PCB 的引脚分配和上电时序
```

**核心职责:**
- ESP32-S3 引脚矩阵管理（全 48 GPIO 分配表）
- 时钟树配置（XTAL→PLL→CPU/APB/SPI/I2S）
- 启动顺序管理（6 阶段初始化：Critical→Bus→Peripheral→Network→App）
- Strapping 引脚和 eFuse 管理
- 工厂产线自检（I2C scan → SPI test → I2S loopback → PWM test → Sensor check）

---

## 4. Master 编排流程

### 4.1 标准开发流程（更新后）

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
              │  hardware-driver bsp    │
              │  sensor-fusion calibration│
              │  audio-pipeline voice    │
              │  display-engine lvgl-ui  │ → 模块设计 + 代码
              │  motion-control          │
              │  behavior-system         │
              │  ai-dialog               │
              │  cloud-communication     │
              │  wifi-setup ota          │
              │  power-management        │
              │  3d-structure            │
              │  ide-integration         │
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

---

## 5. 场景化使用示例

### 5.1 场景：新增 WiFi 配网功能

```
/wifi-setup + /feature
请用 SoftAP 方式实现 WiFi 配网：
1. 开机无 WiFi → 自动进入 SoftAP 模式
2. 手机连接热点 "RobotBuddy-Setup"
3. 浏览器打开 192.168.4.1 选择 WiFi 并输入密码
4. 配网成功 → 眼睛变 HAPPY + 语音"配网成功"
5. 配网失败 → 眼睛变 ERROR + 语音提示重试
```

执行: `wifi-setup` skill 设计配网流程 → `display-engine` 设计反馈表情 → `coding` 实现 → `testing` 验证

### 5.2 场景：校准所有传感器

```
/calibrate
请引导我校准 RobotBuddy 的所有传感器
```

执行: `calibration` skill 启动交互式校准 → IMU 校准(静止10s) → 边缘校准(放桌面中心) → 避障校准 → ADC校准 → 结果保存NVS

### 5.3 场景：OTA 升级到 V1.1.0

```
/deploy --version 1.1.0
请将 firmware_v1.1.0.bin OTA 部署到设备 RB-0003
```

执行: `ota` skill 验证固件签名 → HTTPS 下载 → 写入 ota_1 分区 → 标记启动分区 → 重启 → 验证新固件 → commit

### 5.4 场景：开发番茄钟 UI 界面

```
/feature + lvgl-ui skill
请为 RobotBuddy 开发番茄钟专用 UI 界面
```

执行: `requirement` 分析 → `architecture` 设计 → `lvgl-ui` 实现 LVGL 页面 → `display-engine` 联动表情 → `behavior-system` 定义行为 → `coding` 编码 → `testing` 验证

---

## 6. Skills 调用关系图

### 6.1 完整依赖关系（更新后）

```
master (编排器)
  ├── requirement ──────────────────────┐
  ├── architecture ─────────────────────┤
  │                                     │
  ├── 【硬件域】                         │
  │   ├── hardware-driver               │
  │   │     └── 使用: SPI/I2C/I2S/PWM   │
  │   ├── bsp ──────────────────────────┤
  │   │     ├── 依赖: hardware-driver   │
  │   │     └── 被引用: pin-check       │
  │   ├── sensor-fusion                 │
  │   │     └── 依赖: hardware-driver   │
  │   ├── calibration                   │
  │   │     ├── 依赖: sensor-fusion     │
  │   │     └── 依赖: NVS (存储)       │
  │   └── 3d-structure                  │
  │         └── 依赖: hardware-driver   │
  │                                     │
  ├── 【软件域】                         │
  │   ├── freertos-system               │
  │   │     └── 被多个 Skill 依赖       │
  │   ├── display-engine                │
  │   │     ├── 依赖: SPI driver        │
  │   │     └── 协作: behavior-system   │
  │   ├── lvgl-ui                       │
  │   │     ├── 依赖: display-engine    │
  │   │     └── 依赖: SPI driver        │
  │   ├── behavior-system               │
  │   │     ├── 依赖: display-engine    │
  │   │     ├── 依赖: motion-control    │
  │   │     ├── 依赖: audio-pipeline    │
  │   │     └── 依赖: power-management  │
  │   └── coding                        │
  │         └── 使用所有 Skill 的输出   │
  │                                     │
  ├── 【音频/语音域】                     │
  │   ├── audio-pipeline                │
  │   │     └── 依赖: I2S driver        │
  │   └── voice                         │
  │         ├── 依赖: audio-pipeline    │
  │         ├── 依赖: ai-dialog         │
  │         └── 依赖: cloud-communication│
  │                                     │
  ├── 【运动/电源域】                     │
  │   ├── motion-control                │
  │   │     ├── 依赖: PWM driver        │
  │   │     └── 依赖: sensor-fusion     │
  │   └── power-management              │
  │         ├── 依赖: ADC driver        │
  │         └── 协作: freertos-system   │
  │                                     │
  ├── 【网络/AI 域】                      │
  │   ├── wifi-setup                    │
  │   │     └── 依赖: NVS driver        │
  │   ├── cloud-communication           │
  │   │     ├── 依赖: wifi-setup        │
  │   │     └── 协作: audio-pipeline    │
  │   ├── ai-dialog                     │
  │   │     └── 依赖: cloud-communication│
  │   ├── ota                           │
  │   │     ├── 依赖: wifi-setup        │
  │   │     └── 依赖: cloud-communication│
  │   └── ide-integration               │
  │         └── 依赖: cloud-communication│
  │                                     │
  ├── 【质量域】                         │
  │   ├── testing ──────────────────────┤
  │   ├── review ───────────────────────┤
  │   └── document ─────────────────────┘
  │
  └── 【进阶域 (V2+)】
      ├── local-ai
      │     └── 依赖: ESP-SR, TFLite Micro
      └── benchmark
            └── 依赖: freertos-system, sysmon
```

### 6.2 Commands → Skills 映射（更新后）

```
/feature           → master → requirement → architecture → [domain skills*] → coding → testing → review → document
/hardware-bringup  → hardware-driver → sensor-fusion(如需) → calibration(如需) → testing
/firmware          → architecture → freertos-system → coding → testing → review
/scaffold          → (独立，不调用其他skills)
/flash             → (独立，idf.py 工具链)
/simulate          → (独立，HAL Mock)
/review            → review
/test              → testing
/release           → review → testing → document → ota(如需)
/deploy            → ota → cloud-communication
/debug             → (诊断方法，按问题类型选择)
/log               → (独立，日志工具)
/status            → (独立，项目扫描)
/pin-check         → bsp(如需)
/calibrate         → calibration → testing
/clean             → (独立，idf.py 工具链)
/deps              → (独立，环境检测)
```

---

## 7. Templates 使用说明

### 7.1 现有模板

| 模板 | 用途 | 何时使用 |
|------|------|---------|
| `hardware-driver.md` | 驱动文档模板 | 完成硬件驱动开发后 |
| `component-bom.md` | 物料清单模板 | BOM 变更或成本核算 |
| `api-protocol.md` | API 协议模板 | 新增/修改云端 API |

### 7.2 建议新增模板

| 模板 | 用途 | 位置 |
|------|------|------|
| `task-design.md` | FreeRTOS Task 设计卡片 | `.claude/templates/task-design.md` |
| `calibration-guide.md` | 传感器校准步骤记录 | `.claude/templates/calibration-guide.md` |
| `architecture-design.md` | 架构设计文档模板 | `.claude/templates/architecture-design.md` |
| `test-plan.md` | 测试计划模板 | `.claude/templates/test-plan.md` |
| `state-machine.md` | 状态机设计模板 | `.claude/templates/state-machine.md` |

---

## 8. Standards & Checklists 使用说明

### 8.1 现有标准

| 标准 | 用途 |
|------|------|
| `embedded-coding.md` | ESP32 C 编码规范（13 条强制禁止项） |

### 8.2 建议新增标准

| 标准 | 用途 | 触发时机 |
|------|------|---------|
| `python-coding.md` | Local Bridge Python 编码规范 | V2+ IDE 集成开发 |
| `api-design.md` | REST/WebSocket/MQTT API 设计标准 | 云端 API 开发 |
| `protocol-buffer.md` | Protobuf 消息定义规范 | IDE 集成数据交换 |

### 8.3 现有检查清单

| 清单 | 等级 | 触发时机 |
|------|------|---------|
| `pre-commit.md` | 快速/完整/发布前 | commit/PR/release |

### 8.4 建议新增检查清单

| 清单 | 用途 | 触发时机 |
|------|------|---------|
| `hardware-test.md` | 硬件模块调通验证清单 | hardware-bringup 后 |
| `safety-checklist.md` | 桌面安全 + 网络安全 | release 前 |
| `release-checklist.md` | 发布检查 | 每个版本发布前 |

---

## 9. 最佳实践

### 9.1 开发前：先问 master

```
我要给 RobotBuddy 增加一个功能：[功能描述]
请用 master skill 告诉我需要哪些步骤
```

### 9.2 硬件先行，软件跟进

- 新传感器 → `hardware-bringup` → `calibration` → 集成
- 引脚冲突 → `pin-check` → `bsp` skill 重新分配
- 时序问题 → 示波器确认 → 再优化代码

### 9.3 小步提交，频繁审查

- commit → `pre-commit.md` 快速检查
- PR → 完整检查 + `/review`
- 发布 → `/test --stress` + `release-checklist`

### 9.4 文档即代码

- 修改 API → 同步 `api-protocol.md`
- 新增驱动 → 套用 `hardware-driver.md` 模板
- 改变引脚 → 更新 BOM 和 `pin-check`
- 新 Task → `task-design.md` 卡片

### 9.5 Skills 选择决策流程

```
需要改硬件? → [Y] → 涉及新芯片? → hardware-driver
              [Y] → 涉及引脚?   → bsp + pin-check
              [Y] → 涉及传感器? → sensor-fusion → calibration
              [Y] → 涉及外壳?   → 3d-structure

需要改软件? → [Y] → 涉及UI/表情? → display-engine + lvgl-ui
              [Y] → 涉及语音?   → audio-pipeline + voice
              [Y] → 涉及AI?     → ai-dialog
              [Y] → 涉及运动?   → motion-control
              [Y] → 涉及WiFi?   → wifi-setup
              [Y] → 涉及OTA?    → ota
              [Y] → 涉及电源?   → power-management
              [Y] → 涉及行为?   → behavior-system

需要改代码? → coding → testing → review

需要发布? → release → deploy
```

---

## 10. 常见问题

### Q1: Skills 和 Commands 有什么区别？

- **Commands**（`/feature`、`/review` 等）是**工作流** — 定义了从 A 到 B 的完整步骤序列
- **Skills** 是**专业能力** — 每个 Skill 是一个领域的专家，由 Command 或用户按需调用

### Q2: 新增 skill 和 command 如何组织？

1. `.claude/skills/<name>/SKILL.md` → 定义专业能力
2. `.claude/commands/<name>.md` → 定义工作流命令
3. `master/SKILL.md` → 注册新 skill 到编排规则
4. 本文档 → 添加使用说明

### Q3: LVGL UI 和 display-engine 怎么分工？

- `display-engine` 负责表情渲染：眼睛动画、眨眼、虹膜颜色、视线方向
- `lvgl-ui` 负责功能性 UI：代码滚动、番茄钟、设置菜单、通知弹窗
- 两者可以共存：表情引擎在背景渲染，LVGL widget 覆盖在表情之上

### Q4: voice 和 audio-pipeline 怎么分工？

- `audio-pipeline` 负责音频硬件层：I2S 驱动、DMA buffer、Ring Buffer、WebSocket 音频流
- `voice` 负责语音业务逻辑：唤醒词管理、VAD 策略、ASR/TTS 选型、对话流状态机
- 关系：voice 调用 audio-pipeline 的 API，voice 不直接操作 I2S

### Q5: calibration 和 sensor-fusion 怎么分工？

- `sensor-fusion` 负责传感器数据融合：滤波、姿态计算、安全判定
- `calibration` 负责传感器校准流程：零偏采样、阈值计算、NVS 存储
- 关系：calibration 的产出（校准参数）是 sensor-fusion 的输入

### Q6: ota 和 cloud-communication 怎么分工？

- `cloud-communication` 提供传输层能力：HTTPS 下载、TLS、MQTT 通知
- `ota` 负责固件升级业务逻辑：分区管理、签名验证、回滚策略
- 关系：ota 使用 cloud-communication 的网络能力

---

## 11. 版本-Skill 对照矩阵

### 11.1 V1.0 MVP — 基础功能

| 模块 | 使用的 Skills | 使用的 Commands |
|------|-------------|----------------|
| 基础表情 (6种) | display-engine, coding, testing | feature, flash, review |
| 语音对话 | audio-pipeline, ai-dialog, cloud-communication | feature, flash, test |
| 基础移动 | motion-control, hardware-driver | hardware-bringup, firmware |
| WiFi+OTA | wifi-setup, ota, cloud-communication | hardware-bringup, deploy |
| 电源管理 | power-management | firmware, test |
| 传感器 | sensor-fusion, calibration | hardware-bringup, calibrate |
| BSP | bsp, hardware-driver | pin-check, hardware-bringup |
| 行为系统 | behavior-system | feature, test |
| 3D 外壳 | 3d-structure | feature |

### 11.2 V2.0 增强版 — 新增能力

| 新增功能 | 使用的 Skills | 使用的 Commands |
|---------|-------------|----------------|
| 代码片段滚动 | lvgl-ui, display-engine | feature, flash |
| 番茄钟 UI | lvgl-ui, behavior-system | feature |
| LVGL 设置菜单 | lvgl-ui, wifi-setup | feature, scaffold |
| 本地唤醒词 | voice, local-ai | feature, test |
| VS Code 集成 | ide-integration, cloud-communication | feature, deploy |
| Git 通知 | ide-integration, behavior-system | feature |
| 多轮对话 | ai-dialog, voice | feature, test |
| TOF 精确避障 | sensor-fusion, calibration | hardware-bringup, calibrate |
| 触摸互动 | hardware-driver, behavior-system | hardware-bringup |
| 手机 APP | cloud-communication, wifi-setup | feature, deploy |

### 11.3 V3.0 专业版 — 进阶能力

| 新增功能 | 使用的 Skills | 使用的 Commands |
|---------|-------------|----------------|
| 本地 AI 推理 | local-ai, voice | feature, test |
| 情绪感知 | local-ai, behavior-system | feature |
| CI/CD 通知 | ide-integration, behavior-system | feature, deploy |
| 终端联动 | ide-integration | feature |
| SLAM 桌面导航 | motion-control, sensor-fusion | feature, test |
| 性能基准 | benchmark | test, feature |
| 安全审计 | review (安全维度) | review |

---

## 12. 任务决策树

```
开始: 我要为 RobotBuddy 做什么?
│
├── 新增一个功能
│   └── → /feature
│       ├── 涉及硬件选型? → hardware-bringup → bsp
│       ├── 涉及传感器? → sensor-fusion → calibration
│       ├── 涉及表情? → display-engine
│       ├── 涉及UI界面? → lvgl-ui
│       ├── 涉及AI对话? → ai-dialog
│       ├── 涉及语音? → voice
│       ├── 涉及WiFi? → wifi-setup
│       ├── 涉及OTA? → ota
│       ├── 涉及行为联动? → behavior-system
│       ├── 涉及外壳? → 3d-structure
│       └── 涉及电源? → power-management
│
├── 调通新硬件
│   └── → /hardware-bringup → /pin-check → /calibrate(如需)
│
├── 开发固件模块
│   └── → /scaffold → /firmware → /flash → /test
│
├── 诊断问题
│   ├── Bug/Crash → /debug
│   ├── 日志分析 → /log
│   └── 环境问题 → /deps → /clean
│
├── 质量保证
│   ├── 代码审查 → /review
│   ├── 运行测试 → /test
│   └── 性能基准 → benchmark skill
│
├── 发布部署
│   ├── 发布版本 → /release
│   ├── OTA部署 → /deploy
│   └── 手动烧录 → /flash
│
└── 日常维护
    ├── 查看状态 → /status
    ├── 引脚检查 → /pin-check
    └── 清理环境 → /clean
```

---

## 附录 A: 文件结构（更新后）

```
.claude/
├── commands/
│   ├── feature.md              ← 功能开发全流程
│   ├── hardware-bringup.md     ← 硬件模块调通
│   ├── firmware.md             ← 固件模块开发
│   ├── scaffold.md             ← 模块脚手架生成
│   ├── flash.md                ← 构建烧录监控
│   ├── simulate.md             ← PC端模拟运行
│   ├── review.md               ← 代码审查
│   ├── test.md                 ← [NEW] 一键测试
│   ├── release.md              ← 版本发布
│   ├── deploy.md               ← [NEW] OTA远程部署
│   ├── debug.md                ← 固件调试
│   ├── log.md                  ← [NEW] 日志收集分析
│   ├── status.md               ← 项目状态一览
│   ├── pin-check.md            ← 引脚冲突检查
│   ├── calibrate.md            ← [NEW] 传感器校准
│   ├── clean.md                ← [NEW] 清理重置
│   └── deps.md                 ← [NEW] 依赖检查
│
├── skills/
│   ├── master/SKILL.md         ← 编排器
│   ├── requirement/SKILL.md    ← 需求分析
│   ├── architecture/SKILL.md   ← 架构设计
│   ├── hardware-driver/SKILL.md ← 外设驱动
│   ├── bsp/SKILL.md            ← [NEW] 板级支持包
│   ├── sensor-fusion/SKILL.md  ← 传感器融合
│   ├── calibration/SKILL.md    ← [NEW] 传感器校准
│   ├── 3d-structure/SKILL.md   ← 3D外壳设计
│   ├── freertos-system/SKILL.md ← FreeRTOS系统
│   ├── display-engine/SKILL.md ← 表情引擎
│   ├── lvgl-ui/SKILL.md        ← [NEW] LVGL UI开发
│   ├── behavior-system/SKILL.md ← 行为决策
│   ├── audio-pipeline/SKILL.md ← 音频管道
│   ├── voice/SKILL.md          ← [NEW] 语音交互系统
│   ├── motion-control/SKILL.md ← 运动控制
│   ├── power-management/SKILL.md ← 电源管理
│   ├── wifi-setup/SKILL.md     ← [NEW] WiFi配网
│   ├── cloud-communication/SKILL.md ← 云端通信
│   ├── ai-dialog/SKILL.md      ← AI对话引擎
│   ├── ota/SKILL.md            ← [NEW] OTA升级管理
│   ├── ide-integration/SKILL.md ← IDE集成
│   ├── coding/SKILL.md         ← 编码实现
│   ├── testing/SKILL.md        ← 测试验证
│   ├── review/SKILL.md         ← 代码审查
│   ├── document/SKILL.md       ← 文档生成
│   ├── local-ai/SKILL.md       ← [P2] 本地AI推理
│   └── benchmark/SKILL.md      ← [P2] 性能基准
│
├── templates/
│   ├── hardware-driver.md
│   ├── component-bom.md
│   ├── api-protocol.md
│   ├── task-design.md          ← [NEW]
│   ├── calibration-guide.md    ← [NEW]
│   ├── architecture-design.md  ← [NEW]
│   ├── test-plan.md            ← [NEW]
│   └── state-machine.md        ← [NEW]
│
├── standards/
│   ├── embedded-coding.md
│   ├── python-coding.md        ← [NEW]
│   ├── api-design.md           ← [NEW]
│   └── protocol-buffer.md      ← [NEW]
│
├── checklists/
│   ├── pre-commit.md
│   ├── hardware-test.md        ← [NEW]
│   ├── safety-checklist.md     ← [NEW]
│   └── release-checklist.md    ← [NEW]
│
└── docs/
    └── gap-analysis.md         ← [NEW] 缺口分析报告
```

## 附录 B: 快速命令参考（更新后）

| 我想做什么 | 使用的命令/Skill | 版本要求 |
|-----------|----------------|---------|
| 开发新功能 | `/feature` | V1.0 |
| 调通新硬件 | `/hardware-bringup` | V1.0 |
| 生成模块骨架 | `/scaffold` | V1.0 |
| 编译+烧录+监控 | `/flash` | V1.0 |
| PC 模拟验证 | `/simulate` | V1.0 |
| 运行测试 | `/test` | V1.0 **[NEW]** |
| 审查代码 | `/review` | V1.0 |
| 发布版本 | `/release` | V1.0 |
| OTA 部署 | `/deploy` | V1.0 **[NEW]** |
| 诊断 Bug | `/debug` | V1.0 |
| 分析日志 | `/log` | V1.0 **[NEW]** |
| 查看项目状态 | `/status` | V1.0 |
| 检查引脚冲突 | `/pin-check` | V1.0 |
| 校准传感器 | `/calibrate` | V1.0 **[NEW]** |
| 清理环境 | `/clean` | V1.0 **[NEW]** |
| 检查依赖 | `/deps` | V1.0 **[NEW]** |
| WiFi 配网 | `wifi-setup` skill | V1.0 **[NEW]** |
| BSP 设计 | `bsp` skill | V1.0 **[NEW]** |
| LVGL UI 开发 | `lvgl-ui` skill | V1.0 **[NEW]** |
| 传感器校准 | `calibration` skill | V1.0 **[NEW]** |
| OTA 管理 | `ota` skill | V1.0 **[NEW]** |
| 语音系统 | `voice` skill | V1.0 **[NEW]** |
| 电源优化 | `power-management` skill | V1.0 |
| 行为编排 | `behavior-system` skill | V1.0 |
| 传感器融合 | `sensor-fusion` skill | V1.0 |
| AI 对话设计 | `ai-dialog` skill | V1.0 |
| 3D 外壳设计 | `3d-structure` skill | V1.0 |
| 本地 AI 推理 | `local-ai` skill | V2.0 **[P2]** |
| 性能基准 | `benchmark` skill | V2.0 **[P2]** |

---

> **文档版本:** 2.0
> **最后更新:** 2026-07-10
> **维护者:** 项目团队
> **相关文档:**
> - `桌面机器人设计需求.md` (产品 PRD V2.0)
> - `gap-analysis.md` (缺口分析报告)
> - `embedded-async-framework-skills` (参考框架)
