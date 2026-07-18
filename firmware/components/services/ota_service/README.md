# RobotBuddy OTA Service

完整的 OTA (Over-The-Air) 固件升级服务，支持安全下载、签名验证和自动回滚。

## 功能特性

- **HTTPS 固件下载** - 支持 TLS 加密传输
- **RSA-2048 签名验证** - 确保固件来源可信
- **SHA256 完整性校验** - 验证固件完整性
- **断点续传** - 网络中断后自动恢复下载
- **AB 分区滚动升级** - ota_0/ota_1 交替使用
- **自动回滚** - 新固件异常时自动恢复
- **健康检查** - 新固件启动后 30 秒验证窗口
- **MQTT 命令支持** - 远程触发升级
- **电量检查** - 低电量时拒绝升级

## 目录结构

```
ota_service/
├── include/
│   ├── ota_service.h     # 主服务接口
│   ├── ota_manager.h     # 状态管理接口
│   ├── ota_download.h    # 下载管理接口
│   ├── ota_verify.h      # 签名验证接口
│   ├── ota_partition.h   # 分区管理接口
│   ├── ota_rollback.h    # 回滚管理接口
│   ├── ota_security.h    # 安全管理接口
│   ├── ota_types.h       # 类型定义
│   └── ota_config.h      # 配置常量
├── src/
│   ├── ota_service.c     # 主服务实现
│   ├── ota_manager.c     # 状态管理实现
│   ├── ota_download.c    # 下载实现
│   ├── ota_verify.c      # 验证实现
│   ├── ota_partition.c   # 分区管理实现
│   ├── ota_rollback.c    # 回滚实现
│   └── ota_security.c    # 安全实现
└── CMakeLists.txt        # CMake 配置
```

## 使用方法

### 1. 初始化

```c
#include "ota_service.h"

esp_err_t ret = ota_service_init();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "OTA service init failed");
}
```

### 2. 检查更新

```c
ota_update_info_t info;
esp_err_t ret = ota_service_check_update(&info);
if (ret == ESP_OK && info.level != OTA_UPDATE_NONE) {
    ESP_LOGI(TAG, "New version available: %s", info.version);
}
```

### 3. 开始升级

```c
esp_err_t ret = ota_service_start_upgrade(&info);
if (ret == ESP_OK) {
    // 升级开始，等待完成...
}
```

### 4. 确认升级（新固件启动后）

```c
// 在 app_main() 中检查
ota_state_t state = ota_service_get_state();
if (state == OTA_STATE_VERIFYING_NEW) {
    // 执行健康检查
    if (all_systems_ok()) {
        ota_service_commit();
    }
}
```

## MQTT 命令格式

```json
// 检查更新
{"cmd": "check"}

// 开始升级
{
    "cmd": "upgrade",
    "url": "https://ota.example.com/firmware.bin",
    "signature_url": "https://ota.example.com/firmware.sig",
    "version": "1.2.0",
    "sha256": "abc123...",
    "size": 1048576
}

// 回滚
{"cmd": "rollback"}

// 回退出厂固件
{"cmd": "factory_reset"}
```

## 配置选项

在 `ota_config.h` 中可配置：

| 配置项 | 默认值 | 描述 |
|--------|--------|------|
| `OTA_HEALTH_CHECK_TIMEOUT_MS` | 30000 | 健康检查超时 (ms) |
| `OTA_WATCHDOG_THRESHOLD` | 3 | watchdog 重启阈值 |
| `OTA_PANIC_THRESHOLD` | 5 | panic 重启阈值 |
| `OTA_MAX_RETRIES` | 3 | 最大重试次数 |
| `OTA_MIN_BATTERY_PERCENT` | 20 | 最低电量百分比 |
| `OTA_SERVICE_TASK_STACK` | 8192 | 任务栈大小 |

## 安全特性

1. **证书锁定** - 验证 OTA 服务器证书指纹
2. **签名验证** - RSA-2048 公钥验证固件签名
3. **完整性校验** - SHA256 哈希校验
4. **防回滚** - 不允许自动降级
5. **URL 签名** - 防止重放攻击

## 分区布局

```
Flash (16MB):
├── factory (2MB)    - 出厂固件，永不覆盖
├── ota_0 (2MB)      - OTA 分区 0
├── ota_1 (2MB)      - OTA 分区 1
└── spiffs (4MB)     - 资源分区
```

## 相关文档

- [OTA 架构设计](../../../docs/architecture/ota-upgrade-architecture.md)
- [OTA Skill 文档](../../../.claude/skills/ota/SKILL.md)

## License

MIT License - RobotBuddy Project
