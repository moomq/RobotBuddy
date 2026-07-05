# Audio Pipeline Skill — RobotBuddy

## Role

RobotBuddy 音频管道专家，负责麦克风采集、扬声器播放、云端 ASR/TTS 流式处理的完整音频链路。

## Domain

I2S 音频、MEMS 麦克风、D 类功放、音频编码 (PCM/Opus)、流式音频传输、云端 STT/TTS API。

## Goal

构建低延迟 (< 1.5s)、高可靠的机器人语音交互音频管道。

## Inputs

- 音频硬件规格（INMP441 + MAX98357A 驱动）
- 云端 AI API 规范（Whisper ASR, TTS 服务）
- FreeRTOS 任务架构

## Outputs

- `firmware/services/audio/audio_manager.c` — 音频管理器
- `firmware/services/audio/i2s_driver.c` — I2S 底层封装
- `firmware/services/audio/stream_handler.c` — WebSocket 流处理
- `docs/audio-pipeline.md` — 音频管道文档

## Audio Pipeline Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                       Audio Pipeline                          │
│                                                               │
│  ┌─────────┐   ┌──────────┐   ┌──────────┐   ┌───────────┐  │
│  │ I2S IN  │ → │ Ring Buf │ → │ VAD      │ → │ Opus Enc  │  │
│  │ 16kHz   │   │ 16KB ×2  │   │ WebRTC   │   │ (optional)│  │
│  │ 16bit   │   │ DMA      │   │          │   │           │  │
│  └─────────┘   └──────────┘   └────┬─────┘   └─────┬─────┘  │
│                                     │               │         │
│                      ┌──────────────┘               │         │
│                      ↓                              │         │
│  ┌──────────────────────────────────────┐           │         │
│  │           Cloud API Gateway           │           │         │
│  │                                       │           │         │
│  │  ┌───────┐   ┌──────┐   ┌─────────┐  │           │         │
│  │  │Whisper│ → │ LLM  │ → │Edge-TTS │  │           │         │
│  │  │ ASR   │   │      │   │         │  │           │         │
│  │  └───────┘   └──────┘   └────┬────┘  │           │         │
│  └───────────────────────────────┼────────┘           │         │
│                                   ↓                    │         │
│  ┌─────────┐   ┌──────────┐   ┌──────────┐           │         │
│  │Speaker  │ ← │MAX98357A │ ← │ Ring Buf │ ←─────────┘         │
│  │  3W     │   │  I2S OUT │   │ 32KB ×2  │    PCM Audio         │
│  └─────────┘   └──────────┘   └──────────┘                      │
└──────────────────────────────────────────────────────────────┘
```

## I2S Configuration

```c
// MIC Input (I2S0)
i2s_config_t mic_config = {
    .mode = I2S_MODE_MASTER | I2S_MODE_RX,
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,          // 1024 samples × 2 bytes × 1 ch = 2KB per buffer
    .use_apll = true,             // 精确时钟，避免采样率漂移
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0,
};

// Speaker Output (I2S0 — shared or I2S1 if simultaneous full-duplex)
i2s_config_t spk_config = {
    .mode = I2S_MODE_MASTER | I2S_MODE_TX,
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
    .dma_buf_count = 4,
    .dma_buf_len = 2048,
    .use_apll = true,
};
```

## VAD (Voice Activity Detection)

```c
// 基于 WebRTC VAD，判断语音起止
typedef enum {
    VAD_STATE_SILENCE,
    VAD_STATE_SPEECH_START,
    VAD_STATE_SPEECH_ONGOING,
    VAD_STATE_SPEECH_END,
} vad_state_t;

typedef struct {
    vad_state_t state;
    uint32_t silence_duration_ms;    // 连续静音时长
    uint32_t speech_duration_ms;     // 本次语音时长
} vad_result_t;
```

## Audio Manager API

```c
// 音频管理器初始化
esp_err_t audio_manager_init(void);

// 开始录音 → 云端 ASR
esp_err_t audio_capture_start(void);
esp_err_t audio_capture_stop(void);

// 播放 PCM 数据
esp_err_t audio_playback_start(const uint8_t *pcm_data, size_t len);
esp_err_t audio_playback_stop(void);

// 音量控制 (0-100)
esp_err_t audio_set_volume(uint8_t volume);

// 获取当前音频状态
audio_state_t audio_get_state(void);

// TTS 流式播放 (边接收边播放)
esp_err_t audio_tts_stream_play_start(void);
esp_err_t audio_tts_stream_feed(const uint8_t *chunk, size_t len);
esp_err_t audio_tts_stream_finish(void);
```

## Audio Streaming (WebSocket)

```c
// WebSocket 音频上行 (记录 → 云端)
// URL: wss://api.robotbuddy.local/v1/asr/stream
// 每次发送 320 bytes (10ms PCM @ 16kHz 16bit)

// WebSocket 音频下行 (云端 → TTS → 播放)
// URL: wss://api.robotbuddy.local/v1/tts/stream
// 每次接收 ~640 bytes (20ms PCM @ 16kHz 16bit)

typedef struct {
    char url[256];
    uint16_t chunk_size;        // 每次发送字节数
    uint16_t chunk_interval_ms; // 发送间隔
    uint32_t timeout_ms;
} audio_stream_config_t;
```

## Performance Targets

| 指标 | 目标 | 测量方法 |
|------|------|---------|
| 采集延迟 | < 10ms | 声→Ring Buffer 入口 |
| ASR 延迟 | < 500ms | 音频上传→文字返回 |
| LLM 延迟 | < 500ms | ASR 完成→回复文字 |
| TTS 首字延迟 | < 300ms | →首段 PCM 返回 |
| 端到端延迟 | < 1.5s | 用户说完→开始播放 |
| 音频丢帧率 | 0% | DMA underrun 计数 |
| 播放卡顿 | < 1次/10min | Ring Buffer 空事件 |

## Rules

1. I2S DMA buffer 至少 4 个，每个 ≥ 1024 samples
2. 使用 APLL 时钟源确保精确采样率
3. VAD 检测到静音 500ms 后判定语音结束
4. WebSocket 使用 TLS 加密传输音频数据
5. TTS 首段 PCM 到达即开始播放（流式，不等全部）
6. 离线模式下降级为本地提示音
7. 音频数据在 PSRAM 分配（减少 DRAM 压力）

## Checklist

- [ ] INMP441 采集波形正常（示波器确认）
- [ ] MAX98357A 播放无杂音/爆音
- [ ] VAD 准确率 ≥ 90%（静音/语音判定）
- [ ] ASR 识别准确率 ≥ 95%（安静环境）
- [ ] TTS 音质清晰、音量合适
- [ ] 全双工无冲突（I2S 共享模式验证）
- [ ] WiFi 波动下音频不丢帧（自动重传？）
- [ ] 长时间运行无 DMA 累积错误
