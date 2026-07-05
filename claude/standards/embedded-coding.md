# RobotBuddy Embedded C Coding Standard

> ESP32-S3 固件编码规范 — 基于 MISRA C:2012 + ESP-IDF 最佳实践

---

## 1. 语言标准

- **C99** (ESP-IDF 默认)
- C++17 (如使用 Arduino-ESP32 组件，但本项目推荐纯 C)
- 不使用 GCC/Clang 特有扩展（除非 `__attribute__` 等必要场景）

## 2. 命名规范

### 文件
```
源文件:  snake_case.c       audio_manager.c, emotion_engine.c
头文件:  snake_case.h       audio_manager.h, emotion_engine.h
路径:    lowercase/          drivers/display/, services/cloud/
```

### 代码
```
函数:          snake_case          audio_capture_start()
类型/结构体:   PascalCase          EmotionState, MotionCommand
枚举类型:      PascalCase          EmotionId
枚举值:        UPPER_SNAKE_CASE    EMOTION_IDLE, WIFI_STATE_CONNECTED
宏/常量:       UPPER_SNAKE_CASE    #define MAX_AUDIO_BUF_SIZE 4096
全局变量:      g_前缀              g_wifi_state (尽量少用)
静态变量:      s_前缀              s_mutex
成员变量:      snake_case          state, count (无需前缀或 m_前缀)
指针:          描述性名称          *out_ctx, *input_buf
```

## 3. 类型

```c
// ✅ 使用标准整数类型
#include <stdint.h>
int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t
size_t (长度), ssize_t (有符号长度)

// ✅ 使用标准布尔
#include <stdbool.h>
bool flag = true;

// ✅ ESP-IDF 错误码
#include "esp_err.h"
esp_err_t   // 所有可失败的函数统一返回

// ❌ 禁止
int, long, short              // 嵌入式平台位宽不确定
char (用于非字符串)            // 有符号性不确定！用 int8_t 或 uint8_t
```

## 4. 函数

```c
// ✅ 每个函数单一职责，≤ 50 行
// ✅ 参数 ≤ 5 个（过多用结构体封装）
// ✅ 所有可失败的函数返回 esp_err_t

// ✅ 输出参数用指针，标注 out:
esp_err_t get_state(state_t *out_state);

// ✅ 配置参数用 const 指针
esp_err_t init(const config_t *cfg);

// ✅ 内部函数声明为 static
static void internal_helper(void);

// ❌ 禁止可变参数函数（va_list）— 嵌入式不安全
// ❌ 禁止递归 — 栈溢出风险
// ❌ 禁止超过 3 层的嵌套
```

## 5. 变量

```c
// ✅ 声明时初始化
int32_t count = 0;
void *ptr = NULL;

// ✅ 最小作用域
for (int i = 0; i < N; i++) { ... }  // C99

// ✅ 无副作用的全局变量用 const
static const uint8_t CMD_TABLE[] = { ... };

// ❌ 禁止未初始化的局部变量
// ❌ 尽量避免全局可变变量
```

## 6. 内存管理

```c
// 分配优先级: 静态 > DRAM > PSRAM

// ✅ 静态分配 (编译期确定)
static uint8_t s_buffer[4096];

// ✅ DRAM 动态分配
void *buf = malloc(size);

// ✅ PSRAM 动态分配 (大缓冲区)
void *buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);

// ✅ DMA 缓冲区 (从 PSRAM 分配时注意对齐和 cache)
uint8_t *dma_buf = heap_caps_aligned_alloc(4, size,
    MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);

// ✅ 释放后置 NULL
free(ptr);
ptr = NULL;

// ❌ 禁止: malloc 不检查返回值
// ❌ 禁止: 释放后继续使用指针
// ❌ 禁止: 双重释放
// ❌ 禁止: realloc (嵌入式堆碎片风险)
```

## 7. 错误处理

