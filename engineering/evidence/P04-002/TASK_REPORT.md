# P04-002 任务报告: JSONL 事件与稳定错误码 (v1.1 开发包)

**任务 ID**: P04-002
**阶段**: P04 (CLI 协议)
**门禁**: G4 (CLI 协议)
**完成日期**: 2026-07-25
**负责人**: P04-002 子 Agent
**基线 commit**: P04-001 完成 (VERDICT: PASS)

---

## 1. 任务目标

依据 `engineering/tasks/P04-002.md` 与 v1.1 开发包工作流规则，实现：

1. **stdout 仅机器事件**: stdout 严格输出 JSONL 事件流 (每行一个 JSON 对象)，stderr 仅输出人类可读日志。
2. **JSONL 事件 schema**: 定义稳定的事件类型 (accepted/stage_started/stage_start/stage_completed/stage_end/progress/quality_metric/warning/result/error/failed/cancelled/completed)，字段名稳定供 GUI 消费。
3. **稳定错误码注册表**: 扩展 `ASTROCS_*` 错误码 (含 numeric_code/exit_name/code 字符串/category/meaning/retryable/exit_code_match)，0-10 为进程退出码，20-28 为模块特定非退出码，100+ 预留扩展。
4. **错误码一致性**: 进程退出码 == JSONL `exit_code` (顶层) == `error.numeric_code`，三种来源必须一致。
5. **失败路径事件完整性**: stage1/stage2 失败时也必须输出 `stage_end` 事件 (含 duration_ms + status=failed) 和 `error` 事件 (含数字 exit_code)。
6. **capabilities 扩展**: 输出 `exit_codes` 数组 (含 numeric_code/name/code 三元组)、`events` 数组、`jsonl_schema` 与 `error_code_registry` 路径引用。

---

## 2. 实现方案

### 2.1 契约文件 (engineering/contracts/)

**`jsonl_event_schema.json`** (`astrocs.cli.jsonl_event.v1`, Draft 2020-12):
- 必需字段: `schema_version` (const=1)、`type`、`job_id`、`timestamp` (ISO 8601 UTC)
- `type` enum: 13 种事件类型 (accepted/stage_started/stage_start/stage_completed/stage_end/progress/quality_metric/warning/result/error/failed/cancelled/completed)
- 可选字段: `stage`、`frame`、`duration_ms`、`duration_sec`、`status` (ok/failed/degraded)、`progress` (0-1)、`metric`、`rms_arcsec`、`n_pairs`、`snr_phot`、`message`、`output`、`hash` (SHA-256)、`effective_config_hash`、`exit_code`、`error` (含 code/numeric_code/message/module_code/details)
- `additionalProperties: false` (禁止未知字段)
- `$defs.exit_code_mapping`: 数字退出码到稳定名称的映射 (信息性)

**`error_code_registry.csv`** (扩展为 22 条):
- 列: `numeric_code,exit_name,code,category,meaning,retryable,exit_code_match`
- 新增 `exit_code_match` 列: 标识该码是否可作为进程退出码 (0-10=true, 20-100=false)
- P04-002 新增条目: `TIMEOUT(9)`、`CANCELLED(10)`、`STAR_DETECT_FAILED(20)`、`PSF_FAILED(21)`、`PHOTOMETRIC_FAILED(22)`、`SNR_FAILED(23)`、`STACK_FAILED(24)`、`HISS_INVALID(25)`、`HCSD_INVALID(26)`、`MODULE_ABI_UNSUPPORTED(27)`、`INPUT_INVALID(28)`、`MODULE_SPECIFIC_BASE(100)`

### 2.2 代码实现 (lib/orchestrator/cpp/)

**`include/orchestrator.h`** (`AstroCsExitCode` 命名空间扩展):
- 新增常量: `TIMEOUT=9`、`CANCELLED=10` (P04-004 用)
- 新增模块特定非退出码 (20-28, 100): `STAR_DETECT_FAILED`、`PSF_FAILED`、`PHOTOMETRIC_FAILED`、`SNR_FAILED`、`STACK_FAILED`、`HISS_INVALID`、`HCSD_INVALID`、`MODULE_ABI_UNSUPPORTED`、`INPUT_INVALID`、`MODULE_SPECIFIC_BASE`
- 新增 `error_code_string(int)`: 数字码到字符串码 (`ASTROCS_*`) 的稳定映射
- 新增 `is_process_exit_code(int)`: 判断是否为合法进程退出码 (0-10)

