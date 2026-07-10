# Protobuf 使用规范 — RobotBuddy IDE 集成通信

> 适用范围：RobotBuddy Local Bridge 与 ESP32-S3 之间的结构化数据通信
> 版本：1.0
> 最后更新：2026-07-10

---

## 1. 概述

RobotBuddy 在 V2.0 阶段引入 IDE 集成功能（VS Code 插件 + Local Bridge），需要 Local Bridge (Python) 与 ESP32-S3 (C) 之间的高效结构化通信。Protobuf 提供跨语言、紧凑、向后兼容的消息格式。

---

## 2. .proto 文件组织

```
protos/
├── common.proto          # 通用类型（状态枚举、错误码）
├── device.proto          # 设备状态、传感器数据
├── build.proto           # 构建/编译状态
├── git.proto             # Git 仓库状态
├── notification.proto    # 通知消息
├── ota.proto             # OTA 升级消息
├── command.proto         # 命令下发
└── emotion.proto         # 表情/动画控制
```

---

## 3. .proto 编写规范

### 3.1 命名规范

```protobuf
// 文件名: snake_case.proto
// 包名: robotbuddy.v1
// 消息名: PascalCase
// 字段名: snake_case
// 枚举名: PascalCase, 值: UPPER_SNAKE_CASE
// Service 名: PascalCase

syntax = "proto3";
package robotbuddy.v1;

// 枚举值必须以 0 开始
enum DeviceStatus {
    DEVICE_STATUS_UNSPECIFIED = 0;  // 必须有 UNSPECIFIED = 0
    DEVICE_STATUS_ONLINE = 1;
    DEVICE_STATUS_OFFLINE = 2;
    DEVICE_STATUS_UPDATING = 3;
}
```

### 3.2 字段编号规则

```protobuf
message BuildStatus {
    // 字段编号从 1 开始，按顺序递增
    // 删除字段时使用 reserved，不重用编号
    string project = 1;
    BuildResult result = 2;
    int32 error_count = 3;
    int32 warning_count = 4;
    float duration_seconds = 5;
    int64 timestamp = 6;  // Unix 时间戳（毫秒）
    
    // 已删除的字段
    reserved 7, 8;
    reserved "old_field_name";
}
```

### 3.3 消息设计原则

```protobuf
// ✅ 好的设计：扁平结构，字段语义清晰
message SensorData {
    float accel_x = 1;
    float accel_y = 2;
    float accel_z = 3;
    float gyro_x = 4;
    float gyro_y = 5;
    float gyro_z = 6;
    int64 timestamp = 7;
}

// ❌ 差的设计：嵌套过深，解析复杂
message SensorDataBad {
    message Vector3 {
        float x = 1;
        float y = 2;
        float z = 3;
    }
    Vector3 accel = 1;  // 额外的嵌套增加编码/解码复杂度
    Vector3 gyro = 2;
}
```

---

## 4. 核心消息定义

### 4.1 通用类型 (common.proto)

```protobuf
syntax = "proto3";
package robotbuddy.v1;

// 通用状态枚举
enum StatusCode {
    STATUS_CODE_UNSPECIFIED = 0;
    STATUS_CODE_OK = 1;
    STATUS_CODE_ERROR = 2;
    STATUS_CODE_TIMEOUT = 3;
    STATUS_CODE_BUSY = 4;
}

// 错误信息
message Error {
    int32 code = 1;
    string message = 2;
    string detail = 3;
}
```

### 4.2 设备状态 (device.proto)

```protobuf
syntax = "proto3";
package robotbuddy.v1;
import "common.proto";

message DeviceStatusReport {
    string device_id = 1;        // MAC 地址
    string firmware_version = 2;
    int32 battery_percent = 3;
    float battery_voltage = 4;
    int32 wifi_rssi = 5;
    int64 uptime_seconds = 6;
    int32 heap_free_dram = 7;    // DRAM 空闲 (bytes)
    int32 heap_free_psram = 8;   // PSRAM 空闲 (bytes)
    int64 timestamp = 9;
}
```

### 4.3 构建状态 (build.proto)

