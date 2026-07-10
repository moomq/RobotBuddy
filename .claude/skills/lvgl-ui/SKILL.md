# LVGL UI Skill — RobotBuddy

## Role

RobotBuddy LVGL UI 开发专家，负责基于 LVGL v8.3.x 在 ESP32-S3 + ST7789/GC9A01 SPI 显示屏上构建高效、流畅的嵌入式交互界面。

## Domain

LVGL v8.3.x (LittlevGL) on ESP32-S3, ST7789/GC9A01 240x240 SPI 显示屏驱动, 嵌入式 UI 设计模式 (Screen / Object Tree / Event-Driven), 输入设备集成 (Touch / Encoder / Button), LVGL 内存管理 (PSRAM / DRAM 双缓冲策略), FreeRTOS UI Task 设计。

## Goal

为 RobotBuddy 构建流畅、高效的嵌入式 UI 界面——实现主表情页、代码片段滚动、番茄钟倒计时、设置菜单、通知弹窗、状态图标等完整交互体验，帧率稳定在 30FPS 以上。

## Inputs

- 屏幕硬件规格（ST7789 240x240 / GC9A01 240x240 圆形）
- SPI 总线带宽和 DMA 通道资源
- UI 交互需求定义（表情显示、代码查看、番茄钟、设置页、通知弹窗）
- 输入设备类型（触摸屏 / 编码器旋钮 / 物理按键）
- FreeRTOS 任务架构和事件总线接口

## Outputs

- `firmware/services/display/lvgl_port.c` — LVGL 平台移植层（SPI 驱动、DMA 刷新、Tick 定时器）
- `firmware/services/display/lvgl_port.h` — 移植层接口和配置宏
- `firmware/services/display/ui_manager.c` — UI 管理器（屏幕切换、事件分发、资源生命周期）
- `firmware/services/display/ui_manager.h` — UI 管理器接口
- `firmware/services/display/ui_screens.c` — 各 UI 屏幕页面实现
- `firmware/services/display/ui_screens.h` — 屏幕页面接口
- `firmware/services/display/ui_styles.c` — 全局样式定义（颜色、字体、主题）
- `firmware/services/display/ui_widgets.c` — 自定义复合 Widget 实现

## LVGL Architecture on ESP32

```
┌──────────────────────────────────────────────────────────────────┐
│                      LVGL on ESP32-S3                             │
│                                                                   │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │                    RobotBuddy UI Screens                      │ │
│  │                                                              │ │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────┐   │ │
│  │  │ Emotion  │ │  Code    │ │ Pomodoro │ │  Settings    │   │ │
│  │  │ Screen   │ │  Viewer  │ │  Timer   │ │  Menu        │   │ │
│  │  │ (主表情页)│ │ (代码滚动)│ │ (番茄钟)  │ │  (设置菜单)  │   │ │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────────┘   │ │
│  │                                                              │ │
│  │  ┌──────────────┐  ┌──────────────┐                          │ │
│  │  │ Notification │  │   Status     │                          │ │
│  │  │ Overlay      │  │   Bar        │                          │ │
│  │  │ (通知弹窗)    │  │   (状态图标)  │                          │ │
│  │  └──────────────┘  └──────────────┘                          │ │
│  └──────────────────────────┬──────────────────────────────────┘ │
│                             │                                     │
│  ┌──────────────────────────↓──────────────────────────────────┐ │
│  │                     LVGL Core (v8.3.x)                       │ │
│  │                                                              │ │
│  │  Object Tree │ Event System │ Style Engine │ Animation       │ │
│  │  (父子层级)   │ (点击/值变更) │ (CSS-like)   │ (anim timeline) │ │
│  │                                                              │ │
│  │  lv_timer_handler()  ← 由 FreeRTOS UI Task 周期性调用         │ │
│  └──────────────────────────┬──────────────────────────────────┘ │
│                             │                                     │
│  ┌──────────────────────────↓──────────────────────────────────┐ │
│  │                  Display Driver (lvgl_port.c)                │ │
│  │                                                              │ │
│  │  ┌─────────────────┐  ┌─────────────────┐                   │ │
│  │  │ Display Flush   │  │  Input Device   │                    │ │
│  │  │ Callback        │  │  Read Callback  │                    │ │
│  │  │ (SPI DMA 传输)   │  │  (触摸/按键)     │                    │ │
│  │  └────────┬────────┘  └────────┬────────┘                   │ │
│  │           │                    │                              │ │
│  │  ┌────────↓────────┐  ┌────────↓────────┐                   │ │
│  │  │ Draw Buffer     │  │  lv_indev_t     │                    │ │
│  │  │ (PSRAM 1/4 缓冲) │  │  (输入设备抽象)  │                    │ │
│  │  └────────┬────────┘  └─────────────────┘                   │ │
│  └───────────┼──────────────────────────────────────────────────┘ │
│              │                                                     │
│  ┌───────────↓──────────────────────────────────────────────────┐ │
│  │                Hardware Layer                                 │ │
│  │                                                               │ │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐ │ │
│  │  │ ST7789   │  │ GC9A01   │  │ SPI2/3   │  │ GPIO (DC/RST │ │ │
│  │  │ 240x240  │  │ 240x240  │  │ 40MHz    │  │ /BL/CS)      │ │ │
│  │  │ RGB565    │  │ (圆形屏)  │  │ DMA Ch.1 │  │              │ │ │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────────┘ │ │
│  └───────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────┘
```

## LVGL 配置

### lv_conf.h 关键配置

