# Voice Skill — RobotBuddy

## Role

RobotBuddy 语音交互系统专家，负责唤醒词检测、ASR/TTS 服务选型与调度、对话流程状态机、全链路延迟优化和多语言切换。本 skill 关注业务逻辑和对话流程编排；底层 I2S 音频硬件由 `audio-pipeline` skill 负责；LLM Prompt 工程由 `ai-dialog` skill 负责。

## Domain

唤醒词检测 (ESP-SR/MultiNet)、VAD 语音活动检测 (WebRTC)、ASR 服务选型 (Whisper/讯飞/百度/阿里/CozTherapy)、LLM 对话路由、TTS 服务选型 (Edge-TTS/火山引擎/百度/微软Azure)、全链路延迟优化、多语言切换 (普通话/English)、方言识别 (V2+)。

## Goal

构建低延迟 (< 1.5s)、高准确率 (ASR >= 95%)、自然流畅的端到端语音交互体验，使 RobotBuddy 成为桌面上的"可对话伙伴"。

## Inputs

- PCM 音频流（来自 `audio-pipeline` skill 的 Ring Buffer，16kHz 16bit mono）
- VAD 语音段标记（来自 `audio-pipeline` skill 的 WebRTC VAD 结果）
- LLM 对话文本（来自 `ai-dialog` skill 的 LLM 回复）
- 用户偏好配置（语言选择、TTS 音色/语速、隐私模式开关）

## Outputs

- `firmware/app/voice_assistant/voice_manager.c` — 语音交互管理器（顶层状态机）
- `firmware/app/voice_assistant/wake_word.c` — 唤醒词检测模块
- `firmware/app/voice_assistant/asr_service.c` — ASR 服务抽象层 + 多厂商适配
- `firmware/app/voice_assistant/tts_service.c` — TTS 服务抽象层 + 多厂商适配
- `firmware/app/voice_assistant/voice_state_machine.c` — 语音状态机
- `firmware/app/voice_assistant/latency_monitor.c` — 全链路延迟监控
- `firmware/app/voice_assistant/language_switch.c` — 多语言切换
- `docs/architecture/voice-interaction.md` — 语音交互设计文档

## Voice Pipeline Architecture

```
┌──────────────────────────────────────────────────────────────────────────┐
│                         Voice Interaction Pipeline                        │
│                                                                           │
│  ┌──────────┐    ┌───────────┐    ┌───────────┐    ┌─────────────────┐  │
│  │ 用户语音  │ →  │ I2S Mic   │ →  │ Ring      │ →  │ VAD             │  │
│  │          │    │ (16kHz)   │    │ Buffer    │    │ (WebRTC)        │  │
│  └──────────┘    └───────────┘    └───────────┘    └────────┬────────┘  │
│                                                             │            │
│        ┌────────────────────────────────────────────────────┘            │
│        │  audio-pipeline skill: 硬件层 (I2S/DMA/编码)                    │
│  ─ ─ ─ ┼ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─       │
│        │  voice skill: 业务逻辑层 (唤醒/ASR/TTS 调度 + 状态机)           │
│        ↓                                                                 │
│  ┌──────────────────────────────────────────────────────────────┐        │
│  │                   Voice Manager (业务调度)                     │        │
│  │                                                               │        │
│  │  ┌───────────┐   ┌───────────┐   ┌───────────┐              │        │
│  │  │ Wake Word │ → │ ASR       │ → │ Dialog    │              │        │
│  │  │ Detection │   │ Service   │   │ Flow      │              │        │
│  │  │ ESP-SR    │   │ Selector  │   │ Controller│              │        │
│  │  └───────────┘   └─────┬─────┘   └─────┬─────┘              │        │
│  │                        │               │                     │        │
│  │               ┌────────┼───────────────┼───────┐            │        │
│  │               │ 云端   │               │ 云端  │            │        │
│  │               │ ┌──────↓──────┐  ┌─────↓─────┐│            │        │
│  │               │ │ ASR Cloud   │  │ TTS Cloud ││            │        │
│  │               │ │ Whisper/    │  │ Edge-TTS/ ││            │        │
│  │               │ │ 讯飞/百度   │  │ 火山引擎  ││            │        │
│  │               │ └──────┬──────┘  └─────┬─────┘│            │        │
│  │               └────────┼───────────────┼───────┘            │        │
│  └────────────────────────┼───────────────┼─────────────────────┘        │
│                           │               │                              │
│                           ↓               ↓                              │
│  ┌──────────────────────────────────────────────────────────────┐        │
│  │                     ai-dialog skill                           │        │
│  │  ┌──────────┐   ┌───────────┐   ┌──────────┐                │        │
│  │  │ Intent   │ → │ Prompt    │ → │ LLM      │                │        │
│  │  │ Classify │   │ Builder   │   │ Call     │                │        │
│  │  └──────────┘   └───────────┘   └──────────┘                │        │
│  └───────────────────────────────┬──────────────────────────────┘        │
│                                  │ LLM 文本回复                           │
│                                  ↓                                       │
│  ┌──────────────────────────────────────────────────────────────┐        │
│  │ 回传 voice skill                                              │        │
│  │  ┌───────────┐   ┌───────────┐   ┌───────────┐              │        │
│  │  │ Response  │ → │ TTS       │ → │ Ring      │ → I2S Speaker│        │
│  │  │ Segmenter │   │ Cloud     │   │ Buffer    │   (播放)     │        │
│  │  └───────────┘   └───────────┘   └───────────┘              │        │
│  └──────────────────────────────────────────────────────────────┘        │
└──────────────────────────────────────────────────────────────────────────┘
```

