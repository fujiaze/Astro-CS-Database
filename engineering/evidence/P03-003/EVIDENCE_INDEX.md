# P03-003 证据索引

**任务 ID**: P03-003
**生成日期**: 2026-07-25

---

## 1. 代码变更文件

| 文件 | SHA-256 | 大小 (字节) | 说明 |
|---|---|---|---|
| `lib/orchestrator/cpp/include/orchestrator.h` | `B84B24D842C0810480E59EF300C130B0FD3B1A41F8E618CE4478E502FEFB59A0` | 13087 | 新增 AstroCsExitCode 命名空间 + TaskResult.exit_code 字段 |
| `lib/orchestrator/cpp/src/orchestrator.cpp` | `01CFC832994BCBFB4DF4E82CCCC1F80FA0259373F1911E2A972910E648FB1FB8` | 161183 | 87 处 P03-003 标记，覆盖所有必需 stage 失败路径 |
| `lib/orchestrator/cpp/src/cli_command.cpp` | `29CB3885B8ED4B97739158AEF726F4216922CD32F33E197D6A0EEF0088A2E505` | 23976 | 4 个入口点传播 exit_code |
| `lib/orchestrator/cpp/tests/test_orchestrator_cli.cpp` | `0D8816E2A5BC24FD2EB277B3581DC255B3F39DF2F5BC9715007F5DD1B48A4E55` | 42586 | 测试 6 期望退出码从 2 改为 7 |

## 2. 构建产物

| 文件 | SHA-256 | 大小 (字节) | 说明 |
|---|---|---|---|
| `lib/orchestrator/cpp/orchestrator.exe` | `8969B37FA09451178237F4A511B9F3084F6E92D5F2D6959822AC50D9E77E34DA` | 3984808 | P03-003 重新编译后的 orchestrator.exe |
| `build/artifacts/orchestrator.exe` | `8969B37FA09451178237F4A511B9F3084F6E92D5F2D6959822AC50D9E77E34DA` | 3984808 | 复制到 build/artifacts 的同一产物 |

## 3. 测试产物

| 文件 | 说明 |
|---|---|
| `engineering/evidence/P03-003/exit_code_evidence.log` | 端到端退出码验证日志（5 个 CLI 场景） |

## 4. 报告文件

| 文件 | 说明 |
|---|---|
| `engineering/evidence/P03-003/TASK_REPORT.md` | 任务报告 |
| `engineering/evidence/P03-003/TEST_REPORT.md` | 测试报告 |
| `engineering/evidence/P03-003/EVIDENCE_INDEX.md` | 本证据索引 |
| `engineering/evidence/P03-003/REVIEW_REPORT.md` | 评审报告 |
| `engineering/evidence/P03-003/error_code_registry.json` | 错误码注册表（机器可读） |

## 5. 测试结果摘要

| 测试类别 | 通过/总数 |
|---|---|
| 集成测试 (test_orchestrator_cli) | 136/136 |
| 端到端退出码验证 | 5/5 |
| **合计** | **141/141** |

## 6. 复现命令

### 6.1 构建

```powershell
cd "f:\Astro dev\Astro CS Normalization Database\lib\orchestrator\cpp"
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
make clean
make all
make test_orchestrator_cli
```

### 6.2 集成测试

```powershell
.\tests\test_orchestrator_cli.exe
```

### 6.3 端到端退出码验证

```powershell
$exe = ".\orchestrator.exe"
& $exe --help | Out-Null; $LASTEXITCODE  # 期望 0
& $exe run "Z:/nonexistent.fits" 2>&1 | Out-Null; $LASTEXITCODE  # 期望非 0
& $exe run "Z:/nonexistent.fits" --config "Z:/nonexistent.json" 2>&1 | Out-Null; $LASTEXITCODE  # 期望 7
& $exe run-batch "Z:/nonexistent_dir_xyz" 2>&1 | Out-Null; $LASTEXITCODE  # 期望 8
& $exe unknown_subcommand 2>&1 | Out-Null; $LASTEXITCODE  # 期望 1
```

---

**证据生成日期**: 2026-07-25
**子 Agent**: P03-003
