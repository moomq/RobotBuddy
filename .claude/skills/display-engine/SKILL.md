# Display Engine Skill — RobotBuddy

## Role

RobotBuddy 显示与表情引擎专家，负责屏幕驱动、表情状态机、动画渲染和 UI 布局。

## Domain

ST7789/GC9A01 SPI LCD，LVGL 嵌入式 UI 框架，像素级表情渲染，帧动画系统。

## Goal

实现 30FPS 流畅的表情动画和状态显示，将机器人"眼睛"打造为有灵魂的交互界面。

## Inputs

- 屏幕硬件规格（ST7789 240×240 / GC9A01 240×240 圆形）
- 表情需求定义（11 种表情状态）
- SPI 总线带宽评估

## Outputs

- `firmware/services/display/display_manager.c` — 显示管理器
- `firmware/services/display/emotion_engine.c` — 表情引擎
- `firmware/services/display/emotion_states.c` — 各表情状态实现
- `firmware/services/display/animation_tween.c` — 动画补间
- `firmware/assets/emotions/*.c` — 表情帧数据

## Display Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Display Pipeline                         │
│                                                              │
│  ┌───────────┐   ┌─────────────┐   ┌──────────────────────┐ │
│  │ Emotion   │   │ Expression  │   │ Frame Buffer         │ │
│  │ State    ─┼→  │ Renderer   ─┼→  │ (240×240×16bit)     ─┼→│
│  │ Machine   │   │             │   │ PSRAM                │ │
│  └───────────┘   └─────────────┘   └──────────────────────┘ │
│       ↑                                               │      │
│  ┌────┴──────────┐                              ┌────↓─────┐│
│  │ Input Events  │                              │ SPI DMA  ││
│  │ • Cloud Resp  │                              │ 40MHz    ││
│  │ • Build Status│                              │ ST7789   ││
│  │ • User Action │                              └──────────┘│
│  │ • Battery Lvl │                                          │
│  └───────────────┘                                          │
└─────────────────────────────────────────────────────────────┘
```

## Emotion State Machine

```
                    ┌──────────────────────┐
                    │                      │
     ┌─────────┐    │    ┌──────────┐      │
     │  IDLE   │←───┼───→│LISTENING │      │
     └────┬────┘    │    └────┬─────┘      │
          │         │         │             │
     ┌────↓────┐    │    ┌────↓─────┐      │
     │THINKING │←───┼───→│ANSWERING │      │
     └────┬────┘    │    └────┬─────┘      │
          │         │         │             │
     ┌────↓────┐    │         │            │
     │ HAPPY   │    │    ┌────↓─────┐      │
     │ ERROR   │    │    │ CONFUSED │      │
     │ WARNING │    │    │  FOCUS   │      │
     │ EXCITED │    │    │  SLEEP   │      │
     └─────────┘    │    └──────────┘      │
                    │                      │
                    └──────────────────────┘
```

## Emotion State Implementation

```c
typedef enum {
    EMOTION_IDLE,
    EMOTION_LISTENING,
    EMOTION_THINKING,
    EMOTION_ANSWERING,
    EMOTION_HAPPY,        // Build Success
    EMOTION_CONFUSED,
    EMOTION_WARNING,      // Build Warning / Git pending
    EMOTION_ERROR,        // Build Failed
    EMOTION_FOCUS,        // Coding mode
    EMOTION_SLEEP,
    EMOTION_EXCITED,      // New notification
} emotion_id_t;

typedef struct {
    emotion_id_t current;
    emotion_id_t target;
    float transition_progress;  // 0.0 - 1.0
    uint32_t state_start_ms;
    uint32_t state_duration_ms;
    void *state_data;           // State-specific data
} emotion_state_t;