```protobuf
syntax = "proto3";
package robotbuddy.v1;

enum BuildResult {
    BUILD_RESULT_UNSPECIFIED = 0;
    BUILD_RESULT_SUCCESS = 1;
    BUILD_RESULT_FAILURE = 2;
    BUILD_RESULT_WARNING = 3;
}

message BuildStatus {
    string project = 1;
    BuildResult result = 2;
    int32 error_count = 3;
    int32 warning_count = 4;
    float duration_seconds = 5;
    string error_message = 6;
    int64 timestamp = 7;
}
```

### 4.4 表情控制 (emotion.proto)

```protobuf
syntax = "proto3";
package robotbuddy.v1;

enum EmotionState {
    EMOTION_STATE_UNSPECIFIED = 0;
    EMOTION_STATE_IDLE = 1;
    EMOTION_STATE_LISTENING = 2;
    EMOTION_STATE_THINKING = 3;
    EMOTION_STATE_HAPPY = 4;
    EMOTION_STATE_ERROR = 5;
    EMOTION_STATE_WARNING = 6;
    EMOTION_STATE_SLEEP = 7;
}

message EmotionCommand {
    EmotionState state = 1;
    int32 duration_ms = 2;       // 持续时间，0=永久
    string text = 3;             // 可选: 显示文字
    int32 intensity = 4;         // 强度 0-100
}
```

---

## 5. C 端集成 (nanopb)

### 5.1 依赖配置

```cmake
# CMakeLists.txt 中添加 nanopb 依赖
idf_component_register(
    SRCS "protobuf_handler.c"
         "protobuf/messages.pb.c"
    INCLUDE_DIRS "include"
         "protobuf"
    REQUIRES nanopb
)
```

### 5.2 编码/解码

```c
#include <pb_encode.h>
#include <pb_decode.h>
#include "messages.pb.h"

// 编码
bool protobuf_encode_device_status(
    const DeviceStatusReport *msg,
    uint8_t *buffer, size_t buffer_size, size_t *encoded_size)
{
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, buffer_size);
    bool status = pb_encode(&stream, DeviceStatusReport_fields, msg);
    *encoded_size = stream.bytes_written;
    return status;
}

// 解码
bool protobuf_decode_device_status(
    const uint8_t *buffer, size_t size, DeviceStatusReport *msg)
{
    pb_istream_t stream = pb_istream_from_buffer(buffer, size);
    return pb_decode(&stream, DeviceStatusReport_fields, msg);
}
```

### 5.3 内存预算

```c
// Protobuf 编码缓冲区大小（根据最大消息估算）
#define PROTOBUF_ENCODE_BUFFER_SIZE  256    // 编码缓冲区
#define PROTOBUF_DECODE_BUFFER_SIZE   256    // 解码缓冲区

// nanopb 字段回调（变长字符串/bytes）
// 对于固定小消息使用静态分配，大消息使用 PSRAM 动态分配
```

---

## 6. Python 端集成

```python
# 安装依赖
# pip install protobuf grpcio

from google.protobuf.json_format import MessageToDict, ParseDict
import device_pb2

# 编码
status = device_pb2.DeviceStatusReport(
    device_id="AA:BB:CC:DD:EE:FF",
    firmware_version="1.2.3",
    battery_percent=85,
    wifi_rssi=-45,
)
encoded = status.SerializeToString()

# 解码
decoded = device_pb2.DeviceStatusReport()
decoded.ParseFromString(encoded)

# JSON 互转
status_dict = MessageToDict(status)
```

---

## 7. 版本兼容性

| 规则 | 说明 |
|------|------|
| 新增字段 | 使用新编号，旧端忽略未知字段 (proto3 兼容) |
| 删除字段 | 使用 `reserved`，不重用编号 |
| 修改类型 | 不兼容，需新建消息或新字段 |
| 修改编号 | 绝对禁止，破坏二进制兼容 |
| 修改字段名 | 可以（二进制使用编号），但建议保持一致 |

---

## 8. 性能约束

| 指标 | 目标 | 说明 |
|------|------|------|
| 编码延迟 | < 1ms | 设备端 nanopb 编码 |
| 解码延迟 | < 1ms | 设备端 nanopb 解码 |
| 消息大小 | < 512B | 典型状态消息 |
| 编码缓冲区 | < 256B | DRAM 中静态分配 |
| 空闲发送频率 | 1/60s | 空闲时每分钟 1 次状态上报 |
| 活跃发送频率 | 1/s | 活跃时每秒 1 次状态上报 |