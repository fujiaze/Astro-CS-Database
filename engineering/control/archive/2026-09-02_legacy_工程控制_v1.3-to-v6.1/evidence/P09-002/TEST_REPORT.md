# P09-002 TEST_REPORT — 命名修正测试

- 任务: P09-002
- 阶段: P09
- Gate: G9
- 测试日期: 2026-07-27

## 测试目标

验证 INTERNAL_DETECTION_SHARED_EXPORT 命名修正不破坏构建、不改变运行时行为、capabilities 正确声明新能力。

## 测试套件

### T1: 静态字符串测试（命名修正验证）

**方法**: Grep 源码确认所有日志输出字符串已统一为 INTERNAL_DETECTION_SHARED_EXPORT

**结果**:

| 检查项 | 期望 | 实际 | 状态 |
|---|---|---|---|
| `lib/` 下 INTERNAL_DETECTION_SHARED_EXPORT 出现次数 | ≥10 (含 capabilities + 日志 + 注释) | 28 处 | PASS |
| `lib/` 下 logger/LOG_ 字符串中残留 "路径B" | 0 处 | 0 处 | PASS |
| capabilities 中 `internal_detection_shared_export` 字符串 | 存在 | 存在于 cli_command.cpp:1696 | PASS |
| C ABI 函数名 `ipv_solve_from_memory_with_callback` | 保持不变 | 保持不变 | PASS |
| C++ 类型名 `PathBCallbackCtx` / `path_b_detection_callback` | 保持不变（ABI 兼容） | 保持不变 | PASS |

### T2: 编译测试

**方法**: `mingw32-make.exe` 重新编译 ipv_solver.dll + orchestrator.exe

**结果**:

| 产物 | 编译退出码 | 警告 | 错误 | 状态 |
|---|---|---|---|---|
| `lib/plate_solve/cpp/ipv/ipv_solver.dll` | 0 | 已知警告（cast-function-type, unused-parameter，均为既有问题） | 0 | PASS |
| `lib/orchestrator/cpp/orchestrator.exe` | 0 | 1 警告（unused variable，既有问题） | 0 | PASS |

**警告详情**: 所有警告均为既有问题，与本次命名修正无关（详见 `logs/make_ipv.err` 和 `logs/make_orc.err`）。

### T3: 运行时 capabilities 测试

**方法**: 执行 `build/artifacts/orchestrator.exe capabilities`，验证输出包含新能力

**结果**:

ipv_solver 模块 capabilities 输出（节选自 `logs/capabilities.json`）:
```json
{"name":"ipv_solver","version":"unknown","capabilities":[
  "solve_from_memory",
  "solve_from_detections_v1",
  "solve_from_memory_with_callback",
  "internal_detection_shared_export"
]}
```

| 检查项 | 期望 | 实际 | 状态 |
|---|---|---|---|
| `internal_detection_shared_export` 出现在 capabilities 数组中 | 是 | 是 | PASS |
| 退出码 | 0 | 0 | PASS |
| 既有 capabilities 仍存在（`solve_from_memory`, `solve_from_detections_v1`, `solve_from_memory_with_callback`） | 是 | 是 | PASS |
| stages 数组完整（8 个 stage） | 是 | 是 | PASS |
| exit_codes 数组完整（21 项） | 是 | 是 | PASS |

### T4: 710 帧 A/B 既有证据引用测试

**方法**: 不重跑 710 帧（按 P09-001 锁定基线规则），引用 P02-003 + P02-007 既有证据

**引用证据**:

| 既有任务 | 文件 | 关键结论 |
|---|---|---|
| P02-003 | `engineering/evidence/P02-003/TEST_REPORT.md:99` | "全量 710 帧 A/B 测试完成, 路径 B 与旧路径在成功率、RMS、n_pairs 上完全一致" |
| P02-007 | `engineering/evidence/P02-007/TASK_REPORT.md:36-38` | "730 == 730, 每帧恰好 1 次 sdet_detect_ex" |
| P02-007 | `engineering/evidence/P02-007/gate_verification.json` | 5/5 非退化门限 PASS |

**结论**: PASS — P09-002 命名修正不触及算法，既有 710 帧证据继续有效。

### T5: SHA-256 完整性测试

**方法**: 计算修改后构建产物 SHA-256，记录到证据索引

**结果**:

| 产物 | SHA-256 | 大小 |
|---|---|---|
| `build/artifacts/orchestrator.exe` | `13265FCFEA720155C0715505A6E8DAF6DB8B5FE3322E7DB84874A1765ECA148B` | 4126364 字节 |
| `build/artifacts/ipv_solver.dll` | `ECC13B5472BC2322E2978A67CC9AB0320314C370A231C90E8FC535680E157027` | 773232 字节 |

### T6: 兼容性测试（未改 ABI）

**方法**: Grep 确认未修改 ABI/API 关键符号

**结果**:

| 检查项 | 期望 | 实际 | 状态 |
|---|---|---|---|
| `IpvDetectionCallback` 类型签名 | 保持不变 | 保持不变 | PASS |
| `DetectionSinkFn` 类型签名 | 保持不变 | 保持不变 | PASS |
| `ipv_solve_from_memory_with_callback` 函数签名 | 保持不变 | 保持不变 | PASS |
| `ipv_solve_from_detections_v1` 函数签名 | 保持不变 | 保持不变 | PASS |
| `path_b_detection_callback` 函数名 | 保持不变（内部符号，不影响 ABI） | 保持不变 | PASS |
| `PathBCallbackCtx` 结构名 | 保持不变（内部符号，不影响 ABI） | 保持不变 | PASS |
| 历史 evidence JSON 文件名（`path_b_results.json`） | 保持不变（P09-001 锁定） | 保持不变 | PASS |

## 测试汇总

| 测试 | 结果 |
|---|---|
| T1 静态字符串测试 | PASS |
| T2 编译测试 | PASS |
| T3 运行时 capabilities 测试 | PASS |
| T4 710 帧 A/B 既有证据引用 | PASS |
| T5 SHA-256 完整性测试 | PASS |
| T6 兼容性测试 | PASS |

**总计**: 6/6 PASS

## 原始日志

- 编译输出: `engineering_v1.2/evidence/P09-002/logs/make_ipv.{out,err}`、`make_orc.{out,err}`
- 清理输出: `engineering_v1.2/evidence/P09-002/logs/make_clean_ipv.{out,err}`、`make_clean_orc.{out,err}`
- capabilities 输出: `engineering_v1.2/evidence/P09-002/logs/capabilities.json`
- 完整执行日志: `engineering_v1.2/evidence/P09-002/logs/build.log`
- 退出码: 0（capabilities 命令）；其他见 `build.log`

## 超时与失败

- 无超时
- 无失败
- 无未声明 fallback

## 最终判定

**PASS** — 6/6 测试全部通过
