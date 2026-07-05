# Testing Skill — RobotBuddy

## Role

RobotBuddy 测试专家，负责嵌入式固件的单元测试、硬件在环测试和系统集成测试。

## Domain

ESP32 测试框架 (Unity + CMock)，硬件在环 (HIL)，音频回路测试，运动校准，FreeRTOS 压力测试。

## Goal

确保 RobotBuddy 的每个模块和整体系统达到可发布质量标准。

## Inputs

- 模块代码和接口定义
- 需求文档中的验收条件
- 硬件测试环境

## Outputs

- `tests/unit/<module>_test.c` — 单元测试代码
- `tests/hil/<scenario>_test.c` — HIL 测试代码
- `docs/testing/<feature>-test-report.md` — 测试报告

## Test Levels

```
Level 1: 单元测试 (Unity + CMock)
├── 运行环境: PC (Linux/macOS/Windows) 或 QEMU
├── Mock: 硬件接口通过 CMock mock
├── 覆盖: 所有公共 API、边界条件、错误路径
└── 频率: 每次 commit 自动运行

Level 2: 硬件在环 (HIL)
├── 运行环境: 真实 ESP32-S3 + RobotBuddy PCB
├── 覆盖: GPIO、SPI、I2C、I2S、PWM 硬件通路
├── 测试夹具: 自动化测试架 (可选)
└── 频率: 每次 PR / 每日构建

Level 3: 系统集成测试
├── 运行环境: 完整 RobotBuddy 机器人
├── 覆盖: 多任务交互、音频全链路、云端通信
├── 测试: 长时间压力测试 (≥ 24h)
└── 频率: 发布前 / 每周

Level 4: 用户验收测试
├── 运行环境: 最终产品形态
├── 覆盖: 实际使用场景
├── 测试: 真实开发者使用 1 周
└── 频率: 版本发布前
```

## Unit Test Pattern

```c
// tests/unit/test_emotion_engine.c
#include "unity.h"
#include "mock_display_driver.h"    // CMock 自动生成

#include "emotion_engine.h"

static emotion_state_t s_state;

TEST_CASE("emotion_init: valid config initializes correctly", "[emotion]")
{
    emotion_config_t cfg = {
        .display_width = 240,
        .display_height = 240,
        .default_emotion = EMOTION_IDLE,
    };

    esp_err_t ret = emotion_init(&cfg);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    emotion_id_t current = emotion_get_current();
    TEST_ASSERT_EQUAL(EMOTION_IDLE, current);
}

TEST_CASE("emotion_transition: IDLE to HAPPY with animation", "[emotion]")
{
    emotion_init(&(emotion_config_t){.default_emotion = EMOTION_IDLE});

    // 触发切换
    emotion_transition_to(EMOTION_HAPPY, 500); // 500ms transition

    // 检查目标
    TEST_ASSERT_EQUAL(EMOTION_HAPPY, emotion_get_target());

    // 模拟 30 帧后 (1s @ 30FPS)，过渡应该完成
    for (int i = 0; i < 30; i++) {
        emotion_render_frame(&s_state, NULL); // NULL = don't check fb
    }

    TEST_ASSERT_EQUAL(EMOTION_HAPPY, emotion_get_current());
}

TEST_CASE("emotion_transition: invalid emotion returns error", "[emotion]")
{
    esp_err_t ret = emotion_transition_to((emotion_id_t)99, 500);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);
}
```

## HIL Test Pattern

```c
// tests/hil/test_audio_pipeline.c
// 运行在真实 ESP32-S3 硬件上

TEST_CASE("audio_pipeline: I2S mic captures audible signal", "[audio][hil]")
{
    // 初始化音频管道
    TEST_ASSERT_EQUAL(ESP_OK, audio_manager_init());

    // 开始采集
    TEST_ASSERT_EQUAL(ESP_OK, audio_capture_start());

    // 采集 100ms 数据
    vTaskDelay(pdMS_TO_TICKS(100));

    int16_t buf[1600]; // 100ms @ 16kHz
    size_t captured = audio_capture_read(buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, captured);

    // 验证: 信号中有非零值（环境噪声 = 有信号）
    bool has_signal = false;
    for (size_t i = 0; i < captured / 2; i++) {
        if (abs(buf[i]) > 100) { // 高于噪声阈值
            has_signal = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(has_signal);

    audio_capture_stop();
}

TEST_CASE("audio_pipeline: playback produces output", "[audio][hil]")
{
    // 生成 440Hz 正弦波 (A4 音符), 100ms
    int16_t sine_wave[1600];
    for (int i = 0; i < 1600; i++) {
        sine_wave[i] = (int16_t)(32767.0 * sinf(2.0f * M_PI * 440.0f * i / 16000.0f));
    }

    // 播放
    TEST_ASSERT_EQUAL(ESP_OK, audio_playback_start((uint8_t*)sine_wave, sizeof(sine_wave)));

    // 等待播放完成
    vTaskDelay(pdMS_TO_TICKS(200));

    // 检查: 无错误日志
    // 实际操作: 用示波器测 MAX98357A 输出或人耳确认
}
```

