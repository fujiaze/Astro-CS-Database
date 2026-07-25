# P04-002 证据索引

**任务 ID**: P04-002
**任务名称**: JSONL 事件与稳定错误码 (v1.1 开发包)
**完成日期**: 2026-07-25
**门禁**: G4 (CLI 协议)

---

## 1. 证据文件清单

### 1.1 报告文件

| 文件 | 说明 | 大小 |
|---|---|---|
| `TASK_REPORT.md` | 任务报告 (实现方案/关键发现/交付物清单/兼容性风险) | ~12KB |
| `TEST_REPORT.md` | 测试报告 (229/229 通过, Part 7 详情, JSONL 样本) | ~10KB |
| `EVIDENCE_INDEX.md` | 本文件 (证据索引) | ~3KB |
| `REVIEW_REPORT.md` | 独立复核报告 (VERDICT: PASS) | ~5KB |

### 1.2 测试证据

| 文件 | 说明 |
|---|---|
| `jsonl_event_samples.jsonl` | JSONL 事件样本 (10 行代表性事件: accepted/stage_started/stage_start/stage_end/error/failed) |
| `error_code_registry.json` | 错误码注册表 JSON 格式 (22 条记录) |
| `stdout_stderr_separation.json` | stdout/stderr 分离证据 (JSON) |

### 1.3 命令输出捕获

| 文件 | 说明 | 行数 |
|---|---|---|
| `cap.stdout.log` | capabilities 命令 stdout 捕获 (能力声明 JSON) | 25 |
| `cap.stderr.log` | capabilities 命令 stderr 捕获 (日志) | 1 |
| `stage1_fail.stdout.log` | stage1 失败 stdout 捕获 (6 行 JSONL 事件) | 6 |
| `stage1_fail.stderr.log` | stage1 失败 stderr 捕获 (日志) | 2 |
| `inspect_ok.stdout.log` | inspect 成功 stdout 捕获 (effective_config JSON) | 10 |
| `inspect_ok.stderr.log` | inspect 成功 stderr 捕获 (日志) | 1 |
| `nc.stdout.log` | no command 失败 stdout 捕获 (error + failed 事件) | 2 |
| `nc.stderr.log` | no command 失败 stderr 捕获 (日志) | 2 |
| `nf.stdout.log` | file not found 失败 stdout 捕获 (error + failed 事件) | 2 |
| `nf.stderr.log` | file not found 失败 stderr 捕获 (日志) | 1 |

### 1.4 提交文件

| 文件 | 说明 |
|---|---|
| `commit_msg.txt` | Git commit 消息文件 (供 vq-commit.ps1 使用) |

---

## 2. 代码变更清单

### 2.1 契约文件

| 文件 | 变更类型 | 关键变更 |
|---|---|---|
| `engineering/contracts/jsonl_event_schema.json` | 修改 | 扩展 type enum (13 种事件) + 新增 stage/frame/duration_ms/status/exit_code/error 字段定义 |
| `engineering/contracts/error_code_registry.csv` | 修改 | 新增 exit_code_match 列 + 12 条新错误码 (TIMEOUT/CANCELLED/STAR_DETECT_FAILED 等) |

### 2.2 源代码文件

| 文件 | 变更类型 | 关键变更 |
|---|---|---|
| `lib/orchestrator/cpp/include/orchestrator.h` | 修改 | AstroCsExitCode 命名空间扩展: 新增 TIMEOUT(9)/CANCELLED(10) + 模块特定码 (20-28, 100) + error_code_string/is_process_exit_code 辅助函数 |
| `lib/orchestrator/cpp/include/cli_command.h` | 修改 | 新增 output_jsonl_event_ex 方法声明 (支持 exit_code/duration_ms/status/extra_json) |
| `lib/orchestrator/cpp/src/cli_command.cpp` | 修改 | 新增 output_jsonl_event_ex 实现 + cmd_inspect/cmd_request 错误路径 JSONL 事件输出 + cmd_capabilities 扩展 (numeric_code/name/code 三元组 + events 数组 + jsonl_schema/error_code_registry 路径) |
| `lib/orchestrator/cpp/tests/test_orchestrator_cli.cpp` | 修改 | 新增 Part 7 测试 (7 个用例, 41 个断言) |

