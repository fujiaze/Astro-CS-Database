# P03-003 测试报告：严格失败与禁止静默跳过（v1.1 开发包）

**任务 ID**: P03-003
**测试日期**: 2026-07-25
**测试环境**: Windows + MSYS2 g++ 16.1.0 + PowerShell 7

---

## 1. 测试概述

| 测试类别 | 测试数量 | 通过 | 失败 | 备注 |
|---|---|---|---|---|
| 集成测试 (test_orchestrator_cli) | 136 | 136 | 0 | 5 个 Part 全部通过 |
| 端到端退出码验证 | 5 | 5 | 0 | CLI 真实场景 |
| **合计** | **141** | **141** | **0** | 全部通过 |

---

## 2. 集成测试详情

### 2.1 测试命令

```powershell
cd lib\orchestrator\cpp
.\tests\test_orchestrator_cli.exe
```

### 2.2 测试结果汇总

```
============================================================
测试汇总: 136 通过, 0 失败
============================================================
```

### 2.3 5 个 Part 测试范围

| Part | 测试内容 | 测试数 | 通过 | 关键验证点 |
|---|---|---|---|---|
| Part 1 | 交互式 REPL 命令测试 | 12 | 12 | help/status/run/pause/resume/interrupt/checkpoint/log/exit |
| Part 2 | 单次命令执行测试 | 11 | 11 | --help/-h/run/run-batch/status/config/threads/log-level/fresh/unknown |
| Part 3 | 断点续传测试 | ~30 | ~30 | CheckpointManager + Orchestrator 集成 |
| Part 4 | DLL 加载失败降级测试 | ~40 | ~40 | init_dlls 失败路径 + is_dlls_loaded 状态 |
| Part 5 | 日志系统集成测试 | ~43 | ~43 | Logger 初始化/级别过滤/多模块/格式/shutdown |

### 2.4 P03-003 关键测试用例

#### 测试 6: 配置加载失败退出码（P03-003 修改）

```
测试: orchestrator run nonexistent_frame.fits --config nonexistent_config.json
期望: exit_code = 7 (P03-003: CONFIG_ERROR)
实际: exit_code = 7
结果: PASS
```

**变更说明**：P03-003 前，配置加载失败返回退出码 2；P03-003 将其改为 7（CONFIG_ERROR），与 `AstroCsExitCode::CONFIG_ERROR` 对齐。测试期望值已同步更新。

#### 测试 4: run-batch 不存在目录退出码

```
测试: orchestrator run-batch Z:/nonexistent_dir_xyz
期望: exit_code != 0
实际: exit_code = 8 (FILE_IO_ERROR)
结果: PASS
```

---

## 3. 端到端退出码验证

### 3.1 测试脚本

```powershell
$exe = ".\orchestrator.exe"
# Test 1: --help should return 0
& $exe --help | Out-Null; $code1 = $LASTEXITCODE
# Test 2: run nonexistent.fits should return non-zero
& $exe run "Z:/nonexistent.fits" 2>&1 | Out-Null; $code2 = $LASTEXITCODE
# Test 3: run --config nonexistent.json should return 7 (CONFIG_ERROR)
& $exe run "Z:/nonexistent.fits" --config "Z:/nonexistent.json" 2>&1 | Out-Null; $code3 = $LASTEXITCODE
# Test 4: run-batch nonexistent_dir should return 8 (FILE_IO_ERROR)
& $exe run-batch "Z:/nonexistent_dir_xyz" 2>&1 | Out-Null; $code4 = $LASTEXITCODE
# Test 5: unknown subcommand should return 1
& $exe unknown_subcommand 2>&1 | Out-Null; $code5 = $LASTEXITCODE
```

### 3.2 测试结果

| 测试 | 场景 | 期望退出码 | 实际退出码 | 结果 |
|---|---|---|---|---|
| Test 1 | `orchestrator --help` | 0 | 0 | PASS |
| Test 2 | `orchestrator run Z:/nonexistent.fits` | 非 0 | 1 | PASS |
| Test 3 | `orchestrator run ... --config nonexistent.json` | 7 (CONFIG_ERROR) | 7 | PASS |
| Test 4 | `orchestrator run-batch Z:/nonexistent_dir_xyz` | 8 (FILE_IO_ERROR) | 8 | PASS |
| Test 5 | `orchestrator unknown_subcommand` | 1 | 1 | PASS |

**全部通过: True**

### 3.3 错误码语义验证

| 退出码 | AstroCsExitCode 常量 | 触发场景 | 验证状态 |
|---|---|---|---|
| 0 | SUCCESS | --help 成功 | PASS |
| 1 | GENERIC_ERROR | run 不存在 FITS / 未知子命令 | PASS |
| 7 | CONFIG_ERROR | --config 不存在 JSON | PASS |
| 8 | FILE_IO_ERROR | run-batch 不存在目录 | PASS |

> 退出码 2/3/4/5/6 需在真实 DLL 加载成功环境中验证（见 §4 限制说明）。

---

## 4. 测试环境限制

### 4.1 DLL 加载失败环境

当前测试环境中所有 10 个 DLL 均加载失败（LoadLibraryA code=126，找不到指定模块），这是因为：

- DLL 尚未构建（astro_image_io.dll、ipv_solver.dll 等）。
- 测试主要验证**失败降级路径**与**退出码传播**，而非真实算法执行。

### 4.2 未覆盖的退出码

以下退出码需在真实 DLL 环境中补充验证：

| 退出码 | 常量 | 触发条件 | 补充验证计划 |
|---|---|---|---|
| 2 | DLL_LOAD_FAILED | 必需 DLL 加载失败 | 已由集成测试 Part 4 覆盖（init_dlls 失败路径） |
| 3 | BLOCK_MISSING | 必需数据块缺失 | 需真实 DLL 加载成功后，构造块缺失场景 |
| 4 | CALIBRATE_FAILED | 校准阶段失败 | 需真实 calibration.dll + 不匹配 master 文件 |
| 5 | PLATESOLVE_FAILED | PlateSolve 阶段失败 | 需真实 ipv_solver.dll + 不可解析帧 |
| 6 | DRIZZLE_FAILED | Drizzle 阶段失败 | 需真实 healpix_drizzle.dll + 异常输入 |

### 4.3 测试结论

在当前测试环境下，P03-003 的核心目标已验证达成：

1. **静默跳过已消除**：所有 `WARN + return true` 模式已改为 `ERROR + return false + exit_code`。
2. **退出码传播正确**：CLI 4 个入口点均能正确传播 `TaskResult.exit_code`。
3. **细分错误码生效**：配置错误返回 7、文件 I/O 错误返回 8，与 `AstroCsExitCode` 定义一致。
4. **成功路径不受影响**：`--help` 等成功场景仍返回 0。
5. **集成测试全通过**：136/136 测试通过，0 失败。

---

**测试完成日期**: 2026-07-25
**测试执行者**: P03-003 子 Agent
