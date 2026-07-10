# RobotBuddy Commands & Skills 查缺补漏分析报告

> 对当前 `.claude/` 体系进行全面审计，识别缺口并给出补齐建议，确保体系能支撑桌面机器人项目的完整开发生命周期。

**分析日期:** 2026-07-10
**分析范围:** commands (11个) + skills (19个) + templates (3个) + standards (1个) + checklists (1个)

---

## 目录

1. [总体评估](#1-总体评估)
2. [Commands 缺口分析](#2-commands-缺口分析)
3. [Skills 缺口分析](#3-skills-缺口分析)
4. [Templates / Standards / Checklists 缺口分析](#4-templates--standards--checklists-缺口分析)
5. [跨模块协同问题](#5-跨模块协同问题)
6. [补齐优先级建议](#6-补齐优先级建议)
7. [现有文件改进建议](#7-现有文件改进建议)

---

## 1. 总体评估

### 1.1 覆盖度矩阵

```
开发阶段          Commands           Skills              质量体系
─────────────────────────────────────────────────────────────
需求分析          —                 requirement ✅       —
架构设计          —                 architecture ✅      —
硬件调通          hardware-bringup ✅ hardware-driver ✅  templates/hardware-driver ✅
                                          sensor-fusion ✅
                                          3d-structure ✅
固件开发          firmware ✅        freertos-system ✅   standards/embedded-coding ✅
                 scaffold ✅        coding ✅
                 flash ✅           power-management ✅
                 simulate ✅
音频系统          —                 audio-pipeline ✅    —
显示/表情          —                 display-engine ✅    —
运动控制          —                 motion-control ✅    —
云端通信          —                 cloud-communication ✅ templates/api-protocol ✅
                                          ai-dialog ✅
IDE集成           —                 ide-integration ✅   —
行为系统          —                 behavior-system ✅   —
测试              —                 testing ✅           —
审查              review ✅          review ✅            checklists/pre-commit ✅
发布              release ✅         —                   —
调试              debug ✅           —                   —
状态查看          status ✅          —                   —
引脚管理          pin-check ✅       —                   —
WiFi 配网         ❌                 ❌                   ❌
LVGL UI           ❌                 ❌                   ❌
传感器校准         ❌                 ❌                   ❌
OTA 管理           (release 中提及)   (cloud-comm 中提及)  ❌
语音系统端到端      —                 ❌                   ❌
BSP 板级支持       —                 ❌                   ❌
性能基准           ❌                 ❌                   ❌
安全审计           ❌                 ❌                   ❌
本地 AI            —                 ❌                   ❌
参数调优           ❌                 ❌                   ❌
```

✅ = 已覆盖  ❌ = 缺口  — = 部分覆盖或非独立模块

### 1.2 核心发现

1. **Commands 缺口 6 个** — 缺少测试、清理、依赖检查、校准、日志分析、OTA部署的快捷命令
2. **Skills 缺口 8 个** — 缺少 WiFi配网、LVGL UI、传感器校准、OTA管理、语音系统、BSP、本地AI、性能基准
3. **Templates 缺口 5 个** — 缺少测试计划、架构设计、状态机设计、FreeRTOS任务设计、校准指南模板
4. **Standards 缺口 3 个** — 缺少 Python编码、API设计、Protobuf标准
5. **Checklists 缺口 3 个** — 缺少硬件测试、安全、发布检查清单
6. **跨模块协同问题** — 新增的5个skills未在master中注册；部分skills间职责重叠；依赖关系未文档化

---

## 2. Commands 缺口分析

### 2.1 🔴 必须补齐

#### 2.1.1 `/test` — 一键测试命令

**缺口理由:** 当前 `testing` skill 定义了完整的测试体系，但没有一个 Command 让开发者直接运行 `/test` 来执行测试。`/feature` 和 `/firmware` 内部调用了 testing skill，但在纯测试场景（如回归测试、CI集成测试）中缺少快捷入口。

**适用场景:**
- PR 提交前的完整测试套件运行
- 指定模块的快速单元测试
- CI/CD 流水线中的测试触发

**建议工作流:**
```
第1阶段：测试范围确认 — 全部 / 单元 / HIL / 集成 / 压力
第2阶段：环境准备 — 编译 + 烧录（HIL时）
第3阶段：执行测试 — Unity + CMock or HIL scripts
第4阶段：结果分析 — 覆盖率报告 + 失败用例详细分析
```

#### 2.1.2 `/clean` — 清理命令

**缺口理由:** 嵌入式项目在开发过程中会积累大量构建产物、临时文件。ESP-IDF 的 `idf.py fullclean` 有一定作用，但还需要清理 NVS 分区、重置校准数据、清理 PSRAM dump 等。没有一个统一的 `/clean` 命令。

**适用场景:**
- 切换 ESP-IDF 版本后的完整清理
- 固件异常后的恢复出厂设置
- 发布前的环境清洁确认

#### 2.1.3 `/deps` — 依赖检查命令

**缺口理由:** ESP-IDF 项目对工具链版本、Python 包版本非常敏感。当前没有任何自动检测环境依赖的机制，新开发者经常遇到环境问题。

**适用场景:**
- 新成员加入项目时的环境自检
- 升级 ESP-IDF 版本后的兼容性验证
- CI 环境的依赖安装验证

**建议检查项:**
```
ESP-IDF 版本 (≥ 5.1)
Python 3.8+
pip packages: esptool, esp-coredump
xtensa-esp32s3 toolchain
CMake ≥ 3.16
USB 驱动 (CP210x / CH340)
Git ≥ 2.30
```

#### 2.1.4 `/calibrate` — 传感器校准命令

**缺口理由:** RobotBuddy 有 MPU6050 IMU、红外边缘检测、红外避障、电池 ADC 等需要校准的传感器。当前 `sensor-fusion` skill 中定义了校准逻辑但没有对应的 Command 入口。校准是桌面机器人每次更换桌面/环境都需要执行的操作。

**适用场景:**
- IMU 陀螺仪/加速度计零偏校准
- 桌面边缘基线校准（适应不同桌面颜色/材质）
- 电池 ADC 电压分压系数校准
- TOF 传感器偏移校准

### 2.2 🟡 建议补齐

#### 2.2.5 `/log` — 日志收集与分析命令

**缺口理由:** 当前 `/debug` 命令聚焦于诊断已知问题，但缺少一个主动的日志收集与分析命令。ESP32-S3 运行时的串口日志、Core Dump、堆跟踪日志需要统一的收集和分析入口。

**适用场景:**
- 长时间运行后导出日志进行分析
- 生成系统健康报告（栈水位、堆状态、任务CPU占用）
- Core Dump 解析和 Backtrace 符号化

#### 2.2.6 `/deploy` — OTA 远程部署命令

**缺口理由:** 当前 `/release` 包含 OTA 部署环节，但 OTA 部署（向已在用户手中的设备推送固件）和版本发布（在 Git 上打 tag）是两个不同的操作。需要一个专门的 `/deploy` 命令处理固件上传、OTA 推送、手动烧录等部署场景。

**适用场景:**
- 开发阶段快速 OTA 到测试设备
- 批量部署到多台 RobotBuddy
- 手动烧录（替代 `/flash` 的部分场景，指定更完整的 burn 参数）

---

## 3. Skills 缺口分析

### 3.1 🔴 必须补齐

#### 3.1.1 `wifi-setup` — WiFi 配网 Skill

**缺口理由:** WiFi 连接是桌面机器人的生命线。ESP32-S3 支持多种配网方式（SmartConfig、BLE、SoftAP、WPS），每种都有不同的 UX 和安全考量。当前 `cloud-communication` skill 仅简单提及 WiFi Manager，但没有系统化的配网知识。

**建议内容:**
```
配网方式对比: SmartConfig vs BLE vs SoftAP vs WPS
UX 设计: LED指示、表情反馈、超时处理、错误重试
安全: NVS 加密存储、凭证迁移
多网络: 家庭 WiFi + 公司 WiFi 双配置
离线降级: 无网络时的本地功能矩阵
WiFi 性能: RSSI 监测、频道选择、TX Power调节
```

#### 3.1.2 `lvgl-ui` — LVGL UI 开发 Skill

**缺口理由:** 当前 `display-engine` skill 仅覆盖了表情动画渲染，但 PRD 中明确提到使用 LVGL 做 UI 开发（代码片段滚动显示、番茄钟界面、设置页面等）。LVGL 有独立的 API 体系、设计模式和性能约束，需要一个专门的 skill。

**建议内容:**
```
LVGL 版本: v8.3.x (推荐，ESP32 生态成熟)
显示驱动: ST7789 + LVGL porting (display_flush callback)
输入设备: 触摸屏驱动 + LVGL indev
内存: LVGL buffer 配置 (1/4/全缓冲 取舍)
典型UI: 代码滚动页面、设置菜单、通知弹窗、番茄钟界面
性能: FPS 监控、脏区域刷新、DMA 加速
```

#### 3.1.3 `calibration` — 传感器校准 Skill

**缺口理由:** 当前 `sensor-fusion` skill 中定义了校准数据结构和 API，但没有专门的校准方法论和操作流程。每种传感器（IMU、IR边缘、IR避障、ADC）的校准逻辑完全不同，需要一个统一的 skill 来指导。

**建议内容:**
```
IMU 校准: 静止采样法、6面校准、温度补偿
边缘检测校准: 桌面基线采样、自适应阈值
避障校准: 距离-S曲线、环境光补偿
ADC 校准: 2点校准法、分压系数计算
NVS 持久化: 校准数据结构、版本管理、工厂重置
校准质量: 残差分析、重复性验证
```

#### 3.1.4 `ota` — OTA 升级管理 Skill

**缺口理由:** OTA 是嵌入式设备的生命线功能。当前 OTA 逻辑散落在 `cloud-communication` skill 中，但 OTA 涉及的固件分区管理、回滚策略、签名验证、差分升级等需要系统化的知识体系。

**建议内容:**
```
分区策略: factory + ota_0 + ota_1 滚动升级
升级流程: 检查→下载→校验→标记→重启→确认
安全: RSA-2048 签名、SHA256 校验、证书管理
回滚: 自动回滚条件、手动回滚 API
差分升级: 节省带宽（可选，V2+）
错误处理: 下载中断续传、Flash 写入失败
测试: OTA 测试矩阵（正常/断电/WiFi中断/版本跳跃）
```

#### 3.1.5 `voice` — 语音交互系统 Skill

**缺口理由:** 当前语音交互能力分散在 `audio-pipeline`（I2S采集播放）、`ai-dialog`（LLM对话）、`cloud-communication`（WebSocket传输）三个 skill 中。缺少一个统一的语音交互系统 skill，覆盖从麦克风到扬声器的端到端流程。

**建议内容:**
```
唤醒词: ESP-SR 模型导入、MultiNet 关键词配置
ASR 选型: Whisper vs 讯飞 vs 百度 vs 阿里
TTS 选型: Edge-TTS vs 火山引擎 vs 本地 TTS
语音流程: 唤醒→VAD→ASR→NLU→LLM→TTS→播放
延迟优化: 流式传输、预加载、并发流水线
多语言: 普通话/英文 切换、方言适配
音效: 系统提示音、开机音、通知音设计
```

#### 3.1.6 `bsp` — 板级支持包 (BSP) Skill

**缺口理由:** 当前 `hardware-driver` skill 聚焦于单个外设驱动，`pin-check` command 处理引脚冲突。但缺少一个管理"整块板子"配置的 skill — BSP 涵盖 Clock Tree、引脚矩阵、启动序列、外设初始化顺序、电源域配置等。

**建议内容:**
```
引脚矩阵: ESP32-S3 GPIO Matrix 最优分配
时钟配置: XTAL → PLL → APB/AHB 时钟树
启动顺序: BootROM → 2nd Bootloader → app_main → 各模块 init
外设初始化: SPI > I2C > I2S > PWM 初始化顺序
电源域: VDD_SPI, RTC domain, 外设时钟门控
Boot 配置: Strapping Pin, eFuse, Flash 频率
工厂测试: 产线自检、模块连通性测试
```

### 3.2 🟡 建议补齐

#### 3.2.7 `local-ai` — 本地 AI 推理 Skill

**缺口理由:** PRD V2.0 明确规划了 V2 阶段的本地唤醒词和 V3 阶段的本地 AI 能力。虽然当前 MVP 阶段依赖云端 AI，但提前储备本地 AI 能力对离线场景至关重要。

**建议内容:**
```
ESP-SR: 唤醒词训练、MultiNet 命令词
TensorFlow Lite Micro: 模型量化(FP32→INT8)、算子支持
TinyML: 情绪分类、手势识别（摄像头时）
资源: IRAM 占用、推理延迟、模型存储位置
```

#### 3.2.8 `benchmark` — 性能基准 Skill

**缺口理由:** 桌面机器人有严格的实时性约束（音频16kHz、显示30FPS、电机100Hz）。需要一个 systematic 的性能基准测试能力来确保每次改动不会破坏实时性。

**建议内容:**
```
CPU: CoreMark 基准、Task CPU 占用率
内存: 栈水位趋势、堆碎片率趋势、PSRAM 带宽
延迟: ISR 响应时间、IPC 消息延迟
音频: 丢帧率、DMA underrun 统计
显示: FPS 稳度、SPI 传输时间
长稳: 24h 资源趋势、泄漏检测
```

---

## 4. Templates / Standards / Checklists 缺口分析

### 4.1 🔴 必须补齐的 Templates

| 模板 | 用途 | 建议位置 |
|------|------|---------|
| `test-plan.md` | 测试计划模板（4级测试金字塔） | `.claude/templates/test-plan.md` |
| `architecture-design.md` | 架构设计文档模板（分层+PlantUML） | `.claude/templates/architecture-design.md` |
| `state-machine.md` | 状态机设计模板（Mermaid/PlantUML） | `.claude/templates/state-machine.md` |
| `task-design.md` | FreeRTOS 任务设计卡片模板 | `.claude/templates/task-design.md` |
| `calibration-guide.md` | 传感器校准步骤记录模板 | `.claude/templates/calibration-guide.md` |

### 4.2 🟡 建议补齐的 Standards

| 标准 | 用途 | 建议位置 |
|------|------|---------|
| `python-coding.md` | Python 编码标准（Local Bridge 组件） | `.claude/standards/python-coding.md` |
| `api-design.md` | API 设计标准（REST/WS/MQTT） | `.claude/standards/api-design.md` |
| `protocol-buffer.md` | Protobuf 使用规范 | `.claude/standards/protocol-buffer.md` |

### 4.3 🟡 建议补齐的 Checklists

| 清单 | 用途 | 建议位置 |
|------|------|---------|
| `hardware-test.md` | 硬件模块调通后的验证清单 | `.claude/checklists/hardware-test.md` |
| `safety-checklist.md` | 安全审查清单（桌面机器人物理安全+网络安全） | `.claude/checklists/safety-checklist.md` |
| `release-checklist.md` | 版本发布检查清单 | `.claude/checklists/release-checklist.md` |

---

## 5. 跨模块协同问题

### 5.1 master skill 未包含新增 skills

当前 `master` skill 的编排规则中只列了 14 个原有 skills，未注册以下 5 个新增 skills：
- `power-management`
- `behavior-system`
- `sensor-fusion`
- `ai-dialog`
- `3d-structure`

**修复:** 更新 `master/SKILL.md` 的 Stage Dispatch Rules 表格。

### 5.2 职责重叠/模糊的 Skills 对

| Skill A | Skill B | 重叠领域 | 建议 |
|---------|---------|---------|------|
| `power-management` | `freertos-system` | 电源模式切换在两边都有定义 | freertos-system 定义 API 接口，power-management 定义策略逻辑 |
| `behavior-system` | `display-engine` | 表情触发逻辑 vs 表情渲染 | behavior-system 决定触发哪个表情，display-engine 负责渲染 |
| `sensor-fusion` | `hardware-driver` | 传感器数据采集 vs 融合算法 | hardware-driver 负责数据采集，sensor-fusion 负责滤波融合 |
| `ai-dialog` | `cloud-communication` | LLM 调用 vs HTTP/WS 传输 | ai-dialog 负责对话逻辑，cloud-communication 负责传输层 |
| `audio-pipeline` | `voice` (新增建议) | I2S 底层 vs 语音全链路 | audio-pipeline 负责底层音频，voice 负责语音业务逻辑 |

### 5.3 Skills 调用链不完整

现有 skills 之间的依赖关系虽在 Skill使用指南中有文档化，但缺少以下关键路径:

```
新功能开发完整链条:
requirement → architecture → [freertos-system] → [domain skills] → coding → testing → review → document
                                                                      ↑
新增 skills 应插入此处:                                               
  behavior-system (需要联动多模态时需要)
  sensor-fusion (涉及传感器数据处理时需要)
  ai-dialog (涉及AI对话时需要)
  power-management (涉及功耗策略时需要)
  3d-structure (涉及物理结构变更时需要)
```

### 5.4 产品版本与 Skills 的对应关系缺失

PRD 定义了 V1.0 MVP、V2.0增强版、V3.0专业版，但未说明各版本分别需要用到哪些 Skills 和 Commands。

**建议:** 在 PRD 或 Skill使用指南中添加版本-Skill 对照矩阵。

---

## 6. 补齐优先级建议

### 6.1 立即补齐（本周）— P0

这些缺口直接影响当前 MVP 开发效率：

| 类型 | 名称 | 理由 |
|------|------|------|
| Command | `/test` | 开发中最频繁的操作 |
| Command | `/clean` | 环境问题排查必备 |
| Command | `/deps` | 新成员上手必备 |
| Skill | `wifi-setup` | ESP32 项目刚需 |
| Skill | `lvgl-ui` | V1.0 需要 UI 界面 |
| Skill | `calibration` | 传感器数据不准则一切免谈 |
| Template | `task-design.md` | 每新增一个 Task 都需要的模板 |
| Update | `master/SKILL.md` | 必须注册5个新增 skills |

### 6.2 近期补齐（本月）— P1

这些缺口在功能扩展时会用到：

| 类型 | 名称 | 理由 |
|------|------|------|
| Command | `/calibrate` | 多传感器校准需要统一入口 |
| Command | `/log` | 调试和性能分析必备 |
| Command | `/deploy` | 发布频繁后需要快捷部署 |
| Skill | `ota` | 设备部署后 OTA 是必需品 |
| Skill | `voice` | 语音全链路调优需要 |
| Skill | `bsp` | PCB 设计时需要统一管理 |
| Template | `calibration-guide.md` | 每次校准需要记录 |
| Template | `architecture-design.md` | 复杂功能的架构文档 |
| Template | `test-plan.md` | 系统化测试规划 |

### 6.3 后续补齐（V2 阶段）— P2

这些缺口在 V2.0 增强版开发时重要：

| 类型 | 名称 | 理由 |
|------|------|------|
| Skill | `local-ai` | V2 本地唤醒词和关键词识别 |
| Skill | `benchmark` | 性能回归测试 |
| Standard | `python-coding.md` | Local Bridge 代码量增大时 |
| Standard | `api-design.md` | 云端 API 增多时 |
| Standard | `protocol-buffer.md` | IDE 集成使用 Protobuf 时 |
| Checklist | `hardware-test.md` | 硬件模块增多时 |
| Checklist | `safety-checklist.md` | 桌面机器人安全认证 |
| Checklist | `release-checklist.md` | 正式发布流程 |

---

## 7. 现有文件改进建议

### 7.1 Commands 改进

| 文件 | 问题 | 改进建议 |
|------|------|---------|
| `/flash` | 未包含 BLE 配网流程 | 添加 BLE provisioning 章节 |
| `/simulate` | 未与 CI/CD 集成 | 添加 GitHub Actions 模拟测试示例 |
| `/status` | 未扫描新增5个skills的模块目录 | 更新模块扫描列表 |
| `/scaffold` | 仅 driver/service 层模板 | 添加 app 层和 system 层模板 |
| `/hardware-bringup` | 缺少 WiFi/蓝牙模组调通流程 | 添加通信模组调通章节 |

### 7.2 Skills 改进

| 文件 | 问题 | 改进建议 |
|------|------|---------|
| `master/SKILL.md` | 5个新增skills未注册 | 更新调度规则表 |
| `power-management` | 与 freertos-system 有接口重叠 | 明确边界：power-mgmt 是策略，freertos 是机制 |
| `behavior-system` | 场景表仅4个示例 | 扩展至 10+ 场景（Git通知、OTA升级、充电等） |
| `sensor-fusion` | 缺少传感器故障检测逻辑 | 添加 watchdog 机制 |
| `ai-dialog` | 意图分类仅关键词匹配 | 添加机器学习分类方案（可选） |
| `3d-structure` | 缺少 PCB 布局约束 | 添加电气间距/EMI/散热约束 |

### 7.3 Skill使用指南 改进

| 问题 | 改进建议 |
|------|---------|
| 未包含 5 个新增 skills | 添加完整章节 |
| 未包含 5 个新增 commands | 添加完整章节 |
| 缺少版本-Skill对照矩阵 | 添加第11节 |
| 缺少决策树流程图 | 添加"这个任务该用哪个Command/Skill"决策树 |

---

## 附录 A: 完整覆盖率矩阵

```
                     V1.0 MVP    V2.0 增强    V3.0 专业
────────────────────────────────────────────────────────
Commands
/feature             ✅ P0        ✅            ✅
/hardware-bringup    ✅ P0        ✅            ✅
/firmware            ✅ P0        ✅            ✅
/review              ✅ P0        ✅            ✅
/release             ✅ P0        ✅            ✅
/debug               ✅ P0        ✅            ✅
/scaffold            ✅ P0        ✅            ✅
/flash               ✅ P0        ✅            ✅
/simulate            ✅ P1        ✅            ✅
/status              ✅ P2        ✅            ✅
/pin-check            ✅ P0        ✅            ✅
/test                🔴 缺失       ✅            ✅
/clean               🔴 缺失       ✅            ✅
/deps                🔴 缺失       ✅            ✅
/calibrate           🟡 可选       ✅ P1         ✅
/log                 🟡 可选       ✅ P2         ✅
/deploy              🟡 可选       ✅ P1         ✅

Skills
master               ✅           ✅            ✅
requirement          ✅           ✅            ✅
architecture         ✅           ✅            ✅
freertos-system      ✅           ✅            ✅
hardware-driver      ✅           ✅            ✅
audio-pipeline       ✅           ✅            ✅
display-engine       ✅           ✅            ✅
motion-control       ✅           ✅            ✅
cloud-communication  ✅           ✅            ✅
ide-integration      —            ✅ P0         ✅
coding               ✅           ✅            ✅
review               ✅           ✅            ✅
testing              ✅           ✅            ✅
document             ✅           ✅            ✅
power-management     ✅ P0        ✅            ✅
behavior-system      ✅ P0        ✅            ✅
sensor-fusion        ✅ P0        ✅            ✅
ai-dialog            ✅ P0        ✅            ✅
3d-structure         ✅ P1        ✅            ✅
wifi-setup           🔴 缺失       ✅            ✅
lvgl-ui              🔴 缺失       ✅            ✅
calibration          🔴 缺失       ✅            ✅
ota                  🟡 可选       ✅ P0         ✅
voice                🟡 可选       ✅ P1         ✅
bsp                  🟡 可选       ✅ P2         ✅
local-ai             —            ✅ P2         ✅ P0
benchmark            🟡 可选       ✅ P2         ✅

Templates
hardware-driver      ✅           ✅            ✅
component-bom        ✅           ✅            ✅
api-protocol         ✅           ✅            ✅
task-design          🔴 缺失       ✅            ✅
calibration-guide    🟡 可选       ✅            ✅
architecture-design  🟡 可选       ✅            ✅
test-plan            🟡 可选       ✅            ✅
state-machine        🟡 可选       ✅            ✅

Standards
embedded-coding      ✅           ✅            ✅
python-coding        🟡 可选       ✅            ✅
api-design           🟡 可选       ✅            ✅
protocol-buffer      —            ✅            ✅

Checklists
pre-commit           ✅           ✅            ✅
hardware-test        🟡 可选       ✅            ✅
safety-checklist     🟡 可选       ✅            ✅
release-checklist    🟡 可选       ✅            ✅
```

✅ = 已覆盖  🔴 = P0 必须补齐  🟡 = P1/P2 建议补齐  — = 不适用

---

## 附录 B: 补齐后的理想文件结构

```
.claude/
├── commands/
│   ├── feature.md              ← 功能开发全流程
│   ├── hardware-bringup.md     ← 硬件模块调通
│   ├── firmware.md             ← 固件模块开发
│   ├── review.md               ← 代码与架构审查
│   ├── release.md              ← 固件发布
│   ├── debug.md                ← 固件调试
│   ├── scaffold.md             ← 模块脚手架生成
│   ├── flash.md                ← 构建烧录监控
│   ├── simulate.md             ← PC端模拟运行
│   ├── status.md               ← 项目状态一览
│   ├── pin-check.md            ← GPIO引脚冲突检查
│   ├── test.md                 ← [NEW] 一键测试
│   ├── clean.md                ← [NEW] 清理重置
│   ├── deps.md                 ← [NEW] 依赖检查
│   ├── calibrate.md            ← [NEW] 传感器校准
│   ├── log.md                  ← [NEW] 日志收集分析
│   └── deploy.md               ← [NEW] OTA远程部署
├── skills/
│   ├── master/SKILL.md         ← 编排器 (需要更新)
│   ├── requirement/SKILL.md
│   ├── architecture/SKILL.md
│   ├── freertos-system/SKILL.md
│   ├── hardware-driver/SKILL.md
│   ├── audio-pipeline/SKILL.md
│   ├── display-engine/SKILL.md
│   ├── motion-control/SKILL.md
│   ├── cloud-communication/SKILL.md
│   ├── ide-integration/SKILL.md
│   ├── coding/SKILL.md
│   ├── review/SKILL.md
│   ├── testing/SKILL.md
│   ├── document/SKILL.md
│   ├── power-management/SKILL.md
│   ├── behavior-system/SKILL.md
│   ├── sensor-fusion/SKILL.md
│   ├── ai-dialog/SKILL.md
│   ├── 3d-structure/SKILL.md
│   ├── wifi-setup/SKILL.md     ← [NEW] WiFi配网
│   ├── lvgl-ui/SKILL.md        ← [NEW] LVGL UI开发
│   ├── calibration/SKILL.md    ← [NEW] 传感器校准
│   ├── ota/SKILL.md            ← [NEW] OTA升级管理
│   ├── voice/SKILL.md          ← [NEW] 语音交互系统
│   ├── bsp/SKILL.md            ← [NEW] 板级支持包
│   ├── local-ai/SKILL.md       ← [NEW] 本地AI推理 (P2)
│   └── benchmark/SKILL.md      ← [NEW] 性能基准 (P2)
├── templates/
│   ├── hardware-driver.md
│   ├── component-bom.md
│   ├── api-protocol.md
│   ├── task-design.md          ← [NEW]
│   ├── calibration-guide.md    ← [NEW]
│   ├── architecture-design.md  ← [NEW]
│   ├── test-plan.md            ← [NEW]
│   └── state-machine.md        ← [NEW]
├── standards/
│   ├── embedded-coding.md
│   ├── python-coding.md        ← [NEW]
│   ├── api-design.md           ← [NEW]
│   └── protocol-buffer.md      ← [NEW]
├── checklists/
│   ├── pre-commit.md
│   ├── hardware-test.md        ← [NEW]
│   ├── safety-checklist.md     ← [NEW]
│   └── release-checklist.md    ← [NEW]
└── docs/
    └── gap-analysis.md         ← [本文件]
```

---

> **分析人:** Claude Code (deepseek-v4-pro)
> **下次审查:** 补齐 P0 项目后重新评估
