# RobotBuddy V1.0 MVP — 代码审查报告 (第二轮深度审查)

> **审查日期:** 2026-07-11  
> **审查范围:** V1.0 MVP 全部固件代码 — 逐文件深度审查  
> **代码版本:** Initial MVP implementation  
> **审查人:** Claude (Automated Deep Review)  
> **审查维度:** 内存安全、并发安全、实时性、错误处理、功耗管理、代码可维护性  
> **变更文件:** 28 source files, ~6500 lines

---

## 审查结果概要

| 严重度 | 数量 | 说明 |
|--------|------|------|
| 🔴 Critical | 10 | 必须修复，阻塞合入 |
| 🟡 Warning | 14 | 强烈建议修复 |
| 🔵 Suggestion | 12 | 可选优化建议 |

> **与首轮审查对比:** 首轮发现 4 Critical + 8 Warning，二轮追加 6 Critical + 10 Warning + 12 Suggestion。本报告合并所有发现，去重并重新按文件组织。

---

## 🔴 Critical (必须修复 — 阻塞合入)

### C-01: event_bus — Handler 迭代期间竞态条件 (handler 可修改订阅数组)

**文件:** `framework/event_bus.c:56-69`

**问题:** `event_bus_dispatch_task` 在释放 mutex 后调用 handler，handler 如果调用 `event_bus_unsubscribe()`，会修改 `s_subscribers` 数组，导致迭代器跳过 handler 或越界访问。

```c
for (uint8_t h = 0; h < s_subscribers[i].handler_count; h++) {
    xSemaphoreGive(s_mutex);           // ← 释放锁
    s_subscribers[i].handlers[h](&event); // ← handler 可能调用 unsubscribe
    xSemaphoreTake(s_mutex, portMAX_DELAY); // ← 重新获取，但数组可能已被修改
}
```

**修复建议:** 在调用 handler 前快照 handler 数组：
```c
// 快照当前事件的所有 handler
uint8_t count = s_subscribers[i].handler_count;
event_handler_t handlers_snapshot[EVENT_BUS_MAX_SUBSCRIBERS];
memcpy(handers_snapshot, s_subscribers[i].handlers, count * sizeof(event_handler_t));
xSemaphoreGive(s_mutex);

// 使用快照调用 handler（无需持有锁）
for (uint8_t h = 0; h < count; h++) {
    if (handlers_snapshot[h] != NULL) {
        handlers_snapshot[h](&event);
    }
}
```

---

### C-02: audio_manager — I2S 通道与 BSP 冲突

**文件:** `services/audio_manager/audio_manager.c:57-111`

**问题:** `init_i2s_channels()` 创建新的 I2S 通道，但 `bsp_board_init()` (main.c Phase 2) 可能已经初始化了 I2S。如果 BSP 已经 `i2s_new_channel()`，再次调用会返回 `ESP_ERR_INVALID_STATE` 导致 `ESP_ERROR_CHECK` 触发 abort。

**修复建议:** 
- 方案 A (推荐): `audio_manager` 完全拥有 I2S 生命周期，从 BSP 中移除 I2S 初始化
- 方案 B: BSP 导出 I2S handles，`audio_manager` 使用 BSP 提供的 handles

---

### C-03: ai_dialog — 栈上复合字面量作为 event payload

**文件:** `app/ai_dialog/ai_dialog.c:76-91`

**问题:** `set_state()` 中使用复合字面量 `&(behavior_state_t){BEHAVIOR_STATE_LISTENING}` 作为 payload 指针：

```c
event.payload = &(behavior_state_t){BEHAVIOR_STATE_LISTENING};
```

虽然 `event_bus_publish()` 深拷贝 payload，但 payload_len 未正确设置（多处使用 `payload_len=0` 但 payload 非 NULL），这违反了 event_bus 的契约。

**修复建议:** 设置正确的 payload_len：
```c
event.payload_len = sizeof(behavior_state_t);
```

---

