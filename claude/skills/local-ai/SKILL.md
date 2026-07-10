# local-ai — 本地 AI 推理 Skill

> **版本优先级:** P2 (V2.0+ 阶段实现)
> **前置依赖:** freertos-system, audio-pipeline, voice

## Role

RobotBuddy 本地 AI 推理专家，负责 ESP32-S3 上的边缘 AI 能力实现，包括离线唤醒词、关键词识别和 TinyML 情绪分类。

## Domain

ESP-SR 语音识别、TensorFlow Lite Micro 推理、Edge Impulse 模型部署、本地 AI 推理性能优化。

## Responsibility

- 设计本地唤醒词检测系统（ESP-SR）
- 实现 MultiNet 关键词识别
- 部署 TensorFlow Lite Micro 模型到 ESP32-S3
- 优化本地 AI 推理性能（内存、延迟、功耗）
- 管理本地模型的生命周期（训练、量化、部署、更新）

## Knowledge Base

### ESP-SR 语音识别框架

```
ESP-SR 架构:
┌─────────────────────────────────────────┐
│               ESP-SR Pipeline            │
│                                         │
│  ┌──────────┐    ┌──────────┐          │
│  │ AFE      │    │ WakeNet  │          │
│  │ 音频前端  │───→│ 唤醒词   │          │
│  │ 处理     │    │ 检测     │          │
│  └──────────┘    └────┬─────┘          │
│                       │ 唤醒后          │
│  ┌──────────┐    ┌────▼─────┐          │
│  │ MultiNet │    │ ASR     │          │
│  │ 命令词   │←───│ 语音识别 │          │
│  │ 识别     │    │ (可选)   │          │
│  └──────────┘    └──────────┘          │
│                                         │
│  资源需求:                               │
│  - IRAM: ~60KB (WakeNet)                │
│  - PSRAM: ~200KB (音频缓冲)              │
│  - CPU: 单核 240MHz 运行                │
│  - 延迟: < 200ms (唤醒)                 │
└─────────────────────────────────────────┘
```

### WakeNet 唤醒词模型

| 模型 | 唤醒词 | IRAM 占用 | 延迟 | 适用场景 |
|------|--------|----------|------|---------|
| WakeNet 7 | "Hi Lexin" | ~60KB | <200ms | 通用场景 |
| WakeNet 7 | 自定义唤醒词 | ~80KB | <300ms | 需要训练 |
| WakeNet 5 | "Alexa" 兼容 | ~40KB | <200ms | 简单场景 |

### MultiNet 命令词

```
支持的命令词数量: 最多 ~100 个
推理时间: < 100ms/命令
内存占用: ~30KB IRAM

自定义命令词示例:
├── "打开 WiFi"    → CMD_WIFI_ON
├── "关闭 WiFi"    → CMD_WIFI_OFF
├── "开始充电"     → CMD_CHARGE_START
├── "停止运动"     → CMD_MOTION_STOP
├── "音量增大"     → CMD_VOL_UP
├── "音量减小"     → CMD_VOL_DOWN
└── "进入休眠"     → CMD_SLEEP
```

### TensorFlow Lite Micro

```c
// TFLite Micro 推理流程
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"

// 1. 模型映射 (模型存储在 Flash)
const unsigned char* model_data = embedded_model_tflite;

// 2. 解释器创建
tflite::MicroInterpreter* interpreter = ...;
// 分配 tensor arena (PSRAM 区域)
constexpr int kTensorArenaSize = 64 * 1024;  // 64KB
uint8_t* tensor_arena = heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM);

// 3. 输入填充
float* input = interpreter->typed_input_tensor<float>(0);
// 填入传感器数据...

// 4. 推理
interpreter->Invoke();

// 5. 输出读取
float* output = interpreter->typed_output_tensor<float>(0);
// 分类结果...
```

### 模型量化策略

| 量化方式 | 精度损失 | 内存减少 | 推理加速 | 适用场景 |
|----------|---------|---------|---------|---------|
| FP32 → FP16 | 极小 | 50% | 1.2x | 调试阶段 |
| FP32 → INT8 | 小 (~1%) | 75% | 2-4x | 部署首选 |
| FP32 → 混合 | 最小 | 60% | 1.5x | 精度敏感层 |
| 二值化 | 较大 | 93% | 10x+ | 极端资源受限 |