**与相邻 skill 的职责边界：**

| 层次 | audio-pipeline | **voice (本 skill)** | ai-dialog |
|------|----------------|---------------------|-----------|
| 硬件 | I2S Mic/Spk 驱动、DMA | 不涉及 | 不涉及 |
| 编码 | Opus/PCM 编解码 | 不涉及 | 不涉及 |
| VAD | WebRTC VAD 底层算法 | VAD 状态变化 → 触发行为 | 不涉及 |
| 唤醒词 | 不涉及 | ESP-SR 模型加载/检测逻辑 | 不涉及 |
| ASR | 音频流上传 (WebSocket) | **ASR 服务选型、供应商切换、结果解析** | 不涉及 |
| TTS | PCM 流式播放 | **TTS 服务选型、供应商切换、文本分段** | 不涉及 |
| LLM | 不涉及 | **将 ASR 文本传给 ai-dialog** | Prompt 构建、模型路由、回复解析 |
| 状态机 | 不涉及 | **顶层语音对话状态机** | 对话状态机（自己的） |
| 多语言 | 不涉及 | 语言检测 + 切换策略 | 多语言 System Prompt |

## Wake Word System

```c
// ── 唤醒词系统 (基于 ESP-SR) ──

// ESP-SR MultiNet 模型配置
// 模型类型: MultiNet 轻量级唤醒词模型
// 支持唤醒词: 3-5 个自定义词条，同时保持低误唤醒率

typedef enum {
    WAKE_WORD_ROBOT_BUDDY,   // "你好小伴" (主唤醒词)
    WAKE_WORD_HEY_BUDDY,     // "Hey Buddy" (英文唤醒词)
    WAKE_WORD_STOP,          // "停下" / "别说了" (中断指令)
    WAKE_WORD_CUSTOM_1,      // 用户自定义唤醒词 1
    WAKE_WORD_CUSTOM_2,      // 用户自定义唤醒词 2
} wake_word_id_t;

typedef struct {
    wake_word_id_t id;
    const char *text;              // 唤醒词文本
    const char *language;          // "zh" / "en"
    float threshold;               // 检测阈值 (0.0 ~ 1.0，默认 0.6)
    uint32_t cooldown_ms;          // 冷却时间，防止连续误触发 (默认 2000ms)
} wake_word_config_t;
```

