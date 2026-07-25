# P03-003 任务报告：严格失败与禁止静默跳过（v1.1 开发包）

**任务 ID**: P03-003
**阶段**: P03 (开发包)
**门禁**: G3 (真实输入)
**完成日期**: 2026-07-25
**负责人**: P03-003 子 Agent

---

## 1. 任务目标

依据 `engineering/tasks/P03-003.md` 与 v1.1 开发包工作流规则，实现"严格失败与禁止静默跳过"：

- 必需 DLL/块/质量失败必须非零退出。
- 删除生产路径 `true-on-skip`（即 `WARN + return true` 静默跳过模式）。
- 建立稳定错误码与非零退出测试。
- 确保失败不提交正式输出。

**硬性约束**：
- 最小改动都要求 commit（工作留痕）。
- 每完成一个阶段 push 一次远端。
- 必需阶段失败必须传播细分退出码到 CLI。

---

## 2. 实现方案

### 2.1 错误码定义（AstroCsExitCode）

在 `orchestrator.h` 中新增 `AstroCsExitCode` 命名空间，定义 9 个稳定退出码：

| 退出码 | 常量 | 含义 |
|---|---|---|
| 0 | SUCCESS | 成功 |
| 1 | GENERIC_ERROR | 通用错误（未细分） |
| 2 | DLL_LOAD_FAILED | 必需 DLL 加载失败 |
| 3 | BLOCK_MISSING | 必需数据块缺失 |
| 4 | CALIBRATE_FAILED | 校准阶段失败 |
| 5 | PLATESOLVE_FAILED | PlateSolve 阶段失败 |
| 6 | DRIZZLE_FAILED | Drizzle 阶段失败 |
| 7 | CONFIG_ERROR | 配置错误 |
| 8 | FILE_IO_ERROR | 文件 I/O 错误 |

### 2.2 TaskResult 扩展

在 `TaskResult` 结构体中新增 `exit_code` 字段（默认 0），由各 stage handler 在失败路径设置对应错误码，由 `cli_command.cpp` 直接返回。

### 2.3 必需/可选阶段分类

| 阶段 | 类型 | 失败行为 |
|---|---|---|
| READ_FITS | 必需 | exit_code=8 (FILE_IO_ERROR) |
| CALIBRATE | 必需 | exit_code=2/4 (DLL/CALIBRATE_FAILED) |
| PLATESOLVE | 必需 | exit_code=2/5 (DLL/PLATESOLVE_FAILED) |
| PSF | 必需 | exit_code=2/3 (DLL/BLOCK_MISSING) |
| PHOTOMETRIC | 必需 | exit_code=2/3 (DLL/BLOCK_MISSING) |
| DRIZZLE | 必需 | exit_code=2/6 (DLL/DRIZZLE_FAILED) |
| SNR | 可选 | 降级到 photo_stats，不阻塞 stage1 |
| GRADIENT_SPHERE | 必需 (stage2) | exit_code=2/8 (DLL/FILE_IO_ERROR) |
| STACK | 必需 (stage2) | exit_code=2 (DLL_LOAD_FAILED) |

### 2.4 静默跳过消除

枚举并修复所有 `WARN + return true` 和空块跳过模式：

1. **DLL 未加载静默跳过** → 改为 `ERROR + return false + exit_code=2`
2. **frame_ 为空静默跳过** → 改为 `ERROR + return false + exit_code=1`
3. **必需块缺失静默跳过** → 改为 `ERROR + return false + exit_code=3`
4. **块写入失败静默跳过** → 改为 `ERROR + return false + exit_code=3`
5. **stage handler 未设置 exit_code** → 兜底按 stage 类型推导默认退出码

### 2.5 CLI 退出码传播

`cli_command.cpp` 的 4 个入口点（`cmd_run`、`cmd_run_batch`、`cmd_stage1`、`cmd_stage2`）统一改为：

```cpp
return r.success ? 0 : (r.exit_code != 0 ? r.exit_code : 1);
```

失败时优先使用 `TaskResult.exit_code`，若未设置则用 1 兜底。

---

## 3. 实施步骤

1. **代码实现**：
   - 修改 `orchestrator.h` 添加 `AstroCsExitCode` 命名空间 + `TaskResult.exit_code` 字段（已完成）。
   - 修改 `orchestrator.cpp` 所有必需 stage handler 的失败路径设置 `exit_code`（已完成，87 处 P03-003 标记）。
   - 修改 `cli_command.cpp` 4 个入口点传播 `exit_code`（已完成）。

2. **测试同步更新**：
   - 修改 `test_orchestrator_cli.cpp` 测试 6 的期望退出码从 2 改为 7（CONFIG_ERROR）（已完成）。

3. **构建验证**：
   - 执行 `make all` + `make test_orchestrator_cli`，编译成功。
   - `orchestrator.exe` 大小 3,984,808 字节，SHA-256 `8969B37F...`。

4. **集成测试**：
   - 运行 `test_orchestrator_cli.exe`，136/136 测试通过，0 失败。

5. **端到端退出码验证**：
   - 5 个 CLI 场景全部返回预期退出码（见 TEST_REPORT.md）。

---

## 4. 关键发现

### 4.1 静默跳过数量

通过 `grep "WARN.*return true"` 搜索，发现并修复 **12 类静默跳过模式**，覆盖 9 个 stage handler：

