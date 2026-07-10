# Sensor Calibration Skill — RobotBuddy

## Role

RobotBuddy 传感器校准专家，负责所有板载传感器的校准流程设计、校准数据管理、精度验证和自动补偿。

## Domain

MPU6050 IMU 校准（陀螺仪零偏、加速度计零偏、6 点校准、温度补偿）、ITR20001 红外边缘检测校准（桌面基线、自适应阈值、多表面适配）、TCRT5000 红外避障校准、电池 ADC 校准（2 点校准法、分压比系数）、VL53L0X TOF 偏移校准。

## Goal

确保所有传感器数据准确可靠，校准过程标准化、可复现，校准结果持久化到 NVS 并支持版本管理和工厂重置。

## Inputs

- 传感器硬件规格和电气参数（MPU6050、ITR20001、TCRT5000、VL53L0X、电池 ADC 分压电路）
- 各传感器原始数据接口（I2C / ADC / GPIO）
- NVS 存储分区配置
- 校准环境要求（水平桌面、稳定电源、室温 25±5°C）

## Outputs

- `firmware/system/calibration/calibration_manager.c` — 校准管理器（统一入口、状态机、版本管理）
- `firmware/system/calibration/imu_calibrator.c` — IMU 校准例程
- `firmware/system/calibration/edge_calibrator.c` — 边缘检测校准例程
- `firmware/system/calibration/battery_calibrator.c` — 电池 ADC 校准例程
- `firmware/system/calibration/tof_calibrator.c` — TOF 偏移校准例程
- `firmware/system/calibration/calibration_data.c` — 校准数据 NVS 读写
- `docs/firmware/calibration.md` — 传感器校准文档

## Calibration Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                     Calibration Manager                           │
│              (校准状态机 + 版本管理 + 质量评估)                      │
│                                                                   │
│   ┌─────────────┐   ┌────────────┐   ┌──────────────┐           │
│   │  用户触发    │   │  自动检测   │   │  定期校验    │           │
│   │ (串口/APP)  │   │(温度>10°C) │   │ (30天超时)   │           │
│   └──────┬──────┘   └─────┬──────┘   └──────┬───────┘           │
│          │                │                  │                    │
│   ┌──────↓────────────────↓──────────────────↓───────────────┐   │
│   │               Calibration State Machine                    │   │
│   │  IDLE → PREPARING → SAMPLING → ANALYZING → VERIFYING → DONE│   │
│   │           ↓                     ↓                          │   │
│   │        FAILED ←────────── RETRY_3_TIMES                    │   │
│   └──────────────────────┬────────────────────────────────────┘   │
│                          │                                        │
│   ┌──────────────────────↓──────────────────────────────────┐    │
│   │               Per-Sensor Calibrators                      │    │
│   │                                                           │    │
│   │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐    │    │
│   │  │ IMU      │ │ Edge     │ │ Battery  │ │ TOF      │    │    │
│   │  │ Calibrator│ │Calibrator│ │Calibrator│ │Calibrator│    │    │
│   │  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘    │    │
│   │       │            │            │            │            │    │
│   │  ┌────↓────────────↓────────────↓────────────↓────────┐  │    │
│   │  │            Calibration Data Layer                    │  │    │
│   │  │   NVS Read/Write | Version Mgmt | Factory Reset     │  │    │
│   │  └──────────────────────┬──────────────────────────────┘  │    │
│   │                         │                                  │    │
│   │  ┌──────────────────────↓──────────────────────────────┐  │    │
│   │  │                   NVS Partition                      │  │    │
│   │  │         "nvs_cal" (sensor_calibration_t)             │  │    │
│   │  └─────────────────────────────────────────────────────┘  │    │
│   └───────────────────────────────────────────────────────────┘    │
│                                                                   │
│   ┌──────────────────────────────────────────────────────────┐    │
│   │           Calibration Report → sensor-fusion skill        │    │
│   │    输出 cal 结构体供实时传感器补偿使用                       │    │
│   └──────────────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────────────┘
```

## IMU Calibration (MPU6050)

### Gyro Bias Calibration (静态零偏采样)

```c
// 陀螺仪零偏校准 — 静止状态下采集 200 个样本取均值
#define GYRO_CAL_SAMPLES        200       // 采样数量
#define GYRO_CAL_SAMPLE_DELAY_MS  5       // 采样间隔 5ms
#define GYRO_CAL_TOTAL_TIME_MS    1000    // 总校准时间 ≈ 1s
#define GYRO_CAL_MAX_STDDEV       0.01f   // 最大允许标准差 (rad/s), 超过说明有震动

typedef struct {
    float bias[3];              // 零偏值 (rad/s) [x, y, z]
    float stddev[3];            // 采样标准差 [x, y, z]
    uint16_t sample_count;      // 有效采样数
    bool is_valid;              // 校准是否有效
} gyro_cal_result_t;

