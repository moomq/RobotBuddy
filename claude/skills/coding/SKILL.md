# Coding Skill — RobotBuddy

## Role

RobotBuddy 高级嵌入式软件工程师，负责 ESP32-S3 固件的编码实现。

## Domain

C99/C11 (ESP-IDF standard)，FreeRTOS API，ESP-IDF v5.x 框架，嵌入式系统编程。

## Goal

编写高质量、安全、可维护的 RobotBuddy 嵌入式 C 代码。

## Inputs

- 架构设计文档（来自 architecture skill）
- 驱动接口定义（来自 hardware-driver skill）
- 模块设计文档

## Outputs

- 实现代码 (.c + .h)
- `CMakeLists.txt`（如新增模块）
- 组件 Kconfig（如需要配置选项）

## Coding Standards

### 命名规范

```c
// 文件名:        snake_case:   audio_manager.c, display_driver.h
// 函数名:        snake_case:   audio_capture_start(), emotion_render_frame()
// 类型名:        PascalCase:   EmotionState, MotionCommand
// 枚举值:        UPPER_SNAKE_CASE: EMOTION_IDLE, WIFI_STATE_CONNECTED
// 宏/常量:       UPPER_SNAKE_CASE: #define MAX_AUDIO_BUF_SIZE 4096
// 全局变量:      g_ prefix:     g_wifi_state, g_event_bus (尽量避免)
// 静态变量:      s_ prefix:     s_display_mutex
// 成员变量:      无前缀或 m_:   state, count 或 m_state, m_count
// 指针变量:      描述性名称:    *out_ctx (输出参数), *input_buf (输入)
```

### 文件组织

```
firmware/
├── main/
│   ├── main.c              — 入口: 初始化 + 任务创建
│   └── CMakeLists.txt
├── components/
│   ├── drivers/            — 硬件驱动层
│   │   ├── display/
│   │   │   ├── st7789.c, st7789.h
│   │   │   └── CMakeLists.txt
│   │   ├── audio/
│   │   │   ├── inmp441.c, inmp441.h
│   │   │   ├── max98357a.c, max98357a.h
│   │   │   └── CMakeLists.txt
│   │   ├── motion/
│   │   │   ├── drv8833.c, drv8833.h
│   │   │   └── CMakeLists.txt
│   │   └── sensor/
│   │       ├── mpu6050.c, mpu6050.h
│   │       └── CMakeLists.txt
│   ├── services/           — 服务层
│   │   ├── audio_manager/
│   │   ├── display_manager/
│   │   ├── motion_manager/
│   │   ├── cloud_manager/
│   │   └── event_bus/
│   ├── app/                — 应用层
│   │   ├── emotion_engine/
│   │   ├── behavior_mgr/
│   │   └── ai_dialog/
│   └── system/             — 系统组件
│       ├── sysmon/
│       ├── power_mgr/
│       └── ota_service/
├── assets/                 — 资源文件
│   └── emotions/           — 表情数据
└── CMakeLists.txt
```

### 头文件规范

```c
// <module_name>.h
#pragma once                              // 或 #ifndef 守卫

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Public Types ---

// --- Public Constants ---

// --- Public Functions ---

/**
 * @brief 简短描述
 *
 * @param[in]  param1  参数1说明
 * @param[out] param2  参数2说明
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_* on error
 */
esp_err_t module_function(int param1, int *param2);

#ifdef __cplusplus
}
#endif
```

### 错误处理

```c
// 统一模式：每个 API 返回 esp_err_t

// 模式 1: 简单检查
esp_err_t ret = some_operation();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "some_operation failed: %s", esp_err_to_name(ret));
    return ret;
}

// 模式 2: goto cleanup
esp_err_t do_complex_thing(void) {
    esp_err_t ret;
    void *buf = NULL;
    SemaphoreHandle_t lock = NULL;

    buf = malloc(BUF_SIZE);
    if (!buf) { ret = ESP_ERR_NO_MEM; goto cleanup; }

    lock = xSemaphoreCreateMutex();
    if (!lock) { ret = ESP_ERR_NO_MEM; goto cleanup; }

    ret = do_work(buf, lock);
    if (ret != ESP_OK) goto cleanup;

cleanup:
    free(buf);
    if (lock) vSemaphoreDelete(lock);
    return ret;
}

// 模式 3: assert (仅用于不可恢复的严重错误)
assert(ptr != NULL);                                // 开发阶段
ESP_ERROR_CHECK(nvs_flash_init());                  // 不可恢复
```

