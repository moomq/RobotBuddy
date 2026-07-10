# Behavior System Skill — RobotBuddy

## Role

RobotBuddy 行为决策专家，负责协调表情、运动、音频和通知的多模态联动行为编排。

## Domain

行为状态机（Behavior FSM）、场景编排、番茄钟/专注模式、空闲巡逻、通知响应、情绪-动作-声音联动。

## Goal

实现有"性格"的机器人行为系统——让 RobotBuddy 的反应自然、连贯、有情感，而非机械的命令执行。

## Inputs

- 事件总线消息（云端响应、编译状态、传感器、用户交互）
- 电源状态（来自 power-management skill）
- 当前表情和运动状态
- 用户偏好配置

## Outputs

- `firmware/app/behavior_mgr/behavior_manager.c` — 行为管理器
- `firmware/app/behavior_mgr/behavior_states.c` — 行为状态定义
- `firmware/app/behavior_mgr/scene_orchestrator.c` — 场景编排器
- `firmware/app/behavior_mgr/pomodoro.c` — 番茄钟模块
- `docs/architecture/behavior-system.md` — 行为系统文档

## Behavior Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                     Behavior System                           │
│                                                               │
│  ┌────────────┐                                               │
│  │  Events    │  来自 EventBus 的所有输入事件                    │
│  │  Input     │  ├── CLOUD_RESPONSE  ─── AI 回复到达           │
│  │            │  ├── BUILD_STATUS    ─── 编译结果              │
│  │            │  ├── GIT_STATUS      ─── Git 状态变化          │
│  │            │  ├── USER_INTERACT   ─── 语音/触摸交互         │
│  │            │  ├── SENSOR_ALERT    ─── 传感器告警            │
│  │            │  ├── POWER_ALERT     ─── 电量告警              │
│  │            │  ├── TIMER_EVENT     ─── 定时器事件            │
│  │            │  └── NOTIFICATION    ─── 外部通知              │
│  └─────┬──────┘                                               │
│        ↓                                                       │
│  ┌─────────────────────────────────────────────────────┐      │
│  │            Behavior Manager (决策层)                  │      │
│  │                                                      │      │
│  │  ┌──────────────┐  ┌──────────────┐                 │      │
│  │  │ Scene        │  │ Behavior     │                  │      │
│  │  │ Orchestrator │  │ Priority     │                  │      │
│  │  │ (场景编排)    │  │ Resolver     │                  │      │
│  │  └──────┬───────┘  └──────────────┘                 │      │
│  └─────────┼───────────────────────────────────────────┘      │
│            ↓ (行为指令)                                         │
│  ┌─────────┼───────────────────────────────────────────┐      │
│  │         ↓           多模态输出协调                     │      │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐            │      │
│  │  │ Emotion  │ │ Motion   │ │ Audio    │             │      │
│  │  │ Engine   │ │ Control  │ │ Manager  │             │      │
│  │  └──────────┘ └──────────┘ └──────────┘            │      │
│  └─────────────────────────────────────────────────────┘      │
└──────────────────────────────────────────────────────────────┘
```

## Behavior State Machine

```c
// 顶层行为状态
typedef enum {
    BEHAVIOR_IDLE,          // 空闲待命
    BEHAVIOR_LISTENING,     // 聆听用户语音
    BEHAVIOR_THINKING,      // AI 处理中
    BEHAVIOR_ANSWERING,     // 回复用户中
    BEHAVIOR_FOCUS,         // 专注/番茄钟模式
    BEHAVIOR_CELEBRATING,   // 庆祝（编译成功等）
    BEHAVIOR_ALERTING,      // 告警（编译失败/低电量等）
    BEHAVIOR_SLEEPING,      // 休眠
    BEHAVIOR_PATROLLING,    // 桌面巡逻
    BEHAVIOR_CHARGING,      // 充电中
} behavior_state_t;

// 行为指令（输出到各子系统）
typedef struct {
    // Emotion
    emotion_id_t target_emotion;
    uint32_t emotion_transition_ms;

    // Motion
    motion_command_t motion_cmd;
    bool has_motion;

    // Audio
    audio_action_t audio_action;
    const char *audio_prompt;      // TTS 文本 或 音效名

    // Display
    const char *status_text;       // 屏幕文字（如错误信息）
    uint16_t display_duration_ms;
} behavior_command_t;
```

## Scene Definitions

```c
// 场景：每个场景定义了完整的联动行为
// 场景 = 事件触发 + 行为序列 + 持续时间 + 退出条件

