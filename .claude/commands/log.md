# /log — RobotBuddy 日志收集与分析

## 用途

收集、过滤和分析 RobotBuddy ESP32-S3 运行时日志，辅助问题定位和系统健康评估。

## 适用场景

- 长时间运行后导出日志进行离线分析
- 生成系统健康报告（内存趋势、任务栈使用、异常频率）
- Core Dump 解析与 Backtrace 符号化
- 特定模块异常排查（WiFi 断连、音频卡顿、显示异常）
- NVS 错误记录导出

## 工作流

```
第 1 阶段：日志收集
├── 1.1 串口日志收集
│   ├── idf.py monitor → 实时串口输出（921600bps）
│   ├── Putty / minicom 日志抓取 → 保存为 .log 文件
│   └── 输出：uart_log_YYYYMMDD_HHMMSS.log
├── 1.2 Flash 日志分区读取
│   ├── 使用 NVS 分区读取持久化的错误计数器
│   ├── 读取 ota_boot namespace: watchdog_count / panic_count
│   └── 输出：flash_error_snapshot.txt
├── 1.3 Core Dump 收集
│   ├── idf.py coredump-info → 解析当前 Core Dump
│   ├── idf.py coredump-debug → GDB 调试 Core Dump
│   ├── esptool.py read_flash coredump分区 → 导出原始数据
│   └── 输出：coredump_analysis.txt
├── 1.4 NVS 错误记录导出
│   ├── 读取系统错误计数器（watchdog/panic/reboot 次数）
│   ├── 读取 WiFi 断连历史记录
│   └── 输出：nvs_error_log.txt
└── 1.5 输出：日志收集汇总

第 2 阶段：日志过滤与分类
├── 2.1 按日志级别过滤
│   ├── --errors: 仅显示 ESP_LOGE / ESP_LOGW
│   ├── --warnings: 仅显示 ESP_LOGW
│   └── --verbose: 显示所有级别（含 ESP_LOGD / ESP_LOGV）
├── 2.2 按模块过滤
│   ├── --module wifi: WiFi 相关（esp_wifi / WIFI_EVENT / IP_EVENT）
│   ├── --module audio: 音频相关（I2S / A2DP / WebSocket 流）
│   ├── --module display: 显示相关（SPI / LVGL / ST7789）
│   ├── --module motion: 电机控制（PWM / MPU6050 / I2C）
│   ├── --module ota: OTA 升级流程（esp_https_ota / 分区切换）
│   └── --module system: FreeRTOS 系统（任务创建 / 内存分配 / 看门狗）
├── 2.3 按时间范围过滤
│   ├── --since 2h: 最近 2 小时的日志
│   ├── --since "2026-07-10 10:00": 从指定时间开始
│   └── --range "2026-07-10 10:00" "2026-07-10 12:00": 时间区间
├── 2.4 按关键词过滤
│   ├── --grep "panic": 搜索包含 panic 的行
│   ├── --grep "heap_caps_alloc failed": 内存分配失败
│   └── --grep "disconnect, reason": WiFi 断线原因
└── 2.5 输出：过滤后的日志文件

第 3 阶段：日志分析
├── 3.1 错误频率统计
│   ├── 统计各类错误出现次数与时间分布
│   ├── 绘制错误时间线（异常事件序列）
│   └── 输出：错误频率统计表 + 时间线图
├── 3.2 内存趋势分析
│   ├── 提取 heap_caps_get_free_size() 日志点
│   ├── 绘制 8MB PSRAM + 512KB DRAM 使用趋势
│   ├── 检测内存泄漏特征（持续下降不回收）
│   └── 输出：内存趋势图 + 泄漏告警
├── 3.3 WiFi 断连模式识别
│   ├── 统计断开频率与原因码分布
│   ├── 识别 Beacon 超时 / 认证失败 / AP 不可达 模式
│   ├── 检查 RSSI 时序（弱信号区间检测）
│   └── 输出：WiFi 稳定性报告
├── 3.4 任务重启计数分析
│   ├── 统计 watchdog reset / task reset 次数
│   ├── 分析各 FreeRTOS 任务的栈水位趋势
│   ├── 检测特定任务栈溢出前兆（水位持续下降）
│   └── 输出：任务健康报告
└── 3.5 输出：日志分析摘要

第 4 阶段：报告生成
├── 4.1 系统健康报告 (md 格式)
│   ├── 设备信息 (MAC / 固件版本 / 运行时长)
│   ├── 内存状态 (DRAM / PSRAM 峰值与当前值)
│   ├── 任务状态 (各任务栈水位 / CPU 占用)
│   ├── 异常统计 (PANIC / Watchdog / OOM 计数)
│   ├── WiFi 连接统计 (断开次数 / 平均 RSSI)
│   └── 健康评分 (0-100，各项指标加权)
├── 4.2 故障时间线报告
│   ├── 按时间顺序列出所有异常事件
│   ├── 标注根因与影响范围
│   └── 关联相关日志片段
├── 4.3 Core Dump 解析报告
│   ├── 崩溃原因摘要
│   ├── Backtrace 符号化结果
│   ├── 寄存器快照分析
│   └── 修复建议
└── 4.4 输出：{report_type}_{device_id}_{date}.md
```

