# Sensor Fusion Skill — RobotBuddy

## Role

RobotBuddy 传感器融合专家，负责多传感器数据采集、滤波融合、边缘/障碍检测和安全决策。

## Domain

MPU6050 IMU（6轴）、TCRT5000 红外避障（×2）、ITR20001 红外边缘检测（×2）、VL53L0X TOF（可选）、ADC 电量检测、传感器数据滤波与融合。

## Goal

构建可靠的传感器融合系统，为运动安全、姿态检测和行为决策提供准确的感知数据。

## Inputs

- 传感器硬件规格和驱动接口
- 运动控制安全需求（边缘检测、碰撞回避）
- IMU 数据精度需求（姿态角、跌落检测）

## Outputs

- `firmware/services/sensor/sensor_manager.c` — 传感器管理器
- `firmware/services/sensor/imu_processor.c` — IMU 数据处理
- `firmware/services/sensor/edge_detector.c` — 边缘检测逻辑
- `firmware/services/sensor/obstacle_detector.c` — 障碍物检测逻辑
- `firmware/services/sensor/sensor_fusion.c` — 传感器融合算法
- `docs/architecture/sensor-fusion.md` — 传感器融合文档

## Sensor Fusion Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                     Sensor Fusion System                      │
│                                                               │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐       │
│  │ MPU6050  │ │ TCRT5000 │ │ITR20001  │ │ VL53L0X  │       │
│  │ IMU 6轴  │ │ IR ×2    │ │ IR Edge  │ │ TOF (opt)│       │
│  │ I2C 100Hz│ │ ADC/GPIO │ │ GPIO ×2  │ │ I2C 20Hz │       │
│  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘       │
│       │            │            │            │               │
│  ┌────↓────────────↓────────────↓────────────↓──────────┐   │
│  │                  Sensor Manager                        │   │
│  │               (统一采样 + 数据分发)                      │   │
│  └────┬────────────┬────────────┬───────────────────────┘   │
│       │            │            │                            │
│  ┌────↓─────┐ ┌────↓─────┐ ┌────↓──────┐                  │
│  │ IMU      │ │ Edge     │ │ Obstacle  │                   │
│  │ Processor│ │ Detector │ │ Detector  │                   │
│  │ 姿态/跌落│ │ 桌面边缘 │ │ 前方障碍  │                    │
│  └────┬─────┘ └────┬─────┘ └────┬──────┘                   │
│       │            │            │                            │
│  ┌────↓────────────↓────────────↓───────────────────────┐   │
│  │              Sensor Fusion (决策层)                     │   │
│  │                                                        │   │
│  │  输出:                                                 │   │
│  │  ├── safety_state_t (SAFE/WARNING/DANGER)              │   │
│  │  ├── attitude_t (pitch/roll/yaw)                       │   │
│  │  ├── fall_detected (bool)                              │   │
│  │  └── environment_map (简单桌面地图)                     │   │
│  └────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
```

## IMU Processor

```c
// IMU 数据结构
typedef struct {
    // 原始数据
    float accel[3];         // 加速度 (m/s²) [x, y, z]
    float gyro[3];          // 角速度 (rad/s) [x, y, z]

    // 融合后姿态
    float pitch;            // 俯仰角 (rad)
    float roll;             // 横滚角 (rad)
    float yaw;              // 航向角 (rad) — 需磁力计或陀螺仪积分

    // 衍生数据
    bool is_falling;        // 自由落体检测
    bool is_tilted;         // 倾斜检测 (>30°)
    float linear_accel;     // 线性加速度模 (减去重力)
} imu_data_t;

// IMU 处理 API
esp_err_t imu_processor_init(void);
esp_err_t imu_processor_update(void);   // 每 10ms 调用一次
esp_err_t imu_get_data(imu_data_t *data);

// 姿态融合算法：互补滤波（轻量，适合 ESP32）
typedef struct {
    float alpha;            // 互补滤波系数 (0.98 推荐)
    float gyro_bias[3];     // 陀螺仪零偏
    bool calibrated;        // 是否已完成校准
} complementary_filter_t;

// 校准流程
esp_err_t imu_calibrate(void);
// 静止放置 2s → 采样 200 次取均值作为零偏
```

## Edge Detector

```c
// 边缘检测状态
typedef enum {
    EDGE_SAFE,              // 安全 — 桌面颜色/反射正常
    EDGE_WARNING,           // 警告 — 接近边缘（反射变化）
    EDGE_DETECTED,          // 检测到边缘 — 立即停止
} edge_state_t;

// 边缘检测数据
typedef struct {
    edge_state_t front_left;    // 左前边缘状态
    edge_state_t front_right;   // 右前边缘状态
    uint16_t raw_left;          // 左传感器原始 ADC 值
    uint16_t raw_right;         // 右传感器原始 ADC 值
    bool any_edge;              // 任意边缘检测到
} edge_data_t;

// 边缘检测 API
esp_err_t edge_detector_init(void);
esp_err_t edge_detector_update(void);   // 每 20ms 调用
esp_err_t edge_get_data(edge_data_t *data);