esp_err_t imu_calibrate_gyro_bias(gyro_cal_result_t *result)
{
    // 前提条件: 机器人静止放置在水平面, 状态机进入 SAMPLING
    float sum[3] = {0};
    float sum_sq[3] = {0};
    float samples[GYRO_CAL_SAMPLES][3];
    uint16_t valid_count = 0;

    // 丢弃前 20 个样本（传感器稳定）
    for (int i = 0; i < 20; i++) {
        imu_read_gyro_raw(&raw);
        vTaskDelay(pdMS_TO_TICKS(GYRO_CAL_SAMPLE_DELAY_MS));
    }

    // 采集 200 个样本
    for (int i = 0; i < GYRO_CAL_SAMPLES; i++) {
        imu_read_gyro_raw(&raw);

        // 异常值过滤: 超过 3σ 丢弃
        if (i > 0 && fabsf(raw.gyro_x - sum[0] / valid_count) > 3.0f * sqrtf(sum_sq[0] / valid_count)) {
            continue;   // 跳过振动尖峰
        }

        samples[valid_count][0] = raw.gyro_x;
        samples[valid_count][1] = raw.gyro_y;
        samples[valid_count][2] = raw.gyro_z;
        sum[0] += raw.gyro_x;
        sum[1] += raw.gyro_y;
        sum[2] += raw.gyro_z;
        sum_sq[0] += raw.gyro_x * raw.gyro_x;
        sum_sq[1] += raw.gyro_y * raw.gyro_y;
        sum_sq[2] += raw.gyro_z * raw.gyro_z;
        valid_count++;

        vTaskDelay(pdMS_TO_TICKS(GYRO_CAL_SAMPLE_DELAY_MS));
    }

    // 计算均值和标准差
    for (int axis = 0; axis < 3; axis++) {
        result->bias[axis] = sum[axis] / valid_count;
        result->stddev[axis] = sqrtf(sum_sq[axis] / valid_count -
                                     result->bias[axis] * result->bias[axis]);
    }
    result->sample_count = valid_count;
    result->is_valid = (result->stddev[0] < GYRO_CAL_MAX_STDDEV &&
                        result->stddev[1] < GYRO_CAL_MAX_STDDEV &&
                        result->stddev[2] < GYRO_CAL_MAX_STDDEV);

    return result->is_valid ? ESP_OK : ESP_ERR_INVALID_STATE;
}
```

### Accel 6-Point Calibration (加速度计 6 点校准)

```c
// 加速度计 6 点校准: ±X, ±Y, ±Z 六个方向分别测量
// 重力加速度 g = 9.80665 m/s² 作为参考
typedef enum {
    ACCEL_AXIS_PLUS_X,      // 机器人右侧朝下
    ACCEL_AXIS_MINUS_X,     // 机器人左侧朝下
    ACCEL_AXIS_PLUS_Y,      // 机器人前侧朝下
    ACCEL_AXIS_MINUS_Y,     // 机器人后侧朝下
    ACCEL_AXIS_PLUS_Z,      // 机器人正放
    ACCEL_AXIS_MINUS_Z,     // 机器人倒置
    ACCEL_POINTS_MAX = 6,
} accel_cal_axis_t;

typedef struct {
    float bias[3];              // 零偏 (m/s²)
    float scale[3];             // 缩放因子 (理想值 1.0)
    float cross_axis[3][3];     // 交叉轴灵敏度矩阵
    float residual[6];          // 各点残差 (m/s²)
    bool is_valid;
} accel_cal_result_t;

// 6 点校准流程:
// 1. 引导用户依次将机器人按 6 个方向放置（屏幕显示方向提示）
// 2. 每个方向等待稳定后采集 100 个样本取均值
// 3. 最小二乘法拟合: Y = scale * X + bias
// 4. 理想测量值: [(+g,0,0), (-g,0,0), (0,+g,0), (0,-g,0), (0,0,+g), (0,0,-g)]