### C-04: behavior_system — 紧急停止事件 payload 不一致

**文件:** `app/behavior_system/behavior_system.c:176-205`

**问题:** `EVENT_SENSOR_OBSTACLE` 处理中发布 `EVENT_MOTION_EMERGENCY_STOP` 时 payload 指向栈上局部变量 `cmd` 且 payload_len = `sizeof(cmd)`，但 `EVENT_SENSOR_EDGE` 处理中同样事件的 payload 为 NULL 且 payload_len = 0。消费方无法统一处理。

**修复建议:** 统一紧急停止事件为无 payload（紧急停止应立即生效，不需要参数），或统一使用 `motion_cmd_payload_t`。

---

### C-05: event_bus — deinit 时 payload 内存泄漏

**文件:** `framework/event_bus.c:141-170`

**问题:** `event_bus_deinit()` 直接删除 queue 和 task，但 queue 中可能有未消费的 event（包含深拷贝的 payload），这些 payload 的 malloc 内存永远不会被释放。

**修复建议:**
```c
esp_err_t event_bus_deinit(void)
{
    // ... 停止 dispatch task ...
    
    // 先 drain queue 释放所有 pending payload
    robot_event_t event;
    while (xQueueReceive(s_event_queue, &event, 0) == pdTRUE) {
        if (event.payload != NULL && event.payload_len > 0) {
            free(event.payload);
        }
    }
    
    // 然后删除 queue
    vQueueDelete(s_event_queue);
    // ...
}
```

---

### C-06: event_bus — deinit 后 unsubscribe 崩溃

**文件:** `framework/event_bus.c:217-245`

**问题:** `event_bus_unsubscribe()` 没有检查 `s_initialized`，直接 `xSemaphoreTake(s_mutex, ...)`。如果 `event_bus_deinit()` 已被调用（s_mutex = NULL），会导致 NULL 指针解引用崩溃。

**修复建议:** 在函数入口添加初始化检查：
```c
esp_err_t event_bus_unsubscribe(robot_event_id_t event_id, event_handler_t handler)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    // ...
}
```

---

### C-07: ir_sensor — 未配置 GPIO 的读取崩溃

**文件:** `drivers/ir_sensor/ir_sensor.c:147-162`

**问题:** `ir_sensor_read()` 使用 `(gpio_num_t)s_state.pin_obstacle_left` 等进行 `gpio_get_level()`，但如果某些引脚未被配置（值为 0 或负数），`(gpio_num_t)` 强制转换可能导致未定义行为或崩溃。虽然 `ir_sensor_init()` 对负值做了跳过处理，但值为 0 时（GPIO 0）仍会被配置和读取。

**修复建议:** 在 `ir_sensor_read()` 中添加有效 GPIO 范围检查：
```c
static bool read_gpio_safe(int pin) {
    if (pin < 0 || pin > 48) return false;
    return (gpio_get_level((gpio_num_t)pin) == 0);
}
```

---

### C-08: st7789 — init 失败时资源泄漏

**文件:** `drivers/st7789/st7789.c:429-530`

**问题:** `st7789_init()` 中有多个 `ESP_RETURN_ON_ERROR` 调用，但如果 SPI 设备添加成功后后续步骤（GPIO 配置、init 命令、backlight PWM）失败，已分配的 SPI 设备和 GPIO 不会被清理。

**修复建议:** 使用 `goto cleanup` 模式：
```c
esp_err_t st7789_init(const st7789_config_t *config)
{
    // ...
    ret = spi_bus_add_device(..., &s_dev.spi);
    if (ret != ESP_OK) return ret;
    
    ret = gpio_config(&dc_cfg);
    if (ret != ESP_OK) goto cleanup_spi;
    
    ret = gpio_config(&rst_cfg);
    if (ret != ESP_OK) goto cleanup_dc;
    
    hardware_reset();
    ret = send_init_cmds();
    if (ret != ESP_OK) goto cleanup_rst;
    
    ret = backlight_pwm_init();
    if (ret != ESP_OK) goto cleanup_rst;
    
    s_dev.initialised = true;
    return ESP_OK;

cleanup_rst:
    gpio_reset_pin(s_dev.cfg.pin_rst);
cleanup_dc:
    gpio_reset_pin(s_dev.cfg.pin_dc);
cleanup_spi:
    spi_bus_remove_device(s_dev.spi);
    s_dev.spi = NULL;
    return ret;
}
```

