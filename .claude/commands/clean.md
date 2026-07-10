# /clean — RobotBuddy 清理命令

## 用途

清理 RobotBuddy 项目的构建产物、临时文件、缓存数据，重置开发环境至干净状态。

## 适用场景

- 切换 ESP-IDF 版本后，旧构建缓存导致编译异常
- 固件出现未知异常，排除构建缓存污染
- 发布前确保从干净环境构建
- OTA 分区表变更后需完全重建
- 清理磁盘空间（build/ 目录可达数百 MB）
- 重置开发板至出厂状态

## 工作流

```
第 1 层：轻度清理 (idf.py clean)
├── 1.1 删除 build/ 目录中的编译产物（.o, .a, .elf）
├── 1.2 保留 sdkconfig（配置不变，下次编译更快）
├── 1.3 清理 CMake 生成文件
├── 1.4 适用: 常规重新编译、切换分支后
└── 1.5 命令: idf.py clean

第 2 层：深度清理 (idf.py fullclean)
├── 2.1 删除整个 build/ 目录（含 sdkconfig）
├── 2.2 删除 CMake 缓存
├── 2.3 删除组件依赖关系图
├── 2.4 下次编译需重新运行 CMake 配置（较慢）
├── 2.5 适用: ESP-IDF 版本升级后、menuconfig 配置异常
└── 2.6 命令: idf.py fullclean

第 3 层：完全重置
├── 3.1 执行 idf.py fullclean → 清理所有构建产物
├── 3.2 擦除 ESP32-S3 Flash
│   ├── 命令: idf.py erase-flash
│   ├── 或: esptool.py --chip esp32s3 erase_flash
│   └── 注意: 擦除所有分区数据（NVS / OTA / SPIFFS）
├── 3.3 清理项目临时文件
│   ├── 删除: *.swp, *.swo, *~ (vim 临时文件)
│   ├── 删除: .vscode/.browse.VC.db (VS Code IntelliSense 缓存)
│   ├── 删除: dependencies.lock (ESP-IDF 依赖锁文件)
│   ├── 删除: managed_components/ (ESP-IDF 组件缓存)
│   ├── 删除: __pycache__/ 及 *.pyc (Python 缓存)
│   ├── 删除: coverage_report/ (测试覆盖率报告)
│   ├── 删除: stress_logs/ (压力测试日志)
│   ├── 删除: coredump/ (Core Dump 文件)
│   └── 删除: sdkconfig.old (旧的 menuconfig 备份)
├── 3.4 校准数据重置（需确认）
│   ├── 擦除 NVS 分区中的校准参数:
│   │   ├── IMU 零偏校准数据
│   │   ├── 电机零点校准数据
│   │   ├── 触摸屏校准矩阵
│   │   └── 电池电量校准曲线
│   ├── 命令: idf.py erase-flash (全擦除)
│   ├── 或指定分区: parttool.py erase_partition --partition-name=nvs
│   └── ⚠️ 警告: 校准数据重置不可逆，需重新执行校准流程
└── 3.5 输出：清理完成报告
```

## 清理范围选项

| 层级 | 命令 | 清理内容 | 编译影响 | 适用时机 |
|------|------|---------|---------|---------|
| 轻度 | `idf.py clean` | 编译产物 (.o/.a/.elf) | 保留 sdkconfig，增量编译 | 日常开发 |
| 深度 | `idf.py fullclean` | 全部 build/ 目录 | 需重新 CMake 配置 | 版本切换 |
| Flash 擦除 | `idf.py erase-flash` | 整个 Flash | 需重新烧录 | 固件异常恢复 |
| 校准重置 | `parttool.py erase_partition --partition-name=nvs` | NVS 分区 | 需重新校准 | 校准数据异常 |
| 临时文件 | 手动删除 | 编辑器/工具缓存 | 无影响 | 发布前清洁 |
| 完全重置 | 以上全部 | 所有数据 | 需完整重建 | 环境迁移/售出 |

## 常用命令

