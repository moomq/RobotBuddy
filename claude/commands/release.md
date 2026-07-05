# /release — RobotBuddy 固件发布

## 用途

将 RobotBuddy 固件从开发状态推进到发布状态，生成 OTA 升级包和发布说明。

## 适用场景

- V1.0 MVP 发布
- 版本迭代发布（V1.1, V2.0 等）
- 紧急热修复发布
- Beta 测试版发布

## 工作流

```
第 1 阶段：发布前检查
├── 1.1 执行 /review 全维度审查 → 无 Critical 问题
├── 1.2 执行 /firmware 集成测试 → 所有测试通过
├── 1.3 对照 checklists/pre-commit.md → 全项通过
├── 1.4 确认所有 P0/P1 功能正常
└── 1.5 输出：发布就绪确认

第 2 阶段：版本管理
├── 2.1 版本号更新（SemVer: MAJOR.MINOR.PATCH）
│   ├── PATCH: Bug 修复、性能优化
│   ├── MINOR: 新功能、新传感器支持
│   └── MAJOR: 架构重构、不兼容变更
├── 2.2 更新 CHANGELOG.md
│   ├── Added — 新增功能
│   ├── Changed — 功能变更
│   ├── Fixed — Bug 修复
│   └── Deprecated — 废弃功能
├── 2.3 更新版本号到代码:
│   └── app_version.h: #define FW_VERSION "x.y.z"
└── 2.4 输出：版本号 + CHANGELOG

第 3 阶段：构建发布固件
├── 3.1 idf.py clean + idf.py build → Release 构建
├── 3.2 生成 OTA 固件包（esp_https_ota）
├── 3.3 计算固件校验和: sha256sum
├── 3.4 固件签名（可选，生产环境必须）
├── 3.5 输出：firmware_vX.Y.Z.bin + checksum

第 4 阶段：发布文档
├── 4.1 调用 document skill → 生成 Release Notes
├── 4.2 更新 docs/ 相关文档
├── 4.3 输出：Release Notes

第 5 阶段：归档 & 部署
├── 5.1 Git tag: vX.Y.Z
├── 5.2 Git push --tags
├── 5.3 固件上传到 OTA 服务器
├── 5.4 更新 MQTT OTA 通知 Topic
└── 5.5 输出：发布完成确认
```

## 版本规则

```
V1.0.0 — MVP: 基础表情 + 语音对话 + 移动 + AI 对话
V1.1.0 — 新增: 代码片段滚动显示
V1.2.0 — 新增: 本地唤醒词 (ESP-SR)
V2.0.0 — 重大: VS Code 插件联动 + Git 通知
V3.0.0 — 重大: 本地 AI + SLAM 导航
```

## OTA 固件规范

```
固件分区表 (partitions.csv):
├── nvs          — 0x6000  (24KB)
├── otadata      — 0x2000  (8KB)
├── phy_init     — 0x1000  (4KB)
├── factory      — 0x200000 (2MB) — 出厂固件
├── ota_0        — 0x200000 (2MB) — OTA 分区 0
├── ota_1        — 0x200000 (2MB) — OTA 分区 1
└── spiffs       — 0x400000 (4MB) — 表情资源/音频文件

OTA URL 格式:
https://ota.robotbuddy.local/firmware_vX.Y.Z.bin
```

## 回滚策略

```
自动回滚条件:
├── OTA 升级后启动失败（3次 watchdog 重启）
├── 运行中 panic（连续 5 次）
└── APP 健康检查超时（启动后 30s 内未上报）

回滚方式:
└── esp_ota_mark_app_invalid_rollback_and_reboot()
      自动切换到上一个正常分区
```