**`include/cli_command.h`** (新增 `output_jsonl_event_ex`):
- 签名: `output_jsonl_event_ex(event_type, job_id, stage, progress, message, result_json, error_json, exit_code, duration_ms, status, extra_json)`
- 输出字段: `schema_version/type/job_id/timestamp/stage/duration_ms/status/progress/message/result/error/exit_code` + 额外字段 (extra_json)
- `exit_code=-1` 时不输出该字段; `duration_ms=-1.0` 时不输出; `status=""` 时不输出

**`src/cli_command.cpp`** (扩展约 200 行):

1. **`output_jsonl_event_ex`**: 扩展事件输出函数，支持 `duration_ms`、`status`、`exit_code`、`extra_json` (额外字段如 `frame`、`rms_arcsec`、`n_pairs`)

2. **`cmd_inspect` 错误路径**: 
   - request 文件不存在/无法打开: 输出 `error` 事件 + `failed` 事件 (含 `numeric_code=8`、`exit_code=8`、`code=ASTROCS_INPUT_INVALID`)
   - request JSON 缺少 command: 输出 `error` 事件 + `failed` 事件 (含 `numeric_code=7`、`exit_code=7`、`code=ASTROCS_CONFIG_INVALID`)

3. **`cmd_request` 错误路径**:
   - request 文件不存在/无法打开: 输出 `error` + `failed` 事件 (FILE_IO_ERROR=8)
   - 缺少 command: 输出 `error` + `failed` 事件 (CONFIG_ERROR=7)
   - stage1 缺少 frame/output: 输出 `error` + `failed` 事件 (CONFIG_ERROR=7)
   - stage2 缺少 output/frame: 输出 `error` + `failed` 事件 (CONFIG_ERROR=7)
   - 未知 command: 输出 `error` + `failed` 事件 (CONFIG_ERROR=7)

4. **`cmd_request` 成功路径**:
   - 输出 `accepted` 事件 (含 effective_config_hash)
   - 输出 `stage_started` (旧版兼容) + `stage_start` (新版, 含 `frame` 字段)
   - 执行 stage1/stage2, 计时开始/结束
   - 成功: 输出 `stage_completed` + `stage_end` (含 duration_ms + status=ok) + `result` (含 output + hash) + `completed`
   - 失败: 输出 `stage_end` (含 duration_ms + status=failed + error) + `error` (含 exit_code) + `failed`

5. **`cmd_capabilities` 扩展**:
   - `exit_codes` 数组改为三元组: `{numeric_code, name, code}` (含字符串 `ASTROCS_*`)
   - 新增 `events` 数组: 13 种事件类型
   - 新增 `jsonl_schema` 与 `error_code_registry` 路径引用

### 2.3 错误码一致性算法

```
失败路径:
1. ec_exit = (r.exit_code != 0) ? r.exit_code : GENERIC_ERROR(1)
2. ec_str = AstroCsExitCode::error_code_string(ec_exit)  // "ASTROCS_*"
3. err_json = {
     "code": ec_str,                  // 字符串错误码 (ASTROCS_INTERNAL 等)
     "numeric_code": ec_exit,         // 数字错误码
     "message": r.error_msg,
     "exit_code": ec_exit             // 与 numeric_code 一致
   }
4. output_jsonl_event_ex("stage_end", ..., exit_code=ec_exit, status="failed")
5. output_jsonl_event_ex("error", ..., exit_code=ec_exit, status="failed")
6. output_jsonl_event("failed", ..., err_json)  // 向后兼容
7. return ec_exit  // 进程退出码 == numeric_code == exit_code
```

### 2.4 stdout/stderr 严格分离