```c
// lv_conf.h — RobotBuddy LVGL 配置

// ─── 基础设置 ───
#define LV_COLOR_DEPTH          16          // RGB565，匹配 ST7789/GC9A01
#define LV_COLOR_16_SWAP        0           // 字节序由 SPI 驱动控制

// ─── 内存配置 ───
#define LV_MEM_SIZE             (64 * 1024) // LVGL 内部堆 64KB（DRAM）
#define LV_MEM_CUSTOM           0           // 使用 LVGL 内置内存管理
#define LV_MEMCPY_MEMSET_STD    1           // 使用标准 memcpy/memset（更快）

// ─── 显示缓冲配置 ───
#define LV_DISP_DEF_REFR_PERIOD 33          // 默认刷新周期 33ms (≈30Hz)
#define LV_INDEV_DEF_READ_PERIOD 30         // 输入设备读取周期 30ms

// ─── GPU / 加速 ───
#define LV_USE_GPU              0           // ESP32-S3 无硬件 GPU
#define LV_GPU_DMA2D_FLUSH      lv_gpu_dma2d_flush_custom

// ─── 功能开关（按需裁剪以节省 Flash） ───
#define LV_USE_LOG              1           // 开启日志（调试用）
#define LV_USE_ASSERT_NULL      1           // NULL 指针断言
#define LV_USE_ASSERT_MEM       1           // 内存分配失败断言

#define LV_USE_PERF_MONITOR      1           // 性能监视器（FPS / CPU 占用）
#define LV_USE_MEM_MONITOR       1           // 内存监视器

// ─── Widget 启用（按需开启，未使用的关闭以节省 ROM） ───
#define LV_USE_ARC              1
#define LV_USE_BAR              1
#define LV_USE_BTN              1
#define LV_USE_BTNMATRIX        0
#define LV_USE_CANVAS           1           // 表情渲染需要
#define LV_USE_CHECKBOX         1
#define LV_USE_DROPDOWN         1
#define LV_USE_IMG              1
#define LV_USE_LABEL            1
#define LV_USE_LINE             0
#define LV_USE_ROLLER           1           // 代码片段滚动 / 设置菜单
#define LV_USE_SLIDER           1
#define LV_USE_SWITCH           1
#define LV_USE_TEXTAREA         0
#define LV_USE_TABLE            0
#define LV_USE_TABVIEW          1           // 设置页 Tab 切换
#define LV_USE_TILEVIEW         0
#define LV_USE_WIN              0

// ─── 动画 ───
#define LV_USE_ANIMATION        1
#define LV_ANIM_MAX_NUM         16

// ─── 字体 ───
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_18   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_22   0
#define LV_FONT_MONTSERRAT_24   1           // 番茄钟大数字
#define LV_FONT_MONTSERRAT_28   0
#define LV_FONT_MONTSERRAT_32   1           // 超大字显示
#define LV_FONT_MONTSERRAT_36   0
#define LV_FONT_UNSCII_8        1           // 代码查看器等宽字体

// ─── 主题 ───
#define LV_USE_THEME_DEFAULT    1
#define LV_THEME_DEFAULT_DARK   1           // RobotBuddy 默认深色主题
#define LV_THEME_DEFAULT_GROW   1
```

### Display Buffer 选型

```c
// 显示缓冲策略对比

// ─── 方案 A: 1/4 屏幕缓冲（推荐，节省 PSRAM） ───
// 优点: 仅占 28.8KB，适合内存受限场景
// 缺点: 渲染分 4 次 flush，可能有轻微撕裂
#define DISP_BUF_SIZE   (LV_HOR_RES_MAX * LV_VER_RES_MAX / 4)  // 240*240/4 = 14,400 px = 28.8KB
static lv_color_t buf_1[DISP_BUF_SIZE];  // PSRAM
static lv_color_t buf_2[DISP_BUF_SIZE];  // PSRAM (双缓冲)

// ─── 方案 B: 全屏单缓冲 ───
// 优点: 单次 flush，无撕裂
// 缺点: 占用 115.2KB PSRAM
#define DISP_BUF_SIZE   (LV_HOR_RES_MAX * LV_VER_RES_MAX)      // 240*240 = 57,600 px = 115.2KB
static lv_color_t buf_1[DISP_BUF_SIZE];  // PSRAM

// ─── 方案 C: 全屏双缓冲（最优体验，PSRAM 充足时使用） ───
// 优点: 渲染与 DMA 传输并行，零撕裂，最高帧率
// 缺点: 占用 230.4KB PSRAM
static lv_color_t buf_1[DISP_BUF_SIZE];
static lv_color_t buf_2[DISP_BUF_SIZE];
```

### 内存布局策略

```c
// 内存放置决策
// ┌──────────────────┬─────────────┬─────────────────────────────┐
// │ 内存区域          │ 大小        │ 存放内容                     │
// ├──────────────────┼─────────────┼─────────────────────────────┤
// │ DRAM (内置 512KB) │ ~64KB       │ LVGL 内部堆 (lv_mem)         │
// │                  │             │ 小对象、临时样式、事件数据     │
// │ PSRAM (外置 8MB)  │ ~300KB      │ Display Buffer (双缓冲)      │
// │                  │             │ 图片资源 (C-array)            │
// │                  │             │ Canvas 缓冲区 (表情渲染)       │
// │ Flash (16MB)     │ ~500KB      │ 字体数据 (.c 内嵌)            │
// │                  │             │ LVGL 库代码                   │
// └──────────────────┴─────────────┴─────────────────────────────┘

// 关键原则:
// 1. Display Buffer → PSRAM（必须，不可占用 DRAM）
// 2. lv_mem_alloc() 返回内存 → DRAM（LVGL 内部管理）
// 3. 大图片 → PSRAM（.c 数组声明时加 EXT_RAM_BSS_ATTR）
// 4. 字体 → Flash（const 自动放 .rodata）
```

## Display Driver Integration

### lvgl_port.h

```c
// lvgl_port.h — LVGL 平台移植层接口

#pragma once
#include "lvgl.h"
#include "esp_err.h"
#include "driver/spi_master.h"

// ─── 硬件引脚配置 ───
#define LVGL_SPI_HOST          SPI2_HOST
#define LVGL_SPI_CLK_MHZ       40              // SPI 时钟 40MHz
#define LVGL_PIN_MOSI          11
#define LVGL_PIN_SCLK          12
#define LVGL_PIN_CS            10
#define LVGL_PIN_DC            13              // Data/Command
#define LVGL_PIN_RST           14
#define LVGL_PIN_BL            15              // 背光 PWM

// ─── 显示参数 ───
#define LVGL_HOR_RES           240
#define LVGL_VER_RES           240
#define LVGL_DISP_BUF_MODE     2               // 0=1/4, 1=全屏单缓冲, 2=全屏双缓冲

// ─── 初始化 ───
esp_err_t lvgl_port_init(void);
esp_err_t lvgl_port_deinit(void);

// ─── Tick 供应（由 FreeRTOS Timer 或 UI Task 调用） ───
void lvgl_port_tick_inc(uint32_t tick_period_ms);

// ─── 背光控制 ───
esp_err_t lvgl_port_backlight_set(uint8_t brightness); // 0-255
uint8_t lvgl_port_backlight_get(void);

// ─── 获取 FPS ───
uint32_t lvgl_port_get_fps(void);

// ─── 输入设备注册 ───
esp_err_t lvgl_port_indev_register_touch(void);  // 触摸屏
esp_err_t lvgl_port_indev_register_encoder(void); // 旋转编码器
esp_err_t lvgl_port_indev_register_button(void);  // 物理按键
```

