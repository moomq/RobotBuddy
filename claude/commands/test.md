# /test — RobotBuddy 一键测试

## 用途

一键运行 RobotBuddy ESP32-S3 固件各级测试套件，从单元测试到硬件在环压力测试。

## 适用场景

- PR 提交前全量回归测试
- CI/CD 流水线集成测试
- 新模块开发后的单元测试
- 硬件变更后的 HIL 验证
- 发布前的压力测试
- 指定模块的定向测试

## 工作流

```
第 1 阶段：测试范围确认
├── 1.1 全部测试 — 运行 L1→L4 全量套件
├── 1.2 单元测试 — 仅 Unity + CMock 单元测试（L1）
├── 1.3 HIL 测试  — 真实 ESP32-S3 硬件在环测试（L2）
├── 1.4 集成测试 — 多模块联动测试（L3）
├── 1.5 压力测试 — 长时间运行 + 边界条件（L4）
└── 1.6 指定模块 — 仅测试指定模块（如 audio / motion / display）

第 2 阶段：环境准备
├── 2.1 确认 ESP-IDF 环境已激活（idf.py --version）
├── 2.2 确认测试框架依赖已安装（Unity + CMock + ctest）
├── 2.3 编译固件（idf.py build）
├── 2.4 HIL 测试时，确认 ESP32-S3 已连接并烧录
│   ├── 检查 USB 串口: ls /dev/ttyUSB* (Linux) / COM端口 (Windows)
│   ├── 烧录固件: idf.py flash
│   └── 确认启动日志: idf.py monitor (启动无 Panic)
└── 2.5 输出：环境就绪确认

第 3 阶段：执行测试
├── 3.1 L1 单元测试（Unity + CMock）
│   ├── cd build && ctest --output-on-failure
│   ├── 生成覆盖率: lcov --capture --directory . --output-file coverage.info
│   ├── 各模块独立测试:
│   │   ├── motion_control  — PID 算法、运动学逆解
│   │   ├── audio_pipeline  — I2S 配置、音频编解码
│   │   ├── display_engine  — LVGL 控件、帧缓冲
│   │   ├── behavior_system — 状态机转换、事件路由
│   │   ├── cloud_com       — MQTT 消息序列化、TLS 握手
│   │   ├── sensor_fusion   — 传感器数据融合算法
│   │   └── wifi_manager    — WiFi 连接/重连状态机
│   └── 输出：单元测试结果 + 覆盖率数据
│
├── 3.2 L2 HIL 测试（真实 ESP32-S3）
│   ├── 烧录测试固件: idf.py flash
│   ├── 运行 HIL 脚本:
│   │   ├── tests/hil/test_motion_hil.py    — 电机驱动 HIL
│   │   ├── tests/hil/test_audio_hil.py     — 音频输入/输出 HIL
│   │   ├── tests/hil/test_display_hil.py   — 屏幕刷新 HIL
│   │   ├── tests/hil/test_sensor_hil.py    — IMU/ToF 传感器 HIL
│   │   └── tests/hil/test_wifi_hil.py      — WiFi 连接稳定性 HIL
│   └── 输出：HIL 测试结果 + 硬件状态日志
│
├── 3.3 L3 集成测试
│   ├── 多任务通信测试: 事件总线消息传递验证
│   ├── 任务间同步测试: Queue / Semaphore / Event Group
│   ├── 启动顺序测试: 各模块初始化顺序及依赖验证
│   ├── OTA 升级流程测试: 下载→校验→切换→回滚
│   └── 输出：集成测试结果
│
└── 3.4 L4 压力测试
    ├── 长时间运行: ≥24h 连续运行监测
    │   ├── 定期记录: heap_caps_get_free_size / uxTaskGetStackHighWaterMark
    │   ├── 内存泄漏检测: 每小时快照对比
    │   └── WiFi 断连恢复: 模拟断网 + 重连循环
    ├── 边界条件:
    │   ├── PSRAM 接近满载时行为验证
    │   ├── 所有任务同时激活时的调度验证
    │   └── 极限温度下运行验证（如适用）
    └── 输出：压力测试报告 + 资源趋势图

第 4 阶段：结果分析
├── 4.1 生成覆盖率报告
│   ├── 行覆盖率: 目标 ≥80%
│   ├── 分支覆盖率: 目标 ≥70%
│   └── 生成 HTML: genhtml coverage.info --output-directory coverage_report
├── 4.2 失败用例分析
│   ├── 提取失败用例列表: ctest -N --output-on-failure
│   ├── 每个失败用例: 错误日志 + 堆栈回溯
│   ├── 硬件状态快照（HIL 失败时）
│   └── 分类: 代码缺陷 / 环境问题 / 硬件故障 / 时序问题
├── 4.3 性能指标汇总
│   ├── 测试通过率: PASS / TOTAL
│   ├── 代码覆盖率: 行覆盖 + 分支覆盖
│   ├── 内存安全: 栈余量 + 堆碎片率
│   └── 实时性: 任务周期抖动 + ISR 延迟
└── 4.4 输出：完整测试报告
```

## 测试级别

| 级别 | 名称 | 运行环境 | 执行时间 | 说明 |
|------|------|---------|---------|------|
| L1 | 单元测试 | Host (PC) | < 1min | Unity + CMock，纯软件模拟，无硬件依赖 |
| L2 | HIL 测试 | ESP32-S3 真机 | 5-15min | 硬件在环，真实外设驱动验证 |
| L3 | 集成测试 | ESP32-S3 真机 | 15-30min | 多模块联动，FreeRTOS 任务通信验证 |
| L4 | 压力测试 | ESP32-S3 真机 | ≥24h | 长时间运行，内存泄漏/稳定性检测 |