typedef struct {
    const char *name;
    robot_event_id_t trigger_event;    // 触发事件
    behavior_state_t target_state;     // 目标行为状态
    behavior_command_t *commands;       // 行为指令序列
    uint8_t command_count;
    uint32_t total_duration_ms;        // 场景总时长
    behavior_state_t return_state;     // 场景结束后的返回状态
} scene_definition_t;

// 预定义场景表
static const scene_definition_t SCENE_TABLE[] = {
    // ─── 编译成功 ───
    {
        .name = "build_success",
        .trigger_event = EVENT_BUILD_STATUS,
        .target_state = BEHAVIOR_CELEBRATING,
        .commands = (behavior_command_t[]){
            { EMOTION_HAPPY,   300, {0}, false, AUDIO_TTS, "编译成功！", NULL, 3000 },
            { EMOTION_HAPPY,   0,   {.angular_velocity=6.28f, .duration_ms=1000}, true, AUDIO_NONE, NULL, NULL, 0 },
        },
        .command_count = 2,
        .total_duration_ms = 3000,
        .return_state = BEHAVIOR_IDLE,
    },

    // ─── 编译失败 ───
    {
        .name = "build_failed",
        .trigger_event = EVENT_BUILD_STATUS,
        .target_state = BEHAVIOR_ALERTING,
        .commands = (behavior_command_t[]){
            { EMOTION_ERROR,   200, {0}, false, AUDIO_TTS, "编译失败了，有 $n 个错误", NULL, 3000 },
            { EMOTION_ERROR,   0,   {.linear_velocity=-0.1f, .duration_ms=500}, true, AUDIO_NONE, NULL, NULL, 0 },
        },
        .command_count = 2,
        .total_duration_ms = 4000,
        .return_state = BEHAVIOR_IDLE,
    },

    // ─── 新通知 ───
    {
        .name = "new_notification",
        .trigger_event = EVENT_NOTIFICATION,
        .target_state = BEHAVIOR_ALERTING,
        .commands = (behavior_command_t[]){
            { EMOTION_EXCITED, 200, {.linear_velocity=0.05f, .duration_ms=300}, true, AUDIO_EFFECT, "notification", NULL, 0 },
        },
        .command_count = 1,
        .total_duration_ms = 2000,
        .return_state = BEHAVIOR_IDLE,
    },

    // ─── 低电量 ───
    {
        .name = "low_battery",
        .trigger_event = EVENT_POWER_ALERT,
        .target_state = BEHAVIOR_ALERTING,
        .commands = (behavior_command_t[]){
            { EMOTION_WARNING, 300, {0}, false, AUDIO_TTS, "电量低，请充电", NULL, 3000 },
        },
        .command_count = 1,
        .total_duration_ms = 4000,
        .return_state = BEHAVIOR_IDLE,
    },
};
```

## Pomodoro Timer

```c
// 番茄钟配置
#define POMODORO_WORK_MINUTES       25      // 工作时长
#define POMODORO_BREAK_MINUTES      5       // 休息时长
#define POMODORO_LONG_BREAK_MINUTES 15      // 长休息时长（每4轮）
#define POMODORO_ROUNDS_BEFORE_LONG 4       // 长休息前的轮数

typedef enum {
    POMODORO_STATE_STOPPED,
    POMODORO_STATE_WORKING,
    POMODORO_STATE_BREAK,
    POMODORO_STATE_LONG_BREAK,
} pomodoro_state_t;

typedef struct {
    pomodoro_state_t state;
    uint8_t current_round;          // 当前第几轮
    uint32_t remaining_seconds;     // 剩余秒数
    uint32_t total_seconds;         // 当前阶段总秒数
} pomodoro_status_t;

// 番茄钟 API
esp_err_t pomodoro_start(void);
esp_err_t pomodoro_stop(void);
esp_err_t pomodoro_pause(void);
esp_err_t pomodoro_resume(void);
esp_err_t pomodoro_get_status(pomodoro_status_t *status);

