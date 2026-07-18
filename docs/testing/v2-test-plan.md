# RobotBuddy V2.0 测试计划

> **版本:** 2.0
> **日期:** 2026-07-18
> **状态:** 测试进行中

---

## 1. 测试级别

### L1 单元测试

| 模块 | 测试文件 | 状态 | 覆盖率目标 |
|------|---------|------|-----------|
| event_bus | test_event_bus.c | ✅ 已有 | 80% |
| emotion_engine | test_emotion_engine.c | ✅ 已有 | 80% |
| behavior_system | test_behavior_system.c | ✅ 已有 | 80% |
| text_display | test_v2_modules.c | ✅ 新增 | 70% |
| pomodoro | test_v2_modules.c | ✅ 新增 | 70% |
| power_manager | test_v2_modules.c | ✅ 新增 | 70% |
| wake_word | test_v2_modules.c | ✅ 新增 | 60% |
| mqtt_client | — | ⚠️ 需要模拟网络 | 50% |
| ota_service | — | ⚠️ 需要模拟 HTTP | 50% |
| web_server | — | ⚠️ 需要模拟 HTTP | 50% |

### L2 硬件在环测试 (HIL)

| 测试项 | 描述 | 状态 |
|--------|------|------|
| ST7789 显示 | 点亮屏幕，渲染表情 | ⏳ 待硬件 |
| I2S 音频采集 | 麦克风输入测试 | ⏳ 待硬件 |
| I2S 音频播放 | 扬声器输出测试 | ⏳ 待硬件 |
| DRV8833 电机 | 正转/反转/停止 | ⏳ 待硬件 |
| MPU6050 IMU | 加速度/陀螺仪读取 | ⏳ 待硬件 |
| VL53L0X TOF | 距离测量 | ⏳ 待硬件 |
| 触摸传感器 | 单击/双击/长按检测 | ⏳ 待硬件 |
| 红外避障 | 障碍物检测 | ⏳ 待硬件 |
| 红外防跌落 | 桌面边缘检测 | ⏳ 待硬件 |
| 电池电压 | ADC 读取准确度 | ⏳ 待硬件 |

### L3 集成测试

| 测试场景 | 描述 | 状态 |
|---------|------|------|
| 唤醒词 → AI 对话 | 完整语音交互流程 | ⏳ 待硬件 |
| MQTT 编译结果推送 | VS Code → 机器人表情联动 | ⏳ 待集成 |
| Web 控制台配置 | WiFi/API Key 配置保存 | ⏳ 待硬件 |
| OTA 升级 | HTTP 固件下载和安装 | ⏳ 待硬件 |
| 番茄钟完整周期 | 25分钟工作 + 5分钟休息 | ⏳ 待硬件 |
| 电源管理自动休眠 | 空闲5分钟 → 降亮度 → 休眠 | ⏳ 待硬件 |

### L4 验收测试

| 验收条件 | PRD 要求 | 测试方法 | 状态 |
|---------|---------|---------|------|
| 开机时间 | < 3 秒 | 秒表计时 | ⏳ |
| 语音响应延迟 | < 1.5 秒 | 示波器/音频分析 | ⏳ |
| 表情刷新率 | ≥ 30 FPS | 帧计数器 | ⏳ |
| WiFi 重连 | < 3 秒 | 断开路由器测试 | ⏳ |
| 续航（活跃） | ≥ 4 小时 | 满电连续运行 | ⏳ |
| 续航（待机） | ≥ 24 小时 | 待机计时 | ⏳ |

---

## 2. 新模块测试用例

### 2.1 text_display 测试用例

