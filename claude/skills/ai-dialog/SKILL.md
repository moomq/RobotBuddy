# AI Dialog Skill — RobotBuddy

## Role

RobotBuddy AI 对话引擎专家，负责多模型 LLM 路由、上下文管理、Prompt 工程和对话流程控制。

## Domain

大语言模型 API（Claude / GPT-4 / DeepSeek）、语音对话流程管理、上下文记忆、Prompt 工程、多轮对话、离线降级策略。

## Goal

构建流畅、智能、低延迟的 AI 编程助手对话体验——让 RobotBuddy 成为开发者桌面上真正有用的编程伙伴。

## Inputs

- ASR 转写文本（来自 audio-pipeline skill）
- 云端 AI API 规范和密钥配置
- 用户偏好（默认模型、语言、专业领域）
- 对话历史

## Outputs

- `firmware/app/ai_dialog/ai_dialog_manager.c` — AI 对话管理器
- `firmware/app/ai_dialog/prompt_builder.c` — Prompt 构建器
- `firmware/app/ai_dialog/context_manager.c` — 上下文管理器
- `firmware/app/ai_dialog/model_router.c` — 模型路由器
- `firmware/app/ai_dialog/offline_handler.c` — 离线降级处理
- `docs/cloud/ai-dialog-design.md` — AI 对话设计文档

## AI Dialog Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                     AI Dialog System                          │
│                                                               │
│  ┌──────────────┐                                             │
│  │ ASR Input    │  语音转文字结果                               │
│  └──────┬───────┘                                             │
│         ↓                                                     │
│  ┌──────────────┐                                             │
│  │ Intent       │  意图分类（编程/闲聊/控制/查询）               │
│  │ Classifier   │  本地关键词匹配 → 轻量分类                     │
│  └──────┬───────┘                                             │
│         ↓                                                     │
│  ┌──────────────┐                                             │
│  │ Prompt       │  构建 System Prompt + 注入上下文              │
│  │ Builder      │  ┌─────────────────────────────────┐       │
│  │              │  │ System: 你是 RobotBuddy...       │       │
│  │              │  │ Context: 用户在写 Python 项目... │       │
│  │              │  │ History: 最近 5 轮对话摘要       │       │
│  │              │  │ User: [ASR 文本]                 │       │
│  │              │  └─────────────────────────────────┘       │
│  └──────┬───────┘                                             │
│         ↓                                                     │
│  ┌──────────────┐                                             │
│  │ Model        │  选择最优模型                                │
│  │ Router       │  ┌─────────┬─────────┬──────────┐          │
│  │              │  │ Claude  │ GPT-4o  │ DeepSeek │          │
│  │              │  │ (编程强) │ (通用)  │ (性价比) │          │
│  │              │  └────┬────┴────┬────┴────┬─────┘          │
│  └──────┬───────┘     │         │         │                  │
│         ↓              ↓         ↓         ↓                  │
│  ┌──────────────┐                                             │
│  │ Cloud API    │  HTTP/WS 调用（cloud-communication skill）   │
│  │ Client       │                                             │
│  └──────┬───────┘                                             │
│         ↓                                                     │
│  ┌──────────────┐                                             │
│  │ Response     │  解析 LLM 回复 → 分段 TTS                    │
│  │ Processor    │  ├── 提取代码片段 → 屏幕显示                  │
│  │              │  ├── 长回复分段 → 逐段 TTS                    │
│  │              │  └── 代码块标记 → 语音读"代码开始...代码结束" │
│  └──────┬───────┘                                             │
│         ↓                                                     │
│  ┌──────────────┐                                             │
│  │ Context      │  更新对话历史 + 摘要压缩                      │
│  │ Manager      │                                             │
│  └──────────────┘                                             │
└──────────────────────────────────────────────────────────────┘
```

## Intent Classification

```c
// 意图分类（本地关键词匹配，无需联网）
typedef enum {
    INTENT_CODE_EXPLAIN,     // 代码解释："解释一下这段代码"
    INTENT_CODE_DEBUG,       // Bug 调试："为什么报错"
    INTENT_CODE_REVIEW,      // 代码审查："帮我看看这段代码"
    INTENT_CODE_WRITE,       // 代码生成："写一个排序函数"
    INTENT_TECH_QUESTION,    // 技术问答："React 和 Vue 哪个好"
    INTENT_COMMAND_QUERY,    // 命令速查："docker 怎么查看日志"
    INTENT_ARCHITECTURE,     // 架构建议："微服务用什么消息队列"
    INTENT_GENERAL_CHAT,     // 闲聊
    INTENT_ROBOT_CONTROL,    // 机器人控制（本地处理，不需 LLM）
    INTENT_UNKNOWN,          // 未知意图
} intent_type_t;

typedef struct {
    intent_type_t type;
    float confidence;
    const char *keyword_matched;
} intent_result_t;