- **stdout**: `output_jsonl_event` / `output_jsonl_event_ex` 使用 `std::cout`，输出 JSONL 事件流 (每行一个 JSON 对象)
- **stderr**: `LOG_INFO`/`LOG_WARN`/`LOG_ERROR` 通过 `Logger::log` 输出到 `std::cerr` (默认 `stderr_output_=true`)
- **格式分离**: stdout 全部为 JSONL (机器可读)，stderr 为 `[YYYY-MM-DD HH:MM:SS][LEVEL][module] message` 格式 (人类可读)
- **测试验证**: Part 7 测试 6 验证 stderr 不含完整 JSONL 行 (每行不以 `{` 开头并以 `}` 结尾)

---

## 3. 实施步骤

1. **契约定义**: 创建 `jsonl_event_schema.json` (扩展事件类型) 与更新 `error_code_registry.csv` (新增 9/10/20-28/100) (已完成)
2. **代码实现**: 修改 `orchestrator.h` (新增 TIMEOUT/CANCELLED + 模块特定码 + error_code_string/is_process_exit_code)、`cli_command.h` (新增 output_jsonl_event_ex)、`cli_command.cpp` (扩展 cmd_inspect/cmd_request/cmd_capabilities) (已完成)
3. **集成测试**: 在 `test_orchestrator_cli.cpp` 新增 Part 7 测试 (7 个测试用例，覆盖 capabilities/事件类型/错误码一致性/JSONL 有效性/stdout-stderr 分离) (已完成)
4. **构建**: `mingw32-make` 编译 orchestrator.exe + test_orchestrator_cli.exe (已完成)
5. **测试运行**: 229/229 集成测试通过 (Part 1-7, 0 失败) (已完成)
6. **证据生成**: 保存 JSONL 样本、错误码注册表 JSON、stdout/stderr 分离证据 (已完成)
7. **独立复核**: 检查契约一致性、错误码一致性、向后兼容性 (本报告后执行)

---

## 4. 关键发现

### 4.1 事件类型扩展策略

采用**双事件并发**策略保持向后兼容:
- `stage_started` (旧) + `stage_start` (新, 含 `frame` 字段): 同时输出，旧 GUI 可继续消费 `stage_started`，新 GUI 可消费 `stage_start` 获取 frame 信息
- `stage_completed` (旧) + `stage_end` (新, 含 `duration_ms` + `status`): 同时输出，旧 GUI 消费 `stage_completed`，新 GUI 消费 `stage_end` 获取 duration 与 status
- `failed` (旧) + `error` (新, 含 `exit_code`): 同时输出，旧 GUI 消费 `failed`，新 GUI 消费 `error` 获取数字 exit_code

这种策略避免了破坏性变更，同时为 GUI 提供更丰富的字段。

### 4.2 错误码分层设计

错误码注册表分三层:
1. **进程退出码 (0-10)**: `exit_code_match=true`，可直接作为进程退出码，也是 JSONL `exit_code` 字段的值
2. **模块特定非退出码 (20-28)**: `exit_code_match=false`，不作为进程退出码，但出现在 JSONL `error.numeric_code` 字段
3. **预留扩展码 (100+)**: `MODULE_SPECIFIC_BASE`，为未来模块扩展预留

分层设计允许模块内部使用细分错误码 (如 `STAR_DETECT_FAILED=20`)，同时保持进程退出码的简洁性 (失败时进程退出码为 `GENERIC_ERROR=1` 或对应阶段码如 `PLATESOLVE_FAILED=5`)。

### 4.3 错误码一致性保证

三种来源的数字必须一致:
- 进程退出码 (`return ec_exit`)
- JSONL 顶层 `exit_code` 字段 (`output_jsonl_event_ex` 的 exit_code 参数)
- JSONL `error.numeric_code` 字段 (err_json 中的 `numeric_code`)

实现通过统一使用 `ec_exit` 变量 (从 `TaskResult.exit_code` 派生) 保证三者一致。Part 7 测试 7 验证了一致性。

### 4.4 stage_end 在失败路径的输出

P04-001 实现中，stage_end 事件仅在成功路径输出。P04-002 修复: 失败路径也输出 `stage_end` 事件，包含:
- `duration_ms`: stage 实际执行时间 (即使失败)
- `status: "failed"`: 明确标识失败
- `error`: 错误详情 (含 code/numeric_code/message/exit_code)
- `exit_code`: 数字退出码

这为 GUI 提供了 stage 级别的失败时间统计能力。

### 4.5 capabilities 输出与 GUI 消费契约