| Stage | 静默跳过类型 | 修复前 | 修复后 |
|---|---|---|---|
| READ_FITS | AIO DLL 未加载 | WARN+return true | ERROR+return false (exit=2) |
| READ_FITS | data 块写入失败 | WARN+return true | ERROR+return false (exit=3) |
| CALIBRATE | DLL 未加载 | WARN+return true | ERROR+return false (exit=2) |
| CALIBRATE | Master 验证失败 | WARN+return true | ERROR+return false (exit=4) |
| CALIBRATE | data 块写回失败 | WARN+return true | ERROR+return false (exit=3) |
| PLATESOLVE | DLL 未加载 | WARN+return true | ERROR+return false (exit=2) |
| PLATESOLVE | star_det 块写入失败 | WARN+return true | ERROR+return false (exit=3) |
| PSF | DLL 未加载 | WARN+return true | ERROR+return false (exit=2) |
| PHOTOMETRIC | DLL 未加载 | WARN+return true | ERROR+return false (exit=2) |
| DRIZZLE | DLL 未加载 | WARN+return true | ERROR+return false (exit=2) |
| GRADIENT_SPHERE | DLL 未加载 | WARN+continue | ERROR+return false (exit=2) |
| STACK | DLL 未加载 | WARN+continue | ERROR+return false (exit=2) |

### 4.2 兜底退出码机制

在 `run_stage1` 和 `run_stage2` 的 stage 执行循环中新增兜底逻辑：当 stage handler 返回失败但未设置 `exit_code` 时，按 stage 类型自动推导默认退出码，确保任何失败路径都不会返回 0。

### 4.3 SNR 可选阶段降级策略

SNR 阶段保持可选属性（drizzle 不强依赖 snr_model 块），但在降级时：
- 将 `SNR_STATUS=degraded` 写入 `photo_stats`，便于下游感知。
- 不设置非零 `exit_code`，允许 stage1 继续执行。
- 仅当 SNR DLL 加载成功但内部逻辑出错时记录 ERROR 日志。

---

## 5. 交付物清单

| 文件 | 位置 | 说明 |
|---|---|---|
| TASK_REPORT.md | `engineering/evidence/P03-003/TASK_REPORT.md` | 本报告 |
| TEST_REPORT.md | `engineering/evidence/P03-003/TEST_REPORT.md` | 测试报告 |
| EVIDENCE_INDEX.md | `engineering/evidence/P03-003/EVIDENCE_INDEX.md` | 证据索引 |
| REVIEW_REPORT.md | `engineering/evidence/P03-003/REVIEW_REPORT.md` | 评审报告 |
| error_code_registry.json | `engineering/evidence/P03-003/error_code_registry.json` | 错误码注册表（机器可读） |
| exit_code_evidence.log | `engineering/evidence/P03-003/exit_code_evidence.log` | 端到端退出码验证日志 |

### 代码变更

| 文件 | 变更类型 | 说明 |
|---|---|---|
| `lib/orchestrator/cpp/include/orchestrator.h` | 修改 | 新增 AstroCsExitCode 命名空间 + TaskResult.exit_code 字段 |
| `lib/orchestrator/cpp/src/orchestrator.cpp` | 修改 | 87 处 P03-003 标记，覆盖所有必需 stage 失败路径 |
| `lib/orchestrator/cpp/src/cli_command.cpp` | 修改 | 4 个入口点传播 exit_code |
| `lib/orchestrator/cpp/tests/test_orchestrator_cli.cpp` | 修改 | 测试 6 期望退出码从 2 改为 7 |

---

## 6. 兼容性、回滚和残留风险

### 6.1 兼容性

- **向后兼容**：成功路径行为不变（exit_code=0）。
- **失败路径行为变更**：原本返回 0 的静默跳过现在返回非零，可能影响依赖旧行为的脚本。
- **测试同步**：`test_orchestrator_cli.cpp` 测试 6 已同步更新期望值。

### 6.2 回滚

- 若需回滚，恢复 `orchestrator.h` / `orchestrator.cpp` / `cli_command.cpp` / `test_orchestrator_cli.cpp` 至 P03-003 前的 commit。
- 回滚后需重新构建 `orchestrator.exe`。

### 6.3 残留风险

- **SNR 降级路径**：SNR 失败时仍允许 stage1 成功，若下游强依赖 snr_model 块需额外检查（当前 drizzle 已做可选处理）。
- **DLL 加载失败测试环境**：集成测试环境中 DLL 全部加载失败（code 126），测试的是降级路径而非真实 DLL 加载成功路径。真实环境需后续 P03-004+ 任务验证。

---

## 7. 后续建议

1. **真实 DLL 环境验证**：在 DLL 全部加载成功的环境中重新运行端到端退出码验证，确认成功路径返回 0。
2. **全量回归**：依据 v1.1 开发包规则，PlateSolve 全量测试（710 帧）是合并生产版本的前置硬门限，建议在 P03-004 或后续任务中执行。
3. **错误码契约同步**：`engineering/contracts/error_code_registry.csv` 已包含 ASTROCS_* 错误码定义，建议后续任务将其与 `AstroCsExitCode` 命名空间做映射校验。

---

**报告完成日期**: 2026-07-25
**子 Agent**: P03-003