// 关键词匹配规则
static const struct {
    intent_type_t type;
    const char *keywords[5];
} INTENT_KEYWORDS[] = {
    { INTENT_CODE_EXPLAIN,  {"解释", "explain", "这段代码", "原理", "什么意思"} },
    { INTENT_CODE_DEBUG,    {"报错", "bug", "error", "为什么", "调试"} },
    { INTENT_CODE_REVIEW,   {"审查", "review", "看看", "优化", "改进"} },
    { INTENT_CODE_WRITE,    {"写一个", "实现", "生成", "create", "write"} },
    { INTENT_TECH_QUESTION, {"哪个好", "区别", "对比", "选型", "vs"} },
    { INTENT_COMMAND_QUERY, {"怎么", "命令", "如何", "怎么用", "how to"} },
    { INTENT_ARCHITECTURE,  {"架构", "设计", "方案", "微服务", "系统"} },
    { INTENT_ROBOT_CONTROL, {"停下", "过来", "转圈", "睡觉", "番茄钟"} },
};
```

## Prompt Builder

```c
// System Prompt 模板
#define SYSTEM_PROMPT_BASE \
    "你是 RobotBuddy，一个放在开发者桌面上的 AI 编程助手机器人。\n" \
    "你的特点：\n" \
    "- 专注编程和技术话题，回答简洁专业\n" \
    "- 可以解释代码、调试错误、建议架构\n" \
    "- 语音回复，所以避免长段落，用短句和列表\n" \
    "- 代码片段请用「代码开始」和「代码结束」标记\n" \
    "- 中文为主，技术术语保留英文\n"

// 按意图定制的 System Prompt 扩展
static const char *INTENT_PROMPTS[] = {
    [INTENT_CODE_EXPLAIN]  = "用户在询问代码解释，请逐行或逐段解释，重点说清为什么而非做什么。",
    [INTENT_CODE_DEBUG]    = "用户遇到了 Bug，请先分析可能原因，给出最可能的修复方案，附带代码示例。",
    [INTENT_CODE_REVIEW]   = "用户请求代码审查，请从正确性、性能、可维护性三方面评价，指出具体问题。",
    [INTENT_CODE_WRITE]    = "用户需要写代码，请给出可直接使用的代码，附简短说明。",
    [INTENT_TECH_QUESTION] = "用户在做技术选型，请客观对比优劣，给出适用场景推荐。",
    [INTENT_COMMAND_QUERY] = "用户查询命令用法，请给出命令+参数+示例，简洁直接。",
    [INTENT_ARCHITECTURE]  = "用户咨询架构设计，请先了解场景再给建议，画简单的文字架构图。",
};

// Prompt 构建 API
esp_err_t prompt_build(intent_type_t intent, const char *user_text,
                        const char *context, char *out_prompt, size_t max_len);
```

## Context Manager

```c
// 对话上下文管理
#define MAX_CONTEXT_ROUNDS      10      // 保留最近 10 轮对话
#define MAX_CONTEXT_TOKENS      2048    // 上下文 Token 预算
#define SUMMARY_THRESHOLD       6       // 超过 6 轮时压缩摘要

typedef struct {
    char role[8];           // "user" / "assistant" / "system"
    char content[512];      // 对话内容（截断）
    uint32_t timestamp;     // 时间戳
} dialog_turn_t;

typedef struct {
    dialog_turn_t turns[MAX_CONTEXT_ROUNDS];
    uint8_t turn_count;
    char summary[256];      // 历史对话摘要（压缩后）
    char user_context[128]; // 用户当前项目/语言上下文
} dialog_context_t;

// 上下文管理 API
esp_err_t context_manager_init(void);
esp_err_t context_add_turn(const char *role, const char *content);
esp_err_t context_get_for_prompt(char *out, size_t max_len);
esp_err_t context_compact(void);         // 压缩旧对话为摘要
esp_err_t context_clear(void);
esp_err_t context_set_user_info(const char *project, const char *language);
```

## Model Router

```c
// 支持的 AI 模型
typedef enum {
    MODEL_CLAUDE_SONNET,    // 日常对话（性价比最优）
    MODEL_CLAUDE_OPUS,      // 复杂编程问题
    MODEL_GPT4O,            // 备用模型
    MODEL_DEEPSEEK_V3,      // 高性价比编程
    MODEL_DEEPSEEK_CODER,   // 代码专用
} ai_model_t;

// 模型配置
typedef struct {
    ai_model_t model;
    const char *api_url;
    const char *model_id;
    uint16_t max_tokens;
    float temperature;
    bool stream;                // 是否流式响应
    uint32_t timeout_ms;
    uint8_t priority;           // 优先级（0=最高）
} model_config_t;