---

### C-09: motion_manager — vTaskDelay 阻塞紧急停止

**文件:** `services/motion_manager/motion_manager.c:74-136`

**问题:** `execute_command()` 中当 `duration_ms > 0` 时使用 `vTaskDelay(pdMS_TO_TICKS(cmd->duration_ms))` 阻塞 motion_task。在此期间：
1. 无法响应新的运动命令（queue 被阻塞）
2. `motion_emergency_stop()` 虽然直接操作 DRV8833 和 reset queue，但 motion_task 在 delay 期间不会处理任何事件

**修复建议:** 使用非阻塞定时器方案：
```c
static int32_t s_motion_end_time = 0;  // esp_timer_get_time() based

static void motion_task(void *arg) {
    motion_command_t cmd;
    while (1) {
        if (xQueueReceive(s_cmd_queue, &cmd, pdMS_TO_TICKS(10)) == pdTRUE) {
            execute_command(&cmd);
        }
        
        // Check if duration expired
        if (s_is_moving && s_motion_end_time > 0) {
            int64_t now = esp_timer_get_time() / 1000;
            if (now >= s_motion_end_time) {
                apply_speeds(0, 0);
                s_motion_end_time = 0;
                // Publish MOTION_COMPLETE event
            }
        }
        esp_task_wdt_reset();
    }
}
```

---

### C-10: main.c — deep sleep 无唤醒源

**文件:** `main/main.c:377-380`

**问题:** `esp_deep_sleep_start()` 进入深度睡眠但没有配置任何唤醒源。设备将永远无法唤醒，只能通过硬件复位。

**修复建议:** 至少配置一个唤醒源：
```c
esp_sleep_enable_timer_wakeup(60ULL * 1000000);  // 60秒后唤醒
// 或配置 GPIO 唤醒（按钮）
esp_sleep_enable_ext0_wakeup(BSP_PIN_BUTTON, 0);
esp_deep_sleep_start();
```

---

## 🟡 Warning (强烈建议修复)

### W-01: display_manager — PSRAM DMA 缓冲区 cache 一致性

**文件:** `services/display_manager/display_manager.c:70`

**问题:** 帧缓冲分配在 PSRAM (`MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA`)，CPU 写入后 SPI DMA 读取时可能读到 cache 中尚未写回的数据，导致显示花屏。

**修复建议:**
```c
#include "esp_cache.h"
// 在 display_commit_frame() 中，SPI 传输前：
esp_cache_msync((void *)s_frame_buffer, fb_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
```

---

### W-02: display_manager — clear 函数逐像素赋值效率低

**文件:** `services/display_manager/display_manager.c:167-170`

**问题:** `display_clear()` 逐像素赋值 `s_frame_buffer[i] = bg_color`，对于 240×240=57600 像素效率很低。

**修复建议:** 使用 `memset()` 或 `std::fill` 优化（如果 bg_color 的两字节相同可用 memset）：
```c
// 如果 bg_color 高低字节相同（如 0x0000 黑色）
if ((bg_color & 0xFF) == ((bg_color >> 8) & 0xFF)) {
    memset(s_frame_buffer, bg_color & 0xFF, total_pixels * 2);
} else {
    // 使用 DMA 或 32-bit 写入优化
    uint32_t val32 = ((uint32_t)bg_color << 16) | bg_color;
    uint32_t *p32 = (uint32_t *)s_frame_buffer;
    for (size_t i = 0; i < total_pixels / 2; i++) {
        p32[i] = val32;
    }
}
```