```c
// MultiNet 初始化与配置
// ESP-SR 模型路径: spiffs:/models/wakenet_model.bin
// MultiNet 模型路径: spiffs:/models/multinet_model.bin

esp_err_t wake_word_init(void);

// 加载唤醒词模型
esp_err_t wake_word_load_model(const char *model_path);

// 添加自定义唤醒词（需重新训练模型或使用 MultiNet 在线适配）
esp_err_t wake_word_add_custom(const char *keyword, float threshold);

// 开始/停止唤醒词检测
esp_err_t wake_word_detection_start(void);
esp_err_t wake_word_detection_stop(void);

// 检测结果回调
typedef void (*wake_word_callback_t)(wake_word_id_t id, float confidence);
esp_err_t wake_word_set_callback(wake_word_callback_t cb);

// 获取当前误唤醒率统计
float wake_word_get_false_positive_rate(void);   // 每小时误唤醒次数
```

```c
// 离线唤醒 → 联网对话的过渡动画
// 1. 检测到唤醒词 → 眼睛亮起+表情变化 (emotion: EXCITED, 200ms)
// 2. 播放"滴"提示音 (audio-pipeline 本地音效)
// 3. 进入 LISTENING 状态，开始录音 (VAD 接管)
// 4. 如果 WiFi 未连接 → 播放离线提示音 "网络不可用" → 回到 IDLE
//
// 自定义唤醒词训练流程:
// 1. 用户在 App 输入新唤醒词文本
// 2. 云端生成 TTS 合成的正样本音频 (×100 变体)
// 3. 使用公开噪声/对话数据集构造负样本
// 4. 对 MultiNet 输出层进行微调 (transfer learning)
// 5. 量化 → INT8 → 下发到 ESP32 SPIFFS
// 6. 更新 wake_word_config_t 配置
```

```c
// 唤醒词检测性能优化
// 1. 使用 ESP32 的神经网络加速器 (ESP-NN)，INT8 量化推理
// 2. MultiNet 推理频率: 每 30ms 一帧 (16kHz × 30ms = 480 samples)
// 3. 两阶段检测: 粗筛 (低阈值 0.3) → 精筛 (高阈值 0.6)，减少误唤醒
// 4. 误唤醒率目标: < 1次/小时 (安静办公环境)
// 5. 唤醒响应时间: < 200ms (从说出到检测)
// 6. 噪声抑制: 使用 ESP-SR AEC (回声消除) 预处理
```

## ASR 选型对比

```c
// ASR 服务抽象层
typedef enum {
    ASR_PROVIDER_WHISPER_OPENAI,    // OpenAI Whisper API
    ASR_PROVIDER_WHISPER_LOCAL,     // 本地 Whisper.cpp (离线)
    ASR_PROVIDER_IFLYTEK,           // 讯飞语音听写
    ASR_PROVIDER_BAIDU,             // 百度语音识别
    ASR_PROVIDER_ALIBABA,           // 阿里云语音识别
    ASR_PROVIDER_COZTHERAPY,        // CozTherapy (玩具级，中文友好)
    ASR_PROVIDER_COUNT,
} asr_provider_t;

typedef struct {
    asr_provider_t provider;
    const char *name;
    const char *api_url;
    const char *api_key;            // 加密存储
    uint32_t timeout_ms;
    bool supports_streaming;        // 是否支持流式识别
    bool supports_offline;          // 是否支持离线识别
    uint8_t priority;               // 优先级 (0=最高，用于故障转移)
} asr_service_config_t;
```

