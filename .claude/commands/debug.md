# /debug — RobotBuddy 固件调试

## 用途

系统化诊断 RobotBuddy ESP32-S3 固件问题，从症状到根因。

## 适用场景

- 固件崩溃（Panic / Guru Meditation Error）
- 内存不足（OOM / Stack Overflow）
- WiFi 断连/重连异常
- 音频卡顿/丢帧
- 屏幕花屏/刷新异常
- 电机控制异常

## 工作流

```
第 1 阶段：信息收集
├── 1.1 获取串口日志（idf.py monitor 完整输出）
├── 1.2 获取 Core Dump（espcoredump 解析）
├── 1.3 获取当前代码版本（git rev-parse HEAD）
├── 1.4 复现步骤记录
└── 1.5 输出：问题信息汇总

第 2 阶段：分类诊断
├── 2.1 Panic 诊断流程:
│   ├── 解析 Backtrace: xtensa-esp32s3-elf-addr2line
│   ├── 识别异常类型: LoadProhibited / StoreProhibited / DivideByZero / ...
│   ├── 检查 EXCVADDR → 确定非法地址来源
│   └── 检查 A0 (返回地址) → 追溯调用链
│
├── 2.2 内存问题诊断流程:
│   ├── 检查栈水位: uxTaskGetStackHighWaterMark() for each task
│   ├── 检查堆状态: heap_caps_get_info() → 碎片率
│   ├── 检查 PSRAM: heap_caps_get_free_size(MALLOC_CAP_SPIRAM)
│   ├── 启用堆跟踪: CONFIG_HEAP_TRACING=y → 检测泄漏
│   └── 检查 malloc 返回值是否为 NULL
│
├── 2.3 WiFi 问题诊断流程:
│   ├── 检查 WiFi 事件日志: WIFI_EVENT / IP_EVENT
│   ├── 检查 RSSI: esp_wifi_sta_get_rssi()
│   ├── 检查重连次数: 自动重连是否启用
│   ├── 检查 DNS: ping 测试 IP / 域名
│   └── 电源管理: CONFIG_ESP_WIFI_PS 设置
│
├── 2.4 音频问题诊断流程:
│   ├── 检查 I2S DMA buffer 是否足够（≥ 4096）
│   ├── 检查 WiFi 带宽是否充足（WebSocket 流）
│   ├── 检查 I2S 时钟精度（APLL 锁定）
│   └── 检查音频任务 CPU 占用（vTaskGetRunTimeStats）
│
├── 2.5 显示问题诊断流程:
│   ├── 检查 SPI 时钟（ST7789: ≤40MHz）
│   ├── 检查帧缓冲 DMA 对齐
│   ├── 检查 LVGL 任务栈大小
│   └── 信号完整性: SPI 线长度/干扰
│
└── 2.6 输出：根因分析报告

第 3 阶段：修复验证
├── 3.1 提出修复方案
├── 3.2 实现修复
├── 3.3 回归测试（原复现步骤 + 24h 压力测试）
└── 3.4 输出：修复验证报告
```

## 常见错误速查

| 错误 | 原因 | 检查 |
|------|------|------|
| `Guru Meditation: LoadProhibited` | 访问 NULL/已释放指针 | backtrace 定位 |
| `Stack overflow in task xxx` | 任务栈太小 | 增加栈 2x |
| `heap_caps_alloc failed` | PSRAM/DRAM 不足 | 检查内存泄漏 |
| `wifi: disconnect, reason:200` | Beacon 超时 | WiFi 电源策略 |
| `I2S: buffer underrun` | 消费不及时 | 增大 DMA buffer |
| `SPI: transmission timeout` | 硬件连接/频率过高 | 降低频率 |
| `Watchdog trigger: Task xxx` | 任务死循环/阻塞 | 加 vTaskDelay / 喂狗 |

## 调试工具

```
硬件:
├── ESP-Prog (JTAG) — 断点调试、寄存器查看
├── 逻辑分析仪     — SPI/I2C/I2S 信号捕获
├── 示波器         — 电源纹波、PWM 波形
└── USB-UART       — 串口日志 (921600bps)

软件:
├── idf.py monitor              — 串口日志
├── espcoredump.py              — Core dump 解析
├── xtensa-esp32s3-elf-gdb      — GDB 调试
├── idf.py size-components      — 固件大小分析
├── idf.py app-flash-size       — 分区使用分析
└── SystemView                  — FreeRTOS 任务时序可视化
```