### Edge Impulse 工作流

```
1. 数据采集
   ├── 通过 ESP32-S3 采集传感器数据
   ├── Edge Impulse SDK 上传数据
   └── 标注数据类别

2. 模型训练
   ├── Edge Impulse 在线训练
   ├── 自动量化 (INT8)
   └── 生成 C++ 库

3. 集成部署
   ├── 下载 C++ 库
   ├── 集成到 ESP-IDF 组件
   ├── 分配 PSRAM 作为 tensor arena
   └── 测试推理延迟和精度
```

## 本地 AI 能力矩阵

| 能力 | 版本 | 模型/方案 | 资源占用 | 延迟 |
|------|------|----------|---------|------|
| 唤醒词 | V2.0 | ESP-SR WakeNet 7 | ~60KB IRAM | <200ms |
| 命令词 | V2.0 | ESP-SR MultiNet | ~30KB IRAM | <100ms |
| 情绪分类 | V2.0 | TFLite Micro (INT8) | ~64KB PSRAM | <50ms |
| 手势识别 | V3.0 | TFLite Micro (INT8) | ~128KB PSRAM | <100ms |
| 姿态识别 | V3.0 | TFLite Micro (INT8) | ~128KB PSRAM | <80ms |

## 性能约束

```
ESP32-S3 本地 AI 资源预算:
├── CPU: 单核 Core 1 运行 AI 推理（Core 0 运行 WiFi/蓝牙）
├── IRAM: 本地 AI 最多使用 100KB（含 WakeNet + MultiNet）
├── PSRAM: tensor arena 最多 256KB
├── Flash: 模型文件最多 1MB（存储在 SPIFFS 或 Flash 分区）
├── 推理频率: 唤醒词连续检测 (10Hz), 命令词按需 (1Hz)
└── 功耗: 本地 AI 推理时功耗增加 ~80mA
```

## 与云端 AI 的协同

```
本地 AI (离线)                     云端 AI (在线)
├── 唤醒词检测                      ├── 自然语言理解
├── 简单命令词                      ├── 代码问答
├── 情绪分类                        ├── 技术讨论
└── 离线降级                        └── 复杂推理

协同策略:
├── 本地唤醒词 → 激活云端 ASR
├── 本地命令词 → 直接执行（低延迟路径）
├── 本地情绪 → 影响对话风格
└── 网络断开 → 降级到纯本地模式
```

## 离线模式行为矩阵

| 功能 | 在线模式 | 离线模式 |
|------|---------|---------|
| 唤醒词 | ✅ ESP-SR | ✅ ESP-SR |
| 命令词 | ✅ MultiNet | ✅ MultiNet |
| 自然语言 | ✅ 云端 LLM | ❌ 降级为命令词 |
| TTS 播报 | ✅ 云端 TTS | ❌ 预置提示音 |
| 表情动画 | ✅ 完整 | ⚠️ 简化版 |
| 编程助手 | ✅ 云端 | ❌ 不可用 |
| 番茄钟 | ✅ 本地 | ✅ 本地 |
| 移动控制 | ✅ 本地 | ✅ 本地 |

## 硬约束

1. **本地 AI 推理不能阻塞高优先级任务** — WakeNet 运行在 Core 1，不得影响 Core 0 的音频和显示任务
2. **tensor arena 必须分配在 PSRAM** — DRAM 空间不足以容纳推理缓冲区
3. **模型文件大小 ≤ 1MB** — Flash 空间有限，需量化压缩
4. **唤醒词误唤醒率 < 1次/小时** — 生产环境标准
5. **唤醒词检测延迟 < 300ms** — 用户无感知延迟上限
6. **离线模式必须能正常运行本地功能** — 不得因离线而崩溃

## Outputs

- 本地 AI 模型选择和量化方案
- ESP-SR 集成代码（WakeNet + MultiNet）
- TFLite Micro 推理框架代码
- 离线模式降级策略文档
- 性能基准测试报告（推理延迟、内存占用、功耗）