| 指标 | Whisper (API) | Whisper.cpp (本地) | 讯飞语音 | 百度语音 | 阿里云 | CozTherapy |
|------|---------------|---------------------|---------|---------|--------|------------|
| **准确率 (中文)** | 95%+ | 90%+ (small) | 97% | 96% | 96% | 85% |
| **准确率 (英文)** | 98% | 93%+ (small) | 90% | 92% | 92% | 80% |
| **延迟 (首字)** | 300-800ms | 500-2000ms (CPU) | 200-500ms | 200-400ms | 200-400ms | 300-600ms |
| **价格** | $0.006/min | 免费 | ¥3.5/万次 | ¥2.0/万次 | ¥3.0/万次 | ¥0.5/万次 |
| **离线支持** | 否 | 是 (本地推理) | 否 | 否 (有离线SDK) | 否 | 否 |
| **流式识别** | 否 (需完整音频) | 否 (需完整音频) | 是 (实时流) | 是 (实时流) | 是 (实时流) | 否 |
| **中文质量** | 优秀 (large-v3) | 良好 | 优秀 (行业最佳) | 优秀 | 优秀 | 一般 |
| **方言支持** | 有限 | 有限 | 粤语/四川话/河南话 | 粤语/四川话 | 粤语 | 无 |
| **适用场景** | 非实时/高精度 | 离线/隐私 | 实时/中文优先 | 实时/性价比 | 生态集成 | 玩具/低成本 |

```c
// ASR 服务选择策略
// 默认: 讯飞 (实时流式 + 中文最优)
// 英文对话: Whisper API (英文精度最高)
// 离线模式: Whisper.cpp small (本地), 回退到本地关键词
// 故障转移: 讯飞超时 3s → 百度 → Whisper API
//
// API:
esp_err_t asr_service_init(asr_provider_t default_provider);
esp_err_t asr_service_recognize(const uint8_t *pcm_data, size_t len,
                                 char *text_out, size_t max_len);
esp_err_t asr_service_recognize_stream_start(void);   // 流式识别开始
esp_err_t asr_service_recognize_stream_feed(const uint8_t *chunk, size_t len);
esp_err_t asr_service_recognize_stream_stop(char *text_out, size_t max_len);
asr_provider_t asr_service_get_current(void);
esp_err_t asr_service_switch_provider(asr_provider_t provider);
```

## TTS 选型对比

```c
typedef enum {
    TTS_PROVIDER_EDGE_TTS,          // Microsoft Edge TTS (免费)
    TTS_PROVIDER_VOLCENGINE,        // 火山引擎 (字节跳动)
    TTS_PROVIDER_BAIDU,             // 百度语音合成
    TTS_PROVIDER_AZURE,             // 微软 Azure Speech
    TTS_PROVIDER_COUNT,
} tts_provider_t;

typedef struct {
    tts_provider_t provider;
    const char *name;
    const char *api_url;
    const char *api_key;
    const char *default_voice;      // 默认音色 ID
    uint32_t timeout_ms;
    bool supports_streaming;        // 是否支持流式合成
    bool supports_ssml;             // 是否支持 SSML 标记
} tts_service_config_t;
```

| 指标 | Edge-TTS | 火山引擎 | 百度 TTS | 微软 Azure |
|------|----------|---------|---------|------------|
| **音色自然度** | 良好 (4.0/5) | 优秀 (4.5/5) | 良好 (4.0/5) | 优秀 (4.5/5) |
| **中文音色数** | 5+ | 30+ | 10+ | 10+ |
| **延迟 (首字节)** | 200-400ms | 100-250ms | 150-300ms | 150-300ms |
| **流式合成** | 是 (分句流式) | 是 (SSML 流) | 是 | 是 |
| **价格** | 免费 | ¥2.0/万字 | ¥2.0/万字 | $15/百万字符 |
| **SSML 支持** | 有限 | 完整 (停顿/重音/语速) | 完整 | 完整 |
| **离线支持** | 否 | 否 | 否 (有离线SDK) | 否 |
| **中英混合** | 良好 | 优秀 | 良好 | 优秀 |
| **适用场景** | 开发/免费 | 商业级音质 | 中文优先/性价比 | 企业/多语言 |

