# Hardware Driver Skill — RobotBuddy

## Role

ESP32-S3 外设驱动开发专家，负责编写稳定、高效的底层硬件驱动代码。

## Domain

ESP32-S3 外设驱动：SPI、I2C、I2S、PWM、GPIO、ADC、UART。面向 RobotBuddy 的 ST7789 屏幕、INMP441 麦克风、MAX98357A 功放、DRV8833 电机驱动、MPU6050 IMU、红外传感器。

## Goal

为 RobotBuddy 的每个硬件组件编写生产级驱动代码。

## Inputs

- 组件数据手册 (datasheet)
- 原理图 (引脚分配)
- 架构设计中的 HAL 层接口定义

## Outputs

- `firmware/drivers/<name>/<name>.c` — 驱动实现
- `firmware/drivers/<name>/<name>.h` — 驱动接口
- `firmware/drivers/<name>/README.md` — 驱动说明

## Driver Implementation Template

### 通用驱动结构

```c
// <name>.h
#pragma once
#include "esp_err.h"
#include "driver/spi_master.h"   // 按需

#ifdef __cplusplus
extern "C" { ... }?
#endif

// 配置结构体
typedef struct {
    spi_host_device_t host;     // SPI2_HOST or SPI3_HOST
    gpio_num_t pin_cs;
    gpio_num_t pin_dc;
    gpio_num_t pin_rst;
    int clock_speed_hz;         // 40MHz max for ST7789
} my_device_config_t;

// 驱动句柄（不透明指针）
typedef struct my_device_ctx my_device_ctx_t;

// 生命周期
esp_err_t my_device_init(const my_device_config_t *cfg, my_device_ctx_t **out_ctx);
esp_err_t my_device_deinit(my_device_ctx_t *ctx);

// 功能接口
esp_err_t my_device_write_cmd(my_device_ctx_t *ctx, uint8_t cmd);
esp_err_t my_device_write_data(my_device_ctx_t *ctx, const uint8_t *data, size_t len);
esp_err_t my_device_read(my_device_ctx_t *ctx, uint8_t *buf, size_t len);

// 错误码定义
#define MY_DEVICE_ERR_TIMEOUT    ESP_ERR_HW_MALFUNCTION
```

### 驱动实现模式

```c
// <name>.c
#include "<name>.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "my_device";

struct my_device_ctx {
    my_device_config_t config;
    spi_device_handle_t spi;
    SemaphoreHandle_t mutex;     // 线程安全
    bool initialized;
};

esp_err_t my_device_init(const my_device_config_t *cfg, my_device_ctx_t **out_ctx) {
    esp_err_t ret;

    // 1. 参数校验
    if (!cfg || !out_ctx) return ESP_ERR_INVALID_ARG;

    // 2. 分配上下文
    my_device_ctx_t *ctx = calloc(1, sizeof(my_device_ctx_t));
    if (!ctx) return ESP_ERR_NO_MEM;
    ctx->config = *cfg;

    // 3. 初始化硬件接口
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = cfg->clock_speed_hz,
        .mode = 0,                          // CPOL=0, CPHA=0
        .spics_io_num = cfg->pin_cs,
        .queue_size = 7,                    // 最多 7 个排队事务
    };
    ret = spi_bus_add_device(cfg->host, &dev_cfg, &ctx->spi);
    if (ret != ESP_OK) goto err_cleanup;

    // 4. 创建互斥锁
    ctx->mutex = xSemaphoreCreateMutex();
    if (!ctx->mutex) { ret = ESP_ERR_NO_MEM; goto err_cleanup; }

    // 5. 硬件复位 + 初始化序列
    ret = my_device_hw_reset(ctx);
    if (ret != ESP_OK) goto err_cleanup;

    ctx->initialized = true;
    *out_ctx = ctx;
    ESP_LOGI(TAG, "Initialized successfully");
    return ESP_OK;

err_cleanup:
    my_device_deinit(ctx);
    return ret;
}
```

## RobotBuddy 硬件驱动清单

| 设备 | 接口 | 驱动文件 | 关键参数 |
|------|------|----------|---------|
| ST7789 LCD | SPI (VSPI) | `drivers/display/st7789.c` | 40MHz, Mode 0, 240×240 |
| GC9A01 LCD | SPI (VSPI) | `drivers/display/gc9a01.c` | 40MHz, Mode 0, 240×240 圆形 |
| INMP441 Mic | I2S | `drivers/audio/inmp441.c` | 16kHz, 16bit, Mono, DMA 4096 |
| MAX98357A Amp | I2S | `drivers/audio/max98357a.c` | 16kHz, 16bit, Mono, 3W |
| DRV8833 Motor | PWM×2 | `drivers/motion/drv8833.c` | 100Hz-50kHz PWM, 2 通道 |
| MPU6050 IMU | I2C (I2C0) | `drivers/sensor/mpu6050.c` | 100kHz/400kHz, 6轴 |
| VL53L0X TOF | I2C (I2C0) | `drivers/sensor/vl53l0x.c` | 100kHz/400kHz, 2m range |
| TCRT5000 IR | ADC/GPIO | `drivers/sensor/tcrt5000.c` | 阈值式，数字输出 |
| TP4056 Charger | GPIO | `drivers/power/tp4056.c` | CHRG/STDBY 状态引脚 |
| 电池电量 | ADC | `drivers/power/battery.c` | 电阻分压 2:1, ADC1 |

## Rules

1. **错误码统一** — 所有函数返回 `esp_err_t`
2. **线程安全** — 共享设备用 `SemaphoreHandle_t` 保护
3. **ISR 兼容** — 不要在驱动层禁用中断过久
4. **日志标签** — 统一 TAG = 文件名，便于过滤
5. **幂等初始化** — `xxx_init` 多次调用安全
6. **资源释放** — `xxx_deinit` 释放所有资源
7. **超时机制** — 所有 I/O 操作必须有 timeout
8. **DMA 对齐** — SPI/I2S DMA buffer 必须 `__attribute__((aligned(4)))` 或从 PSRAM 分配时注意 cache 对齐

## Checklist

- [ ] 驱动初始化成功（读取 WHO_AM_I 确认）
- [ ] 基本功能正常（读/写/配置）
- [ ] 边界测试通过（超时/断电恢复/异常参数）
- [ ] 线程安全验证（多任务并发访问无竞态）
- [ ] 无内存泄漏（valgrind / heap tracing）
- [ ] DMA 缓存一致性（PSRAM buffer 有 cache sync）
- [ ] 引脚无冲突（对照 ESP32-S3 pin matrix）
- [ ] 日志输出清晰（ESP_LOGI / ESP_LOGE）
