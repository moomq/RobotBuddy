# RobotBuddy PC 模拟器 — 编译调试指南

> **版本:** 1.0  
> **日期:** 2026-07-11  
> **适用范围:** `simulator/` 目录下的PC端表情引擎模拟器  

---

## 目录

1. [环境准备](#1-环境准备)
2. [编译](#2-编译)
3. [运行](#3-运行)
4. [调试技巧](#4-调试技巧)
5. [常见问题排查](#5-常见问题排查)
6. [开发工作流](#6-开发工作流)
7. [架构速查](#7-架构速查)

---

## 1. 环境准备

### 1.1 依赖清单

| 依赖 | 最低版本 | 用途 |
|------|---------|------|
| CMake | 3.16+ | 构建系统 |
| C编译器 | C99 | GCC/Clang/MSVC |
| SDL2 | 2.0.22+ | 窗口渲染 |
| 数学库 | — | sin/cos（emotion_engine.c） |

### 1.2 Windows 环境安装

#### 方案 A：MSYS2 + MinGW64（推荐）

```powershell
# 1. 下载并安装 MSYS2
# https://www.msys2.org/ 下载安装器，默认安装到 C:\msys64

# 2. 打开 MSYS2 UCRT64 终端，安装工具链和 SDL2
pacman -Syu                          # 更新包数据库
pacman -S mingw-w64-ucrt-x86_64-gcc  # GCC 编译器
pacman -S mingw-w64-ucrt-x86_64-cmake # CMake
pacman -S mingw-w64-ucrt-x86_64-SDL2  # SDL2 开发库
pacman -S mingw-w64-ucrt-x86_64-make  # make
pacman -S mingw-w64-ucrt-x86_64-ninja  #ninja
pacman -S make                        # Make

# 3. 验证安装
gcc --version
cmake --version
sdl2-config --version

cd simulator/build
cmake --build .

或者：

cd simulator/build
ninja

cd simulator/build
cmake -G "MinGW Makefiles" ..
cmake --build .
./robotbuddy_sim.exe


```

#### 方案 B：vcpkg + MSVC

```powershell
# 1. 安装 Visual Studio 2022（含 C++ 桌面开发工作负载）

# 2. 安装 vcpkg
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg install sdl2:x64-windows

# 3. 设置环境变量
set VCPKG_ROOT=C:\vcpkg
```

### 1.3 macOS 环境安装

```bash
# 1. 安装 Xcode Command Line Tools
xcode-select --install

# 2. 安装 Homebrew（如未安装）
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 3. 安装依赖
brew install cmake sdl2

# 4. 验证
gcc --version
cmake --version
sdl2-config --version
```

### 1.4 Linux (Ubuntu/Debian) 环境安装

```bash
# 1. 安装编译工具和 SDL2 开发库
sudo apt-get update
sudo apt-get install -y build-essential cmake libsdl2-dev

# 2. 验证
gcc --version
cmake --version
pkg-config --modversion sdl2
```

---

## 2. 编译

### 2.1 标准编译流程

```bash
cd simulator
mkdir build
cd build
cmake ..
make -j$(nproc)       # Linux/macOS
# 或
cmake --build . --config Release   # Windows/通用
```

### 2.2 Windows MSYS2 编译

```bash
# 在 MSYS2 UCRT64 终端中
cd /f/04\ code/RobotBuddy/simulator
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
mingw32-make -j4

# 运行
./robotbuddy_sim.exe
```

### 2.3 Windows vcpkg + MSVC 编译

```powershell
cd "F:\04 code\RobotBuddy\simulator"
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake"
cmake --build . --config Release

# 运行
Release\robotbuddy_sim.exe
```

### 2.4 macOS 编译

```bash
cd simulator
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)

# 运行
./robotbuddy_sim
```

### 2.5 CMake 配置选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `CMAKE_BUILD_TYPE` | Debug/Release | Release |
| `SDL2_DIR` | SDL2配置路径（vcpkg） | 自动检测 |
| `CMAKE_C_STANDARD` | C标准 | 99 |

```bash
# Debug 编译（带调试信息）
cmake -DCMAKE_BUILD_TYPE=Debug ..

# 指定 SDL2 路径
cmake -DSDL2_DIR=/usr/lib/x86_64-linux-gnu/cmake/SDL2 ..

# vcpkg 集成
cmake -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake ..
```

### 2.6 编译输出

| 文件 | 说明 |
|------|------|
| `robotbuddy_sim` (Linux/macOS) | 可执行文件 |
| `robotbuddy_sim.exe` (Windows) | 可执行文件 |

---

## 3. 运行

### 3.1 启动模拟器

```bash
# Linux/macOS
./robotbuddy_sim

# Windows (MSYS2)
./robotbuddy_sim.exe

# Windows (MSVC)
Release\robotbuddy_sim.exe
```

### 3.2 命令行参数

当前版本不支持命令行参数。所有配置通过键盘交互控制。

### 3.3 键盘控制

| 按键 | 功能 | 表情特征 |
|------|------|---------|
| `1` | IDLE | 青色眼睛，微动，定时眨眼 |
| `2` | LISTENING | 放大眼睛，聚焦 |
| `3` | THINKING | 蓝色眼睛，左右移动 |
| `4` | ANSWERING | 青色眼睛，节奏微动 |
| `5` | HAPPY | 绿色弯月笑眼 |
| `6` | CONFUSED | 橙色眼睛，歪头 |
| `7` | WARNING | 黄色/橙色交替闪烁 |
| `8` | ERROR | 红色怒眼 |
| `9` | FOCUS | 青色半闭眼 |
| `0` | SLEEP | 深青色闭眼，呼吸动画 |
| `Q` | EXCITED | 青色大眼，跳动 |
| `Space` | 日志输出 | （眨眼是自动的） |
| `+` / `=` | 放大窗口 | 1×→2×→3×→4× |
| `-` | 缩小窗口 | 4×→3×→2×→1× |
| `H` | 显示帮助 | 控制台打印按键映射 |
| `ESC` | 退出 | 关闭模拟器 |

### 3.4 窗口标题栏

```
RobotBuddy Simulator | IDLE | FPS: 30.0 | 1-0/Q:emotion  Space:blink  +/-:zoom  ESC:quit
```

实时显示当前表情名称和FPS数值。

---

## 4. 调试技巧

### 4.1 日志级别

模拟器使用 `ESP_LOGI/W/E/D` 宏（映射到 `printf`），输出到 stdout/stderr：

```
I [emotion] Emotion: IDLE -> HAPPY
I [simulator] Emotion: HAPPY
E [display_sim] SDL_UpdateTexture failed: ...
```

| 宏 | 目标 | 用途 |
|----|------|------|
| `ESP_LOGI` | stdout | 关键状态变化 |
| `ESP_LOGW` | stderr | 警告 |
| `ESP_LOGE` | stderr | 错误 |
| `ESP_LOGD` | stdout | 调试信息 |

### 4.2 Debug 编译

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

Debug 编译启用 `-g` 调试信息，可以用 GDB/LLDB 调试：

```bash
# GDB (Linux/MSYS2)
gdb ./robotbuddy_sim

# LLDB (macOS)
lldb ./robotbuddy_sim

# 常用调试命令
(gdb) break emotion_render_frame    # 在表情渲染函数设断点
(gdb) break display_commit_frame    # 在帧提交函数设断点
(gdb) run
(gdb) continue
(gdb) print s_current_emotion      # 查看当前表情
(gdb) print s_left_eye              # 查看左眼状态
(gdb) print s_frame_counter         # 查看帧计数器
```

### 4.3 帧缓冲调试

在 `display_commit_frame()` 中添加调试代码查看帧内容：

```c
// 在 display_sim.c 的 display_commit_frame() 中临时添加：
ESP_LOGD("display", "Frame %d: pixel[0]=0x%04X, pixel[120*240+120]=0x%04X",
         s_frame_count, s_frame_buffer[0], s_frame_buffer[120*240+120]);
```

### 4.4 性能分析

```bash
# Linux perf
perf stat ./robotbuddy_sim

# macOS Instruments
instruments -t "Time Profiler" ./robotbuddy_sim
```

窗口标题栏显示的FPS是性能的主要指标：
- **≥60 FPS**：性能充裕
- **30-60 FPS**：正常范围
- **<30 FPS**：需要优化（检查是否有大量打印输出）

### 4.5 Valgrind 内存检查

```bash
# Linux only
valgrind --leak-check=full --show-leak-kinds=all ./robotbuddy_sim

# 运行后按键切换表情，然后 ESC 退出
# 检查是否有内存泄漏报告
```

### 4.6 表情引擎调试

```bash
# 在 GDB 中调试表情切换逻辑
(gdb) break emotion_set_state
(gdb) commands 1
> print emotion
> print emotion_get_name(emotion)
> continue
> end
(gdb) run
# 每次按键切换表情时，GDB会打印表情名称
```

---

## 5. 常见问题排查

### 5.1 编译错误

#### "Could not find SDL2"

**症状：**
```
CMake Error at CMakeLists.txt:30 (find_package):
  Could not find a package configuration file provided by "SDL2"
```

**解决方案：**

```bash
# Ubuntu/Debian
sudo apt-get install libsdl2-dev

# macOS
brew install sdl2

# Windows MSYS2
pacman -S mingw-w64-ucrt-x86_64-SDL2

# Windows vcpkg
vcpkg install sdl2:x64-windows
cmake -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake ..
```

如果SDL2安装在非标准路径，手动指定：

```bash
cmake -DSDL2_DIR=/path/to/sdl2/cmake ..
```

#### "undefined reference to sin/cos"

**症状：**
```
undefined reference to `sin'
undefined reference to `cos'
```

**原因：** 数学库未链接。

**解决方案：** CMakeLists.txt 已包含 `-lm`，检查链接顺序。Linux 上确保 `target_link_libraries` 中 `m` 在 `SDL2` 之后。

#### "esp_err.h: No such file or directory"

**症状：**
```
fatal error: esp_err.h: No such file or directory
```

**原因：** include路径优先级不正确，编译器找到了ESP-IDF的`esp_err.h`而不是模拟器的。

**解决方案：** 确认CMakeLists.txt中`platform/`目录在include路径中排在**第一位**：

```cmake
target_include_directories(robotbuddy_sim PRIVATE
    ${PLATFORM_DIR}    # ← 必须排在第一位！
    ...
)
```

#### "SDL2/SDL.h: No such file or directory"

**症状：**
```
fatal error: SDL2/SDL.h: No such file or directory
```

**原因：** 已修复。代码已改为 `<SDL.h>`，由CMake提供正确的include路径。

#### "multiple definition of esp_timer_get_time"

**症状：**
```
multiple definition of `esp_timer_get_time'
```

**原因：** `esp_timer.h` 中的函数被多个源文件包含，且没有使用 `static inline`。

**解决方案：** 确认 `esp_timer.c` 是唯一的实现文件，`esp_timer.h` 只有声明，不包含实现。

### 5.2 运行时问题

#### 窗口一闪而过

**原因：** SDL初始化失败或帧缓冲分配失败。

**排查：**
```bash
# 查看详细日志
./robotbuddy_sim 2>&1 | tee sim.log

# 检查SDL版本
sdl2-config --version   # Linux/macOS
pacman -Qs sdl2          # MSYS2
```

#### 窗口黑屏无表情

**原因：** 表情引擎初始化失败。

**排查：** 查看日志中是否有 `"Emotion engine init failed"` 错误。

#### FPS很低 (<10)

**原因：**
1. 调试模式编译（Debug模式会慢）
2. 大量 `ESP_LOGD` 输出拖慢
3. 旧版SDL2软件渲染

**解决方案：**
```bash
# 确保Release编译
cmake -DCMAKE_BUILD_TYPE=Release ..

# 禁用调试日志：在esp_log.h中将ESP_LOGD改为空操作
```

#### 窗口很小

**解决方案：** 按 `+` 键放大窗口（默认2倍），或修改 `display_sim.c` 中的 `s_scale` 初始值。

#### 表情切换无反应

**原因：** 键盘焦点不在模拟器窗口上。

**解决方案：** 点击窗口使其获得焦点，然后再按数字键。

### 5.3 跨平台特殊问题

#### Windows 中文乱码

Windows终端默认使用GBK编码，导致日志中的中文表情名称乱码。

```powershell
# 临时修改终端编码为UTF-8
chcp 65001
```

#### macOS "SDL2 is not installed" 错误

```bash
# macOS需要在运行时指定库路径
export DYLD_LIBRARY_PATH=/usr/local/lib:$DYLD_LIBRARY_PATH
./robotbuddy_sim
```

#### Linux "libSDL2-2.0.so.0: cannot open shared object file"

```bash
# 更新库缓存
sudo ldconfig

# 或指定库路径
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

---

## 6. 开发工作流

### 6.1 修改表情参数

编辑共享文件 `firmware/components/services/emotion_engine/emotion_engine.c`：

```c
// 修改 IDLE 表情的眼睛颜色
[EMOTION_IDLE] = {
    .id = EMOTION_IDLE,
    .name = "IDLE",
    .eye_color = COLOR_CYAN,      // ← 改为 COLOR_GREEN 试试
    .bg_color = COLOR_DARK_BG,
    .blink_probability = 3,
    .animation_interval_ms = 50,
},
```

重新编译模拟器，立即看到效果：

```bash
cd simulator/build
make -j4 && ./robotbuddy_sim
```

### 6.2 添加新表情

1. 在 `firmware/components/framework/include/robot_events.h` 的 `emotion_id_t` 枚举中添加新项
2. 在 `firmware/components/services/emotion_engine/emotion_engine.c` 的 `s_emotion_configs[]` 中添加配置
3. 在 `update_emotion_animation()` 的 switch 中添加动画逻辑
4. 在 `simulator/main.c` 的 `s_key_map[]` 中添加按键映射
5. 重新编译

### 6.3 修改眼睛位置和大小

编辑 `simulator/main.c` 中的 `emotion_engine_config_t`：

```c
emotion_engine_config_t emotion_cfg = {
    .display_width = 240,
    .display_height = 240,
    .left_eye_cx = 80,     // ← 左眼中心X
    .left_eye_cy = 120,    // ← 左眼中心Y
    .right_eye_cx = 160,   // ← 右眼中心X
    .right_eye_cy = 120,    // ← 右眼中心Y
    .eye_radius = 35,       // ← 眼睛半径
};
```

### 6.4 模拟器与ESP32同步

修改共享文件后，**两边都需要重新编译**：

```bash
# PC模拟器
cd simulator/build && make -j4

# ESP32固件（在项目根目录）
cd firmware && idf.py build
```

**验证零修改：**

```bash
# 确认共享代码未被修改
diff firmware/components/services/emotion_engine/emotion_engine.c \
     firmware/components/services/emotion_engine/emotion_engine.h \
     --no-index

# 应该显示无差异
```

### 6.5 Git 工作流

```bash
# 模拟器代码在 simulator/ 目录下
git add simulator/
git commit -m "feat: add PC emotion simulator (FR-01)"

# 如果修改了共享的表情引擎代码
git add firmware/components/services/emotion_engine/
git commit -m "feat: add new emotion animation (shared code)"
```

---

## 7. 架构速查

### 7.1 文件依赖关系

```
simulator/main.c
    ├── display_manager.h          (共享接口)
    ├── display_sim.h              (PC专用: 窗口/缩放)
    ├── emotion_engine.h          (共享接口)
    ├── robot_events.h             (共享事件定义)
    ├── esp_log.h                 (兼容层)
    └── esp_random.h              (兼容层)

simulator/drivers/display_sim.c
    ├── display_manager.h          (共享接口)
    └── <SDL.h>                   (SDL2)

firmware/.../emotion_engine.c     (共享源文件，零修改)
    ├── emotion_engine.h          (共享)
    ├── display_manager.h          (共享接口)
    ├── esp_log.h                 (兼容层)
    ├── esp_random.h              (兼容层)
    ├── freertos/FreeRTOS.h       (兼容层)
    └── freertos/task.h           (兼容层)
```

### 7.2 兼容层映射

| ESP-IDF API | PC兼容层 | 映射关系 |
|-------------|---------|---------|
| `esp_err_t` | 自定义 `int32_t` | 错误码值对齐ESP-IDF v5.x |
| `ESP_LOGI/W/E/D` | `fprintf` | TAG前缀 + 格式化输出 |
| `heap_caps_malloc(SPIRAM)` | `malloc()` | 忽略内存类型标志 |
| `esp_timer_get_time()` | `clock_gettime` / `QueryPerformanceCounter` | 微秒级时间戳 |
| `esp_random()` | `rand()<<16 ^ rand()` | 双rand组合32位 |
| `vTaskDelay(ms)` | `Sleep(ms)` / `usleep(ms*1000)` | 跨平台延时 |
| `pdMS_TO_TICKS(ms)` | 直接返回ms | PC上1tick=1ms |
| `xTaskCreatePinnedToCore()` | no-op | 单线程，main loop驱动 |
| `st7789_draw_bitmap()` | `SDL_UpdateTexture` + `SDL_RenderCopy` | 帧缓冲→窗口 |

### 7.3 快速定位问题

| 现象 | 检查文件 | 关键变量/函数 |
|------|---------|-------------|
| 窗口不出现 | `display_sim.c` | `display_manager_init()` |
| 表情不显示 | `main.c` | `emotion_render_frame()` 调用 |
| 表情切换不响应 | `main.c` | `s_key_map[]` 键盘映射 |
| FPS异常低 | `display_sim.c` | `update_fps()` / `SDL_GetTicks()` |
| 窗口大小不对 | `display_sim.c` | `s_scale` 初始值 |
| 编译找不到头文件 | `CMakeLists.txt` | `target_include_directories` 顺序 |
| 链接错误 | `CMakeLists.txt` | `target_link_libraries` SDL2 |

---

> **文档版本:** 1.0  
> **最后更新:** 2026-07-11  
> **相关文档:**
> - `docs/requirement/fr01-pc-simulator-requirements.md` (需求规格)
> - `docs/architecture/fr01-pc-simulator-architecture.md` (架构设计)
> - `simulator/README.md` (快速入门)