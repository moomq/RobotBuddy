# Motion Control Skill — RobotBuddy

## Role

RobotBuddy 运动控制专家，负责双轮差速驱动、IMU 姿态感知和桌面安全移动。

## Domain

N20 减速电机、DRV8833 双 H 桥、MPU6050 IMU、PID 控制、里程计、桌面防跌落。

## Goal

实现稳定、安全、有"性格"的桌面移动控制。

## Inputs

- 电机/驱动硬件规格
- 运动场景需求（前进/后退/转向/原地旋转/表情动作）
- FreeRTOS 任务架构（100Hz 控制周期）

## Outputs

- `firmware/services/motion/motion_manager.c` — 运动管理器
- `firmware/services/motion/motor_pid.c` — PID 控制器
- `firmware/services/motion/odometry.c` — 里程计
- `firmware/services/motion/imu_handler.c` — IMU 数据处理
- `docs/motion-control.md` — 运动控制文档

## Motion Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Motion Control Pipeline                  │
│                                                              │
│  ┌───────────┐   ┌──────────┐   ┌─────────┐   ┌──────────┐ │
│  │Behavior   │   │Motion    │   │ PID     │   │ DRV8833  │ │
│  │Mgr (100Hz)│──→│Planner   │──→│ Left    │──→│ PWM L    │─┼→ N20 Left
│  │           │   │          │   │ Right   │   │ PWM R    │ │  N20 Right
│  └───────────┘   └──────────┘   └────┬────┘   └──────────┘ │
│                                       │                      │
│  ┌───────────┐                   ┌────↓────┐               │
│  │  MPU6050  │──→ IMU Handler ──→│Fusion   │               │
│  │  6-axis   │                   │(Yaw)    │               │
│  └───────────┘                   └─────────┘               │
│                                                              │
│  ┌───────────┐   ┌──────────┐                               │
│  │ IR Edge   │──→│Safety    │──→ Emergency Stop             │
│  └───────────┘   └──────────┘                               │
└─────────────────────────────────────────────────────────────┘
```

## Motor Control API

```c
// 运动命令
typedef struct {
    float linear_velocity;      // m/s (-0.3 to 0.3)
    float angular_velocity;     // rad/s (-π to π)
    uint32_t duration_ms;       // 0 = infinite
    bool relative;              // true = 相对当前位置
} motion_command_t;

// 运动管理器初始化
esp_err_t motion_manager_init(void);

// 执行运动命令
esp_err_t motion_execute(const motion_command_t *cmd);

// 紧急停止
esp_err_t motion_emergency_stop(void);

// 获取当前状态
typedef struct {
    float linear_velocity;
    float angular_velocity;
    float odom_x, odom_y, odom_yaw;  // 里程计位置
    bool is_moving;
    bool edge_detected;
    bool collision_detected;
} motion_state_t;

esp_err_t motion_get_state(motion_state_t *state);
```

## PID Controller

```c
// PID 参数（需实验调参）
typedef struct {
    float kp;      // 比例
    float ki;      // 积分
    float kd;      // 微分
    float integral_limit;
    float output_limit;
} pid_config_t;

// 默认 PID 参数 (N20 + DRV8833)
// 需要实际测试调整
static const pid_config_t DEFAULT_PID = {
    .kp = 1.5f,
    .ki = 0.3f,
    .kd = 0.1f,
    .integral_limit = 100.0f,
    .output_limit = 255.0f,    // 8-bit PWM
};

// PID 更新 (每个控制周期调用一次)
float pid_update(pid_state_t *pid, float setpoint, float measurement, float dt);

// 双轮速度 → PWM 占空比转换
void motion_wheel_speed_to_pwm(float left_speed, float right_speed,
                               uint8_t *left_pwm, uint8_t *right_pwm,
                               bool *left_dir, bool *right_dir);
```

## PWM Configuration

```c
// DRV8833 控制引脚
// AIN1 + AIN2 → Left motor
// BIN1 + BIN2 → Right motor
// PWM 频率: 10kHz (N20 减速电机推荐范围 1-50kHz)

#define MOTOR_PWM_FREQ_HZ       10000
#define MOTOR_PWM_RESOLUTION    LEDC_TIMER_8_BIT  // 0-255
#define MOTOR_PWM_TIMER         LEDC_TIMER_0

// 左右电机占空比映射
// forward:  IN1=PWM, IN2=0
// backward: IN1=0, IN2=PWM
// brake:    IN1=1, IN2=1 (coast: IN1=0, IN2=0)
```

## Desktop Safety

```c
// 边缘检测 (ITR20001 红外对管 ×2)
typedef enum {
    EDGE_SAFE,         // 桌面安全区域
    EDGE_WARNING,      // 接近边缘 (减速)
    EDGE_DETECTED,     // 检测到边缘 (紧急停止)
} edge_state_t;

// 碰撞检测 (TCRT5000 红外 ×2)
typedef enum {
    OBSTACLE_CLEAR,
    OBSTACLE_NEAR,     // 障碍物靠近 (减速)
    OBSTACLE_CLOSE,    // 障碍物很近 (停止)
} obstacle_state_t;

// 安全回调
typedef void (*motion_safety_callback_t)(const char *reason);
void motion_set_safety_callback(motion_safety_callback_t cb);
```

## Movement Behaviors (编程场景)

```c
// 对应编程场景的预定义运动

// 编译中 → 原地微动 + 转圈
void motion_building_animation(void);

// 编译成功 → 原地旋转一圈庆祝
void motion_build_success_celebration(void);

// 编译失败 → 后退一步 + 屏幕下移
void motion_build_failure_retreat(void);

// 收到新消息 → 前进一小步
void motion_notification_forward(void);

// 番茄钟提醒 → 移动到用户面前 + 摇摆
void motion_pomodoro_alert(void);

// 空闲 → 随机漫步（安全区域内）
void motion_idle_patrol(void);
```

## Performance Targets

| 指标 | 目标 |
|------|------|
| 控制周期 | 10ms (100Hz) |
| 速度范围 | 0.05 ~ 0.30 m/s |
| 旋转速度 | 0.2 ~ 3.14 rad/s |
| 速度精度 | ±5% (带编码器) |
| 边缘检测延迟 | < 5ms |
| 紧急制动距离 | < 5cm |
| PWM 分辨率 | 8-bit (0-255) |

## Rules

1. 运动前必须检查边缘传感器（桌面防跌落）
2. 速度限制 ≤ 0.3 m/s（桌面安全）
3. 紧急停止优先级最高（ISR 中直接操作 GPIO）
4. PID 参数需实际调参，文档值仅为初始值
5. IMU 异常（自由落体检测）→ 立即停止所有电机
6. 电机堵转检测（电流/编码器反馈）→ 停止 + 报警
7. 运动任务优先级中等 (3)，不影响音频和显示

## Checklist

- [ ] 双路 PWM 正常输出（示波器确认占空比）
- [ ] 前进/后退/左转/右转/原地旋转 功能正常
- [ ] 边缘检测灵敏可靠（不同桌面颜色测试）
- [ ] 紧急停止响应 < 10ms
- [ ] PID 调参完成：无震荡、无静差
- [ ] IMU 姿态数据正确（静止时 pitch/roll ≈ 0）
- [ ] 里程计精度可接受（>5cm 累计误差需修正）
- [ ] 堵转保护生效
- [ ] 电池低电量时自动限速