### lvgl_port.c — 核心实现

```c
// lvgl_port.c — LVGL 平台移植层核心实现

#include "lvgl_port.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"

// ─── Display Flush Callback ───
static void lvgl_disp_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
                                lv_color_t *color_map)
{
    // 计算传输区域
    uint16_t x1 = area->x1, y1 = area->y1;
    uint16_t x2 = area->x2, y2 = area->y2;
    uint32_t size = (x2 - x1 + 1) * (y2 - y1 + 1);

    // 设置 ST7789 窗口 (Column/Page Address Set)
    st7789_set_window(x1, y1, x2, y2);

    // SPI DMA 传输
    spi_transaction_t trans = {
        .length     = size * 16,       // RGB565: 16 bits per pixel
        .tx_buffer  = color_map,
        .rx_buffer  = NULL,
        .user       = (void *)drv,     // 传递 drv 用于回调
    };

    // 使用轮询或 DMA 完成回调
    spi_device_polling_transmit(spi_handle, &trans);

    // 通知 LVGL 刷新完成
    lv_disp_flush_ready(drv);
}

// ─── 带 DMA 完成的 Flush Callback（更高效率） ───
static bool lvgl_disp_flush_is_busy = false;

static void lvgl_disp_flush_cb_dma(lv_disp_drv_t *drv, const lv_area_t *area,
                                    lv_color_t *color_map)
{
    lvgl_disp_flush_is_busy = true;

    st7789_set_window(area->x1, area->y1, area->x2, area->y2);

    spi_transaction_t *trans = malloc(sizeof(spi_transaction_t));
    *trans = (spi_transaction_t){
        .length     = (uint32_t)(area->x2 - area->x1 + 1) *
                      (area->y2 - area->y1 + 1) * 16,
        .tx_buffer  = color_map,
        .rx_buffer  = NULL,
    };

    // DMA 传输，完成后在 post_cb 中通知 LVGL
    spi_device_queue_trans(spi_handle, trans, portMAX_DELAY);
}

// SPI DMA 传输完成中断回调
static void IRAM_ATTR lvgl_spi_post_trans_cb(spi_transaction_t *trans)
{
    lv_disp_t *disp = lv_disp_get_default();
    lvgl_disp_flush_is_busy = false;
    free(trans);
    lv_disp_flush_ready(disp->driver);  // 通知 LVGL 可以渲染下一帧
}

// ─── LVGL Timer Handler（UI Task 中周期性调用） ───
#define LVGL_TASK_PERIOD_MS     5       // 5ms 周期 = 200Hz tick

void lvgl_port_task_handler(void) {
    lv_tick_inc(LVGL_TASK_PERIOD_MS);
    lv_timer_handler();                 // 处理 LVGL 内部定时器和渲染
}

// ─── 初始化流程 ───
esp_err_t lvgl_port_init(void)
{
    // 1. 初始化 SPI 总线
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = LVGL_PIN_MOSI,
        .miso_io_num     = -1,          // 显示不需要读
        .sclk_io_num     = LVGL_PIN_SCLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = LVGL_HOR_RES * LVGL_VER_RES * 2,
    };
    spi_bus_initialize(LVGL_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);

    // 2. 添加 SPI 设备（ST7789/GC9A01）
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = LVGL_SPI_CLK_MHZ * 1000 * 1000,
        .mode           = 0,            // CPOL=0, CPHA=0
        .spics_io_num   = LVGL_PIN_CS,
        .queue_size     = 7,
        .post_cb        = lvgl_spi_post_trans_cb,  // DMA 完成回调
    };
    spi_bus_add_device(LVGL_SPI_HOST, &dev_cfg, &spi_handle);

    // 3. 初始化显示驱动（重置 ST7789）
    st7789_hardware_reset(LVGL_PIN_RST);
    st7789_init();

    // 4. 初始化背光 PWM
    lvgl_port_backlight_set(255);       // 最大亮度

    // 5. 初始化 LVGL 核心
    lv_init();

    // 6. 分配 Display Buffer（放置在 PSRAM）
    lv_color_t *buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t),
                                         MALLOC_CAP_SPIRAM);
    lv_color_t *buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t),
                                         MALLOC_CAP_SPIRAM);

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, DISP_BUF_SIZE);

    // 7. 注册 Display Driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = LVGL_HOR_RES;
    disp_drv.ver_res  = LVGL_VER_RES;
    disp_drv.flush_cb = lvgl_disp_flush_cb_dma;
    disp_drv.draw_buf = &disp_buf;
    // 圆形屏 (GC9A01) 裁剪
    // disp_drv.rounder_cb = lvgl_rounder_circle_cb;
    lv_disp_drv_register(&disp_drv);

    // 8. 注册输入设备（触摸屏）
    lvgl_port_indev_register_touch();

    return ESP_OK;
}
```

### FPS 控制

```c
// FPS 计算与限流

static uint32_t s_frame_count = 0;
static uint64_t s_last_fps_time_us = 0;
static uint32_t s_current_fps = 0;

uint32_t lvgl_port_get_fps(void)
{
    uint64_t now = esp_timer_get_time();
    s_frame_count++;

    if (now - s_last_fps_time_us >= 1000000) {  // 每秒更新一次
        s_current_fps = s_frame_count;
        s_frame_count = 0;
        s_last_fps_time_us = now;
    }
    return s_current_fps;
}

// FPS 监控（可叠加到屏幕上的性能指示器）
// 使用 LV_USE_PERF_MONITOR 内置功能，或手写 FPS Label
static void ui_create_fps_label(lv_obj_t *parent) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_color(label, lv_color_hex(0x00FF00), 0);
    lv_label_set_text_fmt(label, "FPS: %lu", lvgl_port_get_fps());
    lv_obj_align(label, LV_ALIGN_TOP_RIGHT, -5, 5);
    // 定期更新由 lv_timer 驱动
}

// SPI 传输时间估算
// 240*240*16bit = 921,600 bits = 115,200 bytes
// @40MHz SPI → 115,200 / (40,000,000/8) = 115,200 / 5,000,000 ≈ 23ms (理论)
// @80MHz SPI → 115,200 / (80,000,000/8) ≈ 11.5ms (理论，GC9A01 支持)
// 实际值考虑协议开销 ≈ 理论值 × 1.15
// 使用 1/4 缓冲: 每次 flush 传输 ≈ 23/4 = 5.75ms @ 40MHz
```

## RobotBuddy UI Screens