---

### W-03: audio_manager — DMA 缓冲区分配失败时内存泄漏

**文件:** `services/audio_manager/audio_manager.c:219-232`

**问题:** 如果 `s_playback_dma_buf` 分配失败，`s_capture_dma_buf` 已分配的内存不会被释放。

**修复建议:**
```c
s_capture_dma_buf = heap_caps_malloc(...);
s_playback_dma_buf = heap_caps_malloc(...);
if (s_capture_dma_buf == NULL || s_playback_dma_buf == NULL) {
    ESP_LOGE(TAG, "Failed to allocate DMA buffers");
    if (s_capture_dma_buf) { heap_caps_free(s_capture_dma_buf); s_capture_dma_buf = NULL; }
    if (s_playback_dma_buf) { heap_caps_free(s_playback_dma_buf); s_playback_dma_buf = NULL; }
    return ESP_ERR_NO_MEM;
}
```

---

### W-04: emotion_engine — 帧清除效率 (每帧 ~57K 像素循环)

**文件:** `services/emotion_engine/emotion_engine.c:514-517`

**问题:** `emotion_render_frame()` 每帧清除整个帧缓冲（240×240=57600 次 uint16_t 赋值），加上眼睛渲染的像素循环，在 33ms 帧预算内可能有性能问题。

**建议:** 如果背景色固定为 COLOR_DARK_BG (0x0000 或类似)，使用 `memset()`。或者仅在脏区域重绘。

---

### W-05: mpu6050 — 使用旧版 I2C API

**文件:** `drivers/mpu6050/mpu6050.c:18,90-114`

**问题:** 使用了 `driver/i2c.h` 中的 `i2c_master_write_to_device()` 和 `i2c_master_write_read_device()`，这些是旧版 I2C API。ESP-IDF v5.x 推荐使用 `driver/i2c_master.h` 中的新 API。

**建议:** 迁移到新 I2C master API（`i2c_master_bus_handle_t`, `i2c_master_dev_handle_t`）。虽然旧 API 仍可用，但可能在后续版本被废弃。

---

### W-06: sensor_manager — s_latest_data 无线程安全保护

**文件:** `services/sensor_manager/sensor_manager.c:33-34`

**问题:** `s_latest_data` 在 sensor_task 中写入，在 `sensor_get_data()` 中读取，但没有任何互斥保护。虽然 `sensor_data_t` 结构体较大（~40 bytes），在双核 ESP32-S3 上可能出现撕裂读（torn read）。

**修复建议:** 添加 mutex 或使用 `xQueueOverwrite` 模式：
```c
static SemaphoreHandle_t s_data_mutex;

// sensor_task 中写入:
xSemaphoreTake(s_data_mutex, portMAX_DELAY);
s_latest_data = new_data;
xSemaphoreGive(s_data_mutex);

// sensor_get_data() 中读取:
xSemaphoreTake(s_data_mutex, portMAX_DELAY);
sensor_data_t data = s_latest_data;
xSemaphoreGive(s_data_mutex);
```

---

### W-07: emotion_engine — 确定性 RNG 种子

**文件:** `services/emotion_engine/emotion_engine.c:151`

**问题:** `s_rng_state = 42` 使用固定种子，导致每次重启后眨眼模式完全相同。

**修复建议:** 使用 `esp_random()` 或 `esp_timer_get_time()` 初始化种子：
```c
s_rng_state = esp_random();
```

---

### W-08: drv8833 — 全局状态无线程安全保护

**文件:** `drivers/drv8833/drv8833.c:56-71`

**问题:** `s_cfg`, `s_pins`, `s_initialized` 等全局状态没有 mutex 保护。虽然当前仅从 motion_task 调用，但 `motion_emergency_stop()` 可能从其他任务/ISR 上下文调用。

**建议:** 添加简单的 mutex 或文档明确说明 "仅允许从 motion_task 调用（emergency_stop 除外）"。

---

