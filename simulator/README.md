# RobotBuddy Emotion Simulator

在 PC 上运行 RobotBuddy 表情引擎，无需 ESP32 硬件。

## 功能

- 🖥️ 240×240 窗口实时渲染机器人眼睛表情
- 🎹 键盘切换 11 种表情
- 📊 标题栏显示 FPS 和当前表情名称
- 🔍 窗口缩放（2× 默认）

## 按键映射

| 按键 | 表情 | 说明 |
|------|------|------|
| `1` | IDLE | 待机 — 微动、眨眼 |
| `2` | LISTENING | 聆听 — 放大眼睛 |
| `3` | THINKING | 思考 — 左右移动 |
| `4` | ANSWERING | 回答 — 节奏微动 |
| `5` | HAPPY | 开心 — 弯月笑眼 |
| `6` | CONFUSED | 困惑 — 歪头 |
| `7` | WARNING | 警告 — 黄色闪烁 |
| `8` | ERROR | 错误 — 红色怒眼 |
| `9` | FOCUS | 专注 — 半闭眼 |
| `0` | SLEEP | 睡眠 — 闭眼呼吸 |
| `Q` | EXCITED | 兴奋 — 跳动 |
| `Space` | — | 触发眨眼 |
| `+` / `-` | — | 窗口缩放 |
| `H` | — | 显示帮助 |
| `ESC` | — | 退出 |

## 安装依赖

### Ubuntu / Debian

```bash
sudo apt-get install libsdl2-dev cmake build-essential
```

### macOS

```bash
brew install sdl2 cmake
```

### Windows (MSYS2 / MinGW)

```bash
pacman -S mingw-w64-x86_64-SDL2 mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc
```

或使用 vcpkg:

```bash
vcpkg install sdl2:x64-windows
```

## 编译

```bash
cd simulator
mkdir build && cd build
cmake ..
make -j$(nproc)    # Linux/macOS
# 或
cmake --build . --config Release    # Windows/通用
```

### Windows (vcpkg)

```bash
cd simulator
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

## 运行

```bash
./robotbuddy_sim          # Linux/macOS
robotbuddy_sim.exe        # Windows
```

## 共享代码

模拟器编译的 `emotion_engine.c` 与 ESP32 固件版本**完全相同**（零修改）：

```
firmware/components/services/emotion_engine/emotion_engine.c  ← 共享源文件
firmware/components/services/emotion_engine/include/emotion_engine.h  ← 共享头文件
firmware/components/framework/include/robot_events.h  ← 共享类型定义
firmware/components/services/display_manager/include/display_manager.h  ← 共享接口
```

兼容层在 `simulator/platform/` 中提供 ESP-IDF 等价 API：

| ESP-IDF API | PC 兼容层 | 说明 |
|------------|----------|------|
| `esp_err_t` | 自定义类型 | 错误码兼容 |
| `ESP_LOGI/W/E/D` | `fprintf` | 日志兼容 |
| `heap_caps_malloc(SPIRAM)` | `malloc()` | 内存分配 |
| `esp_timer_get_time()` | `clock_gettime()` | 定时器 |
| `esp_random()` | `rand()` | 随机数 |
| `vTaskDelay()` | `SDL_Delay()` | 延时 |

## 故障排除

### "Could not find SDL2"

确保 SDL2 开发库已安装，cmake 能找到它。可以设置 `SDL2_DIR` 环境变量：

```bash
export SDL2_DIR=/usr/lib/x86_64-linux-gnu/cmake/SDL2  # Linux
set SDL2_DIR=C:\vcpkg\installed\x64-windows\share\sdl2   # Windows vcpkg
```

### 编译错误 "undefined reference to sin/cos"

确保链接了数学库 `-lm`（CMakeLists.txt 已包含）。

### 窗口很小

按 `+` 键放大窗口，或修改 `display_sim.c` 中的 `s_scale` 初始值。

## 许可证

MIT License — Copyright (c) 2026 RobotBuddy Project