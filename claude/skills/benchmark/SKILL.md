# benchmark — 性能基准 Skill

> **版本优先级:** P2 (V2.0+ 阶段实现)
> **前置依赖:** freertos-system, coding

## Role

RobotBuddy 性能基准专家，负责建立和执行系统性的性能基准测试，确保每次代码变更不会破坏实时性约束。

## Domain

嵌入式系统性能测试、FreeRTOS 任务分析、内存泄漏检测、音频/显示/电机延迟基准、长时间稳定性测试。

## Responsibility

- 定义各子系统的性能 KPI 和基准指标
- 建立自动化性能基准测试框架
- 检测性能退化（每次 PR 的基准对比）
- 内存泄漏和栈溢出早期预警
- 生成性能趋势报告

## Knowledge Base

### 性能 KPI 矩阵

| 子系统 | KPI | 目标值 | 告警阈值 | 临界阈值 |
|--------|-----|--------|---------|---------|
| **CPU** | CoreMark 分数 | ≥ 350 | < 320 | < 280 |
| **CPU** | Task CPU 占用率 | < 80% | > 85% | > 95% |
| **DRAM** | 空闲堆 | ≥ 100KB | < 80KB | < 50KB |
| **PSRAM** | 空闲堆 | ≥ 4MB | < 3MB | < 2MB |
| **栈** | 各任务栈余量 | ≥ 30% | < 20% | < 10% |
| **ISR** | 最大中断延迟 | ≤ 10μs | > 20μs | > 50μs |
| **IPC** | 消息传递延迟 | ≤ 1ms | > 5ms | > 20ms |
| **音频** | 丢帧率 | 0 | > 0.1% | > 1% |
| **音频** | DMA underrun | 0 | > 0次/h | > 5次/h |
| **显示** | FPS | ≥ 30 | < 25 | < 15 |
| **显示** | SPI 传输时间 | ≤ 15ms/帧 | > 20ms | > 33ms |
| **电机** | PID 响应延迟 | ≤ 10ms | > 20ms | > 50ms |
| **WiFi** | 连接建立时间 | ≤ 3s | > 5s | > 10s |
| **WiFi** | MQTT 消息延迟 | ≤ 100ms | > 500ms | > 2s |
| **LLM** | 端到端响应 | ≤ 1.5s | > 3s | > 5s |
| **启动** | 冷启动时间 | ≤ 3s | > 5s | > 10s |

### 基准测试框架

```c
// benchmark_framework.h — 性能基准测试框架

typedef struct {
    const char* name;           // 测试名称
    const char* category;       // 分类: cpu/memory/latency/audio/display/motion/wifi
    float target_value;         // 目标值
    float warning_threshold;    // 告警阈值
    float critical_threshold;   // 临界阈值
    const char* unit;           // 单位: ms / KB / % / fps / μs
} benchmark_metric_t;

typedef struct {
    benchmark_metric_t metric;
    float measured_value;       // 实测值
    bool passed;                // 是否通过 (实测 < 告警阈值)
    const char* message;        // 附加消息
} benchmark_result_t;

// 基准测试注册
void benchmark_register(const benchmark_metric_t* metric);

// 运行所有基准测试
void benchmark_run_all(void);

// 运行指定分类的基准测试
void benchmark_run_category(const char* category);

// 生成基准报告
void benchmark_report_generate(void);
```

### 各子系统基准测试方法

#### CPU 基准

```c
// CoreMark 基准
void benchmark_cpu_coremark(void) {
    // 运行标准 CoreMark 测试
    // 记录: Iterations/MHz, 总分
    // 对比: ESP32-S3 @ 240MHz 目标 ≥ 350 CoreMark
}

// 任务 CPU 占用率
void benchmark_cpu_task_usage(void) {
    // 启用 CONFIG_FREERTOS_USE_TRACE_FACILITY=y
    // 记录各任务运行时间占比
    // 持续监测 60s 取平均值
}
```

#### 内存基准

```c
// 堆内存趋势
void benchmark_memory_heap(void) {
    // 每 10s 采样一次 heap_caps_get_free_size()
    // 记录 DRAM 和 PSRAM 的空闲堆变化趋势
    // 持续运行 24h 检测泄漏
    // 输出: 初始值 → 最小值 → 最终值 → 趋势斜率
}

// 栈水位监控
void benchmark_memory_stack(void) {
    // 记录各任务的 uxTaskGetStackHighWaterMark()
    // 对比任务创建时的栈大小
    // 计算栈使用率百分比
}
```

#### 延迟基准

```c
// ISR 响应时间
void benchmark_latency_isr(void) {
    // 使用 GPIO 中断 + 示波器测量
    // 或 esp_timer_get_time() 差值测量
    // 记录: 最小值 / 平均值 / 最大值 / P99
}

// IPC 消息传递延迟
void benchmark_latency_ipc(void) {
    // FreeRTOS Queue Send → Receive 延迟
    // 测试不同消息大小: 4B / 64B / 256B / 1KB
    // 记录: 平均延迟 / P99 延迟
}
```

#### 音频基准

