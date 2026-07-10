# /deps — RobotBuddy 开发环境依赖检查

## 用途

检查 RobotBuddy ESP32-S3 开发环境的所有依赖项是否就绪，确保开发工具链、Python 包和硬件驱动均已正确安装且版本满足要求。

## 适用场景

- 新成员首次搭建开发环境后的验证
- 升级 ESP-IDF 版本后确认工具链兼容性
- CI/CD 流水线中的环境就绪检查
- 长时间未开发后恢复工作前的环境诊断
- 编译或烧录失败时排查环境问题

## 工作流

```
第 1 阶段：工具链检查
├── 1.1 检查 ESP-IDF 版本（≥ 5.1）
│   ├── 读取 IDF_PATH 环境变量
│   ├── 检查 idf.py --version 输出
│   └── 验证 xtensa-esp32s3 工具链路径
├── 1.2 检查 CMake 版本（≥ 3.16）
│   └── cmake --version
├── 1.3 检查 Python 版本（≥ 3.8）
│   └── python3 --version / python --version
├── 1.4 检查 Git 版本（≥ 2.30）
│   └── git --version
├── 1.5 检查 xtensa-esp32s3 交叉编译工具链
│   ├── xtensa-esp32s3-elf-gcc --version
│   └── 确认工具链在 PATH 中
└── 1.6 输出：工具链检查报告

第 2 阶段：Python 包检查
├── 2.1 检查 ESP-IDF 核心依赖包
│   ├── esptool — 烧录工具
│   ├── esp-coredump — Core Dump 解析
│   ├── pyserial — 串口通信
│   ├── idf-component-manager — 组件管理
│   └── freertos_gdb — FreeRTOS GDB 调试
├── 2.2 检查可选 Python 包
│   ├── pytest — 单元测试框架
│   ├── pylint — 代码风格检查
│   └── kconfiglib — Kconfig 解析
├── 2.3 验证 ESP-IDF Python 虚拟环境（idf.py 自带安装检查）
│   └── idf.py check-python-dependencies
└── 2.4 输出：Python 包检查报告

第 3 阶段：硬件驱动检查
├── 3.1 Windows 驱动检查
│   ├── 检查 CP210x USB-UART 驱动（设备管理器 → 端口）
│   ├── 检查 CH340 USB-UART 驱动
│   ├── 验证串口设备 COMx 是否存在
│   └── 检查驱动版本是否为最新
├── 3.2 Linux 驱动检查
│   ├── 检查 udev 规则是否配置（/etc/udev/rules.d/）
│   ├── 检查当前用户是否在 dialout 组
│   ├── 验证 /dev/ttyUSBx 或 /dev/ttyACMx 权限
│   └── 检查 brltty 是否抢占串口（常见冲突）
├── 3.3 macOS 驱动检查
│   ├── 检查 /dev/cu.usbserial-* 是否存在
│   └── 验证无 kext 驱动冲突
└── 3.4 输出：硬件驱动检查报告
```

## 检查项详细表格

### 工具链

| 检查项 | 最低版本 | 检查命令 | 说明 |
|--------|---------|----------|------|
| ESP-IDF | 5.1.0 | `idf.py --version` | RobotBuddy 基于 ESP-IDF v5.x 开发 |
| xtensa-esp32s3-gcc | 12.2.0 | `xtensa-esp32s3-elf-gcc --version` | ESP32-S3 交叉编译器 |
| CMake | 3.16.0 | `cmake --version` | ESP-IDF 构建系统要求 |
| Python | 3.8.0 | `python3 --version` | IDF 工具脚本和组件管理 |
| Git | 2.30.0 | `git --version` | 版本控制和子模块管理 |

### Python 包

| 检查项 | 最低版本 | 检查命令 | 说明 |
|--------|---------|----------|------|
| esptool | 4.6 | `pip show esptool` | ESP32 Flash/烧录工具 |
| esp-coredump | 1.5 | `pip show esp-coredump` | Core Dump 解析 |
| pyserial | 3.5 | `pip show pyserial` | 串口通信库 |
| idf-component-manager | 1.4 | `pip show idf-component-manager` | 组件依赖管理 |
| freertos_gdb | 1.0 | `pip show freertos_gdb` | FreeRTOS GDB 调试脚本 |

### 硬件驱动