```c
// TTS 服务选择策略
// 默认: Edge-TTS (免费 + 流式 + 音色不错)
// 商业部署: 火山引擎 (中文音色最优 + 低延迟)
// 中英混合: 火山引擎 / Azure
// 离线模式: 本地预合成提示音包 (spiffs:/audio/prompts/)
//
// TTS API:
esp_err_t tts_service_init(tts_provider_t default_provider);
esp_err_t tts_service_synthesize(const char *text, uint8_t **pcm_out, size_t *len);
esp_err_t tts_service_synthesize_stream_start(const char *voice_id);
esp_err_t tts_service_synthesize_stream_feed(const char *text_segment);  // 流式送入文本
esp_err_t tts_service_synthesize_stream_finish(void);                     // 等待最后一段 PCM
tts_provider_t tts_service_get_current(void);
esp_err_t tts_service_switch_provider(tts_provider_t provider);

// TTS 文本预处理: 长文本→分段
// 按标点符号断句 (.。！？!?\n)，每段 < 200 字符
esp_err_t tts_text_segment(const char *full_text, char segments[][256], uint8_t *count);
```

## Voice State Machine

```c
// ── 顶层语音对话状态机 ──
// 此状态机管理从唤醒到回复完成的完整语音交互生命周期

typedef enum {
    VOICE_STATE_IDLE,            // 空闲，等待唤醒词
    VOICE_STATE_WAKING,          // 检测到唤醒词，过渡动画中
    VOICE_STATE_LISTENING,       // 用户正在说话 (VAD 检测到语音)
    VOICE_STATE_VAD_SILENCE,     // 用户停止说话，等待 VAD 尾静音确认 (500ms)
    VOICE_STATE_PROCESSING_ASR,  // 音频已上传，等待 ASR 文字结果
    VOICE_STATE_THINKING,        // LLM 推理中 (ai-dialog skill)
    VOICE_STATE_SPEAKING,        // TTS 播放中
    VOICE_STATE_ERROR,           // 出错 (网络/超时/服务失败)
} voice_state_t;

// 状态转换表
//
// IDLE ──[唤醒词检测]──→ WAKING
// WAKING ──[动画完成]──→ LISTENING
// WAKING ──[WiFi离线]──→ ERROR (播放离线提示音) → IDLE
// LISTENING ──[VAD: speech_start]──→ (保持 LISTENING)
// LISTENING ──[VAD: silence 500ms]──→ VAD_SILENCE
// LISTENING ──[60s 无语音]──→ IDLE (超时收起)
// VAD_SILENCE ──[确认静音]──→ PROCESSING_ASR
// VAD_SILENCE ──[重新检测到语音]──→ LISTENING
// PROCESSING_ASR ──[ASR 成功]──→ THINKING
// PROCESSING_ASR ──[ASR 超时 3s]──→ ERROR → IDLE
// THINKING ──[LLM 首段回复到达]──→ SPEAKING (流式 TTS)
// THINKING ──[LLM 超时 5s]──→ ERROR ("我还在想...") → 重试 1 次 → IDLE
// SPEAKING ──[TTS 播放完成]──→ IDLE
// SPEAKING ──[唤醒词/按钮中断]──→ IDLE (打断播放)
//
// 任何状态 ──[唤醒词]──→ WAKING (高优先级中断)
// 任何状态 ──[WiFi 断开]──→ ERROR → IDLE

// 情绪联动 (与 behavior-system skill 协调)
// IDLE:           EMOTION_IDLE
// WAKING:         EMOTION_EXCITED
// LISTENING:      EMOTION_LISTENING (眼睛微动，注意力集中)
// VAD_SILENCE:    EMOTION_THINKING (眼睛斜视，思考状)
// PROCESSING_ASR: EMOTION_THINKING
// THINKING:       EMOTION_THINKING
// SPEAKING:       EMOTION_ANSWERING (嘴部随 RMS 动)
// ERROR:          EMOTION_ERROR
```

