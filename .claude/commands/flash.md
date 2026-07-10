# /flash — RobotBuddy 构建烧录监控

## 用途

一键完成 RobotBuddy 固件的构建、烧录和串口监控的快捷流程。

## 适用场景

- 日常开发迭代（修改代码后快速验证）
- 驱动调试（修改后立即看到效果）
- 临时测试（快速烧录验证某个功能点）

## 工作流

```
第 1 阶段：预检查
├── 1.1 检查 ESP-IDF 环境是否就绪（idf.py 可用）
├── 1.2 检查串口设备是否连接（Windows: COMx, Linux: /dev/ttyUSBx）
├── 1.3 检查当前代码是否有未提交的修改（git status）
├── 1.4 检查 sdkconfig 是否存在且有效
└── 1.5 输出：环境就绪确认

第 2 阶段：构建
├── 2.1 清理（可选）: idf.py fullclean — 仅在配置变更时
├── 2.2 增量构建: idf.py build
├── 2.3 检查构建结果:
│   ├── 0 error, 0 warning → 继续
│   ├── 有 error → 停止，输出错误信息
│   └── 有 warning → 提示但继续
├── 2.4 输出固件大小分析: idf.py size-components
└── 2.5 输出：编译成功的固件

第 3 阶段：烧录
├── 3.1 自动检测串口: esptool.py 列出可用端口
├── 3.2 烧录: idf.py -p <PORT> flash
│   ├── 成功 → 继续监控
│   ├── 超时 → 提示检查连接/按 BOOT 键
│   └── 失败 → 提示检查驱动/串口/权限
└── 3.3 输出：烧录成功确认

第 4 阶段：监控
├── 4.1 启动串口监控: idf.py -p <PORT> monitor
├── 4.2 关键日志过滤:
│   ├── 启动信息: "cpu_start: Starting scheduler"
│   ├── WiFi 连接: "got ip"
│   ├── 错误信息: "Guru Meditation" / "assert failed"
│   └── 模块初始化: ESP_LOGI 输出
├── 4.3 Ctrl+] 退出监控
└── 4.4 输出：运行日志
```

## 快捷命令

```bash
# 一键构建+烧录+监控（最常用）
idf.py -p COM3 flash monitor

# 仅构建
idf.py build

# 仅烧录（不构建）
idf.py -p COM3 flash

# 仅监控
idf.py -p COM3 monitor

# 擦除 Flash 后重新烧录（固件异常时使用）
idf.py -p COM3 erase-flash
idf.py -p COM3 flash monitor

# 查看固件大小分析
idf.py size-components

# 查看分区使用
idf.py partition-table
```

## 串口常见问题

| 问题 | 原因 | 解决 |
|------|------|------|
| 串口未找到 | 驱动未安装 / USB 线仅充电 | 安装 CP210x/CH340 驱动，换数据线 |
| 烧录超时 | ESP32 未进入下载模式 | 按住 BOOT 键再点 RST，或使用 -b 115200 降低波特率 |
| 乱码 | 波特率不匹配 | monitor 默认 115200，确认 sdkconfig 匹配 |
| Permission denied | Linux 串口权限 | sudo usermod -aG dialout $USER |
| 设备忙 | 其他程序占用串口 | 关闭其他串口工具（Putty/Arduino IDE） |

## 烧录验证清单

- [ ] 编译 0 error, 0 warning
- [ ] 烧录成功（无超时/校验错误）
- [ ] 串口日志正常启动（FreeRTOS scheduler running）
- [ ] WiFi 连接成功（如需要）
- [ ] 屏幕点亮（如已接入）
- [ ] 无 Panic / Watchdog 重启
