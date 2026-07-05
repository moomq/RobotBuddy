# /hardware-bringup — RobotBuddy 硬件模块调通

## 用途

为 RobotBuddy 引入新硬件组件时，完成从数据手册阅读到驱动验证的完整流程。

## 适用场景

- 新传感器接入（IMU、TOF、红外、触摸等）
- 新显示模组适配（ST7789、GC9A01 等）
- 新音频模组适配（INMP441、MAX98357A 等）
- 新电机驱动适配（DRV8833、TB6612 等）
- 新电源管理 IC 适配（TP4056、MT3608 等）

## 工作流

```
第 1 阶段：数据手册分析
├── 1.1 获取并阅读组件 datasheet
├── 1.2 确认：接口类型（SPI/I2C/I2S/UART/PWM）、电压电平、时序要求
├── 1.3 确认：ESP32-S3 引脚分配（避免冲突）
└── 1.4 输出：引脚分配表 + 接口参数

第 2 阶段：驱动开发
├── 2.1 调用 hardware-driver skill → 生成驱动框架代码
├── 2.2 实现：初始化、读/写、配置、去初始化
├── 2.3 实现：错误处理和超时机制
├── 2.4 输出：driver/<module>_driver.c + driver/<module>_driver.h

第 3 阶段：硬件连接
├── 3.1 确认原理图正确性
├── 3.2 焊接 / 杜邦线连接
├── 3.3 上电前万用表短路检测
├── 3.4 上电后电压检查（3.3V / 5V 正常）
└── 3.5 输出：接线确认清单

第 4 阶段：驱动验证
├── 4.1 最小功能测试（echo test / whoami register）
├── 4.2 全功能测试（全速率、全模式）
├── 4.3 边界测试（超时、断电恢复、异常数据）
├── 4.4 调用 testing skill → 生成硬件测试报告
└── 4.5 输出：驱动验证报告

第 5 阶段：集成
├── 5.1 将驱动注册到 FreeRTOS 服务层
├── 5.2 加入事件总线消息定义
├── 5.3 更新 BOM 表（templates/component-bom.md）
└── 5.4 输出：更新的 BOM + 集成代码
```

## 前置条件

- ESP32-S3 开发板 / RobotBuddy PCB 可用
- 目标硬件模组在手
- 示波器 / 逻辑分析仪可用（推荐）
- 万用表可用（必须）

## 输出

- 引脚分配表
- 驱动代码（C 源文件 + 头文件）
- 硬件测试报告
- 更新后的 BOM 表

## ESP32-S3 引脚约束

```
优先使用：
├── SPI  (VSPI):  GPIO 35-37 (MOSI/MISO/SCLK), 任意GPIO (CS/DC/RST)
├── I2C  (I2C0):  GPIO 21(SDA), GPIO 22(SCL) — 默认引脚
├── I2S  (I2S0):  GPIO 4(BCLK), GPIO 5(WS), GPIO 6(DIN) — 默认引脚
├── UART (UART0):  GPIO 43(TX), GPIO 44(RX) — USB-JTAG 共享
├── PWM  (LEDC):  任意 GPIO
└── ADC  (ADC1):  GPIO 1-10 — 3.3V 最大输入

避免使用：
├── GPIO 19/20 — USB D+/D- (USB-JTAG)
├── GPIO 45/46 — PSRAM 共享（部分模组）
└── Strapping 引脚: GPIO 0, 2, 46 — 启动模式
```