```c
// Voice State Machine API
typedef struct {
    voice_state_t current_state;
    voice_state_t previous_state;
    uint32_t state_enter_time_ms;       // 进入当前状态的时间戳
    uint32_t state_timeout_ms;          // 当前状态的超时时间
    char asr_text[512];                 // 最新 ASR 识别文本
    char llm_response[2048];            // LLM 最新回复
    emotion_id_t linked_emotion;        // 当前关联的表情 ID
    bool wifi_connected;                // WiFi 连接状态
    bool privacy_mode;                  // 隐私模式 (不录音)
} voice_state_context_t;

esp_err_t voice_state_machine_init(void);
esp_err_t voice_state_transition(voice_state_t new_state);
voice_state_t voice_state_get_current(void);
esp_err_t voice_state_get_context(voice_state_context_t *ctx);

// 事件触发 (由各模块调用)
esp_err_t voice_event_wake_word_detected(wake_word_id_t id, float confidence);
esp_err_t voice_event_vad_speech_start(void);
esp_err_t voice_event_vad_silence_detected(uint32_t silence_duration_ms);
esp_err_t voice_event_asr_complete(const char *text);
esp_err_t voice_event_llm_first_token(void);
esp_err_t voice_event_tts_complete(void);
esp_err_t voice_event_error(const char *reason);
esp_err_t voice_event_wifi_disconnected(void);
esp_err_t voice_event_wifi_reconnected(void);
```

## 全链路延迟分析

整个语音交互链路的延迟从用户停止说话到机器人开始播放回复。以下是每个阶段的延迟预算和优化策略：

| 阶段 | 目标延迟 | 优化策略 |
|------|---------|---------|
| Mic → Ring Buffer | < 10ms | I2S DMA 双缓冲，零拷贝 |
| Ring Buffer → VAD 判定 | < 30ms | WebRTC VAD (最快模式)，30ms 帧长 |
| VAD 尾静音 → 唤醒确认 | < 200ms | ESP-SR MultiNet INT8，两阶段级联检测 |
| 音频上传 → ASR 首字 | < 500ms | 流式 WebSocket，讯飞实时流 (默认)；降级 Whisper 需等待完整音频 |
| ASR 文本 → LLM 首 Token | < 500ms | 流式 HTTP SSE，选择低延迟模型 (DeepSeek-V3 / GPT-4o mini) |
| LLM 回复 → TTS 首字节 | < 300ms | Edge-TTS 分句流式，不等全部合成；火山引擎 SSML 流 |
| **端到端总计** | **< 1.5s** | 并行化流式管道：ASR 边传边识别、LLM 边生成边分段 TTS |

```c
// 延迟监控
typedef struct {
    uint32_t mic_to_buffer_us;          // Mic → Ring Buffer
    uint32_t vad_detection_us;          // VAD 判定
    uint32_t wake_word_us;              // 唤醒词检测
    uint32_t asr_upload_us;             // 音频上传
    uint32_t asr_recognition_us;        // ASR 识别
    uint32_t asr_total_us;              // ASR 总延迟
    uint32_t llm_first_token_us;        // LLM 首 Token
    uint32_t llm_total_us;              // LLM 总延迟
    uint32_t tts_first_byte_us;         // TTS 首字节
    uint32_t tts_total_us;              // TTS 总延迟
    uint32_t e2e_total_us;              // 端到端总延迟
    uint8_t wifi_rssi;                  // 当前 WiFi 信号强度 (dBm)
    const char *slowest_stage;          // 瓶颈阶段名称
} voice_latency_report_t;

esp_err_t voice_latency_monitor_init(void);
esp_err_t voice_latency_mark(const char *stage);  // 打点
esp_err_t voice_latency_get_report(voice_latency_report_t *report);
esp_err_t voice_latency_reset(void);

// 延迟监控集成：在状态机每个转换点调用 voice_latency_mark()
// LISTENING → VAD_SILENCE:    mark("vad_end")
// VAD_SILENCE → PROCESSING_ASR: mark("asr_start")
// PROCESSING_ASR → THINKING:  mark("asr_done")
// THINKING → SPEAKING:        mark("llm_first_token")
// SPEAKING → IDLE:            mark("tts_done")
```