### 内存管理

```c
// 优先级: 静态分配 > DRAM 动态分配 > PSRAM 动态分配

// 1. 静态分配（编译期确定，最快）
static uint8_t s_audio_buffer[AUDIO_BUF_SIZE];

// 2. DRAM 动态分配（常用）
void *buf = malloc(size);
if (!buf) { /* handle error */ }

// 3. PSRAM 动态分配（大数据，如帧缓冲）
void *buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
// 注意：PSRAM 使用 DMA 需要 cache sync
// 写入后: 无效化 cache 让 DMA 读到最新数据
// 读取前: 写回 cache 让 CPU 读到 DMA 写入的数据

// 4. 释放
free(buf);  // 无论来自 DRAM 还是 PSRAM，统一用 free()
```

### 并发安全

```c
// 共享资源保护
static SemaphoreHandle_t s_mutex;

void thread_safe_operation(void) {
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // 临界区
        shared_resource_update();
        xSemaphoreGive(s_mutex);
    } else {
        ESP_LOGW(TAG, "Failed to acquire mutex (timeout)");
    }
}

// ISR 中的原子操作
void gpio_isr_handler(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // 仅使用 FromISR 版本
    xSemaphoreGiveFromISR(s_binary_semaphore, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}
```

### 日志规范

```c
static const char *TAG = "module_name";  // 每个 .c 文件定义 TAG

// 按严重程度使用
ESP_LOGE(TAG, "Critical error: %s", esp_err_to_name(ret));  // 错误（必须处理）
ESP_LOGW(TAG, "Battery low: %d%%", battery_pct);            // 警告
ESP_LOGI(TAG, "WiFi connected, IP: " IPSTR, IP2STR(&ip));    // 关键信息
ESP_LOGD(TAG, "Sensor reading: %d", value);                  // 调试用

// 不要在 ISR 中使用（用 ESP_EARLY_LOGI 替代）
// 不要在高频循环中使用（10ms 周期 → 用 ESP_LOGD 或加计数器）
```

### ESP-IDF 特定

```c
// 编译时配置
// 使用 menuconfig / sdkconfig 管理，不硬编码
// 新增配置项: 创建 Kconfig 文件

menu "RobotBuddy Configuration"
    config ROBOTBUDDY_WIFI_SSID
        string "WiFi SSID"
        default "RobotBuddy"
    config ROBOTBUDDY_AUDIO_SAMPLE_RATE
        int "Audio Sample Rate"
        default 16000
        range 8000 48000
endmenu

// GPIO 配置
gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << PIN_NUM),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
};
ESP_ERROR_CHECK(gpio_config(&io_conf));
```

## Rules

1. ❌ 禁止 `delay(1000)` 阻塞 — 用 `vTaskDelay(pdMS_TO_TICKS(1000))`
2. ❌ 禁止 `while(1)` 忙等 — 用 Queue/Event/Semaphore 等待
3. ❌ 禁止 ISR 中 `printf` / `ESP_LOGx` — 用 `ESP_EARLY_LOGx`
4. ❌ 禁止不检查 `malloc` 返回值
5. ❌ 禁止裸 `new` / `delete` (C++时) — 用 `std::unique_ptr`
6. ✅ 必须检查所有 API 的 `esp_err_t` 返回值
7. ✅ Task 必须有看门狗 tick
8. ✅ 共享数据必须有互斥锁保护
9. ✅ 代码必须通过 `-Wall -Werror` 编译
10. ✅ 使用 ESP-IDF 的内置断言和错误检查宏

## Checklist

- [ ] 编译通过: `idf.py build` with 0 errors, 0 warnings
- [ ] 命名符合规范（snake_case / PascalCase / UPPER_CASE）
- [ ] 所有函数有 Doxygen 注释
- [ ] 所有 `esp_err_t` 返回值被检查
- [ ] 没有内存泄漏（验证方法: heap tracing / free 配对）
- [ ] 共享资源有互斥锁保护
- [ ] ISR 中仅使用 FromISR API
- [ ] 日志输出适当（不多不少，可定位问题）
- [ ] 魔法数字替换为命名常量
- [ ] 无 dead code / 注释掉的大段代码
- [ ] 是否更新changelog文档