## 常用过滤选项

| 选项 | 说明 | 示例 |
|------|------|------|
| `--errors` | 仅显示错误级别日志 (E/W) | `/log --errors` |
| `--warnings` | 仅显示警告级别日志 (W) | `/log --warnings` |
| `--module <name>` | 按模块过滤 (wifi/audio/display/motion/ota/system) | `/log --module audio` |
| `--since <time>` | 从指定时间开始 (2h / 30m / "2026-07-10 10:00") | `/log --since 2h` |
| `--range <start> <end>` | 指定时间区间 | `/log --range "2026-07-10 10:00" "12:00"` |
| `--grep <pattern>` | 按关键词搜索 | `/log --grep "heap_caps_alloc"` |
| `--coredump` | 解析 Core Dump 并生成报告 | `/log --coredump` |
| `--health` | 生成系统健康报告 | `/log --health` |
| `--export <format>` | 导出格式 (md / json / csv) | `/log --export json` |
| `--tail <n>` | 仅查看最后 n 行 | `/log --tail 500` |

## Core Dump 解析流程

```
Core Dump 解析三步走:

第一步：检查 Core Dump 是否存在
├── idf.py coredump-info
│   └── 输出崩溃摘要: 异常类型 / 寄存器快照 / Backtrace 地址
└── 如无 Core Dump → 检查 partitions.csv 是否预留 coredump 分区

第二步：符号化 Backtrace
├── 方法 1: espcoredump.py（推荐）
│   ├── espcoredump.py info_corefile \
│   │     --core build/coredump.bin \
│   │     --core-format raw \
│   │     build/robotbuddy.elf
│   └── 输出：完整符号化调用栈
│
├── 方法 2: addr2line 逐个符号化
│   ├── xtensa-esp32s3-elf-addr2line \
│   │     -e build/robotbuddy.elf \
│   │     -f -p -C 0x400d1234 0x400d1456 ...
│   └── 输出：符号 → 函数名 + 源文件:行号
│
└── 方法 3: GDB 交互式分析
    ├── xtensa-esp32s3-elf-gdb build/robotbuddy.elf
    ├── (gdb) target remote :3333  (需 JTAG 连接)
    ├── (gdb) info registers
    ├── (gdb) bt  (Backtrace)
    ├── (gdb) frame 0
    ├── (gdb) info locals
    └── (gdb) x/16x $sp  (栈内存检查)

第三步：根因分析
├── EXCVADDR 分析 → 确定非法地址类型 (NULL / 已释放 / 越界)
├── A0 寄存器 → 追溯调用链起点
├── 栈内容检查 → 是否有栈溢出痕迹 (模式 0xABABABAB)
└── 映射到源码 → 定位崩溃函数与调用路径
```

## 输出格式：日志分析报告模板

```markdown
# RobotBuddy 系统健康报告

## 基本信息
- 设备 ID: {MAC_ADDRESS}
- 固件版本: V{MAJOR}.{MINOR}.{PATCH}
- 运行时长: {DAYS}d {HOURS}h {MINUTES}m
- 日志时间范围: {START_TIME} ~ {END_TIME}

## 内存状态
| 内存类型 | 总量 | 已用 | 空闲 | 峰值占用 | 碎片率 |
|----------|------|------|------|----------|--------|
| DRAM     | 512KB | - | - | - | - |
| PSRAM    | 8MB | - | - | - | - |
| SPI Flash| 16MB | - | - | - | - |

内存趋势: [正常 / 有泄漏风险 / 严重泄漏]
> 详细数据见 {memory_chart_link}

## 任务状态
| 任务名 | 优先级 | 当前栈水位 | 最小栈水位 | CPU占用% | 状态 |
|--------|--------|------------|------------|----------|------|
| - | - | - | - | - | - |

## 异常统计
| 异常类型 | 次数 | 首次发生 | 最后发生 | 趋势 |
|----------|------|----------|----------|------|
| PANIC (Guru Meditation) | - | - | - | - |
| Watchdog Reset | - | - | - | - |
| OOM (heap_caps_alloc failed) | - | - | - | - |
| Stack Overflow | - | - | - | - |
| I2S Buffer Underrun | - | - | - | - |
| SPI Timeout | - | - | - | - |

## WiFi 连接统计
- 总断开次数: -
- 平均 RSSI: - dBm
- 断开原因分布:
  | 原因码 | 含义 | 次数 |
  |--------|------|------|
  | - | - | - |

## 健康评分: {SCORE}/100
- 内存: {score}/25
- 任务: {score}/25
- 异常: {score}/25
- 网络: {score}/25

## 建议
1. -
2. -
```