// 模型路由策略
// ┌──────────────────┬─────────────────────┐
// │ 意图              │ 推荐模型             │
// ├──────────────────┼─────────────────────┤
// │ CODE_DEBUG       │ DeepSeek-Coder       │
// │ CODE_WRITE       │ DeepSeek-Coder       │
// │ CODE_REVIEW      │ Claude-Sonnet        │
// │ ARCHITECTURE     │ Claude-Opus          │
// │ GENERAL_CHAT     │ Claude-Sonnet        │
// │ 其他             │ Claude-Sonnet (默认)  │
// └──────────────────┴─────────────────────┘

// 模型路由 API
esp_err_t model_router_init(void);
ai_model_t model_router_select(intent_type_t intent);
esp_err_t model_router_call(ai_model_t model, const char *prompt,
                             char *response, size_t max_len);

// 故障转移
// 主模型超时/失败 → 自动切换到备用模型
// 顺序: Claude → GPT-4o → DeepSeek
esp_err_t model_router_call_with_fallback(intent_type_t intent,
                                           const char *prompt,
                                           char *response, size_t max_len);
```

## Response Processor

```c
// LLM 回复后处理
typedef struct {
    char text[2048];           // 纯文本内容
    char code_snippets[4][256]; // 提取的代码片段（最多4个）
    uint8_t code_count;
    bool has_code;
    uint16_t total_tokens;
} processed_response_t;

// 回复处理流程:
// 1. 解析 LLM JSON 响应
// 2. 提取文本和代码块（```...``` 标记）
// 3. 文本分段（按句号/换行分割，每段 < 200 字符，适合 TTS）
// 4. 代码片段发送到屏幕显示
// 5. 文本分段发送到 TTS 逐段播放

esp_err_t response_process(const char *raw_response, processed_response_t *out);
```

## Offline Handler

```c
// 离线降级策略
typedef enum {
    OFFLINE_MODE_NONE,       // 在线
    OFFLINE_MODE_LIMITED,    // WiFi 断开，本地功能可用
    OFFLINE_MODE_MINIMAL,    // 云端不可用，仅本地提示
} offline_mode_t;

// 离线时可用的本地功能
// ├── 唤醒词检测 (ESP-SR)
// ├── 本地关键词识别 → 机器人控制命令
// ├── 预设提示音和表情
// ├── 本地时间/番茄钟
// └── "网络不可用，请检查 WiFi" 语音提示

esp_err_t offline_handler_init(void);
offline_mode_t offline_get_mode(void);
esp_err_t offline_handle_input(const char *text);
```

## Dialog Flow

```c
// 完整对话流程状态机
typedef enum {
    DIALOG_STATE_IDLE,           // 等待唤醒
    DIALOG_STATE_LISTENING,      // 录音中
    DIALOG_STATE_PROCESSING_ASR, // ASR 处理中
    DIALOG_STATE_CLASSIFYING,    // 意图分类
    DIALOG_STATE_CALLING_LLM,    // LLM 调用中
    DIALOG_STATE_SPEAKING,       // TTS 播放中
    DIALOG_STATE_ERROR,          // 出错
} dialog_state_t;

// 状态转换:
// IDLE → LISTENING:        唤醒词检测 / 按钮触发
// LISTENING → PROCESSING:  VAD 检测到语音结束
// PROCESSING → CLASSIFYING: ASR 返回文本
// CLASSIFYING → CALLING:   意图分类完成，调用 LLM
// CALLING → SPEAKING:      LLM 首段回复到达
// SPEAKING → IDLE:         TTS 播放完成
//
// 任何状态 → LISTENING:    用户中断（唤醒词）
// 任何状态 → ERROR:        超时/网络错误 → 离线提示 → IDLE
```

## Rules

1. **语音友好** — 回复控制在 3-5 句话内，长回复自动分段
2. **代码标记** — 代码块用「代码开始/结束」标记，触发屏幕显示
3. **上下文预算** — 对话历史 Token 总数 ≤ 2048，超出自动压缩摘要
4. **故障转移** — 主模型超时 5s → 自动切换备用模型
5. **离线友好** — 网络不可用时给出明确提示，保留本地功能
6. **隐私保护** — 对话历史不上传到非选择的 AI 服务商
7. **流式响应** — 支持 LLM 流式输出，首字到达即开始 TTS
8. **可中断** — 用户随时可通过唤醒词打断当前回复

## Checklist

- [ ] 意图分类准确率 ≥ 85%（本地关键词匹配）
- [ ] Prompt 构建正确（System + Context + History + User）
- [ ] Claude API 调用成功并返回有效回复
- [ ] GPT-4o / DeepSeek 备用切换正常
- [ ] 代码片段正确提取并显示到屏幕
- [ ] 长回复分段 TTS 播放自然
- [ ] 上下文压缩后对话连贯性保持
- [ ] WiFi 断开时离线降级到本地模式
- [ ] 流式响应首字延迟 < 500ms
- [ ] 用户可中断当前回复
- [ ] 对话历史 NVS 持久化（重启后保留最近对话）