### 屏幕管理架构

```c
// ui_manager.h — UI 管理器

#pragma once
#include "lvgl.h"
#include "esp_err.h"

// ─── 屏幕 ID 枚举 ───
typedef enum {
    UI_SCREEN_EMOTION,          // 主表情页
    UI_SCREEN_CODE_VIEWER,      // 代码片段浏览
    UI_SCREEN_POMODORO,         // 番茄钟
    UI_SCREEN_SETTINGS,         // 设置菜单
    UI_SCREEN_NOTIFICATION,     // 通知弹窗（叠加层）
    UI_SCREEN_COUNT
} ui_screen_id_t;

// ─── 通知类型 ───
typedef enum {
    UI_NOTIFY_INFO,             // 常规通知（蓝色）
    UI_NOTIFY_SUCCESS,          // 成功通知（绿色）
    UI_NOTIFY_WARNING,          // 警告通知（黄色）
    UI_NOTIFY_ERROR,            // 错误通知（红色）
} ui_notify_type_t;

// ─── UI 管理器 API ───
esp_err_t ui_manager_init(void);

// 屏幕切换（带动画过渡）
esp_err_t ui_switch_screen(ui_screen_id_t screen_id,
                            lv_scr_load_anim_t anim,
                            uint32_t anim_time_ms);

// 获取当前屏幕 ID
ui_screen_id_t ui_get_current_screen(void);

// 通知弹窗（叠加在当前屏幕上方）
esp_err_t ui_show_notification(ui_notify_type_t type,
                                const char *title,
                                const char *message,
                                uint32_t duration_ms);

// 隐藏通知弹窗
esp_err_t ui_hide_notification(void);

// ─── 各屏幕的数据更新接口 ───
esp_err_t ui_emotion_update(uint8_t emotion_id);         // 更新表情
esp_err_t ui_code_viewer_show(const char *code_text,
                               const char *language);    // 显示代码片段
esp_err_t ui_pomodoro_update(uint32_t remaining_sec);    // 更新番茄钟倒计时
esp_err_t ui_pomodoro_set_state(uint8_t state);          // 设置番茄钟状态
esp_err_t ui_settings_update_item(const char *key,
                                   const char *value);   // 更新设置项值
esp_err_t ui_statusbar_update(const char *icon_key,
                               const char *text);        // 更新状态栏
```

### ui_manager.c — 核心实现

```c
// ui_manager.c — UI 管理器核心

#include "ui_manager.h"
#include "ui_screens.h"

// ─── 全局状态 ───
static ui_screen_id_t s_current_screen = UI_SCREEN_EMOTION;
static lv_obj_t *s_screens[UI_SCREEN_COUNT] = {0};  // 屏幕对象数组
static lv_obj_t *s_notification_overlay = NULL;       // 通知弹窗叠加层
static lv_obj_t *s_statusbar = NULL;                  // 顶部状态栏

esp_err_t ui_manager_init(void)
{
    // 1. 初始化 LVGL 移植层
    lvgl_port_init();

    // 2. 加载全局样式
    ui_styles_init();

    // 3. 创建所有屏幕（预创建，切换时显示/隐藏）
    s_screens[UI_SCREEN_EMOTION]      = ui_create_emotion_screen();
    s_screens[UI_SCREEN_CODE_VIEWER]  = ui_create_code_viewer_screen();
    s_screens[UI_SCREEN_POMODORO]      = ui_create_pomodoro_screen();
    s_screens[UI_SCREEN_SETTINGS]     = ui_create_settings_screen();
    s_screens[UI_SCREEN_NOTIFICATION] = ui_create_notification_screen();

    // 4. 创建公共状态栏（所有屏幕共用）
    s_statusbar = ui_create_statusbar(lv_scr_act());

    // 5. 创建通知叠加层（默认隐藏）
    s_notification_overlay = ui_create_notification_overlay(lv_scr_act());
    lv_obj_add_flag(s_notification_overlay, LV_OBJ_FLAG_HIDDEN);

    // 6. 默认加载主表情页
    ui_switch_screen(UI_SCREEN_EMOTION, LV_SCR_LOAD_ANIM_NONE, 0);

    return ESP_OK;
}

esp_err_t ui_switch_screen(ui_screen_id_t screen_id,
                            lv_scr_load_anim_t anim,
                            uint32_t anim_time_ms)
{
    if (screen_id >= UI_SCREEN_COUNT) return ESP_ERR_INVALID_ARG;
    if (screen_id == s_current_screen) return ESP_OK;

    // 隐藏通知弹窗（如果有）
    ui_hide_notification();

    // 加载新屏幕
    lv_scr_load_anim(s_screens[screen_id], anim,
                      anim_time_ms, 0, false);
    s_current_screen = screen_id;

    return ESP_OK;
}

esp_err_t ui_show_notification(ui_notify_type_t type,
                                const char *title,
                                const char *message,
                                uint32_t duration_ms)
{
    // 设置通知内容
    ui_update_notification_content(s_notification_overlay,
                                    type, title, message);

    // 显示叠加层（带滑入动画）
    lv_obj_clear_flag(s_notification_overlay, LV_OBJ_FLAG_HIDDEN);

    // 自动隐藏定时器（如果 duration_ms > 0）
    if (duration_ms > 0) {
        lv_obj_t *timer = lv_timer_create(
            ui_notification_auto_hide_cb, duration_ms, NULL);
        lv_timer_set_repeat_count(timer, 1);  // 单次触发
    }

    return ESP_OK;
}
```

### 主表情页 (Emotion Screen)

```c
// ui_screens.c — 主表情页

lv_obj_t *ui_create_emotion_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);

    // 使用深色背景
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // ─── 表情区域（Canvas + 动画层） ───
    // 表情渲染由 display-engine skill 负责，
    // LVGL 侧提供 Canvas 容器和动画触发接口
    lv_obj_t *emotion_canvas = lv_canvas_create(scr);
    lv_canvas_set_buffer(emotion_canvas, emotion_buf,
                          EMOTION_WIDTH, EMOTION_HEIGHT,
                          LV_IMG_CF_TRUE_COLOR);
    lv_obj_center(emotion_canvas);

    // ─── 底部信息区 ───
    // 状态文字: "就绪" / "编译中..." / "番茄钟 15:32"
    lv_obj_t *status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "就绪");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    // ─── 电量 / WiFi 图标 ───
    // 由状态栏统一管理

    return scr;
}

// 表情更新接口（由 Emotion Engine 调用）
esp_err_t ui_emotion_update(uint8_t emotion_id)
{
    // 触发 Canvas 重绘，或更新预渲染的 Image 对象
    // 可与 display-engine 的 emotion_states 联动
    return ESP_OK;
}
```