// 阈值校准
esp_err_t edge_calibrate_desktop(void);
// 将机器人放在桌面中心 → 采样 100 次取均值作为基线
// 检测阈值 = 基线 ± 30%（适应不同桌面颜色/材质）
```

## Obstacle Detector

```c
// 障碍物检测状态
typedef enum {
    OBSTACLE_CLEAR,         // 无障碍物
    OBSTACLE_NEAR,          // 障碍物靠近 (< 15cm)
    OBSTACLE_CLOSE,         // 障碍物很近 (< 5cm) — 停止
} obstacle_state_t;

typedef struct {
    obstacle_state_t front_left;
    obstacle_state_t front_right;
    float distance_cm_left;     // 左侧距离 (cm)，TOF 时有效
    float distance_cm_right;    // 右侧距离 (cm)
    bool any_obstacle;
} obstacle_data_t;

// 障碍检测 API
esp_err_t obstacle_detector_init(void);
esp_err_t obstacle_detector_update(void);
esp_err_t obstacle_get_data(obstacle_data_t *data);
```

## Safety Decision (Fusion Output)

```c
// 综合安全状态
typedef enum {
    SAFETY_SAFE,            // 安全，可正常运动
    SAFETY_CAUTION,         // 注意，限速运动
    SAFETY_DANGER,          // 危险，停止运动
    SAFETY_EMERGENCY,       // 紧急，跌落/碰撞中
} safety_state_t;

typedef struct {
    safety_state_t state;

    // 综合感知数据
    edge_data_t edge;
    obstacle_data_t obstacle;
    imu_data_t imu;

    // 安全决策依据
    const char *danger_reason;   // "edge_detected" / "obstacle_close" / "falling"

    // 运动限制建议
    float max_linear_speed;      // 建议最大线速度 (m/s)
    float max_angular_speed;     // 建议最大角速度 (rad/s)
    bool allow_forward;          // 是否允许前进
    bool allow_backward;         // 是否允许后退
} safety_report_t;

// 安全决策 API
esp_err_t sensor_fusion_init(void);
esp_err_t sensor_fusion_update(void);     // 融合所有传感器，输出安全报告
esp_err_t sensor_fusion_get_report(safety_report_t *report);

// 紧急停止回调（在 ISR 或高优先级 Task 中调用）
typedef void (*emergency_stop_cb_t)(const char *reason);
esp_err_t sensor_fusion_set_emergency_callback(emergency_stop_cb_t cb);
```

## Sensor Sampling Schedule

```c
// 各传感器采样率和优先级
//
// 传感器          采样率    优先级   采样方式
// ─────────────────────────────────────────────
// ITR20001 边缘   200Hz     最高    GPIO 中断 + 50ms 轮询
// TCRT5000 避障   20Hz      高     ADC 轮询
// MPU6050 IMU     100Hz     中     I2C + DMA
// VL53L0X TOF     20Hz      中     I2C 轮询
// 电池 ADC        0.03Hz    低     ADC 定时采样
//
// sensor_poll Task (优先级 2, 周期 10ms):
//   每 10ms: 读 MPU6050 (I2C DMA)
//   每 20ms: 读 TCRT5000 + ITR20001
//   每 50ms: 读 VL53L0X (如启用)
//   每 30s:  读电池 ADC
```

## Sensor Calibration

```c
// 校准数据存储 (NVS)
typedef struct {
    // MPU6050 校准
    float gyro_bias[3];         // 陀螺仪零偏
    float accel_bias[3];        // 加速度计零偏
    float accel_scale[3];       // 加速度计缩放

    // 边缘检测校准
    uint16_t edge_baseline_left;    // 左边缘基线值
    uint16_t edge_baseline_right;   // 右边缘基线值
    uint16_t edge_threshold;        // 边缘判定阈值

    // 校准时间戳
    uint32_t calibration_time;      // 上次校准时间
    bool is_calibrated;
} sensor_calibration_t;

esp_err_t sensor_save_calibration(const sensor_calibration_t *cal);
esp_err_t sensor_load_calibration(sensor_calibration_t *cal);
esp_err_t sensor_run_full_calibration(void);
```

## Rules

1. **安全优先** — 边缘/碰撞检测是运动控制的前置条件，不可绕过
2. **超时保护** — 所有 I2C 通信设置 100ms 超时，防止总线挂死
3. **数据有效性** — IMU 数据需经过范围检查和合理性过滤
4. **边缘检测去抖** — 连续 3 次检测到边缘才判定为 EDGE_DETECTED
5. **跌落检测** — 加速度模 < 0.3g 持续 50ms → 判定为自由落体
6. **传感器故障** — 连续 10 次读取失败 → 标记传感器为 FAULT，上报行为系统
7. **校准数据持久化** — 校准结果保存到 NVS，开机自动加载
8. **低功耗采样** — 休眠模式下停止所有传感器采样

## Checklist

- [ ] MPU6050 加速度和角速度读数合理（静止时 ~9.8 m/s², ~0 rad/s）
- [ ] 姿态融合输出平滑无跳变
- [ ] 跌落检测响应 < 100ms
- [ ] 边缘检测在不同桌面颜色/材质下可靠
- [ ] 障碍物检测灵敏度可调
- [ ] 安全状态判定正确（SAFE → CAUTION → DANGER → EMERGENCY）
- [ ] 紧急停止回调能中断运动
- [ ] 传感器故障不影响其他传感器工作
- [ ] 校准数据 NVS 存储和加载正确
- [ ] 长时间运行（24h）IMU 无累积漂移（定期零偏校正）