```c
// 进阶优化策略
// 1. 流式全链路: 音频边采边传 → ASR 边传边识别 → LLM 边生成边分段 → TTS 边合成边播放
// 2. 预连接: 在 WAKING 状态即建立 ASR/TTS 的 WebSocket 连接（减少握手延迟 200-500ms）
// 3. 音频分块: 每 30ms 发送一个音频 chunk，不等 VAD 结束就开始上传
// 4. TTS 首句缓存: 常用回复 (如"好的"、"请稍等") 预合成 TTS 缓存到本地
// 5. WiFi 优化: 保持长连接 + TCP Keep-Alive 30s；WiFi 省电模式切换为高性能模式
// 6. 模型选择: 简单闲聊用 DeepSeek-V3 (首 Token 快)，复杂问题才用 Claude-Opus
```

## 多语言支持

```c
// 多语言管理
typedef enum {
    LANG_ZH_MANDARIN,           // 简体中文/普通话 (默认)
    LANG_EN_US,                 // 英语 (美国)
    LANG_ZH_CANTONESE,          // 粤语 (V2+ / ASR 供应商支持)
    LANG_ZH_SICHUAN,            // 四川话 (V2+)
    LANG_JA_JP,                 // 日语 (V3+)
    LANG_KO_KR,                 // 韩语 (V3+)
    LANG_MIXED_ZH_EN,           // 中英混合模式 (自动检测)
} voice_language_t;

typedef struct {
    voice_language_t language;
    const char *wake_word;              // 当前唤醒词
    asr_provider_t preferred_asr;       // 最佳 ASR 供应商
    tts_provider_t preferred_tts;       // 最佳 TTS 供应商
    const char *tts_voice_id;           // 推荐音色 ID
    const char *system_prompt_lang;     // System Prompt 语言提示
} language_profile_t;

// 语言配置文件
static const language_profile_t LANGUAGE_PROFILES[] = {
    // 普通话
    [LANG_ZH_MANDARIN] = {
        .language = LANG_ZH_MANDARIN,
        .wake_word = "你好小伴",
        .preferred_asr = ASR_PROVIDER_IFLYTEK,
        .preferred_tts = TTS_PROVIDER_EDGE_TTS,
        .tts_voice_id = "zh-CN-XiaoxiaoNeural",
        .system_prompt_lang = "请用中文回复，语气亲切自然。",
    },
    // 英语
    [LANG_EN_US] = {
        .language = LANG_EN_US,
        .wake_word = "Hey Buddy",
        .preferred_asr = ASR_PROVIDER_WHISPER_OPENAI,
        .preferred_tts = TTS_PROVIDER_EDGE_TTS,
        .tts_voice_id = "en-US-JennyNeural",
        .system_prompt_lang = "Please reply in English, with a friendly and natural tone.",
    },
    // 中英混合
    [LANG_MIXED_ZH_EN] = {
        .language = LANG_MIXED_ZH_EN,
        .wake_word = "你好小伴",
        .preferred_asr = ASR_PROVIDER_IFLYTEK,
        .preferred_tts = TTS_PROVIDER_VOLCENGINE,
        .tts_voice_id = "zh_female_xiaoyuan_emo",
        .system_prompt_lang = "请用中文回复（中英混合对话场景，技术术语可保留英文）。",
    },
};

// 语言切换 API
esp_err_t voice_language_init(void);
esp_err_t voice_language_switch(voice_language_t new_lang);
voice_language_t voice_language_get_current(void);
const language_profile_t *voice_language_get_profile(void);

// 自动语言检测 (V2+)
// 1. 检测唤醒词语种 → 设定初始语言
// 2. ASR 文本第一句 → 语言识别 (fastText / langdetect)
// 3. 中英混合: 自动切换 ASR/TTS 供应商 + 调整 System Prompt
// 4. 方言识别: 讯飞方言模型 → 自动映射到对应语言配置
// 5. 用户手动切换: 语音指令 "切换到英文" / App 设置
```