// 眼睛参数（每个表情的核心渲染参数）
typedef struct {
    float eye_radius;           // 眼睛大小
    float eye_spacing;          // 眼距
    float iris_radius;          // 虹膜大小
    float iris_x;               // 虹膜水平偏移 (视线方向)
    float iris_y;               // 虹膜垂直偏移
    float pupil_radius;         // 瞳孔大小
    float eyelid_top;           // 上眼皮位置 (0=睁眼, 1=闭眼)
    float eyelid_bottom;        // 下眼皮位置
    uint16_t iris_color;        // 虹膜颜色 (RGB565)
    bool highlight_enabled;     // 高光点
    float mouth_curve;          // 笑/哭弧度
} eye_params_t;
```

## Expression Rendering

```c
// Per-frame: 根据 eye_params 渲染到帧缓冲
void emotion_render_frame(emotion_state_t *state, uint16_t *framebuffer);

// 表情切换（带过渡动画）
void emotion_transition_to(emotion_id_t target, uint32_t duration_ms);

// 眨眼动画（周期性插入）
void emotion_blink_trigger(uint32_t duration_ms);

// 嘴部动画（随语音 RMS 能量）
void emotion_mouth_set_amplitude(float rms_energy);
```

## Emotion ↔ Scenario Mapping

| 表情 | 触发场景 | 眼睛特征 | 颜色 |
|------|---------|---------|------|
| IDLE | 无交互 | 微动、眨眼 | 青色 iris |
| LISTENING | 语音输入中 | 放大、波纹 | 青色 + 波纹 |
| THINKING | LLM 处理 | 左右移动、加载圈 | 蓝色思考 |
| ANSWERING | TTS 播放中 | 嘴部随音动 | 青色 + 嘴动画 |
| HAPPY | 编译成功 ✅ | 弯月眼 ^_^ | 绿色 iris |
| ERROR | 编译失败 ❌ | 怒眼 ×_× | 红色 iris |
| WARNING | 警告/待处理 ⚠️ | 闪烁 | 黄色 iris |
| CONFUSED | 没理解 ❓ | 歪头 ?_? | 青色 + ? |
| FOCUS | 番茄钟/编码中 | 眼睛微闭 | 蓝色暗 iris |
| SLEEP | 超时休眠 | 闭眼 | 无 iris (黑) |
| EXCITED | 新通知/新功能 | 跳动 ✨ | 金色 iris |

## Animation System

```c
// 缓动函数
typedef float (*easing_func_t)(float t); // t: 0.0→1.0, returns 0.0→1.0

float ease_in_out_cubic(float t);
float ease_out_elastic(float t);
float ease_in_out_bounce(float t);

// 补间动画
typedef struct {
    float from;
    float to;
    float current;
    uint32_t duration_ms;
    uint32_t elapsed_ms;
    easing_func_t easing;
    bool complete;
} tween_t;

void tween_update(tween_t *tween, uint32_t delta_ms);
float tween_get_value(const tween_t *tween);
```

## Performance Targets

| 指标 | 目标 |
|------|------|
| 帧率 | ≥ 30 FPS (33ms/frame) |
| 表情切换延迟 | < 50ms (从事件到首帧变化) |
| SPI 传输时间 | < 10ms (240×240×16bit @ 40MHz) |
| 帧缓冲大小 | 240×240×2 = 115.2 KB |
| 每帧渲染时间 | < 15ms (留 18ms 给 SPI DMA) |
| PSRAM 使用 | 帧缓冲放在 PSRAM |

## Rules

1. 帧缓冲放在 PSRAM，释放 DRAM
2. 使用 LVGL (可选) 或手写轻量渲染器
3. 所有渲染在 single task 中进行（避免 UI 竞态）
4. SPI 传输使用 DMA（不阻塞 CPU）
5. 表情过渡必须平滑（使用 easing 函数）
6. 眼睛始终保持在安全区域内（圆形屏需特殊处理）
7. 颜色使用 RGB565 (16bit)，节省内存

## Checklist

- [ ] ST7789 初始化成功，显示测试图案
- [ ] 帧率稳定 ≥ 30FPS (示波器测 CS 引脚)
- [ ] 11 种表情全部实现并测试
- [ ] 表情切换动画流畅无撕裂
- [ ] 眨眼动画自然（2-5s 随机间隔）
- [ ] 语音同步：嘴部随 RMS 能量变化
- [ ] 颜色准确（RGB565 转换无偏差）
- [ ] 长时间运行无花屏/冻结
- [ ] 圆形屏 (GC9A01) 裁剪正确