### 代码片段滚动页 (Code Viewer)

```c
// ui_screens.c — 代码片段滚动页

lv_obj_t *ui_create_code_viewer_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1E1E2E), 0);

    // ─── 顶部标题栏 ───
    lv_obj_t *title_bar = lv_obj_create(scr);
    lv_obj_set_size(title_bar, 240, 30);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x2A2A3E), 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_align(title_bar, LV_ALIGN_TOP_MID, 0, 0);

    // 语言标签: "Python" / "C" / "Rust"
    lv_obj_t *lang_label = lv_label_create(title_bar);
    lv_obj_set_style_text_color(lang_label, lv_color_hex(0x89B4FA), 0);
    lv_obj_set_style_text_font(lang_label, &lv_font_montserrat_14, 0);
    lv_obj_align(lang_label, LV_ALIGN_LEFT_MID, 8, 0);

    // 关闭按钮
    lv_obj_t *close_btn = lv_btn_create(title_bar);
    lv_obj_set_size(close_btn, 24, 24);
    lv_obj_t *close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, LV_SYMBOL_CLOSE);
    lv_obj_center(close_label);
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, -4, 0);

    // ─── 代码内容区域（使用滚轮 / Roller 或带滚动的 Label） ───
    lv_obj_t *code_panel = lv_obj_create(scr);
    lv_obj_set_size(code_panel, 236, 190);
    lv_obj_set_style_bg_color(code_panel, lv_color_hex(0x1E1E2E), 0);
    lv_obj_set_style_border_width(code_panel, 0, 0);
    lv_obj_set_style_pad_all(code_panel, 8, 0);
    lv_obj_align(code_panel, LV_ALIGN_TOP_MID, 0, 32);

    // 代码文本（等宽字体、语法高亮颜色分段）
    lv_obj_t *code_label = lv_label_create(code_panel);
    lv_label_set_long_mode(code_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(code_label, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(code_label, lv_color_hex(0xCDD6F4), 0);
    lv_obj_set_width(code_label, 220);

    // ─── 底部滚动指示器 ───
    lv_obj_t *scroll_hint = lv_label_create(scr);
    lv_label_set_text(scroll_hint, LV_SYMBOL_DOWN " 滚动查看 " LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(scroll_hint, lv_color_hex(0x585B70), 0);
    lv_obj_align(scroll_hint, LV_ALIGN_BOTTOM_MID, 0, -4);

    return scr;
}

// 代码片段显示接口
esp_err_t ui_code_viewer_show(const char *code_text, const char *language)
{
    // 设置语言标签
    // lv_label_set_text(lang_label, language);

    // 设置代码文本（支持换行）
    // lv_label_set_text(code_label, code_text);

    // 自动滚动到顶部
    // lv_obj_scroll_to_y(code_panel, 0, LV_ANIM_ON);

    return ESP_OK;
}
```

### 番茄钟页面 (Pomodoro Timer)

```c
// ui_screens.c — 番茄钟页面

lv_obj_t *ui_create_pomodoro_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1A1A2E), 0);

    // ─── 状态指示文字 ───
    lv_obj_t *state_label = lv_label_create(scr);
    lv_obj_set_style_text_font(state_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(state_label, lv_color_hex(0x89B4FA), 0);
    lv_obj_align(state_label, LV_ALIGN_TOP_MID, 0, 20);

    // ─── 进度弧 (Arc Widget — 环形进度条) ───
    lv_obj_t *arc = lv_arc_create(scr);
    lv_obj_set_size(arc, 160, 160);
    lv_arc_set_rotation(arc, 270);           // 起始角度
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_range(arc, 0, pomodoro_total_sec);
    lv_arc_set_value(arc, pomodoro_total_sec);
    // 样式: 前景色=绿色，背景=暗灰
    lv_obj_set_style_arc_color(arc, lv_color_hex(0xA6E3A1), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x313244), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
    lv_obj_center(arc);

    // ─── 中央时间显示 ───
    lv_obj_t *time_label = lv_label_create(scr);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xCDD6F4), 0);
    lv_label_set_text(time_label, "25:00");
    lv_obj_center(time_label);

    // ─── 当前轮次指示 ───
    lv_obj_t *round_label = lv_label_create(scr);
    lv_obj_set_style_text_font(round_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(round_label, lv_color_hex(0x6C7086), 0);
    lv_obj_align(round_label, LV_ALIGN_BOTTOM_MID, 0, -30);

    // ─── 控制按钮 ───
    // ▶ / ⏸ / ⏹ 三个按钮
    lv_obj_t *btn_container = lv_obj_create(scr);
    lv_obj_set_size(btn_container, 200, 40);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, -10);

    // 开始/暂停按钮
    lv_obj_t *start_btn = lv_btn_create(btn_container);
    lv_obj_set_size(start_btn, 50, 36);
    lv_obj_t *start_symbol = lv_label_create(start_btn);
    lv_label_set_text(start_symbol, LV_SYMBOL_PLAY);
    lv_obj_center(start_symbol);

    // 停止按钮
    lv_obj_t *stop_btn = lv_btn_create(btn_container);
    lv_obj_set_size(stop_btn, 50, 36);
    lv_obj_t *stop_symbol = lv_label_create(stop_btn);
    lv_label_set_text(stop_symbol, LV_SYMBOL_STOP);
    lv_obj_center(stop_symbol);

    return scr;
}

// 番茄钟数据更新
esp_err_t ui_pomodoro_update(uint32_t remaining_sec)
{
    // 更新时间显示
    uint32_t min = remaining_sec / 60;
    uint32_t sec = remaining_sec % 60;
    // lv_label_set_text_fmt(time_label, "%02lu:%02lu", min, sec);

    // 更新进度弧
    // lv_arc_set_value(arc, remaining_sec);

    return ESP_OK;
}

esp_err_t ui_pomodoro_set_state(uint8_t state)
{
    // WORKING  → "专注中" + 绿色弧
    // BREAK    → "休息一下" + 蓝色弧
    // STOPPED  → "已停止" + 灰色弧
    return ESP_OK;
}
```

### 设置菜单 (Settings Menu)