```c
// 音频丢帧检测
void benchmark_audio_drop(void) {
    // 统计 I2S DMA underrun/overrun 事件
    // 持续监测 1h
    // 记录: 丢帧次数 / 丢帧率
}

// 音频延迟
void benchmark_audio_latency(void) {
    // Mic → 扬声器 往返延迟
    // 测量: 采集延迟 + 处理延迟 + 播放延迟
    // 目标: < 20ms 本地回环
}
```

#### 显示基准

```c
// 显示 FPS 测量
void benchmark_display_fps(void) {
    // 统计 SPI 传输帧率
    // 测量不同场景: 纯表情 / 动画 / LVGL UI
    // 记录: 平均 FPS / 最低 FPS / 丢帧数
}

// 显示内存占用
void benchmark_display_memory(void) {
    // 测量 LVGL 堆占用
    // 测量帧缓冲占用
    // 记录: 静态占用 / 动态峰值 / 碎片率
}
```

### 长时间稳定性测试

```c
// 24h 稳定性基准
void benchmark_stability_24h(void) {
    // 每 60s 记录:
    //   - DRAM 空闲堆 / PSRAM 空闲堆
    //   - 各任务栈水位
    //   - WiFi RSSI
    //   - 重启次数
    //   - 看门狗触发次数
    // 24h 后分析:
    //   - 内存泄漏率 (KB/h)
    //   - 栈水位趋势
    //   - WiFi 稳定性
    //   - 是否有异常重启
}
```

### 基准报告格式

```markdown
## RobotBuddy 性能基准报告

**日期**: YYYY-MM-DD
**固件版本**: vX.Y.Z (git: abc1234)
**测试时长**: XXmin / 24h

### CPU 性能
| 指标 | 实测值 | 目标值 | 状态 |
|------|--------|--------|------|
| CoreMark 分数 | XXX | ≥350 | ✅/⚠️/❌ |
| 音频任务 CPU% | XX% | <80% | ✅/⚠️/❌ |
| 显示任务 CPU% | XX% | <80% | ✅/⚠️/❌ |
| 空闲任务 CPU% | XX% | >5% | ✅/⚠️/❌ |

### 内存性能
| 指标 | 初始值 | 最小值 | 最终值 | 泄漏率 | 状态 |
|------|--------|--------|--------|--------|------|
| DRAM 空闲 (KB) | XXX | XXX | XXX | X.X KB/h | ✅/⚠️/❌ |
| PSRAM 空闲 (MB) | X.X | X.X | X.X | X.X KB/h | ✅/⚠️/❌ |
| audio 栈余量 | XXX | XXX | — | — | ✅/⚠️/❌ |
| display 栈余量 | XXX | XXX | — | — | ✅/⚠️/❌ |

### 延迟性能
| 指标 | 最小 | 平均 | P99 | 最大 | 状态 |
|------|------|------|-----|------|------|
| ISR 延迟 (μs) | X | X | X | X | ✅/⚠️/❌ |
| IPC 延迟 (ms) | X | X | X | X | ✅/⚠️/❌ |
| 音频端到端 (ms) | X | X | X | X | ✅/⚠️/❌ |

### 实时性能
| 指标 | 实测值 | 目标值 | 状态 |
|------|--------|--------|------|
| 显示 FPS | XX | ≥30 | ✅/⚠️/❌ |
| 音频丢帧率 | X.X% | 0% | ✅/⚠️/❌ |
| PID 响应 (ms) | XX | ≤10 | ✅/⚠️/❌ |
| WiFi 连接 (s) | X.X | ≤3 | ✅/⚠️/❌ |

### 稳定性
| 指标 | 结果 | 状态 |
|------|------|------|
| 运行时长 | 24h | ✅ |
| 异常重启 | 0 次 | ✅/❌ |
| 看门狗触发 | 0 次 | ✅/❌ |
| 内存泄漏 | < 0.5 KB/h | ✅/⚠️/❌ |
```

## 性能退化检测

```
基准对比策略:
├── 每次发布前运行完整基准套件
├── 与上一版本基准对比 (benchmark_history.json)
├── 任何指标退化超过 10% → 阻止发布
├── 任何指标退化超过 5% → 标记警告
└── 新增模块必须附带基准测试用例
```

## 与 /test 命令的协作

| 阶段 | /test | /benchmark |
|------|-------|-----------|
| 功能验证 | ✅ 功能正确性 | ❌ 不涉及 |
| 性能验证 | ❌ 不涉及 | ✅ 性能指标 |
| 回归测试 | ✅ 单元/集成测试 | ✅ 性能回归对比 |
| 发布前 | ✅ L4 压力测试 | ✅ 完整基准套件 |

## 硬约束

1. **基准测试不能影响正常运行时序** — 测试代码运行在低优先级任务，不得抢占音频/显示/电机任务
2. **基准结果必须可复现** — 相同固件版本在相同条件下运行，偏差 < 5%
3. **所有 KPI 必须有明确定义和阈值** — 不得有"差不多就行"的模糊指标
4. **内存泄漏率 > 1KB/h 即为严重问题** — 必须在发布前修复
5. **任何任务栈余量 < 10% 即为严重问题** — 必须增大栈空间或优化栈使用

## Outputs

- 性能基准测试代码（各模块 benchmark_*.c）
- 基准测试报告（Markdown 格式）
- 基准历史数据（JSON 格式，用于趋势对比）
- 性能退化告警和修复建议