## Stress Test Pattern

```c
// tests/stress/test_long_run.c
// 长时间运行压力测试

TEST_CASE("stress: 24h continuous operation", "[stress][manual]")
{
    // 启动所有任务
    system_start_all_tasks();

    for (int hour = 0; hour < 24; hour++) {
        ESP_LOGI(TAG, "=== Hour %d/24 ===", hour + 1);

        // 检查内存
        sysmon_report_t report;
        sysmon_get_report(&report);
        ESP_LOGI(TAG, "DRAM free: %lu, PSRAM free: %lu",
                 report.heap_free_dram, report.heap_free_psram);

        // 检查: 无内存泄漏 (heap 不持续下降)
        TEST_ASSERT_GREATER_THAN(32 * 1024, report.heap_free_dram);

        // 检查: 所有任务栈水位安全
        for (int t = 0; t < TASK_COUNT; t++) {
            TEST_ASSERT_GREATER_OR_EQUAL(256, report.stack_free[t]);
        }

        // 每 15 分钟模拟一次对话
        if (hour % 4 == 0) {
            // 模拟语音输入 → AI 回复 → TTS 播放
            simulate_voice_interaction();
        }

        vTaskDelay(pdMS_TO_TICKS(3600000)); // 1 hour
    }

    ESP_LOGI(TAG, "24h stress test PASSED");
}
```

## Test-Specific Checklists

### Audio Testing
- [ ] 麦克风采集波形正常（示波器）
- [ ] 扬声器播放无杂音/爆音
- [ ] 全双工无冲突
- [ ] ASR 识别准确率 ≥ 95%
- [ ] TTS 音质清晰
- [ ] WiFi 断线恢复后音频继续

### Display Testing
- [ ] 帧率稳定 ≥ 30 FPS
- [ ] 所有 11 种表情正确渲染
- [ ] 表情切换动画流畅
- [ ] 无花屏/撕裂/冻结
- [ ] 圆形屏裁剪正确（GC9A01）
- [ ] 24h 运行无显示异常

### Motion Testing
- [ ] 前进/后退/转向/旋转 全部正常
- [ ] 边缘检测可靠（不同桌面颜色）
- [ ] 紧急停止 < 10ms 响应
- [ ] PID 无震荡
- [ ] 堵转保护生效
- [ ] 电池低电量自动限速

### Cloud Testing
- [ ] WiFi 配网成功
- [ ] WiFi 断线自动重连
- [ ] TLS 握手成功
- [ ] HTTP API 响应正确
- [ ] WebSocket 音频流稳定
- [ ] OTA 升级+回滚 验证

### System Testing
- [ ] 所有 Task 栈水位 ≥ 256 bytes
- [ ] 无内存泄漏 (24h heap tracking)
- [ ] 看门狗未触发
- [ ] CPU 使用率 ≤ 80%
- [ ] 冷启动 < 3s
- [ ] 电池续航 ≥ 4h (活跃)

## CI/CD Integration

```yaml
# .github/workflows/test.yml
name: RobotBuddy Tests

on: [push, pull_request]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    container: espressif/idf:release-v5.1
    steps:
      - uses: actions/checkout@v4
      - run: |
          cd firmware
          idf.py build
          cd build && ctest --output-on-failure

  hil-tests:
    runs-on: [self-hosted, esp32-s3]
    steps:
      - uses: actions/checkout@v4
      - run: |
          cd firmware
          idf.py build
          idf.py flash monitor
          # 自动化 HIL 测试脚本
          python ../scripts/run_hil_tests.py
```

## Rules

1. 每个 PR 必须有对应的单元测试
2. 新增驱动代码必须有 HIL 测试
3. 发布前必须通过 24h 压力测试
4. 测试失败 = 不得合入
5. 间接测试不可替代直接测试（"能跑就行"不够）
6. 边界条件必须覆盖（空输入/极大输入/并发输入）
