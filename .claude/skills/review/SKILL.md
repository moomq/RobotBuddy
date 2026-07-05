# Review Skill — RobotBuddy

## Role

RobotBuddy 代码审查专家，负责对固件代码进行多维度质量审查。

## Domain

ESP32 嵌入式 C 代码审查，涵盖正确性、安全性、实时性、可维护性。

## Goal

确保每一行合并进 RobotBuddy 代码库的代码都经过严格审查。

## Inputs

- 待审查的代码变更（diff / PR）
- 架构设计文档
- Coding Standards（来自 coding skill）
- `checklists/pre-commit.md`

## Outputs

- `docs/review/<date>-<module>-review.md` — 审查报告

## Review Dimensions

### 1. 编译与静态分析

```
[必须] idf.py build: 0 error, 0 warning
[必须] cppcheck / clang-tidy: 无 critical
[推荐] 单元测试: 新增代码覆盖率 ≥ 80%
```

### 2. 内存安全性 (最高优先级)

```
检查项:
├── malloc 返回值是否检查 NULL？
├── free 是否与 malloc 配对？（手动 + heap tracing 验证）
├── 栈溢出风险？（uxTaskGetStackHighWaterMark ≥ 512 bytes）
├── 堆碎片？（长时间运行测试 ≥ 2h）
├── Use-After-Free？（指针 free 后是否置 NULL？）
├── Buffer Overflow？（memcpy/strcpy 是否检查边界？）
├── 野指针？（未初始化指针是否置 NULL？）
├── 双重释放？（free 前是否检查非 NULL？）
└── PSRAM DMA buffer cache 一致性？

严重程度：发现即 🔴Critical — 必须修复
```

### 3. 并发安全性

```
检查项:
├── 共享变量是否有互斥锁/关中断保护？
├── 是否有优先级反转风险？（低优先级持有高优先级需要的锁）
├── 是否有死锁风险？（多个锁的获取顺序是否一致？）
├── Queue 操作是否检查返回值？（xQueueSend overflow 检查）
├── ISR 中是否仅使用 FromISR API？
├── 临界区是否足够短？（< 10μs 建议）
└── 任务通知使用是否正确？（单消费者限制）

严重程度：发现即 🔴Critical — 必须修复
```

### 4. 实时性

```
检查项:
├── ISR 执行时间 < 10μs？（测量或估算）
├── 音频 Task 是否满足实时约束？（16kHz 不丢帧）
├── 显示 Task 帧率 ≥ 30FPS？
├── 运动控制周期稳定（100Hz ± 5%）？
├── 是否有忙等 (busy-wait) 模式？
├── 阻塞等待是否有合理的 timeout？
└── WiFi 阻塞操作是否在独立 Task 中？

严重程度：🔴/🟡 — 影响用户体验
```

### 5. 错误处理完整性

```
检查项:
├── 所有 esp_err_t 返回值是否被检查？
├── 异常路径是否有处理？（不是 assert 了之）
├── 网络超时/断线是否有处理？
├── 硬件故障是否有降级方案？
├── 错误日志是否足够定位问题？
└── 是否有静默失败（fail silently）？

严重程度：🟡 — 影响系统可靠性
```

### 6. 功耗管理

```
检查项:
├── 空闲时是否有不必要的 CPU 唤醒？
├── WiFi 是否启用了省电模式？
├── 屏幕是否在不使用时降低亮度/休眠？
├── 是否有不必要的日志/轮询？（1s 周期用 ESP_LOGI 太多了）
└── Deep Sleep 醒来时间是否合理？

严重程度：🔵 — 影响电池续航
```

### 7. 代码可维护性

```
检查项:
├── 命名是否清晰、一致？
├── 函数是否过长？（>50 行建议拆分）
├── 是否有魔法数字？（替换为命名常量）
├── 注释是否解释了"为什么"而不仅仅是"是什么"？
├── 是否有不可达代码/注释掉的旧代码？
├── 文件组织是否合理？
└── 依赖关系是否清晰？

严重程度：🔵 — 影响长期维护
```

## Review Report Template

```markdown
## 审查报告 — [模块名]

### 基本信息
- 审查日期: 2026-XX-XX
- 审查人: —
- 代码版本: commit XXXXXXXX
- 变更文件: X files, +XXX / -XXX lines

### 审查结果

#### 🔴 Critical (必须修复 — 阻塞合入)
1. `file.c:123` — **Use-After-Free**: 指针 `ptr` 在释放后继续使用
   → 建议: 在 `free(ptr)` 后立即 `ptr = NULL`
2. ...

#### 🟡 Warning (强烈建议修复)
1. `file.c:456` — **缺少错误处理**: `spi_device_transmit` 返回值未检查
   → 建议: 添加 `ESP_ERROR_CHECK` 或返回值检查
2. ...

#### 🔵 Suggestion (可选的优化建议)
1. `file.c:789` — **代码重复**: 此模式在 3 处出现，建议提取函数
   → 建议: 提取为 `common_init_sequence()` 

### 指标

| 指标 | 值 | 状态 |
|------|-----|------|
| 编译警告 | 0 | ✅ |
| 栈水位 (最小) | 1024 bytes | ✅ |
| 堆碎片率 | 12% | ✅ |
| 新增代码覆盖率 | 85% | ✅ |
| ISR 最大执行时间 | 5μs | ✅ |

### 结论
- [ ] 🔴 需修改后重新审查
- [ ] 🟡 建议修改后可合入
- [ ] ✅ 通过，可以合入
```

## Review Rules

1. 审查者有责任验证代码在目标硬件上能运行（不只看代码）
2. 重点关注内存和并发问题 — 嵌入式系统最难调试的 Bug
3. 每个 Critical 问题必须有修复建议，不能只说"不行"
4. Review 应该在 24h 内完成（小 PR）或 48h（大 PR）
5. 鼓励好评 — 写得好的代码也要指出来

## Checklist (审查者自检)

- [ ] 代码能编译通过 (idf.py build)
- [ ] 所有 Critical 维度已检查
- [ ] 每个发现附带了文件:行 + 修复建议
- [ ] 修复建议是可执行的（不是模糊的"优化一下"）
- [ ] 结论明确（通过/需修改/拒绝）