`capabilities` 输出扩展为 GUI 提供完整协议入口:
- `exit_codes`: 三元组数组 (numeric_code/name/code)，GUI 可建立数字到字符串的映射
- `events`: 13 种事件类型枚举，GUI 可预先注册事件处理器
- `jsonl_schema`: 指向 `engineering/contracts/jsonl_event_schema.json`，GUI 可加载 schema 验证事件
- `error_code_registry`: 指向 `engineering/contracts/error_code_registry.csv`，GUI 可加载错误码注册表

---

## 5. 交付物清单

| 文件 | 位置 | 说明 |
|---|---|---|
| TASK_REPORT.md | `engineering/evidence/P04-002/TASK_REPORT.md` | 本报告 |
| TEST_REPORT.md | `engineering/evidence/P04-002/TEST_REPORT.md` | 测试报告 (229 测试) |
| EVIDENCE_INDEX.md | `engineering/evidence/P04-002/EVIDENCE_INDEX.md` | 证据索引 |
| REVIEW_REPORT.md | `engineering/evidence/P04-002/REVIEW_REPORT.md` | 独立复核报告 (VERDICT: PASS) |
| jsonl_event_samples.jsonl | `engineering/evidence/P04-002/jsonl_event_samples.jsonl` | JSONL 事件样本 (10 行代表性事件) |
| error_code_registry.json | `engineering/evidence/P04-002/error_code_registry.json` | 错误码注册表 JSON 格式 |
| stdout_stderr_separation.json | `engineering/evidence/P04-002/stdout_stderr_separation.json` | stdout/stderr 分离证据 |
| cap.stdout.log | `engineering/evidence/P04-002/cap.stdout.log` | capabilities 命令 stdout 捕获 |
| cap.stderr.log | `engineering/evidence/P04-002/cap.stderr.log` | capabilities 命令 stderr 捕获 |
| stage1_fail.stdout.log | `engineering/evidence/P04-002/stage1_fail.stdout.log` | stage1 失败 stdout 捕获 (6 行 JSONL) |
| stage1_fail.stderr.log | `engineering/evidence/P04-002/stage1_fail.stderr.log` | stage1 失败 stderr 捕获 |
| inspect_ok.stdout.log | `engineering/evidence/P04-002/inspect_ok.stdout.log` | inspect 成功 stdout 捕获 |
| inspect_ok.stderr.log | `engineering/evidence/P04-002/inspect_ok.stderr.log` | inspect 成功 stderr 捕获 |
| nc.stdout.log | `engineering/evidence/P04-002/nc.stdout.log` | no command 失败 stdout 捕获 (error + failed) |
| nc.stderr.log | `engineering/evidence/P04-002/nc.stderr.log` | no command 失败 stderr 捕获 |
| nf.stdout.log | `engineering/evidence/P04-002/nf.stdout.log` | file not found 失败 stdout 捕获 |
| nf.stderr.log | `engineering/evidence/P04-002/nf.stderr.log` | file not found 失败 stderr 捕获 |
| commit_msg.txt | `engineering/evidence/P04-002/commit_msg.txt` | Commit 消息文件 |

### 代码变更

| 文件 | 变更类型 | 说明 |
|---|---|---|
| `engineering/contracts/jsonl_event_schema.json` | 修改 | 扩展事件类型 enum (13 种) + 新增 stage/frame/duration_ms/status/exit_code/error 字段 |
| `engineering/contracts/error_code_registry.csv` | 修改 | 新增 exit_code_match 列 + 12 条新错误码 (TIMEOUT/CANCELLED/STAR_DETECT_FAILED 等) |
| `lib/orchestrator/cpp/include/orchestrator.h` | 修改 | AstroCsExitCode 命名空间扩展 (TIMEOUT/CANCELLED + 模块特定码 + error_code_string/is_process_exit_code) |
| `lib/orchestrator/cpp/include/cli_command.h` | 修改 | 新增 output_jsonl_event_ex 声明 |
| `lib/orchestrator/cpp/src/cli_command.cpp` | 修改 | 新增 output_jsonl_event_ex 实现 + cmd_inspect/cmd_request/cmd_capabilities 错误路径 JSONL 事件输出 |
| `lib/orchestrator/cpp/tests/test_orchestrator_cli.cpp` | 修改 | 新增 Part 7 测试 (7 个用例) |

