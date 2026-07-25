# P04-001 证据索引: CLI request 与 effective config

**任务 ID**: P04-001
**阶段**: P04 (CLI 协议)
**门禁**: G4
**完成日期**: 2026-07-25
**子 Agent**: P04-001
**基线 commit**: eb6eeb4 (P03 阶段完成)

---

## 1. 任务摘要

实现 CLI `--request` 模式、配置优先级合并 (default < config < overrides < cli)、effective_config 快照与 SHA-256 hash、stdout/stderr 严格分离、JSON schema 测试，新增 `inspect` 与 `capabilities` 子命令。

**测试结果**: 234/234 通过 (189 C++ 集成测试 + 45 Python schema 验证)
**VERDICT**: PASS

---

## 2. 证据文件清单

### 2.1 报告类

| 文件 | 说明 | 关键内容 |
|---|---|---|
| `TASK_REPORT.md` | 任务报告 | 实现方案、关键发现、兼容性、回滚、残留风险、后续建议 |
| `TEST_REPORT.md` | 测试报告 | 234 测试详情、覆盖度评估、回归测试、端到端场景 |
| `EVIDENCE_INDEX.md` | 本索引 | 证据文件清单与说明 |
| `REVIEW_REPORT.md` | 独立复核报告 | 验收检查、风险与建议、VERDICT: PASS |

### 2.2 测试日志与脚本

| 文件 | 说明 | 关键内容 |
|---|---|---|
| `integration_test.log` | C++ 集成测试完整输出 | 189/189 PASS, Part 1-6 全部通过 |
| `schema_validation.log` | Python schema 验证脚本输出 | 45/45 PASS, 7 sections |
| `validate_schemas.py` | Schema 验证脚本 (可重复执行) | jsonschema 库 + subprocess 调用 orchestrator.exe |

### 2.3 实际输出样例 (机器可读证据)

| 文件 | 说明 | 关键内容 |
|---|---|---|
| `cli_request_effective_config.json` | 实际 inspect 输出样例 | effective_config_hash=`2f02216a9313ef189ae42bf741df90d3f82b6e2cf38ffa020b3b502841f011fc`, sources 完整标记 |
| `stdout_stderr_separation.json` | stdout/stderr 分离证据 | stdout=JSON, stderr=日志, line counts |
| `build_artifacts.sha256` | 构建产物 SHA-256 | orchestrator.exe + test_orchestrator_cli.exe |

### 2.4 提交相关

| 文件 | 说明 |
|---|---|
| `commit_msg.txt` | Commit 消息文件 (供 vq-commit.ps1 -MessageFile 使用) |

---

## 3. 代码变更清单

### 3.1 新增文件

| 路径 | 类型 | 说明 |
|---|---|---|
| `engineering/contracts/cli_request_schema.json` | 契约 | CLI request JSON Schema (Draft 2020-12, $id=astrocs.cli.request.v1) |
| `engineering/contracts/effective_config_schema.json` | 契约 | Effective config JSON Schema (Draft 2020-12, $id=astrocs.cli.effective_config.v1) |
| `engineering/evidence/P04-001/validate_schemas.py` | 测试脚本 | JSON Schema 验证 + 端到端语义测试 |

### 3.2 修改文件

| 路径 | 变更说明 |
|---|---|
| `lib/orchestrator/cpp/include/cli_command.h` | 新增 EffectiveConfig 结构体 + cmd_inspect/cmd_capabilities/cmd_request/compute_effective_config/output_jsonl_event 声明 |
| `lib/orchestrator/cpp/src/cli_command.cpp` | 新增 SHA-256 实现 + JSON 合并工具 + inspect/capabilities/cmd_request 实现 + stage1/stage2 --request 参数解析 (新增约 700 行) |
| `lib/orchestrator/cpp/tests/test_orchestrator_cli.cpp` | 新增 Part 6 测试 (12 个用例) + ASSERT_FALSE 宏 |
| `lib/orchestrator/cpp/.gitignore` | 新增 `nul` 项 (Windows 保留名文件) |

---

