# FreeRTOS System Skill — RobotBuddy

## Role

RobotBuddy FreeRTOS 系统专家，负责操作系统的任务管理、事件总线和系统基础架构。

## Domain

ESP-IDF FreeRTOS (Vanilla FreeRTOS with SMP support)，事件驱动架构，系统资源管理。

## Goal

设计和维护一个高效、可靠、可观测的 FreeRTOS 运行时系统。

## Inputs

- 架构设计文档（来自 architecture skill）
- ESP-IDF FreeRTOS API 参考
- ESP32-S3 技术参考手册

## Outputs

- FreeRTOS 任务配置头文件
- 事件总线实现代码
- 系统监控服务代码
- `docs/system/freertos-config.md`

## Core Components

### 1. 任务注册与管理

```c
// task_registry.h
// 所有 Task 的集中注册表，确保优先级/栈/核心不冲突

typedef struct {
    const char *name;
    TaskFunction_t function;
    uint16_t stack_depth;       // words (×4 = bytes)
    UBaseType_t priority;       // 0-8
    BaseType_t core_id;         // 0 = PRO_CPU, 1 = APP_CPU
    uint32_t period_ms;         // 0 = event-driven
    TaskHandle_t *handle_out;
} task_config_t;

// 注册表
static const task_config_t g_task_registry[] = {
    { "audio_cap",  audio_capture_task,  2048, 8, 0, 0,         &h_audio_cap },
    { "audio_play", audio_playback_task, 2048, 7, 0, 0,         &h_audio_play },
    { "display",    display_task,        1024, 6, 1, 33,        &h_display },
    { "emotion",    emotion_task,        1024, 5, 1, 50,        &h_emotion },
    { "cloud",      cloud_task,          3072, 4, 0, 0,         &h_cloud },
    { "motion",     motion_task,          512, 3, 1, 10,        &h_motion },
    { "sensor",     sensor_task,          512, 2, 1, 50,        &h_sensor },
    { "behavior",   behavior_task,       1024, 1, 1, 100,       &h_behavior },
    { "monitor",    monitor_task,         256, 0, 1, 1000,      &h_monitor },
};
```

### 2. 事件总线

```c
// event_bus.h
// 中心化事件分发，解耦任务间通信

typedef void (*event_handler_t)(const robot_event_t *event);

// 订阅事件类型
esp_err_t event_bus_subscribe(robot_event_id_t event_id, event_handler_t handler);

// 发布事件 (ISR-safe 版本)
esp_err_t event_bus_publish(const robot_event_t *event);
esp_err_t event_bus_publish_from_isr(const robot_event_t *event);

// 批量分发（从 event_loop Task 调用）
void event_bus_dispatch(void);
```

### 3. 系统监控

```c
// sysmon.h
// 运行时健康监测

typedef struct {
    // 栈水位 (bytes free)
    uint32_t stack_free[10];      // 每个 Task

    // 堆状态
    uint32_t heap_free_dram;
    uint32_t heap_free_psram;
    uint32_t heap_largest_free_block;

    // CPU 使用率 (%)
    float cpu_usage[2];           // Core 0, Core 1

    // 任务运行时间统计
    uint32_t task_runtime_ms[10];

    // 系统心跳
    uint32_t uptime_seconds;
    uint32_t watchdog_reset_count;

} sysmon_report_t;

void sysmon_get_report(sysmon_report_t *report);
```

### 4. 电源管理

```c
typedef enum {
    POWER_MODE_ACTIVE,      // 全功能运行
    POWER_MODE_LIGHT_SLEEP, // WiFi 保持，屏幕/电机休眠
    POWER_MODE_DEEP_SLEEP,  // 仅唤醒词检测
} power_mode_t;

esp_err_t power_mode_transition(power_mode_t new_mode);
```

## Task Design Rules

1. **入口函数的永恒循环模式**
```c
void my_task(void *arg) {
    // 初始化
    while (1) {
        // 等待事件 (Queue / Notification / Delay)
        if (xQueueReceive(queue, &msg, portMAX_DELAY)) {
            // 处理
        }
    }
    // 清理 (永不执行到此处)
}
```

2. **栈大小设定**
   - 先给 2x 估算值
   - 运行后通过 uxTaskGetStackHighWaterMark() 测量
   - 降低到 watermark × 1.5

3. **优先级分配**
   - 实时约束任务: 6-8
   - 业务逻辑任务: 3-5
   - 后台任务: 0-2
   - 同优先级用 time-slicing (configUSE_TIME_SLICING=1)

4. **Core 分配**
   - Core 0 (PRO_CPU): WiFi/BT 协议栈 + 音频 (默认运行协议栈)
   - Core 1 (APP_CPU): 显示 + 运动 + 行为 (默认运行应用)

5. **喂狗策略**
```c
// 每个有循环的 Task 应该 tick 看门狗
const TickType_t wd_timeout = pdMS_TO_TICKS(task_period_ms * 3);
esp_task_wdt_add(NULL);
while (1) {
    esp_task_wdt_reset();  // 喂狗
    // ... 正常工作
}
```

## Checklist

- [ ] 所有 Task 使用统一的 task_config_t 注册
- [ ] 栈水位检查 ≥ 512 bytes free
- [ ] 堆空间充足 (DRAM ≥ 50KB, PSRAM ≥ 1MB free)
- [ ] 事件总线无消息泄漏 (payload 正确 free)
- [ ] 系统监控 Task 正常运行 (uptime 正确递增)
- [ ] 看门狗正确配置 (所有 Task 喂狗)
- [ ] 电源模式切换无竞态
- [ ] CPU 使用率 ≤ 80% (峰值)
- [ ] 无优先级反转 (互斥量使用正确)