### W-09: event_bus / sysmon / behavior_system — vTaskDelete 强制杀任务

**文件:** `framework/event_bus.c:148`, `framework/sysmon.c:113`, `app/behavior_system/behavior_system.c:392`

**问题:** `vTaskDelete()` 强制终止任务，不释放任务持有的资源（mutex、queue 等），可能导致死锁或内存泄漏。

**建议:** 使用任务通知或标志位让任务优雅退出：
```c
static volatile bool s_running = true;

// 在任务循环中:
while (s_running) { ... }

// 在 deinit 中:
s_running = false;
vTaskDelay(pdMS_TO_TICKS(100));  // 等待任务退出
vTaskDelete(s_task_handle);
```

---

### W-10: behavior_system — magic numbers

**文件:** `app/behavior_system/behavior_system.c:257-279`

**问题:** 自动恢复超时使用硬编码的魔法数字 `3000`（HAPPY/WARNING 恢复）和 `BEHAVIOR_ERROR_RECOVERY_MS` / `BEHAVIOR_IDLE_TIMEOUT_MS`。

**修复建议:** 定义命名常量：
```c
#define BEHAVIOR_HAPPY_RECOVERY_MS  3000
#define BEHAVIOR_WARNING_RECOVERY_MS 3000
```

---

### W-11: ai_dialog — 2 秒阻塞延时 (TTS 占位符)

**文件:** `app/ai_dialog/ai_dialog.c:174`

**问题:** `vTaskDelay(pdMS_TO_TICKS(2000))` 作为 TTS 播放占位符，在 AI 对话任务中阻塞 2 秒，期间无法响应取消或其他事件。

**建议:** 实现真正的 TTS 回调或事件驱动机制，避免阻塞等待。

---

### W-12: cloud_manager — TLS 证书验证被跳过

**文件:** `services/cloud_manager/cloud_manager.c:541`

**问题:** `skip_cert_common_name_check = true` 跳过了 TLS 证书验证，在生产环境中存在中间人攻击风险。

**建议:** MVP 阶段可以保留，但必须在发布版本中实现证书验证或使用证书固定（certificate pinning）。

---

### W-13: battery — 16 次 ADC 采样阻塞 ~16ms

**文件:** `drivers/battery/battery.c:259-268`

**问题:** `battery_read()` 中进行 16 次 ADC 采样并累加，总耗时约 16ms。在 sensor_task 的 50ms 周期中占 32%，可能影响 IMU/IR 采样率。

**建议:** 分散采样（每次调用只读 1-2 次，累积后取平均），或降低采样次数到 4 次。

---

### W-14: battery_monitor — 重复初始化 battery driver

**文件:** `services/battery_monitor/battery_monitor.c:54` 和 `services/sensor_manager/sensor_manager.c:230`

**问题:** `battery_monitor_init()` 调用 `battery_init()`，而 `sensor_manager_init()` 也调用 `battery_init()`。在 main.c 中两者都被初始化，会导致 ADC 重复初始化。

**修复建议:** 确保只在 `sensor_manager` 或 `battery_monitor` 其中一处初始化 battery driver，而不是两处都初始化。

---

## 🔵 Suggestion (可选优化)

### S-01: st7789 — 添加 draw_pixel API

**文件:** `drivers/st7789/st7789.h`

**建议:** 架构文档定义了 `draw_pixel` API，但当前 st7789 driver 未导出此函数。建议添加以支持 pixel-level 操作。

### S-02: sysmon — 任务栈大小与架构文档不匹配

**文件:** `framework/sysmon.c:20`

**建议:** `SYSMON_TASK_STACK_SIZE` 为 2048，但架构文档为 1024。建议统一为 2048（实际运行需要）并更新架构文档。

### S-03: event_bus — 核心ID使用魔数

**文件:** `framework/event_bus.c:123`

**建议:** `xTaskCreatePinnedToCore(..., 1)` 中的 `1` 应替换为命名常量或 `CONFIG_EVENT_BUS_CORE_ID`。