| # | 用例 | 预期结果 |
|---|------|---------|
| TD-01 | 初始化模块 | ESP_OK |
| TD-02 | 显示短消息 "Hello" | 消息加入队列，开始滚动 |
| TD-03 | 显示高优先级消息 | 推到队列前面，立即显示 |
| TD-04 | 消息队列溢出 | 最老消息被丢弃 |
| TD-05 | 超时自动清除 | 10秒后消息消失 |
| TD-06 | 收到编译成功事件 | 显示 "Build Success"，HAPPY 图标 |
| TD-07 | 收到编译失败事件 | 显示错误消息，ERROR 图标 |
| TD-08 | 渲染空队列 | 不崩溃，背景透明 |
| TD-09 | NULL 帧缓冲渲染 | 不崩溃，返回错误 |

### 2.2 pomodoro 测试用例

| # | 用例 | 预期结果 |
|---|------|---------|
| PO-01 | 初始化模块 | ESP_OK, 状态=IDLE |
| PO-02 | 开始番茄钟 | 状态=WORKING, 发布 START 事件 |
| PO-03 | 暂停番茄钟 | 状态=PAUSED, 计时暂停 |
| PO-04 | 恢复番茄钟 | 状态恢复到 WORKING |
| PO-05 | 停止番茄钟 | 状态=IDLE |
| PO-06 | 每秒 TICK 事件 | 发布 POMODORO_TICK |
| PO-07 | 工作时间结束 | 发布 POMODORO_DONE, 进入 BREAK |
| PO-08 | 休息时间结束 | 进入下一轮 WORKING 或 IDLE |
| PO-09 | 达到最大轮数 | 回到 IDLE |

### 2.3 power_manager 测试用例

| # | 用例 | 预期结果 |
|---|------|---------|
| PM-01 | 初始化模块 | ESP_OK, 状态=ACTIVE |
| PM-02 | 通知活动 | 空闲时间重置为 0 |
| PM-03 | 空闲 5 分钟 | 状态=DISPLAY_DIM, 背光降低 |
| PM-04 | 空闲 10 分钟 | 状态=WIFI_LIGHT_SLEEP |
| PM-05 | 空闲 30 分钟 | 状态=DEEP_SLEEP, 进入休眠 |
| PM-06 | 触摸唤醒 | 从休眠恢复到 ACTIVE |
| PM-07 | 语音唤醒 | 从休眠恢复到 ACTIVE |
| PM-08 | 手动设置状态 | 状态切换正确 |
| PM-09 | 低电量事件 | 立即进入 DEEP_SLEEP |

### 2.4 wake_word 测试用例

| # | 用例 | 预期结果 |
|---|------|---------|
| WW-01 | 初始化模块 | ESP_OK |
| WW-02 | 开始监听 | is_listening() = true |
| WW-03 | 停止监听 | is_listening() = false |
| WW-04 | TTS 播放时暂停 | 自动停止监听 |
| WW-05 | TTS 结束后恢复 | 自动恢复监听 |
| WW-06 | 检测到唤醒词 | 发布 WAKE_WORD_DETECTED 事件 |
| WW-07 | 无 ESP-SR 时使用 VAD | 能量检测作为备选 |

### 2.5 mqtt_client 测试用例

| # | 用例 | 预期结果 |
|---|------|---------|
| MQ-01 | 初始化模块 | ESP_OK |
| MQ-02 | 连接 Broker | 发布 MQTT_CONNECTED |
| MQ-03 | 断开连接 | 发布 MQTT_DISCONNECTED |
| MQ-04 | 收到 build 命令 | 发布 BUILD_STATUS 事件 |
| MQ-05 | 收到 git 命令 | 发布 GIT_STATUS 事件 |
| MQ-06 | 收到 text 命令 | 发布 DISPLAY_TEXT_MSG 事件 |
| MQ-07 | 收到 emotion 命令 | 发布 EMOTION_STATE_CHANGE 事件 |
| MQ-08 | 收到 motion 命令 | 发布 MOTION_COMMAND 事件 |
| MQ-09 | 收到 pomodoro 命令 | 发布 POMODORO_START 事件 |
| MQ-10 | 收到 ota 命令 | 发布 OTA_START 事件 |
| MQ-11 | 发布状态 | 定时发送 battery/wifi/emotion |

