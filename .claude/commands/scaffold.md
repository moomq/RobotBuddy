# /scaffold — RobotBuddy 模块脚手架生成

## 用途

快速生成 RobotBuddy 固件模块的脚手架代码，包含完整的目录结构、CMakeLists.txt、Kconfig、头文件模板和测试框架。

## 适用场景

- 新增一个硬件驱动模块
- 新增一个服务管理器模块
- 新增一个应用层模块
- 新增一个系统组件模块

## 工作流

```
第 1 阶段：确定模块类型
├── 1.1 确认模块层级：driver / service / app / system
├── 1.2 确认模块名称（snake_case）
├── 1.3 确认依赖接口（SPI / I2C / I2S / PWM / GPIO / ADC / Queue / EventBus）
└── 1.4 输出：模块定义（名称、层级、接口依赖）

第 2 阶段：生成目录结构
├── 2.1 创建组件目录: firmware/components/<layer>/<module_name>/
├── 2.2 生成 CMakeLists.txt（idf_component_register）
├── 2.3 生成 Kconfig（如需要配置选项）
├── 2.4 生成头文件 <module_name>.h（公共接口）
├── 2.5 生成源文件 <module_name>.c（骨架实现）
├── 2.6 生成 README.md（模块说明）
└── 2.7 输出：完整的模块目录

第 3 阶段：集成到构建系统
├── 3.1 将模块添加到父级 CMakeLists.txt（REQUIRES / PRIV_REQUIRES）
├── 3.2 在 main/CMakeLists.txt 添加依赖
├── 3.3 注册 EventBus 事件 ID（如涉及）
├── 3.4 注册 Task 到 task_registry（如涉及）
└── 3.5 输出：构建系统更新

第 4 阶段：验证
├── 4.1 idf.py build → 编译通过
├── 4.2 检查头文件接口完整性
├── 4.3 检查代码规范（对照 standards/embedded-coding.md）
└── 4.4 输出：可编译的模块骨架
```

## 模板

### Driver 层模板

```c
// firmware/components/drivers/<name>/<name>.h
#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- 配置结构体 ---
typedef struct {
    // TODO: 填写接口引脚和参数
} <name>_config_t;

// --- 驱动句柄 ---
typedef struct <name>_ctx <name>_ctx_t;

// --- 生命周期 ---
esp_err_t <name>_init(const <name>_config_t *cfg, <name>_ctx_t **out_ctx);
esp_err_t <name>_deinit(<name>_ctx_t *ctx);
bool <name>_is_initialized(const <name>_ctx_t *ctx);

// --- 功能接口 ---
// TODO: 添加具体读写/控制 API

#ifdef __cplusplus
}
#endif
```

```cmake
# firmware/components/drivers/<name>/CMakeLists.txt
idf_component_register(
    SRCS "<name>.c"
    INCLUDES "."
    REQUIRES driver esp_timer
)
```

### Service 层模板

```c
// firmware/components/services/<name>/<name>.h
#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- 任务配置 ---
#define <NAME>_TASK_STACK_SIZE    4096
#define <NAME>_TASK_PRIORITY      3
#define <NAME>_TASK_PERIOD_MS     50
#define <NAME>_TASK_CORE_ID       1

// --- 初始化 ---
esp_err_t <name>_init(void);
esp_err_t <name>_start(void);
esp_err_t <name>_stop(void);

#ifdef __cplusplus
}
#endif
```

### Kconfig 模板

```kconfig
menu "RobotBuddy <Module Name> Configuration"
    config ROBOTBUDDY_<NAME>_ENABLE
        bool "Enable <module name>"
        default y
        help
            Enable the <module name> component.

    config ROBOTBUDDY_<NAME>_DEBUG
        bool "Enable <module name> debug logging"
        default n
        help
            Enable verbose debug logging for <module name>.
endmenu
```

### 测试模板

```c
// tests/unit/test_<name>.c
#include "unity.h"
// #include "mock_<dependencies>.h"
#include "<name>.h"

TEST_CASE("<name>_init: initializes with valid config", "[<name>]")
{
    // Arrange
    <name>_config_t cfg = { /* valid config */ };

    // Act
    esp_err_t ret = <name>_init(&cfg, NULL);

    // Assert
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("<name>_init: returns error with NULL config", "[<name>]")
{
    esp_err_t ret = <name>_init(NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);
}
```

## 前置条件

- ESP-IDF 项目已初始化（firmware/ 目录存在 sdkconfig 和顶层 CMakeLists.txt）
- 模块名称唯一，不与现有模块冲突
- 模块层级选择正确

## 输出

- 完整的模块目录（含 .c / .h / CMakeLists.txt / Kconfig / README.md）
- 构建系统更新（依赖添加）
- EventBus 事件定义更新（如需要）
- Task 注册更新（如需要）
- 编译验证通过

## 注意事项

- 头文件必须使用 `#pragma once` 和 `extern "C"` 包裹
- 所有公共 API 返回 `esp_err_t`
- 配置参数优先使用 Kconfig，而非硬编码
- Task 栈大小初始值给 2x 估算，后续用 watermark 优化
- 驱动模块使用不透明指针模式（`ctx_t`），服务模块使用单例模式