esp_err_t imu_calibrate_accel_6point(accel_cal_result_t *result)
{
    float measured[6][3];   // 6 个方向的实测均值
    float ideal[6][3] = {   // 6 个方向的理想值
        { G,  0,  0},       // +X 朝下 → X 轴测到 +1g
        {-G,  0,  0},       // -X 朝下 → X 轴测到 -1g
        { 0,  G,  0},       // +Y 朝下
        { 0, -G,  0},       // -Y 朝下
        { 0,  0,  G},       // +Z 朝下
        { 0,  0, -G},       // -Z 朝下
    };

    for (int point = 0; point < 6; point++) {
        // 用户放置提示
        calibration_ui_show_axis_prompt(point);

        // 等待用户确认放置完成
        if (calibration_wait_user_confirm(30000) != ESP_OK) {
            return ESP_ERR_TIMEOUT;
        }

        // 稳定检测: 连续 50ms 内加速度变化 < 0.005g
        if (calibration_wait_stable(0.005f * G, 200) != ESP_OK) {
            return ESP_ERR_INVALID_STATE;
        }

        // 采集 100 个样本取均值
        float sum[3] = {0};
        for (int i = 0; i < 100; i++) {
            imu_read_accel_raw(&raw);
            sum[0] += raw.accel_x;
            sum[1] += raw.accel_y;
            sum[2] += raw.accel_z;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        measured[point][0] = sum[0] / 100.0f;
        measured[point][1] = sum[1] / 100.0f;
        measured[point][2] = sum[2] / 100.0f;
    }

    // 最小二乘拟合每个轴: measured = scale * ideal + bias
    for (int axis = 0; axis < 3; axis++) {
        float sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
        for (int p = 0; p < 6; p++) {
            sum_x += ideal[p][axis];
            sum_y += measured[p][axis];
            sum_xy += ideal[p][axis] * measured[p][axis];
            sum_xx += ideal[p][axis] * ideal[p][axis];
        }
        result->scale[axis] = (6 * sum_xy - sum_x * sum_y) /
                              (6 * sum_xx - sum_x * sum_x);
        result->bias[axis] = (sum_y - result->scale[axis] * sum_x) / 6;
    }

    // 计算各点残差
    float max_residual = 0;
    for (int p = 0; p < 6; p++) {
        float dx = measured[p][0] - (result->scale[0] * ideal[p][0] + result->bias[0]);
        float dy = measured[p][1] - (result->scale[1] * ideal[p][1] + result->bias[1]);
        float dz = measured[p][2] - (result->scale[2] * ideal[p][2] + result->bias[2]);
        result->residual[p] = sqrtf(dx*dx + dy*dy + dz*dz);
        if (result->residual[p] > max_residual) max_residual = result->residual[p];
    }

    result->is_valid = (max_residual < 0.05f * G);  // 最大残差 < 0.05g
    return result->is_valid ? ESP_OK : ESP_ERR_INVALID_STATE;
}
```

### Temperature Compensation (温度补偿)

```c
// MPU6050 内置温度传感器, 零偏随温度漂移约 0.005~0.02 dps/°C
// 记录多温度点零偏, 建立线性补偿模型
#define TEMP_COMP_POINTS    5   // 5 个温度采样点
#define TEMP_RANGE_MIN_C    15
#define TEMP_RANGE_MAX_C    45

typedef struct {
    float temp_c;               // 温度 (°C)
    float gyro_bias[3];         // 该温度下的陀螺仪零偏
} temp_comp_point_t;

typedef struct {
    float slope[3];             // 零偏温度漂移率 (dps/°C)
    float intercept[3];         // 基准零偏 (0°C)
    temp_comp_point_t points[TEMP_COMP_POINTS];
    uint8_t point_count;
    bool is_valid;
} temp_compensation_t;

// 使用: gyro_corrected = gyro_raw - (temp * slope + intercept)
esp_err_t imu_build_temp_compensation(temp_compensation_t *comp);
```

## Edge Detection Calibration (ITR20001)

### Desktop Baseline Sampling (桌面基线采集)

```c
// 边缘检测校准 — 将机器人放置在桌面中央, 采集基线反射值
#define EDGE_CAL_SAMPLES        100     // 采样数量
#define EDGE_CAL_SAMPLE_DELAY_MS  10    // 采样间隔 10ms
#define EDGE_CAL_STABLE_STDDEV   30     // ADC 稳定阈值 (标准差 < 30)

typedef struct {
    uint16_t baseline_left;         // 左侧基线 ADC 值
    uint16_t baseline_right;        // 右侧基线 ADC 值
    uint16_t threshold_low_left;    // 左下限阈值 (baseline - 30%)
    uint16_t threshold_high_left;   // 左上限阈值 (baseline + 30%)
    uint16_t threshold_low_right;   // 右下限阈值
    uint16_t threshold_high_right;  // 右上限阈值
    uint16_t stddev_left;           // 左传感器标准差
    uint16_t stddev_right;          // 右传感器标准差
    bool is_valid;
} edge_cal_result_t;

esp_err_t edge_calibrate_desktop(edge_cal_result_t *result)
{
    // 前提: 机器人放置在桌面中央, 两个 ITR20001 都在桌面范围内
    calibration_ui_show_message("请将机器人放在桌面中央, 然后确认");

    if (calibration_wait_user_confirm(30000) != ESP_OK) {
        return ESP_ERR_TIMEOUT;
    }

    // 稳定检测: 用户确认后等待 500ms 消除手部晃动
    vTaskDelay(pdMS_TO_TICKS(500));

    // 采集 100 个样本
    uint32_t sum_left = 0, sum_right = 0;
    uint64_t sum_sq_left = 0, sum_sq_right = 0;
    uint16_t sample_count = 0;

    for (int i = 0; i < EDGE_CAL_SAMPLES; i++) {
        uint16_t val_left = itr20001_read_raw(ITR20001_LEFT);
        uint16_t val_right = itr20001_read_raw(ITR20001_RIGHT);

        // 异常值过滤: 与当前均值偏差超过 3σ 丢弃
        if (i > 5) {
            float mean_left = (float)sum_left / sample_count;
            float std_left = sqrtf((float)sum_sq_left / sample_count - mean_left * mean_left);
            if (fabsf((float)val_left - mean_left) > 3.0f * std_left) continue;

            float mean_right = (float)sum_right / sample_count;
            float std_right = sqrtf((float)sum_sq_right / sample_count - mean_right * mean_right);
            if (fabsf((float)val_right - mean_right) > 3.0f * std_right) continue;
        }

        sum_left += val_left;
        sum_right += val_right;
        sum_sq_left += (uint64_t)val_left * val_left;
        sum_sq_right += (uint64_t)val_right * val_right;
        sample_count++;

        vTaskDelay(pdMS_TO_TICKS(EDGE_CAL_SAMPLE_DELAY_MS));
    }

    // 计算统计量
    result->baseline_left = (uint16_t)(sum_left / sample_count);
    result->baseline_right = (uint16_t)(sum_right / sample_count);
    result->stddev_left = (uint16_t)sqrtf(
        (float)sum_sq_left / sample_count - result->baseline_left * result->baseline_left);
    result->stddev_right = (uint16_t)sqrtf(
        (float)sum_sq_right / sample_count - result->baseline_right * result->baseline_right);

    // 自适应阈值: baseline ± 30%
    result->threshold_low_left  = (uint16_t)(result->baseline_left * 0.70f);
    result->threshold_high_left = (uint16_t)(result->baseline_left * 1.30f);
    result->threshold_low_right  = (uint16_t)(result->baseline_right * 0.70f);
    result->threshold_high_right = (uint16_t)(result->baseline_right * 1.30f);

    // 有效性判断: 标准差 < EDGE_CAL_STABLE_STDDEV
    result->is_valid = (result->stddev_left < EDGE_CAL_STABLE_STDDEV &&
                        result->stddev_right < EDGE_CAL_STABLE_STDDEV);

    return result->is_valid ? ESP_OK : ESP_ERR_INVALID_STATE;
}
```

### Multi-Surface Calibration (多表面适配)

```c
// 多表面校准 — 支持在不同桌面材质间切换时自动调整阈值
typedef enum {
    SURFACE_WOOD_LIGHT,     // 浅色木桌
    SURFACE_WOOD_DARK,      // 深色木桌
    SURFACE_GLASS,          // 玻璃桌面 (高风险: 红外穿透玻璃)
    SURFACE_METAL,          // 金属桌面 (高反射)
    SURFACE_PLASTIC,        // 塑料桌面
    SURFACE_USER_CUSTOM,    // 用户自定义
    SURFACE_MAX = 6,
} surface_type_t;

typedef struct {
    surface_type_t type;
    uint16_t baseline_left;
    uint16_t baseline_right;
    uint16_t threshold_low_left;
    uint16_t threshold_high_left;
    uint16_t threshold_low_right;
    uint16_t threshold_high_right;
} surface_profile_t;

// 存储最多 3 个表面配置文件到 NVS
#define MAX_SURFACE_PROFILES    3

esp_err_t edge_save_surface_profile(surface_type_t type, const edge_cal_result_t *cal);
esp_err_t edge_load_surface_profile(surface_type_t type, edge_cal_result_t *cal);
esp_err_t edge_auto_detect_surface(void);  // 开机自检: 采样并匹配已知表面

// 特别注意: 玻璃桌面警告 — ITR20001 红外可能穿透玻璃,
// 导致边缘检测失效。需在玻璃模式下降级为仅依赖 IMU 倾斜检测。
```

## Battery ADC Calibration

### 2-Point Calibration (两点校准)

```c
// 电池 ADC 两点校准法 — 使用已知电压源确定 ADC 转换系数
// 硬件: 电池 → 2:1 电阻分压 → ESP32-S3 ADC1
// 理论: V_bat = ADC_raw / 4095 * 3.3V * (R1+R2)/R2 * cal_coefficient
// 其中 (R1+R2)/R2 = 3 (2:1 分压比的反比)

typedef struct {
    float cal_coefficient;      // 校准系数 (理论值 1.0)
    float voltage_divider_ratio;// 分压比 (实际测量值, 理论值 3.0)
    float adc_vref_mv;          // ADC 参考电压实测值 (mV, 理论值 3300)
    uint16_t raw_at_3v0;        // 3.0V 输入时的 ADC 读数
    uint16_t raw_at_4v2;        // 4.2V 输入时的 ADC 读数
    bool is_valid;
} battery_cal_result_t;

// 两点校准流程:
// 步骤 1: 使用可调电源输入 3.0V 到电池端 → 记录 ADC 读数
// 步骤 2: 使用可调电源输入 4.2V 到电池端 → 记录 ADC 读数
// 步骤 3: 拟合直线: V_bat = (ADC - offset) * slope

esp_err_t battery_calibrate_2point(battery_cal_result_t *result)
{
    // 引导用户:
    // 1. 断开电池
    // 2. 将可调电源接到电池端子, 设置 3.0V
    calibration_ui_show_message("请将可调电源设为 3.00V 接入电池端");

    if (calibration_wait_user_confirm(60000) != ESP_OK) {
        return ESP_ERR_TIMEOUT;
    }

    // 16 次采样取中值 (避免纹波干扰)
    result->raw_at_3v0 = battery_adc_read_median(16);
    ESP_LOGI("CAL", "ADC at 3.0V: %d", result->raw_at_3v0);

    // 设置 4.2V
    calibration_ui_show_message("请将可调电源设为 4.20V");

    if (calibration_wait_user_confirm(60000) != ESP_OK) {
        return ESP_ERR_TIMEOUT;
    }

    result->raw_at_4v2 = battery_adc_read_median(16);
    ESP_LOGI("CAL", "ADC at 4.2V: %d", result->raw_at_4v2);

    // 线性拟合: V = (ADC - offset) * slope
    float slope = (4200.0f - 3000.0f) / (result->raw_at_4v2 - result->raw_at_3v0);
    float offset = result->raw_at_3v0 - 3000.0f / slope;

    // 理论值: slope_theory = (R1+R2)/R2 * Vref / 4095 = 3.0 * 3.3 / 4095 ≈ 0.00242
    float slope_theory = 3.0f * 3.3f / 4095.0f;
    result->cal_coefficient = slope / slope_theory;
    result->voltage_divider_ratio = 3.0f * result->cal_coefficient;
    result->adc_vref_mv = 3300.0f * result->cal_coefficient;

    result->is_valid = (result->cal_coefficient > 0.8f &&
                        result->cal_coefficient < 1.2f);

    // 恢复电池连接提示
    calibration_ui_show_message("校准完成, 请重新连接电池");

    return result->is_valid ? ESP_OK : ESP_ERR_INVALID_STATE;
}

// 16 样本中值滤波器
static uint16_t battery_adc_read_median(uint8_t num_samples)
{
    uint16_t samples[16];
    for (int i = 0; i < num_samples; i++) {
        samples[i] = adc1_get_raw(ADC_CHANNEL_BATTERY);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    // 排序取中值
    for (int i = 0; i < num_samples - 1; i++) {
        for (int j = i + 1; j < num_samples; j++) {
            if (samples[i] > samples[j]) {
                uint16_t tmp = samples[i];
                samples[i] = samples[j];
                samples[j] = tmp;
            }
        }
    }
    return samples[num_samples / 2];
}
```

### Discharge Curve Lookup Table (放电曲线查找表)

```c
// 校准后的放电曲线查找表 (18650 典型 + 校准系数修正)
typedef struct {
    uint16_t voltage_mv;        // 电压 (mV)
    uint16_t adc_raw;           // 对应 ADC 读数
    uint8_t percentage;         // 电量百分比
} discharge_point_t;

// 放电曲线表 (12 点, 线性插值)
// 校准后根据 cal_coefficient 重新计算 adc_raw 列
static const discharge_point_t DISCHARGE_CURVE[] = {
    {4200, 0, 100},  // 满电
    {4100, 0,  90},
    {4000, 0,  80},
    {3900, 0,  70},
    {3800, 0,  60},
    {3700, 0,  50},
    {3600, 0,  40},
    {3500, 0,  30},
    {3400, 0,  20},
    {3300, 0,  10},
    {3200, 0,   5},
    {3000, 0,   0},  // 截止
};

// ADC 读数 → 电压 (mV), 考虑校准系数
static inline uint16_t battery_adc_to_mv(uint16_t adc, const battery_cal_result_t *cal)
{
    // V_bat = ADC * 3.3V / 4095 * voltage_divider_ratio
    float v = (float)adc * cal->adc_vref_mv / 4095.0f * cal->voltage_divider_ratio;
    return (uint16_t)(v + 0.5f);
}
```

## VL53L0X TOF Offset Calibration

```c
// VL53L0X 偏移校准 — 在已知距离下测量, 计算系统偏移
typedef struct {
    int16_t offset_mm;          // 距离偏移 (mm)
    float scale_factor;         // 缩放因子 (理想值 1.0)
    uint16_t raw_50mm;          // 50mm 处的原始读数
    uint16_t raw_200mm;         // 200mm 处的原始读数
    bool is_valid;
} tof_cal_result_t;

esp_err_t tof_calibrate_offset(tof_cal_result_t *result)
{
    // 两点校准:
    // 点 1: 在传感器正前方 50mm 处放置白纸 (90% 反射率)
    calibration_ui_show_message("在 TOF 传感器前方 50mm 放置白纸");
    if (calibration_wait_user_confirm(30000) != ESP_OK) return ESP_ERR_TIMEOUT;

    vTaskDelay(pdMS_TO_TICKS(500));  // 稳定
    result->raw_50mm = vl53l0x_read_range_mm_median(10);

    // 点 2: 在传感器正前方 200mm 处放置白纸
    calibration_ui_show_message("在 TOF 传感器前方 200mm 放置白纸");
    if (calibration_wait_user_confirm(30000) != ESP_OK) return ESP_ERR_TIMEOUT;

    vTaskDelay(pdMS_TO_TICKS(500));
    result->raw_200mm = vl53l0x_read_range_mm_median(10);

    // 线性拟合: actual = scale * raw + offset
    result->scale_factor = (200.0f - 50.0f) / (result->raw_200mm - result->raw_50mm);
    result->offset_mm = (int16_t)(50.0f - result->scale_factor * result->raw_50mm);

    result->is_valid = (result->scale_factor > 0.8f && result->scale_factor < 1.2f);

    return result->is_valid ? ESP_OK : ESP_ERR_INVALID_STATE;
}
```

## Calibration Data Structure

### Main Calibration Struct & NVS Persistence

```c
// 校准数据版本 — 修改结构体时递增
#define CALIBRATION_VERSION     1

// 统一校准数据结构
typedef struct {
    // ============ 版本管理 ============
    uint16_t version;                   // 结构体版本号
    uint32_t magic;                     // 魔数 0xCA1B0001 (数据完整性校验)
    uint32_t calibration_time;          // 校准时间戳 (Unix epoch)
    uint32_t flags;                     // 校准标志位 (哪些传感器已校准)

    // ============ IMU 校准数据 ============
    float gyro_bias[3];                 // 陀螺仪零偏 (rad/s)
    float accel_bias[3];                // 加速度计零偏 (m/s²)
    float accel_scale[3];               // 加速度计缩放因子
    float gyro_temp_slope[3];           // 陀螺仪温度漂移率 (dps/°C)
    float gyro_temp_intercept[3];       // 陀螺仪温度漂移截距

    // ============ 边缘检测校准数据 ============
    uint16_t edge_baseline_left;        // 左边缘基线
    uint16_t edge_baseline_right;       // 右边缘基线
    uint16_t edge_threshold_low_left;   // 左阈值下限
    uint16_t edge_threshold_high_left;  // 左阈值上限
    uint16_t edge_threshold_low_right;  // 右阈值下限
    uint16_t edge_threshold_high_right; // 右阈值上限
    surface_type_t edge_surface_type;   // 当前表面类型

    // ============ 电池 ADC 校准数据 ============
    float battery_cal_coefficient;      // 校准系数 (理论值 1.0)
    float battery_divider_ratio;        // 分压比 (实测值)
    float battery_adc_vref_mv;          // ADC 参考电压
    uint16_t battery_raw_at_3v0;        // 3.0V 时 ADC 读数
    uint16_t battery_raw_at_4v2;        // 4.2V 时 ADC 读数

    // ============ TOF 校准数据 ============
    int16_t tof_offset_mm;              // TOF 距离偏移 (mm)
    float tof_scale_factor;             // TOF 缩放因子

    // ============ 校准元数据 ============
    float cal_temperature_c;            // 校准时的温度 (°C)
    float cal_humidity_pct;             // 校准时的湿度 (%)
    char cal_version_str[16];           // 固件版本字符串 (如 "v1.2.3")
} sensor_calibration_t;

#define CAL_MAGIC               0xCA1B0001
#define CAL_FLAG_GYRO           (1 << 0)
#define CAL_FLAG_ACCEL          (1 << 1)
#define CAL_FLAG_ACCEL_6PT      (1 << 2)
#define CAL_FLAG_TEMP_COMP      (1 << 3)
#define CAL_FLAG_EDGE           (1 << 4)
#define CAL_FLAG_BATTERY        (1 << 5)
#define CAL_FLAG_TOF            (1 << 6)
#define CAL_FLAG_ALL            (0x7F)
```

### NVS Read/Write Functions

```c
// NVS 读写 — namespace "nvs_cal", key "cal_data"
#define NVS_CAL_NAMESPACE       "nvs_cal"
#define NVS_CAL_KEY             "cal_data"

esp_err_t calibration_nvs_save(const sensor_calibration_t *cal)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_CAL_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    // 写入魔数和版本 (用于快速校验)
    err = nvs_set_blob(handle, NVS_CAL_KEY, cal, sizeof(sensor_calibration_t));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t calibration_nvs_load(sensor_calibration_t *cal)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_CAL_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    size_t size = sizeof(sensor_calibration_t);
    err = nvs_get_blob(handle, NVS_CAL_KEY, cal, &size);
    nvs_close(handle);

    // 数据完整性校验
    if (err == ESP_OK) {
        if (cal->magic != CAL_MAGIC) {
            ESP_LOGW("CAL", "Calibration data magic mismatch, loading defaults");
            calibration_load_defaults(cal);
            return ESP_ERR_INVALID_CRC;
        }
        if (cal->version != CALIBRATION_VERSION) {
            ESP_LOGW("CAL", "Calibration data version mismatch (stored:%d, current:%d), migrating",
                     cal->version, CALIBRATION_VERSION);
            // 版本迁移逻辑 — 新字段填充默认值
            calibration_migrate_version(cal);
        }
    }
    return err;
}

// 工厂重置 — 清除所有校准数据
esp_err_t calibration_factory_reset(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_CAL_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_erase_all(handle);
    if (err == ESP_OK) {
        nvs_commit(handle);
    }
    nvs_close(handle);

    ESP_LOGI("CAL", "Factory reset: all calibration data erased");
    return err;
}

// 加载默认值 (未校准时使用)
void calibration_load_defaults(sensor_calibration_t *cal)
{
    memset(cal, 0, sizeof(sensor_calibration_t));
    cal->magic = CAL_MAGIC;
    cal->version = CALIBRATION_VERSION;

    // IMU 默认值 (零偏 = 0, 缩放 = 1)
    cal->accel_scale[0] = 1.0f;
    cal->accel_scale[1] = 1.0f;
    cal->accel_scale[2] = 1.0f;

    // 电池默认值
    cal->battery_cal_coefficient = 1.0f;
    cal->battery_divider_ratio = 3.0f;
    cal->battery_adc_vref_mv = 3300.0f;

    // TOF 默认值
    cal->tof_scale_factor = 1.0f;
}
```

### Calibration Manager State Machine

```c
// 校准管理器状态机
typedef enum {
    CAL_STATE_IDLE,             // 空闲
    CAL_STATE_PREPARING,        // 准备中 (提示用户)
    CAL_STATE_SAMPLING_IMU,     // IMU 采样中
    CAL_STATE_SAMPLING_EDGE,    // 边缘检测采样中
    CAL_STATE_SAMPLING_BATTERY, // 电池 ADC 采样中
    CAL_STATE_SAMPLING_TOF,     // TOF 采样中
    CAL_STATE_ANALYZING,        // 数据分析中
    CAL_STATE_VERIFYING,        // 验证中 (重复性检查)
    CAL_STATE_DONE,             // 校准完成
    CAL_STATE_FAILED,           // 校准失败 (>3 次重试)
} cal_state_t;

typedef struct {
    cal_state_t state;
    uint8_t retry_count;
    sensor_calibration_t cal_data;
} calibration_manager_t;

esp_err_t calibration_manager_init(void);
esp_err_t calibration_run_full(calibration_manager_t *mgr);    // 运行全量校准
esp_err_t calibration_run_single(calibration_manager_t *mgr, uint32_t sensor_flag);
cal_state_t calibration_get_state(void);
```

## Calibration Quality Assessment

### Residual Analysis (残差分析)

```c
// 校准质量评估
typedef struct {
    // IMU 质量
    float gyro_residual_max;        // 陀螺仪最大残差 (rad/s)
    float accel_residual_max;       // 加速度计最大残差 (m/s²)
    bool imu_passed;                // IMU 校准通过

    // 边缘检测质量
    float edge_snr_left;            // 左传感器信噪比 (基线/标准差)
    float edge_snr_right;           // 右传感器信噪比
    bool edge_passed;               // 边缘校准通过

    // 电池 ADC 质量
    float battery_linearity_r2;     // 线性拟合 R²
    bool battery_passed;            // 电池校准通过

    // TOF 质量
    float tof_linearity_r2;         // TOF 线性拟合 R²
    bool tof_passed;                // TOF 校准通过

    // 综合
    bool overall_passed;            // 全部通过
} cal_quality_report_t;

// 质量门槛
#define QUALITY_GYRO_RESIDUAL_MAX    0.01f   // 陀螺仪残差 < 0.01 rad/s
#define QUALITY_ACCEL_RESIDUAL_MAX   0.05f   // 加速度计残差 < 0.05g (乘以 G)
#define QUALITY_EDGE_SNR_MIN         3.0f    // 边缘信噪比 > 3
#define QUALITY_LINEARITY_R2_MIN     0.99f   // R² > 0.99

esp_err_t calibration_assess_quality(const sensor_calibration_t *cal,
                                      cal_quality_report_t *report);
```

### Repeatability Verification (可重复性验证)

```c
// 重复性验证: 连续运行 3 次校准, 检查结果一致性
typedef struct {
    sensor_calibration_t runs[3];   // 3 次校准结果
    float gyro_bias_range[3];       // 各轴零偏范围 (max - min)
    float accel_bias_range[3];      // 各轴零偏范围
    float edge_baseline_range[2];   // 基线范围
    bool is_repeatable;             // 是否可重复
} cal_repeatability_t;

#define REPEATABILITY_GYRO_MAX_RANGE   0.005f  // 零偏差异 < 0.005 rad/s
#define REPEATABILITY_ACCEL_MAX_RANGE  0.02f   // 零偏差异 < 0.02g * G

esp_err_t calibration_verify_repeatability(cal_repeatability_t *result);
```

### Calibration Validity Timeout (校准有效期限)

```c
// 校准有效性管理
#define CAL_VALIDITY_DAYS           30      // 建议 30 天后重新校准
#define CAL_TEMP_DRIFT_THRESHOLD_C  10.0f   // 温度变化 > 10°C 建议重校
#define CAL_HUMIDITY_DRIFT_THRESHOLD 30.0f  // 湿度变化 > 30% 建议重校

typedef struct {
    uint32_t last_calibration_time;     // 上次校准时间
    float last_calibration_temp_c;      // 上次校准时温度
    float last_calibration_humidity_pct;// 上次校准时湿度
    bool is_expired;                    // 是否已过期
    bool temp_drifted;                  // 是否温度漂移超限
    const char *recommendation;         // 建议操作
} cal_validity_t;

esp_err_t calibration_check_validity(cal_validity_t *validity)
{
    sensor_calibration_t cal;
    if (calibration_nvs_load(&cal) != ESP_OK) {
        validity->is_expired = true;
        validity->recommendation = "未找到校准数据, 请立即运行校准";
        return ESP_ERR_NOT_FOUND;
    }

    // 时间检查
    time_t now;
    time(&now);
    uint32_t days_elapsed = (now - cal.calibration_time) / 86400;
    validity->is_expired = (days_elapsed > CAL_VALIDITY_DAYS);

    // 温度漂移检查 — 需要当前温度传感器读数
    float current_temp_c = get_current_temperature();
    validity->temp_drifted = (fabsf(current_temp_c - cal.cal_temperature_c) >
                              CAL_TEMP_DRIFT_THRESHOLD_C);

    if (validity->is_expired && validity->temp_drifted) {
        validity->recommendation = "校准已过期且环境温度变化较大, 强烈建议重新校准";
    } else if (validity->is_expired) {
        validity->recommendation = "校准已超过 30 天, 建议重新校准以确保精度";
    } else if (validity->temp_drifted) {
        validity->recommendation = "环境温度变化 > 10°C, 建议重校 IMU 零偏";
    } else {
        validity->recommendation = "校准数据有效";
    }
    return ESP_OK;
}
```

## Calibration → sensor-fusion Interface

```c
// 校准完成后, 将 sensor_calibration_t 传递给 sensor-fusion skill 使用
// sensor-fusion 中的 IMU 处理器使用:
//   gyro_corrected  = gyro_raw    - cal->gyro_bias - temp * cal->gyro_temp_slope
//   accel_corrected = cal->accel_scale * (accel_raw - cal->accel_bias)

// sensor-fusion 中的边缘检测器使用:
//   is_edge = (raw < cal->edge_threshold_low || raw > cal->edge_threshold_high)

// sensor-fusion / power-management 中的电池监控使用:
//   V_bat = ADC_raw * cal->battery_adc_vref_mv / 4095.0f * cal->battery_divider_ratio

// 接口函数
esp_err_t calibration_apply_to_sensors(const sensor_calibration_t *cal);
// 将校准数据应用到各传感器驱动层的补偿参数
```

## Rules

1. **校准前传感器稳定** — 所有校准必须在传感器上电稳定后进行（IMU 预热 ≥ 2s，ADC 稳定 ≥ 100ms），否则校准结果无效
2. **校准数据必须版本化** — 每次修改 sensor_calibration_t 结构体需递增 CALIBRATION_VERSION，加载时自动迁移旧版本数据
3. **校准失败需要明确的反馈** — IMU 校准失败 → 显示皱眉表情 + 语音"校准失败，请重新放置"; 电池校准失败 → 显示低电量表情 + 提示连接可调电源; 全部成功 → 显示笑脸 + 语音"校准完成"
4. **校准中禁止运动** — 校准期间电机锁死，禁止任何运动指令，防止传感器读数受振动干扰
5. **NVS 写入前验证** — 校准数据必须先通过质量评估和重复性验证才能写入 NVS
6. **工厂重置保护** — 工厂重置需用户确认（长按按键 3s 或串口发送确认指令），防止误操作
7. **部分校准支持** — 允许仅校准部分传感器（如仅校准 IMU），未校准部分使用默认值或上次校准值
8. **环境条件记录** — 每次校准自动记录温度、湿度、固件版本，便于追踪校准质量
9. **校准中断恢复** — 校准过程中异常中断（如掉电），下次启动恢复到 IDLE 状态，不写入不完整数据
10. **校准锁** — 同一时间只允许一个校准流程运行，通过互斥锁保护，防止并发校准导致数据损坏

## Checklist

- [ ] MPU6050 陀螺仪零偏校准 — 静止 200 样本标准差 < 0.01 rad/s
- [ ] MPU6050 加速度计 6 点校准 — 各点残差 < 0.05g
- [ ] MPU6050 温度补偿模型 — 5 个温度点覆盖 15~45°C
- [ ] ITR20001 桌面基线采集 — 100 样本标准差 < 30 ADC counts
- [ ] ITR20001 自适应阈值计算 — baseline ± 30% 阈值合理
- [ ] ITR20001 多表面配置文件存储和切换正常
- [ ] TCRT5000 障碍检测阈值校准（参考边缘检测流程）
- [ ] 电池 ADC 2 点校准 — cal_coefficient 在 0.8~1.2 范围内
- [ ] 电池 16 样本中值滤波有效去除纹波
- [ ] 放电曲线查找表校准系数修正正确
- [ ] VL53L0X TOF 偏移校准 — scale_factor 在 0.8~1.2 范围内
- [ ] 校准数据 NVS 写入和读取 (魔数校验、版本匹配)
- [ ] 校准数据结构版本迁移正常 (旧版本 → 新版本)
- [ ] 工厂重置清除 NVS 数据且需确认
- [ ] 校准质量评估 — 残差、信噪比、R² 通过门槛
- [ ] 可重复性验证 — 3 次连续校准零偏差异 < 阈值
- [ ] 校准有效期检查 — 30 天超时 / 温度漂移 > 10°C 提示重校
- [ ] 校准状态机 — IDLE → PREPARING → SAMPLING → ANALYZING → VERIFYING → DONE
- [ ] 校准失败重试 3 次后进入 FAILED 状态并有明确反馈
- [ ] 校准中断恢复 — 掉电重启后回 IDLE 不写入破损数据
- [ ] 校准锁 — 防止并发校准
- [ ] 校准完成 → sensor-fusion skill 正确接收 cal 结构体
- [ ] 未校准传感器使用默认值且系统正常运行