```bash
# === 轻度清理 ===
# 清理编译产物，保留 sdkconfig
idf.py clean

# 清理并重新编译
idf.py clean build

# 清理并烧录
idf.py clean build flash

# === 深度清理 ===
# 完全清理 build 目录
idf.py fullclean

# 深度清理后从 menuconfig 重新配置
idf.py fullclean
idf.py set-target esp32s3
idf.py menuconfig
idf.py build

# === Flash 擦除 ===
# 擦除整个 Flash（需连接 ESP32-S3）
idf.py erase-flash

# 使用 esptool 直接擦除（指定端口）
esptool.py --chip esp32s3 --port COM3 erase_flash
# Linux: esptool.py --chip esp32s3 --port /dev/ttyUSB0 erase_flash

# 擦除后重新烧录全套固件
idf.py erase-flash
idf.py flash monitor

# === 指定分区擦除 ===
# 仅擦除 NVS 分区（保留固件和 SPIFFS）
parttool.py erase_partition --partition-name=nvs

# 擦除 OTA 数据分区（强制回退到 factory）
parttool.py erase_partition --partition-name=otadata

# 擦除 SPIFFS 分区（清除表情/音频资源）
parttool.py erase_partition --partition-name=spiffs

# === 清理项目临时文件 ===
# Windows (PowerShell)
Get-ChildItem -Recurse -Include *.pyc,__pycache__ | Remove-Item -Recurse -Force
Get-ChildItem -Recurse -Include *.swp,*.swo,*~ | Remove-Item -Force
Remove-Item -Recurse -Force coverage_report,stress_logs,coredump -ErrorAction SilentlyContinue
Remove-Item -Force dependencies.lock,sdkconfig.old -ErrorAction SilentlyContinue

# Linux / macOS
find . -name "*.pyc" -delete
find . -name "__pycache__" -type d -exec rm -rf {} +
find . -name "*.swp" -o -name "*.swo" -o -name "*~" -delete
rm -rf coverage_report stress_logs coredump
rm -f dependencies.lock sdkconfig.old
```

## 完全重置流程（发布前 / 环境迁移）

```bash
# === 步骤 1: 确认 ESP32-S3 已连接 ===
# Windows: 设备管理器 → 端口 (COM & LPT) → 确认 COM 端口号
# Linux: ls /dev/ttyUSB* 或 ls /dev/ttyACM*

# === 步骤 2: 深度清理 ===
idf.py fullclean

# === 步骤 3: 擦除 Flash ===
idf.py erase-flash

# === 步骤 4: 清理临时文件 ===
# Windows PowerShell
Get-ChildItem -Recurse -Include *.pyc,__pycache__ | Remove-Item -Recurse -Force
Remove-Item -Recurse -Force build,coverage_report,stress_logs,coredump,managed_components -ErrorAction SilentlyContinue
Remove-Item -Force dependencies.lock,sdkconfig.old -ErrorAction SilentlyContinue

# === 步骤 5: 从 menuconfig 重新配置 ===
idf.py set-target esp32s3
idf.py menuconfig

# === 步骤 6: 重新编译并烧录 ===
idf.py build
idf.py flash

# === 步骤 7: 烧录 SPIFFS 资源 ===
# 表情动画、音频文件等
idf.py storage-flash

# === 步骤 8: 执行校准流程 ===
# IMU 校准 → 电机校准 → 触摸屏校准（按项目实际校准流程）
```

## 注意事项

- **校准数据重置不可逆**: 擦除 NVS 分区后，IMU 零偏、电机零点、触摸校准矩阵等数据永久丢失，需重新执行完整的硬件校准流程
- **erase-flash 会擦除整个 Flash**: 包括 factory 分区，擦除后必须通过 USB-UART 重新烧录，无法通过 OTA 恢复
- **fullclean 后首次编译较慢**: CMake 需重新解析所有组件依赖，预计耗时 2-5 分钟（取决于 PC 性能）
- **清理前确认无未保存工作**: `fullclean` 会删除 `sdkconfig`，如有自定义 `menuconfig` 配置请先备份: `cp sdkconfig sdkconfig.backup`
- **分区表变更需 fullclean**: 如修改了 `partitions.csv`，必须 `idf.py fullclean` 后重新编译，否则分区偏移可能错误
- **发布前必须 fullclean**: 确保发布固件是从干净环境构建，避免旧缓存污染导致难以排查的问题
- **erase-flash 后 SPIFFS 资源丢失**: 表情动画文件和音频资源需通过 `idf.py storage-flash` 重新烧录
- **长时间使用后建议完全重置**: ESP32-S3 Flash 有擦写寿命（约 10 万次），但 NVS 频繁写入会产生碎片；开发板长期使用后建议完全重置以获得干净状态