---

## 3. 测试结果摘要

| 测试套件 | 测试数 | 断言数 | 通过 | 失败 | 状态 |
|---|---|---|---|---|---|
| Part 1: 交互式 REPL 命令 | 11 | 30+ | 11 | 0 | PASS |
| Part 2: 单次命令执行 | 11 | 20+ | 11 | 0 | PASS |
| Part 3: 断点续传 | 6 | 25+ | 6 | 0 | PASS |
| Part 4: DLL 加载失败降级 | 6 | 20+ | 6 | 0 | PASS |
| Part 5: 日志系统集成 | 6 | 25+ | 6 | 0 | PASS |
| Part 6: P04-001 CLI request | 12 | 40+ | 12 | 0 | PASS |
| Part 7: P04-002 JSONL 事件 | 7 | 41 | 7 | 0 | PASS |
| **总计** | **59** | **229** | **229** | **0** | **PASS** |

**回归状态**: 0 退化 (Part 1-6 既有测试全通过)

---

## 4. 验收点证据映射

| 验收点 | 证据文件 | 验证方法 |
|---|---|---|
| stdout 仅机器事件 (JSONL) | `stage1_fail.stdout.log`, `nc.stdout.log`, `nf.stdout.log` | 每行 JSONL 可解析, Part 7 测试 5 |
| stderr 日志 | `stage1_fail.stderr.log`, `nc.stderr.log`, `nf.stderr.log` | 日志格式 [时间][级别][模块], Part 7 测试 6 |
| 退出码和错误注册表一致 | `error_code_registry.json`, Part 7 测试 7 | 进程退出码 == JSONL exit_code == error.numeric_code |
| JSONL 事件 schema | `engineering/contracts/jsonl_event_schema.json` | 13 种事件类型 + 字段定义 |
| 错误码注册表 | `engineering/contracts/error_code_registry.csv` | 22 条记录, 含 exit_code_match 列 |
| 字段可由 GUI 稳定消费 | `cap.stdout.log` | capabilities 输出 exit_codes 三元组 + events 数组 + schema 路径 |
| 失败路径 stage_end 事件 | `stage1_fail.stdout.log` (第 4 行) | stage_end 含 duration_ms + status=failed + error + exit_code |
| stdout/stderr 严格分离 | `stdout_stderr_separation.json`, Part 7 测试 6 | stderr 不含 JSONL 行 |

---

## 5. 复现步骤

### 5.1 构建

```powershell
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
cd "f:\Astro dev\Astro CS Normalization Database\lib\orchestrator\cpp"
mingw32-make -f Makefile
mingw32-make -f Makefile test_orchestrator_cli
```

### 5.2 运行测试

```powershell
.\tests\test_orchestrator_cli.exe
```

### 5.3 捕获 JSONL 样本

```powershell
$exe = ".\orchestrator.exe"
# stage1 失败路径
& $exe stage1 --request <req.json> 2>err.log > stdout.log
# inspect 成功路径
& $exe inspect --request <req.json> 2>err.log > stdout.log
# capabilities
& $exe capabilities 2>err.log > stdout.log
```

---

## 6. 证据完整性

- 所有证据文件位于 `engineering/evidence/P04-002/`
- 代码变更位于 `lib/orchestrator/cpp/` 与 `engineering/contracts/`
- 测试可重复执行 (无随机性, 无外部依赖)
- JSONL 样本来自实际 orchestrator.exe 运行 (非手工构造)

---

**索引完成日期**: 2026-07-25
**子 Agent**: P04-002