| 检查项 | 平台 | 检查方法 | 说明 |
|--------|------|----------|------|
| CP210x 驱动 | Windows | 设备管理器 → 端口 | RobotBuddy 默认 USB-UART 芯片 |
| CH340 驱动 | Windows | 设备管理器 → 端口 | 部分模组使用 CH340 |
| 串口设备 | Windows | `mode COM3` | 确认 COM 端口可访问 |
| 串口设备 | Linux | `ls /dev/ttyUSB*` | 确认 USB 串口已挂载 |
| dialout 组 | Linux | `groups | grep dialout` | 确认用户有串口权限 |
| udev 规则 | Linux | `ls /etc/udev/rules.d/*esp*` | 自动权限规则 |

## 常见问题修复建议

| 问题 | 现象 | 修复方法 |
|------|------|----------|
| ESP-IDF 未安装 | `idf.py` 命令未找到 | 按照 ESP-IDF 官方文档安装：`git clone -b v5.1 --recursive https://github.com/espressif/esp-idf.git` 然后运行 `install.bat esp32s3` |
| 工具链版本过低 | 编译错误、不支持的指令 | 更新 ESP-IDF 并重新运行 `install.bat esp32s3` |
| Python 包缺失 | `ModuleNotFoundError` | 运行 `idf.py check-python-dependencies` 并按提示安装；或 `pip install -r $IDF_PATH/requirements.txt` |
| 虚拟环境未激活 | 导入 ESP-IDF 工具报错 | 运行 `%IDF_PATH%\export.bat` (Windows) 或 `source $IDF_PATH/export.sh` (Linux/macOS) |
| 串口驱动未安装 | 串口未找到 / COM 口不出现 | [CP210x 驱动下载](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) / [CH340 驱动下载](http://www.wch.cn/downloads/CH341SER_EXE.html) |
| 串口权限不足 | `Permission denied` (Linux) | `sudo usermod -aG dialout $USER` 然后重新登录 |
| 串口被占用 | `设备忙` / `Access denied` | 关闭其他串口工具（PuTTY、Arduino IDE、CuteCom）；Linux 检查 brltty：`sudo apt remove brltty` |
| CMake 版本过低 | 配置阶段失败 | Windows: 安装 CMake 3.16+；Linux: `pip install cmake` 或系统包管理器升级 |
| Python 版本过低 | IDF 脚本语法错误 | 安装 Python 3.8+；Windows 建议直接用 ESP-IDF 自带的 Python |

## 输出格式

```
=== RobotBuddy 开发环境依赖检查报告 ===
日期: 2026-07-10
平台: Windows 10 / Linux / macOS

--- 第 1 阶段：工具链 ---
✅ ESP-IDF:    5.1.2 (OK)
✅ 工具链:      xtensa-esp32s3-elf-gcc 12.2.0 (OK)
✅ CMake:       3.27.0 (OK)
✅ Python:      3.9.13 (OK)
✅ Git:         2.41.0 (OK)

--- 第 2 阶段：Python 包 ---
✅ esptool:                  4.6.1 (OK)
✅ esp-coredump:             1.5.0 (OK)
✅ pyserial:                 3.5 (OK)
❌ idf-component-manager:    未安装
   → 修复: pip install idf-component-manager
✅ freertos_gdb:             1.0.3 (OK)

--- 第 3 阶段：硬件驱动 ---
✅ 串口设备:    COM3 (Silicon Labs CP210x)
❌ 驱动签名:    未检测到 CH340 驱动
   → 修复: 从 http://www.wch.cn/downloads/CH341SER_EXE.html 下载安装

=== 总结 ===
工具链:   ✅ 5/5 全部就绪
Python 包: ⚠️ 4/5 就绪 (1 项缺失)
硬件驱动:  ⚠️ 1/2 就绪 (1 项缺失)

建议：安装 idf-component-manager 和 CH340 驱动后再开始开发。
```

## 前置条件

无。此命令本身就是用来检查开发环境是否就绪的，无需任何前置环境。

## 注意事项

- ESP-IDF 安装路径不要包含空格和中文字符（Windows 常见问题）
- 每次打开新的终端窗口都需要运行 `export.bat` / `export.sh` 来激活 ESP-IDF 环境
- Windows 建议使用 ESP-IDF 自带的 Python，避免与系统 Python 版本冲突
- Linux 下如果串口被 brltty 抢占，需要卸载该包：`sudo apt remove brltty`
- macOS 下 USB 串口芯片可能需要手动允许 kext 加载（Apple Silicon 安全策略）