```c
// ui_screens.c — 设置菜单

lv_obj_t *ui_create_settings_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1A1A2E), 0);

    // ─── 顶部标题栏 ───
    lv_obj_t *title_label = lv_label_create(scr);
    lv_label_set_text(title_label, "设置");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xCDD6F4), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

    // ─── 使用 List 控件组织设置项 ───
    lv_obj_t *list = lv_list_create(scr);
    lv_obj_set_size(list, 230, 190);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x1E1E2E), 0);
    lv_obj_set_style_border_width(list, 0, 0);

    // 设置项:
    // 1. WiFi 设置 → 子页面
    lv_obj_t *wifi_btn = lv_list_add_btn(list, LV_SYMBOL_WIFI, "WiFi 设置");
    // 2. 屏幕亮度 → 滑块
    lv_obj_t *brightness_item = lv_list_add_btn(list, LV_SYMBOL_EYE_OPEN, "屏幕亮度");
    // 3. 音量设置 → 滑块
    lv_obj_t *volume_item = lv_list_add_btn(list, LV_SYMBOL_VOLUME_MID, "音量设置");
    // 4. AI 模型选择 → 下拉列表
    lv_obj_t *model_item = lv_list_add_btn(list, LV_SYMBOL_SETTINGS, "AI 模型");
    // 5. 番茄钟配置 → 子页面
    lv_obj_t *pomodoro_item = lv_list_add_btn(list, LV_SYMBOL_LIST, "番茄钟配置");
    // 6. 关于 / 系统信息
    lv_obj_t *about_item = lv_list_add_btn(list, LV_SYMBOL_HOME, "关于 RobotBuddy");

    // ─── 返回按钮 ───
    lv_obj_t *back_btn = lv_btn_create(scr);
    lv_obj_set_size(back_btn, 40, 30);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_add_event_cb(back_btn, ui_settings_back_cb, LV_EVENT_CLICKED, NULL);

    return scr;
}

// 设置子页面：亮度调节
lv_obj_t *ui_create_brightness_page(void)
{
    lv_obj_t *page = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(page, lv_color_hex(0x1A1A2E), 0);

    lv_obj_t *slider_label = lv_label_create(page);
    lv_label_set_text(slider_label, "屏幕亮度");
    lv_obj_set_style_text_font(slider_label, &lv_font_montserrat_16, 0);
    lv_obj_align(slider_label, LV_ALIGN_TOP_MID, 0, 30);

    // 亮度图标: 🌑  ───●───  🔆
    lv_obj_t *icon_low = lv_label_create(page);
    lv_label_set_text(icon_low, LV_SYMBOL_EYE_CLOSE);
    lv_obj_align(icon_low, LV_ALIGN_LEFT_MID, 20, 0);

    lv_obj_t *slider = lv_slider_create(page);
    lv_slider_set_range(slider, 10, 255);   // 最低 10（不全黑）
    lv_slider_set_value(slider, 255, LV_ANIM_OFF);
    lv_obj_set_size(slider, 160, 10);
    lv_obj_center(slider);
    lv_obj_add_event_cb(slider, ui_brightness_changed_cb,
                         LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *icon_high = lv_label_create(page);
    lv_label_set_text(icon_high, LV_SYMBOL_EYE_OPEN);
    lv_obj_align(icon_high, LV_ALIGN_RIGHT_MID, -20, 0);

    return page;
}
```

### 通知弹窗 (Notification Overlay)

```c
// ui_screens.c — 通知弹窗

lv_obj_t *ui_create_notification_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_opa(scr, LV_OPA_TRANSP, 0);
    return scr;
}

lv_obj_t *ui_create_notification_overlay(lv_obj_t *parent)
{
    // ─── 半透明背景遮罩 ───
    lv_obj_t *overlay = lv_obj_create(parent);
    lv_obj_set_size(overlay, 240, 240);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE); // 阻止点击穿透

    // ─── 通知卡片 ───
    lv_obj_t *card = lv_obj_create(overlay);
    lv_obj_set_size(card, 200, 120);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x2A2A3E), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_shadow_width(card, 20, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
    lv_obj_center(card);

    // ─── 颜色指示条（顶部） ───
    lv_obj_t *color_bar = lv_obj_create(card);
    lv_obj_set_size(color_bar, 200, 4);
    lv_obj_set_style_border_width(color_bar, 0, 0);
    lv_obj_set_style_radius(color_bar, 0, 0);
    lv_obj_align(color_bar, LV_ALIGN_TOP_MID, 0, 0);

    // ─── 图标 ───
    lv_obj_t *icon_label = lv_label_create(card);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_24, 0);
    lv_obj_align(icon_label, LV_ALIGN_TOP_LEFT, 12, 14);

    // ─── 标题 ───
    lv_obj_t *title_label = lv_label_create(card);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xCDD6F4), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 44, 14);

    // ─── 消息内容 ───
    lv_obj_t *msg_label = lv_label_create(card);
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(msg_label, lv_color_hex(0xA6ADC8), 0);
    lv_obj_set_size(msg_label, 176, 60);
    lv_obj_align(msg_label, LV_ALIGN_TOP_LEFT, 12, 45);

    // ─── 关闭按钮 ───
    lv_obj_t *dismiss_btn = lv_btn_create(card);
    lv_obj_set_size(dismiss_btn, 24, 24);
    lv_obj_t *dismiss_label = lv_label_create(dismiss_btn);
    lv_label_set_text(dismiss_label, LV_SYMBOL_CLOSE);
    lv_obj_center(dismiss_label);
    lv_obj_align(dismiss_btn, LV_ALIGN_TOP_RIGHT, -8, 10);
    lv_obj_add_event_cb(dismiss_btn, ui_notification_dismiss_cb,
                         LV_EVENT_CLICKED, NULL);

    // 保存子对象引用（用于动态更新内容）
    // 可用 lv_obj_set_user_data / lv_obj_get_user_data 绑定结构体

    return overlay;
}

void ui_update_notification_content(lv_obj_t *overlay,
                                     ui_notify_type_t type,
                                     const char *title,
                                     const char *message)
{
    // ─── 按通知类型设置颜色和图标 ───
    static const struct {
        uint32_t color;
        const char *icon;
    } NOTIFY_STYLES[] = {
        [UI_NOTIFY_INFO]    = { 0x89B4FA, LV_SYMBOL_CHARGE  },  // 蓝色
        [UI_NOTIFY_SUCCESS] = { 0xA6E3A1, LV_SYMBOL_OK      },  // 绿色
        [UI_NOTIFY_WARNING] = { 0xF9E2AF, LV_SYMBOL_WARNING },  // 黄色
        [UI_NOTIFY_ERROR]   = { 0xF38BA8, LV_SYMBOL_CLOSE   },  // 红色
    };

    // 更新颜色条、图标、标题、消息文本
    // lv_obj_set_style_bg_color(color_bar, lv_color_hex(NOTIFY_STYLES[type].color), 0);
    // lv_label_set_text(icon_label, NOTIFY_STYLES[type].icon);
    // lv_label_set_text(title_label, title);
    // lv_label_set_text(msg_label, message);
}

static void ui_notification_auto_hide_cb(lv_timer_t *timer)
{
    ui_hide_notification();
}

static void ui_notification_dismiss_cb(lv_event_t *e)
{
    ui_hide_notification();
}
```