## 快捷命令

```bash
# === L1 单元测试 ===
# 编译并运行所有单元测试
idf.py build
cd build && ctest --output-on-failure -j4

# 运行指定模块单元测试
ctest -R motion -V                    # 仅运动控制模块
ctest -R audio -V                     # 仅音频模块
ctest -R display -V                   # 仅显示模块

# 生成覆盖率报告
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/test/*' --output-file coverage_filtered.info
genhtml coverage_filtered.info --output-directory coverage_report

# === L2 HIL 测试 ===
# 烧录并运行 HIL 测试
idf.py flash monitor
python tests/hil/test_motion_hil.py   # 运动 HIL
python tests/hil/test_audio_hil.py    # 音频 HIL
python tests/hil/test_display_hil.py  # 显示 HIL
python tests/hil/test_sensor_hil.py   # 传感器 HIL
python tests/hil/test_wifi_hil.py     # WiFi HIL

# 运行全部 HIL 测试
python tests/hil/run_all_hil.py

# === L3 集成测试 ===
# 运行集成测试套件
python tests/integration/run_integration.py

# === L4 压力测试 ===
# 启动 24 小时压力测试
python tests/stress/run_stress_24h.py --log-dir stress_logs/

# 内存监测脚本（配合压力测试）
python tools/monitor_heap.py --interval 60 --duration 86400
```

## 前置条件

- ESP-IDF v5.x 环境已安装并激活（`idf.py --version` 可正常输出）
- 测试框架已配置：
  - Unity 测试框架已集成到项目 CMakeLists.txt
  - CMock 模拟框架已配置（用于 L1 单元测试 mock 硬件接口）
  - Python 3.8+ 及 `pyserial` 库（用于 HIL 测试脚本）
- L2/L3/L4 测试需 ESP32-S3 通过 USB 连接且串口可正常通信
- HIL 测试需外设完整连接（电机、屏幕、麦克风、扬声器、IMU 等）

## 输出格式

```markdown
## 测试报告 — RobotBuddy

**日期**: YYYY-MM-DD HH:MM
**固件版本**: vX.Y.Z
**测试范围**: 全部 / L1 / L2 / L3 / L4 / 指定模块
**执行时间**: XX min

---

### 测试结果总览

| 级别 | 通过 | 失败 | 跳过 | 通过率 |
|------|------|------|------|--------|
| L1 单元测试 | N | N | N | XX% |
| L2 HIL 测试 | N | N | N | XX% |
| L3 集成测试 | N | N | N | XX% |
| L4 压力测试 | N | N | N | XX% |
| **合计** | **N** | **N** | **N** | **XX%** |

### 覆盖率

| 指标 | 当前值 | 目标 | 状态 |
|------|--------|------|------|
| 行覆盖率 | XX% | ≥80% | ✅ / ❌ |
| 分支覆盖率 | XX% | ≥70% | ✅ / ❌ |
| 函数覆盖率 | XX% | ≥90% | ✅ / ❌ |

### 失败用例详情

#### ❌ test_motion_pid_tuning (L1)
- **文件**: `tests/unit/test_motion_control.c:234`
- **错误**: `Expected 0.95f was 0.87f`
- **根因**: PID 参数调整后期望值未更新
- **建议**: 更新测试期望值或回滚 PID 参数

#### ❌ test_audio_i2s_output (L2)
- **文件**: `tests/hil/test_audio_hil.py:89`
- **错误**: `I2S buffer underrun detected`
- **根因**: DMA buffer 不足，消费速率低于生产速率
- **建议**: 增大 DMA buffer 至 8192，检查音频任务优先级

### 资源监控

| 指标 | 最小值 | 最大值 | 平均值 | 状态 |
|------|--------|--------|--------|------|
| 栈余量 (audio) | 1024B | 2048B | 1536B | ✅ |
| 栈余量 (display) | 512B | 1024B | 768B | ✅ |
| 堆空闲 (DRAM) | 256KB | 320KB | 288KB | ✅ |
| 堆空闲 (PSRAM) | 4.2MB | 4.8MB | 4.5MB | ✅ |
| ISR 最大延迟 | — | 5.2μs | 2.1μs | ✅ |

### 结论

- ✅ / ❌ 通过质量门禁
- 待修复: N 项
- 建议: ...
```

## 注意事项

- L1 单元测试在 PC 端运行，无需 ESP32-S3 硬件；但需注意 CMock 模拟的硬件行为与真实硬件可能不一致
- L2 HIL 测试前务必确认硬件连接完整，避免因接线问题导致误报
- L3 集成测试关注任务间通信，务必启用 `CONFIG_FREERTOS_USE_TRACE_FACILITY=y` 以获取任务运行状态
- L4 压力测试建议在夜间或 CI 专用硬件上运行，避免占用开发板
- 覆盖率报告中的 `test/` 目录和第三方库代码建议排除，聚焦业务代码覆盖率
- 测试失败时优先检查环境（串口连接、固件版本、硬件接线），再排查代码问题
- HIL 测试脚本依赖串口通信，Windows 下需确认 COM 端口号并修改脚本配置
- 如遇 HIL 测试不稳定（间歇性超时），检查 USB 线缆质量和供电稳定性