## 常见日志模式识别

| 日志模式 | 含义 | 紧急度 | 排查方向 |
|----------|------|--------|----------|
| `Guru Meditation Error: LoadProhibited` | 加载非法地址（NULL/已释放指针） | 严重 | backtrace 定位，检查指针初始化 |
| `Guru Meditation Error: StoreProhibited` | 写入非法地址（只读/越界） | 严重 | 检查写操作目标地址合法性 |
| `Guru Meditation Error: DivideByZero` | 除零异常 | 严重 | 检查除数是否为 0 的条件 |
| `Guru Meditation Error: InstrFetchProhibited` | 跳转到非法指令地址 | 严重 | 函数指针损坏或栈溢出覆盖 |
| `Stack overflow in task <task_name>` | 任务栈溢出 | 严重 | 增加栈大小 2x，检查局部变量 |
| `heap_caps_alloc failed` | PSRAM/DRAM 分配失败 | 严重 | 检查内存泄漏，评估内存峰值 |
| `assert failed: <expr>` | 断言失败 | 严重 | 检查前置条件，定位违反约束场景 |
| `I2S: buffer underrun` | I2S DMA 消费不及时 | 中等 | 增大 DMA buffer，提高音频任务优先级 |
| `I2S: buffer overrun` | I2S DMA 写入过慢 | 中等 | 检查 I2S 时钟精度，增大 buffer |
| `Watchdog trigger: Task <task_name>` | 任务看门狗超时 | 严重 | 任务死循环/阻塞，加 vTaskDelay 或喂狗 |
| `wifi: disconnect, reason:200` | WiFi Beacon 超时 | 中等 | 检查 AP 距离，调整 WiFi 电源策略 |
| `wifi: disconnect, reason:202` | 认证失败 | 中等 | 检查 WiFi 密码，检查 AP 加密模式 |
| `wifi: disconnect, reason:8` | AP 关联失败 | 中等 | 检查 MAC 过滤，检查 AP 容量 |
| `SPI: transmission timeout` | SPI 通信超时 | 中等 | 降低 SPI 频率，检查硬件连接 |
| `ESP_INTR_FLAG_IRAM: handler not in IRAM` | ISR 不在 IRAM | 严重 | 在 ISR 函数添加 IRAM_ATTR |
| `E (xxx) Brownout detector was triggered` | 电源电压过低 | 严重 | 检查供电，USB 供电 ≥ 5V/2A |
| `reboot: CONFIG_ESP_SYSTEM_RESET_REASON_SOFTWARE` | 软件复位 | 信息 | 检查 esp_restart() 调用位置 |
| `reboot: CONFIG_ESP_SYSTEM_RESET_REASON_PANIC` | PANIC 后重启 | 严重 | 解析 Core Dump，回溯 PANIC 根因 |
| `esp_image: invalid segment length` | 固件镜像损坏 | 严重 | 重新烧录，检查 Flash 芯片状态 |
| `nvs_flash_init: NVS partition truncated` | NVS 分区损坏 | 中等 | 擦除 NVS 或使用出厂 NVS 恢复 |

## 注意事项

- 串口日志缓冲区有限（默认 UART0 环形缓冲约 2KB），高频日志可能导致丢帧，建议使用硬件流控（RTS/CTS）或降低日志级别
- 长时间运行时日志量会很大，建议周期性导出（每 24h 导出一次），避免覆盖有价值的早期日志
- Core Dump 需要预先在 partitions.csv 中配置 coredump 分区（建议 ≥ 64KB），否则崩溃时无法保存
- Core Dump 解析需要与固件匹配的 ELF 文件，确保 ELF 文件和 Core Dump 来自同一次构建
- Backtrace 符号化依赖编译时的调试信息（CONFIG_ELF_LOADER_ENABLE=y），Release 构建时需开启
- NVS 中的错误计数器在正常重启后应清零，若持续增长说明存在未解决的异常重启问题
- 分析日志时注意 ESP32-S3 的 RTC 时间在深度睡眠后会重置，时间戳可能不连续
- `/log --coredump` 依赖 espcoredump.py 工具（ESP-IDF 自带），需确保 ESP-IDF 环境变量已配置
- 同一设备多次导出日志时，建议在文件名中标注导出时间和设备 ID，便于事后追溯
- 健康评分仅供参考，具体阈值需根据 RobotBuddy 实际运行场景调整