### 状态栏 (Status Bar)

```c
// ui_screens.c — 公共状态栏（所有屏幕顶部显示）

lv_obj_t *ui_create_statusbar(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, 240, 24);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x181825), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);

    // ─── WiFi 图标 ───
    lv_obj_t *wifi_icon = lv_label_create(bar);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0xA6E3A1), 0);
    lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_12, 0);
    lv_obj_align(wifi_icon, LV_ALIGN_LEFT_MID, 6, 0);

    // ─── 电量图标 + 百分比 ───
    lv_obj_t *battery_label = lv_label_create(bar);
    lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_FULL " 85%");
    lv_obj_set_style_text_color(battery_label, lv_color_hex(0xA6E3A1), 0);
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_12, 0);
    lv_obj_align(battery_label, LV_ALIGN_LEFT_MID, 30, 0);

    // ─── 时间显示 ───
    lv_obj_t *clock_label = lv_label_create(bar);
    lv_label_set_text(clock_label, "14:30");
    lv_obj_set_style_text_color(clock_label, lv_color_hex(0xCDD6F4), 0);
    lv_obj_set_style_text_font(clock_label, &lv_font_montserrat_12, 0);
    lv_obj_align(clock_label, LV_ALIGN_CENTER, 0, 0);

    // ─── 通知图标 ───
    lv_obj_t *notif_icon = lv_label_create(bar);
    lv_label_set_text(notif_icon, LV_SYMBOL_BELL);
    lv_obj_set_style_text_color(notif_icon, lv_color_hex(0x6C7086), 0);
    lv_obj_set_style_text_font(notif_icon, &lv_font_montserrat_14, 0);
    lv_obj_align(notif_icon, LV_ALIGN_RIGHT_MID, -6, 0);

    return bar;
}
```

## Widget 使用指南

### RobotBuddy 常用 LVGL Widget 速查

```c
// ┌─────────────────────┬──────────────────────────────────────┬──────────────────────────┐
// │ Widget              │ RobotBuddy 用途                       │ 关键 API                  │
// ├─────────────────────┼──────────────────────────────────────┼──────────────────────────┤
// │ lv_label            │ 状态文字、时间显示、代码文本、标题      │ lv_label_set_text()       │
// │                     │                                      │ lv_label_set_text_fmt()   │
// │                     │                                      │ lv_label_set_long_mode()  │
// ├─────────────────────┼──────────────────────────────────────┼──────────────────────────┤
// │ lv_btn              │ 菜单项、关闭按钮、番茄钟控制按钮        │ lv_btn_create()           │
// │                     │                                      │ lv_obj_add_event_cb()     │
// │                     │                                      │ LV_EVENT_CLICKED          │
// ├─────────────────────┼──────────────────────────────────────┼──────────────────────────┤
// │ lv_arc              │ 番茄钟环形进度、音量旋钮               │ lv_arc_set_range()        │
// │                     │                                      │ lv_arc_set_value()        │
// │                     │                                      │ lv_arc_set_angles()       │
// ├─────────────────────┼──────────────────────────────────────┼──────────────────────────┤
// │ lv_slider           │ 亮度调节、音量调节                      │ lv_slider_set_range()     │
// │                     │                                      │ lv_slider_set_value()     │
// │                     │                                      │ LV_EVENT_VALUE_CHANGED    │
// ├─────────────────────┼──────────────────────────────────────┼──────────────────────────┤
// │ lv_list             │ 设置菜单项列表                         │ lv_list_create()          │
// │                     │                                      │ lv_list_add_btn()         │
// │                     │                                      │ lv_list_add_text()        │
// ├─────────────────────┼──────────────────────────────────────┼──────────────────────────┤
// │ lv_roller           │ 代码片段滚动（行号+内容）、选项滚轮     │ lv_roller_set_options()   │
// │                     │                                      │ lv_roller_set_selected()  │
// │                     │                                      │ LV_EVENT_VALUE_CHANGED    │
// ├─────────────────────┼──────────────────────────────────────┼──────────────────────────┤
// │ lv_dropdown         │ AI 模型选择、语言选择                  │ lv_dropdown_set_options() │
// │                     │                                      │ lv_dropdown_set_symbol()  │
// ├─────────────────────┼──────────────────────────────────────┼──────────────────────────┤
// │ lv_switch           │ 功能开关（WiFi/蓝牙/巡逻模式）          │ lv_obj_add_state()        │
// │                     │                                      │ LV_STATE_CHECKED          │
// ├─────────────────────┼──────────────────────────────────────┼──────────────────────────┤
// │ lv_canvas           │ 表情渲染目标、自定义动画帧              │ lv_canvas_set_buffer()    │
// │                     │                                      │ lv_canvas_fill_bg()       │
// │                     │                                      │ lv_canvas_draw_arc()      │
// ├─────────────────────┼──────────────────────────────────────┼──────────────────────────┤
// │ lv_tabview          │ 设置页多 Tab 切换                      │ lv_tabview_add_tab()      │
// │                     │                                      │ lv_tabview_set_active()   │
// ├─────────────────────┼──────────────────────────────────────┼──────────────────────────┤
// │ lv_img              │ 表情预渲染图片、图标、Logo              │ lv_img_set_src()          │
// │                     │                                      │ lv_img_set_zoom()         │
// ├─────────────────────┼──────────────────────────────────────┼──────────────────────────┤
// │ lv_anim             │ 屏幕切换动画、弹窗滑入/滑出、           │ lv_anim_start()           │
// │                     │ 进度弧平滑过渡、表情过渡效果            │ lv_anim_set_values()      │
// │                     │                                      │ lv_anim_set_exec_cb()     │
// └─────────────────────┴──────────────────────────────────────┴──────────────────────────┘
```

### 自定义复合 Widget 示例