// 番茄钟事件 → 行为联动
// 工作中:    表情 FOCUS + 屏幕显示倒计时 + 每隔5分钟眼睛微动
// 休息时:    表情 HAPPY + 语音"该休息了" + 移动到用户面前
// 长休息:    表情 HAPPY + 语音建议 + 眼睛随音乐动
```

## Idle Patrol

```c
// 空闲巡逻（桌面安全区域内随机移动）
typedef struct {
    bool enabled;
    uint32_t idle_threshold_sec;    // 多久无交互开始巡逻 (默认 180s)
    float patrol_speed;             // 巡逻速度 m/s (默认 0.05)
    uint32_t patrol_interval_sec;   // 巡逻间隔 (默认 30s)
    uint32_t patrol_duration_sec;   // 单次巡逻时长 (默认 10s)
} patrol_config_t;

// 巡逻行为
// 1. 随机选择方向（前进/后退/左转/右转）
// 2. 边缘检测保护（实时检查红外传感器）
// 3. 遇障碍自动换方向
// 4. 巡逻中保持 IDLE 表情 + 偶尔眨眼
// 5. 收到任何交互事件立即停止巡逻
```

## Behavior Priority Resolver

```c
// 当多个事件同时到达时的优先级仲裁
typedef enum {
    PRIORITY_CRITICAL = 0,  // 紧急停止 / 跌落检测 / 低电量
    PRIORITY_HIGH,          // 编译失败 / 语音交互 / 告警
    PRIORITY_NORMAL,        // 编译成功 / Git 通知 / 充电状态
    PRIORITY_LOW,           // 空闲巡逻 / 定时提醒
} behavior_priority_t;

// 优先级规则
// 1. 安全事件（跌落/碰撞/低电量）总是最高优先级
// 2. 用户主动交互打断任何自动行为
// 3. 正在播放 TTS 时，新通知排队等待
// 4. 同一场景 10s 内不重复触发（防抖）
// 5. 庆祝场景结束后 30s 内不再庆祝
```

## Behavior ↔ Subsystem Coordination

```c
// 行为指令发送到各子系统的协调逻辑

// 1. 表情 + 语音 同步
//    表情切换先于语音 200ms（让用户先看到表情变化）
//    TTS 播放期间，表情保持 ANSWERING（嘴部随 RMS 动）

// 2. 运动 + 传感器 安全
//    任何运动指令执行前检查：
//    - edge_detected → 禁止前进
//    - obstacle_close → 减速或停止
//    - battery_critical → 禁止所有运动
//    - is_charging → 禁止所有运动

// 3. 音频 + 显示 互斥
//    TTS 播放时屏幕可以同时显示文字（不互斥）
//    麦克风采集时扬声器静音（全双工需硬件支持）

// 4. WiFi 断线 → 行为降级
//    云端 AI 不可用时：
//    - 语音交互 → 提示"网络不可用"
//    - 表情 → WARNING
//    - 保留本地功能（表情/运动/传感器）
```

## Rules

1. **安全第一** — 任何行为不得绕过传感器安全检查
2. **用户优先** — 用户主动交互立即响应，打断自动行为
3. **不重复** — 同类场景短时间内不重复触发
4. **平滑过渡** — 表情和运动状态切换使用过渡动画
5. **可中断** — 长时间行为（巡逻/庆祝）可被高优先级事件中断
6. **电量感知** — 行为根据电量自动调整（低电量减少运动/降低音量）
7. **可配置** — 番茄钟时长、巡逻参数等通过 Kconfig 或 NVS 配置

## Checklist

- [ ] 编译成功/失败场景联动正确（表情+运动+语音）
- [ ] 番茄钟全流程正常（开始→工作→休息→循环→结束）
- [ ] 空闲巡逻在安全区域内运行、遇障碍停止
- [ ] 用户交互能打断任何自动行为
- [ ] 多事件并发时优先级仲裁正确
- [ ] 低电量时行为自动降级
- [ ] WiFi 断线时行为降级到本地模式
- [ ] 表情-语音同步自然（200ms 预切换）
- [ ] 长时间运行（24h）行为系统无死锁/内存泄漏