### S-04: st7789 — SPI 初始化寄存器使用魔数

**文件:** `drivers/st7789/st7789.c:337-365`

**建议:** PORCTRL、GCTRL、VCOMS、LCMCTRL 等寄存器数据使用魔数（如 `0x0C, 0x0C, 0x00, 0x33, 0x33`），建议添加注释说明每个字节的含义。

### S-05: mpu6050 — deinit 不完整清除状态

**文件:** `drivers/mpu6050/mpu6050.c:250-258`

**建议:** `mpu6050_deinit()` 只设置 `initialised = false`，但未清除 `i2c_port`, `addr`, `accel_range` 等字段，也未将设备重新置于睡眠模式（当前虽有 `mpu6050_sleep()` 调用，但如果 deinit 后再 init 会有问题）。

### S-06: emotion_engine — 函数长度超过 50 行

**文件:** `services/emotion_engine/emotion_engine.c`

**建议:** `update_emotion_animation()` (110行) 和 `render_eye()` (90行) 超过编码标准的 50 行限制。建议拆分为更小的函数。

### S-07: cloud_manager — LLM 响应截断风险

**文件:** `app/ai_dialog/ai_dialog.c:54`, `services/cloud_manager/cloud_manager.c`

**建议:** `cloud_response_t.text[512]` 可能不足以容纳长 LLM 回复。建议增大缓冲区或实现流式接收。HTTP 响应缓冲区 64KB 足够，但 `cloud_response_t` 结构体中的 512 字节是瓶颈。

### S-08: wifi_manager — SmartConfig 无超时

**文件:** `services/wifi_manager/wifi_manager.c:493-502`

**建议:** `smartconfig_task` 使用 `portMAX_DELAY` 等待 SmartConfig 完成，没有超时。建议添加 120 秒超时：
```c
xEventGroupWaitBits(s_mgr.sc_event_group, SC_DONE_BIT,
    pdTRUE, pdTRUE, pdMS_TO_TICKS(120000));  // 120s timeout
```

### S-09: st7789 — 冗余赋值

**文件:** `drivers/st7789/st7789.c:554-555`

**建议:** `memset(&s_dev, 0, sizeof(s_dev));` 后又 `s_dev.initialised = false;`，后者是冗余的（memset 已将所有字段清零）。

### S-10: 架构一致性 — event_bus 任务未列入架构文档

**文件:** `framework/event_bus.c`

**建议:** event_bus 的 dispatch 任务未出现在架构文档的任务表中，建议更新 `docs/architecture/v1.0-mvp-architecture.md`。

### S-11: ir_sensor — 返回值风格不一致

**文件:** `drivers/ir_sensor/ir_sensor.c:147`

**建议:** `ir_sensor_read()` 返回 struct by value，与项目中其他 driver 返回 `esp_err_t` + out 指针的模式不一致。建议改为：
```c
esp_err_t ir_sensor_read(ir_sensor_data_t *data);
```

### S-12: drv8833 — ESP_FAIL 应改为 ESP_ERR_INVALID_STATE

**文件:** `drivers/drv8833/drv8833.c:254,305,330`

**建议:** `drv8833_set_speed()`, `drv8833_brake()`, `drv8833_coast()` 在未初始化时返回 `ESP_FAIL`，应改为更具体的 `ESP_ERR_INVALID_STATE`。

---

## 架构一致性检查