## Rules

1. **唤醒词可打断** — 语音播放中检测到唤醒词，立即停止播放并进入 WAKING 状态；任何状态都可被唤醒词中断
2. **背景噪音自适应** — VAD 阈值根据环境噪音 RMS 动态调整（每 5s 采样一次安静帧的 RMS 作为基线，语音起始阈值 = 基线 × 2.5）
3. **WiFi 断线处理** — 实时监测 WiFi 连接状态；断线后立即播放本地预合成提示音 "网络连接已断开，请检查网络"；恢复连接后自动回到 IDLE 待命
4. **隐私模式** — 支持"不录音"隐私模式：唤醒后仅通过 App 文本输入交互（TTS 仍可用）；隐私模式下 LED 指示灯为红色，作为明确的"未录音"信号；用户可通过 App 或物理按钮切换
5. **ASR 故障转移** — 主 ASR 超时 3s → 自动切换备用 ASR；所有云端 ASR 不可用 → 离线关键词匹配回退 (ai-dialog 的 INTENT_ROBOT_CONTROL)
6. **TTS 故障转移** — 主 TTS 超时 2s → 自动切换备用 TTS；所有云端 TTS 不可用 → 屏幕显示文本（display-engine skill）
7. **流式优先** — ASR/TTS 优先采用流式接口，避免等待完整数据，减少端到端延迟
8. **状态机超时保护** — 每个语音状态设置最大超时：LISTENING 60s、PROCESSING_ASR 9s、THINKING 15s、SPEAKING 30s；超时自动回到 IDLE 并上报错误
9. **不录音时不上传** — 隐私模式下，音频数据不离开设备（不写入 Ring Buffer 读取端，不发起 WebSocket 连接）
10. **延迟监控** — 全链路每阶段记录耗时，超过预算 50% 时输出 WARNING 日志，超过 100% 时触发优化告警

## Checklist

- [ ] ESP-SR 唤醒词模型加载成功，唤醒响应 < 200ms
- [ ] MultiNet 自定义唤醒词可正常检测 (≥95% 准确率)
- [ ] 误唤醒率 < 1次/小时 (安静办公环境测试 8 小时)
- [ ] 离线唤醒 → 联网对话过渡动画流畅自然
- [ ] VAD 语音起始/结束检测准确率 ≥ 90%
- [ ] ASR 中文识别准确率 ≥ 95% (讯飞/百度，安静环境)
- [ ] ASR 英文识别准确率 ≥ 95% (Whisper)
- [ ] ASR 故障转移正常 (主→备→离线降级)
- [ ] TTS 音质清晰，中/英文发音自然
- [ ] TTS 流式播放首字节延迟 < 300ms
- [ ] TTS 故障转移正常 (主→备→屏幕显示降级)
- [ ] 语音状态机全生命周期运行正确 (IDLE→WAKING→...→IDLE)
- [ ] 唤醒词可打断任何状态的 TTS 播放
- [ ] 端到端延迟 < 1.5s (安静网络环境)
- [ ] WiFi 断线 → 离线提示音正常播放
- [ ] WiFi 恢复 → 自动回到 IDLE 待命
- [ ] 隐私模式下不录音、不上传音频
- [ ] 中英文切换正常 (唤醒词 + ASR 供应商 + TTS 音色联动切换)
- [ ] 背景噪音自适应 VAD 阈值生效 (安静→嘈杂环境切换)
- [ ] 长时间运行 (2h) 语音系统无内存泄漏 / 状态机无死锁
- [ ] 状态机各阶段超时保护生效 (LISTENING/ASR/THINKING/SPEAKING)
- [ ] 全链路延迟监控数据记录正确 (每个阶段的 us 级耗时)
