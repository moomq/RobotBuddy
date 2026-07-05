# /firmware — RobotBuddy 固件模块开发

## 用途

在 RobotBuddy 现有固件架构基础上，开发新的 FreeRTOS 任务模块或服务模块。

## 适用场景

- 新增一个 FreeRTOS Task（如番茄钟、Git 状态轮询）
- 新增一个 Service Manager（如新传感器管理器）
- 修改现有模块功能
- 优化任务栈/优先级分配

## 工作流

```
第 1 阶段：模块设计
├── 1.1 调用 architecture skill → 确定模块在系统架构中的位置
├── 1.2 调用 freertos-system skill → 任务优先级、栈大小、事件消息设计
├── 1.3 确认与其他模块的通信接口（Queue / Event Group / Ring Buffer）
└── 1.4 输出：模块设计文档

第 2 阶段：编码
├── 2.1 调用 coding skill → 按 ESP32 编码规范实现
├── 2.2 FreeRTOS Task 骨架：xTaskCreate + task handler
├── 2.3 事件总线集成：生产者/消费者队列
├── 2.4 错误处理：assert + error log + watchdog 喂狗
├── 2.5 输出：模块代码（.c + .h）

第 3 阶段：构建 & 烧录
├── 3.1 idf.py build → 确认编译无误
├── 3.2 idf.py flash → 烧录到 ESP32-S3
├── 3.3 idf.py monitor → 串口日志检查
├── 3.4 输出：烧录成功的固件 + 启动日志

第 4 阶段：集成测试
├── 4.1 调用 testing skill → 任务间通信测试
├── 4.2 内存分析：uxTaskGetStackHighWaterMark / heap_caps_get_free_size
├── 4.3 长时间运行测试（≥2h）检测内存泄漏
├── 4.4 输出：集成测试报告 + 内存分析报告

第 5 阶段：审查
├── 5.1 调用 review skill → 代码审查
├── 5.2 检查 ISR 安全（FromISR 后缀函数使用）
├── 5.3 检查优先级反转风险
└── 5.4 输出：审查报告
```

## FreeRTOS 约束

```
优先级分配原则：
├── 音频采集/播放  — 最高优先级 (7-8)  — 实时性要求
├── 屏幕刷新       — 高优先级 (5-6)    — 30FPS 保证
├── 运动控制       — 中优先级 (3-4)    — 100Hz PID
├── 云端通信       — 中优先级 (3-4)    — 事件驱动
├── 行为决策       — 低优先级 (1-2)    — 100ms 周期
└── 空闲监控       — 最低优先级 (0)    — 1s 周期

栈大小建议：
├── 音频任务       — 8KB min
├── 云端通信       — 12KB min (TLS 握手)
├── 显示任务       — 4KB min
├── 简单控制任务   — 2KB min
└── 监控任务       — 1KB min

中断安全：
├── ISR 内只使用 FromISR 后缀的 FreeRTOS API
├── ISR 不能使用 printf / ESP_LOGI（用 ESP_EARLY_LOGI）
├── ISR 不能使用 vTaskDelay
└── 中断优先级 ≤ configMAX_SYSCALL_INTERRUPT_PRIORITY
```
