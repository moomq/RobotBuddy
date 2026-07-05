# ESP32 桌面 AI Work Buddy 产品需求文档（PRD V2.0）

## 1. 产品定位

**AI Work Buddy------基于 ESP32 的桌面智能工作伙伴。**

融合： - 桌面陪伴机器人 - AI Agent - Claude Work Buddy 设计理念 -
云端大模型 - 可扩展 Skills 插件系统

## 2. 核心目标

-   AI 对话
-   情绪陪伴
-   桌面移动
-   工作协助
-   与 PC 联动
-   长期记忆

## 3. 基础功能

### 3.1 显示

-   表情
-   动画
-   状态
-   UI（LVGL）

### 3.2 语音

-   唤醒
-   ASR
-   LLM
-   TTS

### 3.3 移动

-   双轮差速
-   避障
-   巡逻

### 3.4 联网

-   WiFi
-   OTA
-   App
-   Web

## 4. Claude Work Buddy 扩展需求

### 4.1 Workspace Mode

-   Idle
-   Listening
-   Thinking
-   Talking
-   Working
-   Notification
-   Sleep

每个状态对应： - 表情 - 动画 - 灯光 - 动作

### 4.2 AI Agent

Skills： - Weather - Calendar - Todo - GitHub - VSCode - Browser -
Desktop - Music

### 4.3 Desktop Agent

ESP32 \<--WiFi--\> Windows Desktop Agent

Desktop Agent： - MCP - Claude Code - 文件系统 - 浏览器 - Git - VSCode

### 4.4 Claude Code 联动

支持： - Build - Test - Git Pull - Git Push - CI - Code Review

机器人同步显示： - Thinking - Success - Error

### 4.5 工作提醒

-   喝水
-   休息
-   开会
-   番茄钟
-   Todo

### 4.6 Personality

包含： - 昵称 - 性格 - 情绪 - 亲密度 - 说话风格

### 4.7 Memory

长期保存： - 用户偏好 - 常用项目 - IDE - Git 用户 - 工作习惯

### 4.8 Emotion Engine

维护： - Mood - Energy - Friendship - Busy - Curiosity

### 4.9 Multi-Agent

支持： - ChatGPT - Claude - DeepSeek - Gemini

统一调度。

## 5. 软件架构

    Cloud AI
        │
    Desktop Agent
    (MCP/Claude Code)
        │
    WiFi
        │
    ESP32-S3
     ├─Voice
     ├─Display
     ├─Motion
     ├─Emotion
     ├─Sensor
     └─Skills

## 6. 推荐硬件

-   ESP32-S3
-   ST7789 IPS
-   INMP441
-   MAX98357A
-   DRV8833
-   N20 电机
-   18650
-   VL53L0X

## 7. 开发路线

V1： - AI聊天 - 表情 - 双轮 - WiFi

V2： - Desktop Agent - Claude Code - Skills

V3： - Memory - 多Agent - 视觉AI - SLAM