### 2.6 ota_service 测试用例

| # | 用例 | 预期结果 |
|---|------|---------|
| OT-01 | 初始化模块 | ESP_OK, 状态=IDLE |
| OT-02 | 开始 OTA | 状态=DOWNLOADING |
| OT-03 | 下载进度 | 发布 OTA_PROGRESS 事件 |
| OT-04 | 下载完成 | 状态=VERIFYING |
| OT-05 | SHA256 校验通过 | 状态=APPLYING |
| OT-06 | 应用固件 | 状态=REBOOTING |
| OT-07 | 重启后验证 | 标记固件有效 |
| OT-08 | 下载失败 | 状态=ERROR, 发布 OTA_ERROR |
| OT-09 | SHA256 校验失败 | 状态=ERROR |
| OT-10 | 取消 OTA | 状态回到 IDLE |

### 2.7 web_server 测试用例

| # | 用例 | 预期结果 |
|---|------|---------|
| WS-01 | 初始化模块 | ESP_OK |
| WS-02 | 启动服务器 | HTTP 200 on / |
| WS-03 | GET / | 返回 HTML 仪表盘 |
| WS-04 | GET /api/status | 返回 JSON 状态 |
| WS-05 | POST /api/wifi | 保存 WiFi 配置 |
| WS-06 | POST /api/ai-key | 保存 API Key |
| WS-07 | POST /api/ota | 触发 OTA 升级 |
| WS-08 | GET /api/ota-progress | 返回 OTA 进度 |
| WS-09 | POST /api/emotion | 切换表情 |
| WS-10 | POST /api/pomodoro | 控制番茄钟 |
| WS-11 | 停止服务器 | 释放资源 |

---

## 3. V1.0 问题修复验证

| # | 问题 | 修复位置 | 验证方法 |
|---|------|---------|---------|
| 1 | I2C 句柄泄漏 | bsp_board.c | 反复初始化/反初始化，检查内存 |
| 2 | I2S 死代码 | bsp_board.c | 编译通过，无未使用函数 |
| 3 | TTS 占位延迟 | ai_dialog.c, cloud_manager.c | 实际 TTS 流式播放 |
| 4 | MPU6050 旧 I2C API | mpu6050.c | 使用 driver/i2c_master.h |
| 5 | TLS 未验证 | cloud_manager.c | 证书校验开启 |

---

## 4. 测试环境

### 4.1 编译测试

```bash
cd firmware
idf.py build
```

### 4.2 单元测试

```bash
cd firmware
idf.py -p COMX flash monitor
# 在串口控制台运行 Unity 测试
```

### 4.3 覆盖率分析

```bash
# 需要 gcov 支持
idf.py gcov
```

---

## 5. 测试结果摘要

> 待实际硬件测试后填写

| 阶段 | 通过 | 失败 | 阻塞 |
|------|-----|------|------|
| L1 单元测试 | — | — | — |
| L2 HIL 测试 | — | — | ⏳ 待硬件 |
| L3 集成测试 | — | — | ⏳ 待硬件 |
| L4 验收测试 | — | — | ⏳ 待硬件 |

---

## 6. 风险与阻塞

| 风险 | 影响 | 缓解方案 |
|------|------|---------|
| 无硬件设备 | 无法进行 HIL/集成/验收测试 | 使用 PC 模拟器进行部分验证 |
| MQTT Broker 不可达 | 云端功能无法测试 | 使用本地 Mosquitto |
| ESP-SR 组件未安装 | 唤醒词使用 VAD 备选 | 提供安装指南 |
| TLS 证书缺失 | 云端通信风险 | 生成自签名证书用于测试 |

---

> **下一步：** 等待硬件进行 HIL 测试，或使用 PC 模拟器验证部分功能