## 4. 验收检查清单

依据 `engineering/tasks/P04-001.md` 与 `engineering/checklists/G4_CLI_PROTOCOL.md`:

| # | 验收项 | 状态 | 证据 |
|---|---|---|---|
| 1 | 依赖任务均已通过 (P03-003 DONE) | ✅ | MASTER_TASK_REGISTER.csv |
| 2 | request JSON 实现与 schema 测试 | ✅ | cli_request_schema.json + validate_schemas.py (45/45) |
| 3 | 配置优先级实现 (CLI > overrides > config > default) | ✅ | compute_effective_config + TEST_REPORT Section 5 |
| 4 | effective_config 快照与 SHA-256 hash | ✅ | cli_request_effective_config.json + hash 幂等性测试 |
| 5 | stdout/stderr 严格分离 | ✅ | stdout_stderr_separation.json + TEST_REPORT Section 7 |
| 6 | JSON schema 测试 (合法/非法样例) | ✅ | 7 合法 + 10 非法样例全部正确判定 |
| 7 | orchestrator CLI 修改 (inspect/capabilities/--request) | ✅ | cli_command.cpp 新增约 700 行 |
| 8 | 字段和错误码稳定 (供 GUI 消费) | ✅ | capabilities 输出 + AstroCsExitCode 复用 |
| 9 | 本任务目标有可复现证据 | ✅ | validate_schemas.py 可重复执行 |
| 10 | 相关回归全部运行 | ✅ | Part 1-5 既有测试 145/145 + Part 6 新增 44/44 |
| 11 | 独立复核以 VERDICT: PASS 结束 | ✅ | REVIEW_REPORT.md |
| 12 | 更新任务注册表、当前任务和项目状态 | ✅ | MASTER_TASK_REGISTER.csv + CURRENT_TASK.md + PROJECT_STATE.yaml |

---

## 5. 复现步骤

### 5.1 构建与测试

```powershell
# 1. 添加 g++ 到 PATH (MSYS2 MinGW64)
$env:Path = "C:\msys64\mingw64\bin;$env:Path"

# 2. 进入 orchestrator 目录
cd "f:\Astro dev\Astro CS Normalization Database\lib\orchestrator\cpp"

# 3. 构建 orchestrator.exe + test_orchestrator_cli.exe
make all
make test_orchestrator_cli

# 4. 运行 C++ 集成测试 (189 个)
.\tests\test_orchestrator_cli.exe

# 5. 运行 JSON schema 验证 (45 个)
python ..\..\..\engineering\evidence\P04-001\validate_schemas.py
```

### 5.2 手动验证

```powershell
# capabilities 子命令
.\orchestrator.exe capabilities

# inspect 有效 request
$req = @{
    schema_version = 1
    command = "stage1"
    job_id = "demo_job"
    frame = "nonexistent.fits"
    output = "out.hiss"
    config = @{ threads = 4; log_level = "INFO" }
    overrides = @{ threads = 8 }
} | ConvertTo-Json
$req | Out-File -FilePath demo_request.json -Encoding utf8
.\orchestrator.exe inspect --request demo_request.json

# stdout/stderr 分离验证
.\orchestrator.exe inspect --request demo_request.json 2>stderr.log 1>stdout.json
```

---

## 6. 关键指标

| 指标 | 值 |
|---|---|
| 新增代码行数 (cli_command.cpp) | ~700 行 |
| 新增测试用例数 | 12 (C++ Part 6) + 45 (Python schema) = 57 |
| 测试通过率 | 234/234 = 100% |
| 构建产物 SHA-256 (orchestrator.exe) | `94ACB704...D8ECE690` |
| 构建产物 SHA-256 (test_orchestrator_cli.exe) | `03B1FE02...85D88C7` |
| effective_config_hash 样例 | `2f02216a9313ef189ae42bf741df90d3f82b6e2cf38ffa020b3b502841f011fc` |
| 兼容性回归 | 0 退化 (Part 1-5 全部通过) |

---

**索引完成日期**: 2026-07-25
**子 Agent**: P04-001