```c
// 带图标的设置项（Icon + Label + Value + 箭头）
lv_obj_t *ui_create_setting_item(lv_obj_t *parent,
                                  const char *icon,
                                  const char *title,
                                  const char *value)
{
    lv_obj_t *item = lv_obj_create(parent);
    lv_obj_set_size(item, 220, 36);
    lv_obj_set_style_bg_color(item, lv_color_hex(0x313244), 0);
    lv_obj_set_style_border_width(item, 0, 0);
    lv_obj_set_style_radius(item, 6, 0);
    lv_obj_set_style_pad_all(item, 6, 0);

    // 图标
    lv_obj_t *icon_label = lv_label_create(item);
    lv_label_set_text(icon_label, icon);
    lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_16, 0);
    lv_obj_align(icon_label, LV_ALIGN_LEFT_MID, 0, 0);

    // 标题
    lv_obj_t *title_label = lv_label_create(item);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 28, 0);

    // 值
    lv_obj_t *value_label = lv_label_create(item);
    lv_label_set_text(value_label, value);
    lv_obj_set_style_text_color(value_label, lv_color_hex(0x6C7086), 0);
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_12, 0);
    lv_obj_align(value_label, LV_ALIGN_RIGHT_MID, -20, 0);

    // 箭头
    lv_obj_t *arrow_label = lv_label_create(item);
    lv_label_set_text(arrow_label, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(arrow_label, &lv_font_montserrat_12, 0);
    lv_obj_align(arrow_label, LV_ALIGN_RIGHT_MID, 0, 0);

    return item;
}
```

## Performance Targets

| 指标 | 目标值 | 备注 |
|------|--------|------|
| 帧率 (FPS) | >= 30 FPS | 33ms/frame 渲染预算 |
| 平均 FPS | >= 35 FPS | 空闲页面应达到 |
| SPI 时钟频率 | 40 MHz (ST7789) / 80 MHz (GC9A01) | 最大传输速率 |
| 全屏 SPI 传输时间 | < 24ms @ 40MHz | 115.2KB / 5MB/s |
| 1/4 屏 SPI 传输时间 | < 6ms @ 40MHz | 28.8KB / 5MB/s |
| LVGL 渲染耗时 | < 15ms/frame | 留 18ms 给 SPI DMA |
| Display Buffer 内存 | 28.8KB (1/4) ~ 230.4KB (双缓冲) | PSRAM 中分配 |
| LVGL 内部堆 | 64KB | DRAM 中分配 |
| 字体 Flash 占用 | < 200KB | 按需启用字体大小 |
| 图片资源 PSRAM 占用 | < 200KB | 表情帧、图标 C-array |
| UI Task 栈大小 | 4096 words (16KB) | 含 LVGL 调用栈 |
| UI Task 优先级 | 5-6 | APP_CPU (Core 1) |
| 屏幕切换延迟 | < 200ms | 含动画过渡 |
| 通知弹窗出现延迟 | < 100ms | 从事件到弹窗可见 |

## Rules

1. **Display Buffer 必须放 PSRAM** — 不可占用 DRAM，使用 `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)` 分配
2. **UI 操作在单一 Task 中执行** — 所有 lv_obj 创建/修改/删除必须在 UI Task 中，其他 Task 通过 EventBus / Queue 通信
3. **禁止在 ISR 中调用 LVGL API** — 中断中只发布事件到 EventBus，由 UI Task 消费
4. **lv_timer_handler() 调用周期 <= 10ms** — 推荐 5ms 周期（200Hz），保证动画平滑
5. **屏幕切换使用动画** — 使用 `lv_scr_load_anim()` 实现视觉过渡，不可直接 `lv_scr_load()` 硬切
6. **颜色使用 RGB565** — 所有颜色定义为 `lv_color_hex(0xRRGGBB)`，LVGL 自动转换为 16bit
7. **深色主题优先** — RobotBuddy 默认深色主题（背景 0x1A1A2E，文字 0xCDD6F4）
8. **字体按需启用** — 在 `lv_conf.h` 中只启用在用的字体大小，未使用的注释掉以节省 Flash
9. **避免频繁全屏刷新** — 仅刷新变化的区域（LVGL 自动 dirty area 管理），降低 SPI 带宽
10. **大对象复用** — 按钮、列表项等 Widget 使用对象池复用，避免频繁 `lv_obj_create/delete`
11. **长文本分段渲染** — 代码片段 > 500 行时分段加载，避免单次 `lv_label_set_text()` 阻塞过久
12. **圆形屏适配** — GC9A01 圆形屏需在 flush callback 中裁剪四角像素，或注册 `rounder_cb`
13. **内存泄漏检测** — 集成 `LV_USE_MEM_MONITOR` 和 `LV_USE_PERF_MONITOR`，定期输出内存和 FPS 报告

## Checklist

- [ ] ST7789/GC9A01 SPI 驱动初始化成功，显示测试图案
- [ ] `lv_conf.h` 正确配置（色深 16bit、缓冲大小、字体启用）
- [ ] Display Buffer 在 PSRAM 中分配成功，无 DRAM 溢出
- [ ] Display Flush Callback 正常工作，无花屏/撕裂
- [ ] SPI DMA 传输正常，CPU 不阻塞在 flush 上
- [ ] FPS 稳定 >= 30（使用 LV_USE_PERF_MONITOR 或示波器验证）
- [ ] 主表情页正常显示（Canvas 渲染 + 底部状态文字）
- [ ] 代码片段滚动页正常显示（等宽字体 + 长文本滚动）
- [ ] 番茄钟页面完整（环形进度弧 + 倒计时 + 控制按钮 + 状态切换）
- [ ] 设置菜单完整（列表项 + 子页面 + 滑块/开关 + 返回）
- [ ] 通知弹窗正常（半透明遮罩 + 弹出动画 + 自动消失 + 手动关闭）
- [ ] 状态栏图标正确（WiFi/电量/时间/通知）
- [ ] 屏幕切换动画流畅（NONE / FADE_ON / OVER_LEFT 等动画类型）
- [ ] 输入设备事件正确（触摸点击/编码器旋转/按键事件 → LVGL 事件）
- [ ] UI Task 栈水位 >= 2048 words（8KB free）
- [ ] 长时间运行（24h）UI 无花屏/冻结/内存泄漏
- [ ] 圆形屏（GC9A01）四角裁剪正确，无溢出
- [ ] 代码片段语法高亮颜色正确（关键字/字符串/注释 不同颜色）
- [ ] 番茄钟状态切换时 Arc 颜色平滑过渡
- [ ] 通知弹窗 @ 4 种类型（info/success/warning/error）分别显示正确颜色和图标