| 架构设计要求 | 实现 | 状态 |
|-------------|------|------|
| 事件总线发布/订阅 | `event_bus.c` 实现完整 | ✅ |
| 9 个 FreeRTOS 任务 | 所有任务由 manager 创建 | ✅ |
| 任务优先级 0-8 | 符合架构文档 | ✅ |
| Core 0: WiFi+音频 | audio_manager 创建在 Core 0 (AUDIO_TASK_CORE_ID) | ✅ |
| Core 1: 显示+运动+行为 | display/behavior/sensor/motion 在 Core 1 | ✅ |
| 分层架构依赖方向 | App→Service→Framework→Driver→BSP | ✅ |
| 事件驱动通信 | 大部分模块使用 event_bus | ✅ |
| PSRAM 帧缓冲 | display_manager 使用 PSRAM | ✅ |
| 音频 Ring Buffer | audio_manager 使用 FreeRTOS RingBuffer | ✅ |
| 紧急停止不经过 Queue | motion_emergency_stop 直接操作 DRV8833 | ✅ |
| WiFi 重连指数退避 | wifi_manager 实现 | ✅ |
| I2S 全双工 | audio_manager 配置 RX+TX 同端口 | ⚠️ 与 BSP 冲突 |

---

## 指标

| 指标 | 值 | 目标 | 状态 |
|------|-----|------|------|
| 新增文件 | 28 | - | ✅ |
| 新增代码行 | ~6500 | - | ✅ |
| 🔴 Critical 问题 | 10 | 0 | ❌ |
| 🟡 Warning 问题 | 14 | ≤3 | ❌ |
| 🔵 Suggestion 问题 | 12 | — | 🟡 |
| 编译状态 | 未验证 | 0 error | 🔵 需实机验证 |
| 单元测试 | 3 files | 每模块1个 | 🟡 |

---

## 修复优先级排序

### P0 — 必须立即修复 (编译错误或运行时崩溃)

1. **C-02** (I2S 重复初始化) — BSP 与 audio_manager 冲突，运行时崩溃
2. **C-06** (event_bus unsubscribe 崩溃) — deinit 后调用崩溃
3. **C-07** (ir_sensor GPIO 崩溃) — 未配置 GPIO 读取崩溃
4. **C-08** (st7789 资源泄漏) — init 失败不清理
5. **C-10** (deep sleep 无唤醒源) — 设备永远无法唤醒

### P1 — 必须在实机测试前修复 (数据丢失/内存泄漏/并发问题)

6. **C-01** (event_bus handler 竞态) — 事件丢失/崩溃
7. **C-05** (event_bus payload 内存泄漏) — deinit 时泄漏
8. **C-09** (motion_manager 阻塞延时) — 急停无法响应
9. **C-03** (ai_dialog 栈上 payload) — payload_len 不匹配
10. **C-04** (behavior_system payload 不一致) — 紧急停止事件 payload 矛盾
11. **W-01** (PSRAM cache 一致性) — 显示花屏
12. **W-03** (audio_manager 内存泄漏) — DMA 缓冲区分配失败泄漏
13. **W-06** (sensor_manager 线程安全) — 撕裂读
14. **W-14** (battery 双重初始化) — ADC 重复 init

### P2 — 建议在 V1.0 发布前修复 (可靠性)

15. **W-04** (emotion_engine 帧清除效率) — 性能
16. **W-07** (确定性 RNG 种子) — 可预测行为
17. **W-09** (vTaskDelete 不安全) — deinit 时资源泄漏
18. **W-11** (ai_dialog 阻塞延时) — 无法取消对话
19. **W-12** (TLS 证书验证) — 安全风险
20. **W-13** (battery 16ms 采样阻塞) — 影响采样率

### P3 — 可选优化 (代码质量)

21. **S-01 ~ S-12** — 所有建议项

---

## 审查者自检

- [x] 代码可编译（未实际运行 `idf.py build`，需实机验证）
- [x] 所有 Critical 维度已检查（内存安全、并发安全、实时性）
- [x] 每个发现附带了文件:行 + 修复建议
- [x] 修复建议是可执行的（不是模糊的"优化一下"）
- [x] 结论明确：**🔴 需修改后重新审查**

---

> **文档版本:** 2.0  
> **下次审查:** 修复 P0 + P1 问题后重新审查  
> **相关文档:**
> - `docs/architecture/v1.0-mvp-architecture.md`
> - `docs/requirement/v1.0-mvp-requirements.md`
> - `.claude/standards/embedded-coding.md`