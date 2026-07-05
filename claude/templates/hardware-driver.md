# Hardware Driver Document Template

> 用于记录 RobotBuddy 每个硬件驱动模块的详细信息

---

## 驱动信息

| 项目 | 内容 |
|------|------|
| **驱动名称** | `xxx_driver` |
| **硬件型号** | 芯片/模组型号 |
| **接口类型** | SPI / I2C / I2S / UART / PWM / GPIO / ADC |
| **接口实例** | SPI2_HOST / I2C_NUM_0 / I2S_NUM_0 |
| **数据手册** | [链接或文件名] |
| **驱动版本** | 1.0.0 |
| **最后更新** | YYYY-MM-DD |

## 引脚分配

| 引脚名称 | ESP32-S3 GPIO | 说明 |
|----------|---------------|------|
| CS | GPIO_XX | 片选 |
| DC | GPIO_XX | 数据/命令 |
| RST | GPIO_XX | 复位 |
| SCLK | GPIO_XX | 时钟 |
| MOSI | GPIO_XX | 主机输出 |
| MISO | GPIO_XX | 主机输入 (如不需要填 N/C) |

## 电气参数

| 参数 | 值 | 备注 |
|------|-----|------|
| 供电电压 | 3.3V / 5V | — |
| 最大电流 | XXX mA | — |
| 接口电平 | 3.3V | 是否需要电平转换？ |
| 接口频率 | XX MHz | — |

## 初始化序列

```
1. 硬件复位 (RST LOW → 10ms → RST HIGH)
2. 读 WHO_AM_I 寄存器确认设备 (如适用)
3. 软件复位命令
4. 配置寄存器序列:
   ├── Reg 0xXX ← 0xYY
   ├── Reg 0xXX ← 0xYY
   └── ...
5. 退出配置模式 → 进入正常工作模式
```

## API 参考

### 初始化

```c
esp_err_t xxx_init(const xxx_config_t *cfg, xxx_ctx_t **out_ctx);
```

- **参数:**
  - `cfg` — 配置参数，包含引脚、频率等
  - `out_ctx` — 输出驱动句柄
- **返回:** `ESP_OK` 成功，其他为错误码
- **注意:** 同一设备不可重复初始化

### 基本操作

```c
esp_err_t xxx_write(xxx_ctx_t *ctx, const uint8_t *data, size_t len);
esp_err_t xxx_read(xxx_ctx_t *ctx, uint8_t *buf, size_t len);
```

### 去初始化

```c
esp_err_t xxx_deinit(xxx_ctx_t *ctx);
```

## 错误码

| 错误码 | 含义 | 处理建议 |
|--------|------|---------|
| `ESP_ERR_INVALID_ARG` | 参数无效 | 检查指针和配置值 |
| `ESP_ERR_NO_MEM` | 内存不足 | 减少并发分配 |
| `ESP_ERR_TIMEOUT` | 操作超时 | 检查硬件连接和频率 |
| `ESP_ERR_INVALID_STATE` | 状态错误 | 确认初始化顺序 |

## 已知问题

- [ ] 问题描述 — 影响范围 — 计划修复版本

## 性能数据

| 指标 | 值 | 测量条件 |
|------|-----|---------|
| 初始化时间 | XX ms | — |
| 读吞吐量 | XX KB/s | — |
| 写吞吐量 | XX KB/s | — |
| CPU 占用 | X% | — |
| 内存占用 | XX bytes | — |