```c
// ✅ 模式 1: 检查并返回
esp_err_t ret = do_something();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "do_something failed: %s", esp_err_to_name(ret));
    return ret;
}

// ✅ 模式 2: goto cleanup (资源清理)
esp_err_t complex_operation(void) {
    esp_err_t ret;
    void *buf = NULL;
    SemaphoreHandle_t lock = NULL;

    buf = malloc(BUF_SIZE);
    if (!buf) { ret = ESP_ERR_NO_MEM; goto cleanup; }

    lock = xSemaphoreCreateMutex();
    if (!lock) { ret = ESP_ERR_NO_MEM; goto cleanup; }

    ret = do_work(buf, lock);

cleanup:
    free(buf);
    if (lock) vSemaphoreDelete(lock);
    return ret;
}

// ✅ 模式 3: assert (仅不可恢复错误)
configASSERT(ptr != NULL);  // FreeRTOS 的 assert

// ❌ 禁止: 吞掉错误（不检查返回值）
// ❌ 禁止: 在 release 版本使用 assert 做参数校验
```

## 8. 并发与 ISR

```c
// ✅ 互斥锁保护共享资源
static SemaphoreHandle_t s_mutex;

void safe_write(const char *data) {
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // 临界区
        internal_write(data);
        xSemaphoreGive(s_mutex);
    }
}

// ✅ ISR 精简且使用 FromISR API
void IRAM_ATTR gpio_isr_handler(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(s_sem, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// ❌ 禁止: ISR 中使用 printf / ESP_LOGx
// ❌ 禁止: ISR 中使用 vTaskDelay / 阻塞操作
// ❌ 禁止: 中断嵌套中调用非 FromISR 的 FreeRTOS API
// ❌ 禁止: 临界区超过 10μs
```

## 9. 日志

```c
static const char *TAG = "module";

ESP_LOGE(TAG, "...");  // 错误 — 必须处理
ESP_LOGW(TAG, "...");  // 警告 — 可能有问题
ESP_LOGI(TAG, "...");  // 重要信息 — 状态变更、连接成功等
ESP_LOGD(TAG, "...");  // 调试 — 频繁事件
ESP_LOGV(TAG, "...");  // 详细信息 — 仅开发时

// ❌ 禁止: ISR 中日志
// ❌ 禁止: 高频循环中使用 ESP_LOGI (用 ESP_LOGD 或计数器)
// ✅ 高频数据用计数器 + 周期性汇报
```

## 10. 注释

```c
/**
 * @brief 函数功能简述
 * @param[in]  input   输入参数描述
 * @param[out] output  输出参数描述
 * @return ESP_OK 成功，其他错误码
 * @note 注意事项
 */
esp_err_t function_name(int input, int *output);

// 解释 "为什么" 而非 "是什么"
// ❌ // 将 count 加 1
// ✅ // 防止 unsigned 下溢导致死循环，先检查 count == 0

// TODO/FIXME/HACK 标记
// TODO(user): 根据电量调整刷新率
// FIXME: 初始化失败时未清理 I2C 资源
```

## 11. 代码组织

```c
// .c 文件结构 (顺序):
// 1. 文件头注释 + includes
// 2. 宏定义
// 3. 类型定义
// 4. 静态函数声明
// 5. 静态变量
// 6. 静态函数实现
// 7. 公共函数实现

// .h 文件结构:
// 1. #pragma once
// 2. includes
// 3. 公共类型
// 4. 公共常量
// 5. 公共函数声明
// 6. extern "C" (C++兼容)
```

## 12. 编译器标志

```cmake
# CMakeLists.txt — 严格编译
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wextra")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wshadow -Wformat=2")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter")
# ESP-IDF 默认已启用的: -Wno-format, -Wno-missing-field-initializers
```

## 13. 禁止项

| 禁止 | 原因 | 替代 |
|------|------|------|
| `delay()` / `_delay_ms()` | 阻塞 CPU | `vTaskDelay()` |
| `while(1);` 忙等 | 浪费 CPU | Queue/Event/Semaphore 等待 |
| 递归函数 | 栈溢出 | 迭代 |
| `goto` (非错误清理) | 意大利面条 | 正常控制流 |
| 可变参数函数 (`...`) | MISRA C:2012 Rule 17.1 | 结构体 + 数组 |
| `malloc` 不检查返回 | 静默失败 | 检查 NULL |
| 动态分配无 free | 内存泄漏 | RAII 模式 |
| `#pragma` (非 `#pragma once`) | 不可移植 | — |
| 函数指针 (无类型定义) | 可读性差 | typedef |
