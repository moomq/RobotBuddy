# /simulate — RobotBuddy PC 端模拟运行

## 用途

在无硬件的情况下，在 PC 上模拟运行 RobotBuddy 的软件逻辑，验证算法、状态机和业务流程。

## 适用场景

- 无 ESP32 硬件时验证逻辑正确性
- 新算法/状态机开发阶段快速迭代
- CI/CD 流水线中的自动化测试
- 跨平台功能验证

## 工作流

```
第 1 阶段：确定模拟范围
├── 1.1 确认需要模拟的模块（表情引擎/行为系统/传感器融合/对话管理）
├── 1.2 确认是否需要 GUI 可视化
├── 1.3 确认输入数据来源（模拟数据/录播数据/实时交互）
└── 1.4 输出：模拟方案

第 2 阶段：搭建模拟环境
├── 2.1 使用 ESP-IDF Linux 模拟器（如可用）或
├── 2.2 使用 CMake + Unity 在 PC 上编译模块代码
├── 2.3 实现 HAL Mock 层:
│   ├── SPI/I2C/I2S → 内存缓冲区
│   ├── GPIO → 变量读写
│   ├── FreeRTOS → pthread 模拟
│   └── WiFi/HTTP → curl/libevent 模拟
├── 2.4 实现时间加速（可选，加速长时间测试）
└── 2.5 输出：可编译的模拟项目

第 3 阶段：运行模拟
├── 3.1 编译: cmake --build build
├── 3.2 运行测试用例
├── 3.3 注入模拟事件:
│   ├── 模拟编译成功/失败事件
│   ├── 模拟传感器数据（边缘检测/IMU）
│   ├── 模拟云端 AI 响应
│   └── 模拟语音输入
├── 3.4 捕获输出:
│   ├── 表情状态变化序列
│   ├── 运动指令序列
│   ├── 事件总线消息流
│   └── 内存使用趋势
└── 3.5 输出：模拟结果

第 4 阶段：可视化（可选）
├── 4.1 表情预览: 用 Python/SDL 渲染 eye_params 到窗口
├── 4.2 状态机图: 生成状态转换序列图
├── 4.3 时序图: 生成 PlantUML 时序图
└── 4.4 输出：可视化结果
```

## HAL Mock 实现

```c
// test/mocks/hal_mock.h
// PC 端模拟 ESP32 HAL 接口

// GPIO Mock
typedef struct {
    bool level;
    void (*isr_handler)(void *);
} mock_gpio_t;
static mock_gpio_t s_gpios[48] = {0};

static inline void gpio_set_level(gpio_num_t pin, uint32_t level) {
    s_gpios[pin].level = (bool)level;
    if (s_gpios[pin].isr_handler) s_gpios[pin].isr_handler(NULL);
}

// I2C Mock (内存缓冲区模拟寄存器)
static uint8_t s_i2c_regs[256] = {0};
esp_err_t i2c_write(uint8_t dev_addr, uint8_t reg, const uint8_t *data, size_t len) {
    memcpy(&s_i2c_regs[reg], data, len);
    return ESP_OK;
}

// FreeRTOS Mock (pthread 模拟)
typedef pthread_t TaskHandle_t;
typedef pthread_mutex_t SemaphoreHandle_t;

static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t m, TickType_t t) {
    return pthread_mutex_lock(&m) == 0 ? pdTRUE : pdFALSE;
}
```

## 模拟测试用例

```c
// test/simulate/test_behavior_sim.c
// PC 端模拟行为系统

TEST_CASE("sim: build success triggers celebration", "[behavior][sim]")
{
    behavior_manager_init();
    emotion_init(&(emotion_config_t){.default_emotion = EMOTION_IDLE});

    // 注入编译成功事件
    robot_event_t event = {
        .id = EVENT_BUILD_STATUS,
        .payload = "{\"status\": \"success\", \"errors\": 0}",
    };
    event_bus_publish(&event);

    // 模拟 3 秒行为执行
    for (int i = 0; i < 300; i++) {
        behavior_manager_update();
        emotion_render_frame(NULL, NULL);
        usleep(10000); // 10ms
    }

    // 验证: 表情经历了 HAPPY 状态
    TEST_ASSERT_TRUE(was_emotion_triggered(EMOTION_HAPPY));

    // 验证: 执行了旋转庆祝动作
    TEST_ASSERT_TRUE(was_motion_triggered(MOTION_CELEBRATION));

    // 验证: 最终回到 IDLE
    TEST_ASSERT_EQUAL(EMOTION_IDLE, emotion_get_current());
}
```

## 表情可视化（Python）

```python
# test/visualize/emotion_preview.py
# 在 PC 上渲染 RobotBuddy 表情预览

import pygame
import math

WIDTH, HEIGHT = 480, 480  # 2x 放大

def render_eyes(screen, params):
    """根据 eye_params_t 渲染眼睛到 pygame 窗口"""
    bg_color = (0, 0, 0)
    eye_color = (255, 255, 255)
    iris_rgb565 = params['iris_color']
    iris_color = rgb565_to_rgb(iris_rgb565)

    screen.fill(bg_color)

    for side in [-1, 1]:  # 左右眼
        cx = WIDTH // 2 + side * params['eye_spacing'] * 2
        cy = HEIGHT // 3

        # 眼白
        pygame.draw.circle(screen, eye_color, (cx, cy), int(params['eye_radius'] * 2))

        # 虹膜
        ix = cx + int(params['iris_x'] * 2)
        iy = cy + int(params['iris_y'] * 2)
        pygame.draw.circle(screen, iris_color, (ix, iy), int(params['iris_radius'] * 2))

        # 瞳孔
        pygame.draw.circle(screen, (0, 0, 0), (ix, iy), int(params['pupil_radius'] * 2))

        # 高光
        if params['highlight_enabled']:
            hx = ix + int(params['iris_radius'] * 0.6)
            hy = iy - int(params['iris_radius'] * 0.6)
            pygame.draw.circle(screen, (255, 255, 255), (hx, hy), 4)

        # 眼皮
        eyelid_y = cy - int(params['eyelid_top'] * params['eye_radius'] * 2)
        pygame.draw.rect(screen, bg_color, (cx - 60, 0, 120, eyelid_y))

    pygame.display.flip()
```

## 前置条件

- PC 开发环境（Linux/macOS/Windows + WSL）
- CMake 3.16+
- GCC/Clang 编译器
- Python 3.8+（可视化时）
- Unity 测试框架

## 输出

- 可在 PC 上编译运行的模拟项目
- 模拟测试结果
- 表情可视化截图（如启用 GUI）
- 状态机转换序列
- 内存使用趋势图

## 注意事项

- PC 模拟仅验证逻辑正确性，不替代硬件测试
- 时序相关的行为（PID 控制、音频实时性）无法在模拟中验证
- HAL Mock 需要与真实硬件接口保持一致
- 模拟中的浮点精度可能与 ESP32 不同
- 模拟测试通过 ≠ 硬件上可用，必须最终在 ESP32 上验证