---

## 6. 兼容性、回滚和残留风险

### 6.1 兼容性

- **向后兼容**: 旧版事件类型 (`accepted`/`stage_started`/`stage_completed`/`completed`/`failed`) 全部保留，新事件 (`stage_start`/`stage_end`/`error`/`result`) 与旧事件并发输出
- **退出码兼容**: P03-003 的 9 个退出码 (0-8) 全部保留，新增 TIMEOUT(9)/CANCELLED(10) 不影响现有路径
- **错误码字符串兼容**: `error_code_string` 函数对所有已知码返回 `ASTROCS_*` 字符串，未知码返回 `ASTROCS_INTERNAL` (向后兼容)
- **JSONL schema 兼容**: 新增字段全部为可选 (除必需的 schema_version/type/job_id/timestamp)，旧消费者可忽略新字段

### 6.2 回滚

- 若需回滚，恢复 `orchestrator.h` / `cli_command.h` / `cli_command.cpp` / `test_orchestrator_cli.cpp` 至 P04-002 前的 commit
- 恢复 `jsonl_event_schema.json` 与 `error_code_registry.csv` 至 P04-001 版本
- 回滚后需重新构建 `orchestrator.exe`

### 6.3 残留风险

1. **TIMEOUT/CANCELLED 未实际触发**: P04-002 仅定义 TIMEOUT(9)/CANCELLED(10) 错误码与字符串映射，实际超时/取消逻辑在 P04-004 (取消、超时与 partial 输出) 实现
2. **模块特定错误码 (20-28) 未实际使用**: 当前 stage handler 失败时返回 `GENERIC_ERROR(1)` 或对应阶段码 (如 `PLATESOLVE_FAILED=5`)，模块特定码 (如 `STAR_DETECT_FAILED=20`) 需在 P05+ 阶段由模块内部设置
3. **嵌套 JSON 合并未实现**: P04-001 的局限仍存在，`frame.filter` 等嵌套路径在 sources 中标记但不实际覆盖。CURRENT_TASK.md 提及"评估嵌套对象深度合并需求"，当前评估结论: GUI 暂无明确需求，保持 P04-001 的顶层合并实现，留待 P05+ 阶段按需扩展
4. **真实 stage 成功路径未验证**: P04-002 测试使用 nonexistent.fits 触发失败路径，验证了 JSONL 事件流格式与错误码一致性。真实 stage1/stage2 成功路径 (含 `result` 事件、`output`/`hash` 字段) 需在 P05-002 (Stage1 真实数据端到端) 验证
5. **DLL 加载失败环境**: 测试环境中 DLL 全部加载失败，stage1 失败返回 `GENERIC_ERROR(1)` 而非 `DLL_LOAD_FAILED(2)`。这是因为 `run_stage1` 在 DLL 加载失败时设置 `exit_code=1`，未来可改进为更细粒度的错误码

---

## 7. 后续建议

1. **P04-003 (capabilities 与 inspect 命令)**: 已在 P04-001/P04-002 实现基础版本，后续可扩展 inspect 支持配置差异比较、capabilities 支持版本号查询
2. **P04-004 (取消、超时与 partial 输出)**: 实现 `timeouts` 字段的语义 (stage 级超时) 与取消信号处理，实际触发 TIMEOUT(9)/CANCELLED(10) 错误码
3. **P05-002 (Stage1 真实数据端到端)**: 验证 `--request` 模式在真实 FITS 输入下的完整成功路径，包括 `result` 事件的 `output`/`hash` 字段、`stage_end` 的 `duration_ms`/`status=ok` 字段
4. **模块特定错误码传播**: 在 P05+ 阶段，各 stage handler 内部使用细分错误码 (如 `STAR_DETECT_FAILED=20`)，并确保 `error.numeric_code` 正确反映模块内部错误
5. **JSONL schema 验证脚本**: 可创建 Python 脚本使用 `jsonschema` 库验证实际 stdout 输出是否符合 `jsonl_event_schema.json` (类似 P04-001 的 `validate_schemas.py`)

---

**报告完成日期**: 2026-07-25
**子 Agent**: P04-002
