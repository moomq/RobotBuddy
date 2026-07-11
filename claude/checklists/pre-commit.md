# Pre-Commit Checklist — RobotBuddy

> 每次提交代码前必须逐项确认的检查清单

---

## 快速检查 (< 5 分钟，每次 commit)

### 编译
- [ ] `idf.py build` 通过 — 0 errors, 0 warnings
- [ ] 新增文件已加入 `CMakeLists.txt`

### 代码
- [ ] 无 TODO/FIXME 遗留（除非有对应 Issue 号）
- [ ] 无注释掉的大段旧代码
- [ ] 无调试打印残留（`printf`, 临时 `ESP_LOGI`）
- [ ] 命名符合规范 (snake_case / PascalCase / UPPER_CASE)
- [ ] 魔法数字替换为命名常量

### Git
- [ ] Commit message 格式正确: `<type>: <description>`
  - `feat:` — 新功能
  - `fix:` — Bug 修复
  - `refactor:` — 重构
  - `docs:` — 文档
  - `test:` — 测试
  - `chore:` — 杂项
- [ ] 无意外提交的临时文件 (`.swp`, `build/`, `sdkconfig.old`)

---

## 完整检查 (≥ 15 分钟，每个 PR)

### 内存安全
- [ ] 每个 `malloc`/`heap_caps_malloc` 有对应 `free`
- [ ] 每个 `malloc` 返回值被检查
- [ ] 无 Use-After-Free (释放后指针置 NULL)
- [ ] 无 Buffer Overflow (边界检查)
- [ ] Task 栈水位 ≥ 512 bytes (`uxTaskGetStackHighWaterMark` 验证)

### 并发安全
- [ ] 共享变量有互斥锁或关中断保护
- [ ] 互斥锁获取顺序一致 (无死锁风险)
- [ ] Queue 操作检查返回值
- [ ] ISR 仅使用 `FromISR` API
- [ ] 无优先级反转风险

### 错误处理
- [ ] 所有 `esp_err_t` 返回值被检查或使用 `ESP_ERROR_CHECK`
- [ ] 网络/硬件异常的降级路径存在
- [ ] 超时机制存在 (无限等待 → 合理 timeout)

### 实时性
- [ ] 无忙等 (`while(1);` / `for(;;);`)
- [ ] 无阻塞 delay (`delay(1000)` → `vTaskDelay`)
- [ ] ISR 执行时间 < 10μs (估算)
- [ ] 高频 Task 有 `vTaskDelay` 或 Queue 等待释放 CPU

### 硬件
- [ ] GPIO 引脚不与现有冲突
- [ ] ADC 输入电压 ≤ 3.3V (分压)
- [ ] SPI/I2C/I2S 配置参数与硬件匹配
- [ ] 硬件复位序列和执行顺序正确
- [ ] 电源上电时序正确

### 测试
- [ ] 新增代码有至少一个测试用例
- [ ] 所有已有测试通过 (`ctest`)
- [ ] 特殊情况：HIL 测试在目标硬件运行通过

### 文档
- [ ] 新增 API 有 Doxygen 注释
- [ ] 配置参数有 Kconfig 帮助文本（如适用）
- [ ] README / 相关文档已更新
- [ ] UML有使用PlantUML的，统一改成mermaid

### 日志
- [ ] 关键状态变更有 `ESP_LOGI`
- [ ] 错误路径有 `ESP_LOGE`
- [ ] ISR 中无日志调用
- [ ] 高频日志已降为 `ESP_LOGD`

---

## 发布前检查 (每个版本)

- [ ] 完整检查 全部通过
- [ ] 24h 压力测试通过 (无内存泄漏、无 watchdog 触发)
- [ ] 所有表情/动画/语音/运动 功能完整测试
- [ ] 功耗测试: 活跃 ≥ 4h, 待机 ≥ 24h
- [ ] OTA 升级 + 回滚 验证
- [ ] CHANGELOG.md 更新
- [ ] 版本号更新 (`app_version.h`)
- [ ] 固件签名

---

## 常用验证命令

```bash
# 编译
idf.py build

# 运行测试
cd build && ctest --output-on-failure

# 检查固件大小
idf.py size-components

# 检查分区使用
idf.py app-flash-size

# 检查栈水位 (运行时)
idf.py monitor
# 观察 esp_timer 周期性输出或手动查询

# 静态分析
cppcheck --enable=all --inconclusive --std=c99 firmware/main firmware/components 2> cppcheck_report.txt

# Format check
clang-format --dry-run --Werror firmware/main/*.c firmware/components/**/*.c
```
