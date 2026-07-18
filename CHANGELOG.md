# Changelog — RobotBuddy

> 所有版本变更记录。

---

## [0.4.0] - 2026-07-19

### 新增

- **OTA 升级服务** — 完整的远程固件升级系统
  - **RSA-2048 签名验证** — 防止恶意固件注入
  - **SHA256 完整性校验** — 确保固件完整
  - **TLS 证书锁定** — 防中间人攻击
  - **AB 分区滚动升级** — factory + ota_0 + ota_1 策略
  - **自动回滚** — 新固件异常时自动恢复（watchdog ≥3, panic ≥5）
  - **断点续传** — 网络中断后继续下载
  - **低电量保护** — 电量 <20% 拒绝升级
  - **进度显示** — 实时升级进度和状态
  - **MQTT 命令处理** — 远程升级控制
  - **健康检查系统** — WiFi/MQTT/Display/Sensor 检查
  - **NVS 持久化** — 回滚计数和断点信息

- **OTA 文档**
  - 需求分析 (`docs/requirement/ota-upgrade-requirements.md`)
  - 架构设计 (`docs/architecture/ota-upgrade-architecture.md`)
  - 测试计划 (`docs/testing/ota-upgrade-test-plan.md`)
  - API 文档 (`docs/firmware/ota-upgrade-api.md`)

- **OTA 配置**
  - 可配置健康检查超时（默认 30s）
  - 可配置回滚阈值（watchdog=3, panic=5）
  - 可配置最低电量百分比（默认 20%）
  - 可配置重试延迟和下载缓冲区

- **OTA 事件**
  - 新增 15 个 OTA 事件 ID (EVENT_OTA_*, 0x0700-0x07FF)
  - 新增 OTA payload 结构体

### 变更

- 更新 `docs/development-progress.md` — 反映 V2.0 OTA 功能完成
- 固件版本升级至 0.4.0-v2-ota

### 安全

- 实现 RSA-2048 签名验证（使用 mbedTLS）
- 添加 TLS 证书锁定
- 添加 SHA256 固件完整性验证
- 保护 factory 分区不被 OTA 覆盖
- 添加签名 URL 支持（防重放攻击）

### 测试

- 单元测试计划覆盖所有 OTA 模块
- 集成测试覆盖事件总线和 MQTT 命令
- 系统测试覆盖端到端升级流程
- 安全测试覆盖签名验证和证书锁定
- 压力测试覆盖网络中断和长期运行

---

## [0.3.0] - 2026-07-18

### 新增

- **V2.0 增强版功能**
  - 文本显示模块 (`text_display`) — 滚动文本消息和状态图标
  - MQTT 客户端 (`mqtt_client`) — 编译结果/Git 状态推送
  - Web 控制台 (`web_server`) — HTTP REST API + 嵌入式仪表盘
  - OTA 升级服务 (`ota_service`) — 远程固件升级 + SHA256 校验
  - 本地唤醒词 (`wake_word`) — ESP-SR 离线检测（备选 VAD）
  - 番茄钟 (`pomodoro`) — 25 分钟工作计时器
  - 电源管理 (`power_manager`) — 自动休眠策略
  - TOF 传感器驱动 (`vl53l0x`) — VL53L0X 激光测距
  - 触摸传感器驱动 (`touch_sensor`) — 单击/双击/长按检测

- **事件系统扩展**
  - 新增 30+ 事件 ID（显示/触摸/唤醒词/MQTT/OTA/番茄钟/电源）
  - 新增 8 个 payload 结构体

- **行为系统增强**
  - 支持 V2.0 新事件：唤醒词、触摸、编译状态、Git 状态、番茄钟
  - 新增 FOCUS 和 SLEEP 状态联动

- **云端通信增强**
  - 流式 TTS API (`cloud_tts_stream`)
  - API Key 管理接口 (`cloud_set_api_key`, `cloud_get_api_key_masked`)

### 变更

- 固件版本升级至 0.3.0-v2
- main.c 集成 V2.0 模块初始化流程
- CMakeLists.txt 添加 10 个新组件目录

### 已知问题

- VL53L0X 驱动需要 BSP 暴露 I2C bus handle
- 触摸传感器 GPIO 映射需要硬件验证
- ESP-SR 组件路径需要单独安装配置

---

## [0.2.0] - 2026-07-18

### 新增

- **硬件设计方案 — 电路原理图**
  - 硬件需求分析文档 (`docs/requirement/hardware-schematic.md`)
  - 硬件系统架构文档 (`docs/architecture/hardware-architecture.md`)
    - 电源树设计 (18650 → TP4056 → MT3608 → AMS1117)
    - 信号完整性分析 (SPI/I2C/I2S/PWM)
    - ESP32-S3 引脚复用策略
    - 模块接口契约 (引脚连接图)
    - PCB 布局建议
  - 电路原理图说明 (`docs/hardware/schematic-notes.md`)
    - 主控最小系统电路
    - 电源子系统 (充电/升压/LDO/ADC)
    - 显示子系统 (ST7789 SPI 接口)
    - 音频子系统 (INMP441 + MAX98357A I2S 接口)
    - 电机子系统 (DRV8833 PWM 接口)
    - 传感器子系统 (MPU6050 I2C, TCRT5000/ITR20001 GPIO)
    - USB 接口电路
    - 复位与启动电路
  - ESP32-S3 引脚分配表 (`docs/hardware/pinout.md`)
    - 30 个 GPIO 完整分配
    - Strapping 引脚配置
    - 与 bsp_pinmap.h 完全一致
  - 物料清单 BOM (`docs/hardware/bom.md`)
    - 35 个电子元件 + 8 个结构件
    - 成本估算 ~¥209
    - 替代件/备选型号
  - 组装指南 (`docs/hardware/assembly-guide.md`)
  - 设计验证报告 (`docs/testing/hardware-design-test-report.md`)
  - 设计审查报告 (`docs/review/hardware-design-review.md`)

### 变更

- 更新 `docs/桌面机器人设计需求.md` — 添加硬件设计文档完成标记
- 更新 `README.md` — 添加项目结构、文档索引、硬件规格

### 已知问题

- 5V 电源轨裕量紧张 (电机+功放峰值可能超 2A)
- 活跃续航估算约 1.7h (PRD 要求 ≥4h)
- I2C 总线句柄泄漏 (bsp_board.c) — 待修复
- I2S 初始化函数为死代码 — 待清理

---

## [0.1.0] - 2026-07-11

### 新增

- 初始项目结构
- ESP32-S3 BSP 板级支持包
  - `bsp_pinmap.h` — 引脚定义
  - `bsp_board.c/h` — 硬件初始化
- 基础驱动框架
  - ST7789 LCD 驱动
  - DRV8833 电机驱动
  - MPU6050 IMU 驱动
  - INMP441 麦克风 (via audio_manager)
  - MAX98357A 功放 (via audio_manager)
  - 红外传感器驱动
  - 电池电压检测
- 服务层框架
  - WiFi 管理器
  - 音频管理器
  - 显示管理器
  - 运动管理器
  - 传感器管理器
  - 云端通信管理器
  - 情绪引擎
  - 行为系统
  - AI 对话
- 框架层
  - 事件总线
  - 系统监控
- 单元测试框架
- Claude Code skills 和模板定义