# P04-003 证据索引

**任务 ID**: P04-003
**任务名称**: capabilities 与 inspect 命令 (v1.1 开发包)
**完成日期**: 2026-07-25
**门禁**: G4 (CLI 协议)

---

## 1. 证据文件清单

### 1.1 报告文件

| 文件 | 说明 | 大小 |
|---|---|---|
| `TASK_REPORT.md` | 任务报告 (实现方案/关键发现/交付物清单/兼容性风险) | ~14KB |
| `TEST_REPORT.md` | 测试报告 (317/317 通过, Part 8 详情, inspect 真实数据样本) | ~13KB |
| `EVIDENCE_INDEX.md` | 本文件 (证据索引) | ~4KB |
| `REVIEW_REPORT.md` | 独立复核报告 (VERDICT: PASS) | ~6KB |

### 1.2 命令输出捕获

| 文件 | 说明 | 行数 |
|---|---|---|
| `capabilities_output.json` | capabilities 命令 stdout 捕获 (含 modules/stages/schema_versions) | 52 |
| `capabilities.stderr.log` | capabilities 命令 stderr 捕获 (日志) | 1 |
| `inspect_hiss_output.json` | inspect --hiss stdout 捕获 (2 行 JSONL: result + completed) | 2 |
| `inspect_hiss.stderr.log` | inspect --hiss stderr 捕获 (日志) | 1 |
| `inspect_hcsd_output.json` | inspect --hcsd stdout 捕获 (2 行 JSONL: result + completed) | 2 |
| `inspect_hcsd.stderr.log` | inspect --hcsd stderr 捕获 (日志) | 1 |
| `inspect_frame_output.json` | inspect --frame stdout 捕获 (2 行 JSONL: result + completed, 70+ FITS 关键字) | 2 |
| `inspect_frame.stderr.log` | inspect --frame stderr 捕获 (日志) | 1 |

### 1.3 测试证据

| 文件 | 说明 |
|---|---|
| `test_output.log` | 集成测试 stdout 捕获 (317 测试全通过, Part 1-8) |
| `test_error.log` | 集成测试 stderr 捕获 (DLL 加载日志) |

### 1.4 提交文件

| 文件 | 说明 |
|---|---|
| `commit_msg.txt` | Git commit 消息文件 (供 vq-commit.ps1 使用) |

---

## 2. 代码变更清单

### 2.1 源代码文件

| 文件 | 变更类型 | 关键变更 |
|---|---|---|
| `lib/orchestrator/cpp/include/cli_command.h` | 修改 | 新增 cmd_inspect_hiss/cmd_inspect_hcsd/cmd_inspect_frame 声明 + cmd_stage1/cmd_stage2 添加 cancel_on_signal 参数 (P04-004 集成修复) |
| `lib/orchestrator/cpp/src/cli_command.cpp` | 修改 | 实现 3 个 inspect 子命令 (~250 行) + 扩展 cmd_capabilities (modules/stages/schema_versions, ~60 行) + cmd_stage1/cmd_stage2 注册 signal handler (P04-004 集成, ~10 行) + 修复 extern "C" static 语法错误 |
| `lib/orchestrator/cpp/src/dll_loader.cpp` | 修改 | 扩展 get_version 支持更多模块 (ac_version/ipv_version/aio_version 等命名约定) |
| `lib/orchestrator/cpp/src/orchestrator.cpp` | 修改 | 修复 P04-004 字符串拼接错误 (name → std::string(name)) |
| `lib/orchestrator/cpp/tests/test_orchestrator_cli.cpp` | 修改 | 新增 Part 8 测试 (13 个用例, 88 个断言, ~290 行) |

### 2.2 控制文件

| 文件 | 变更类型 | 关键变更 |
|---|---|---|
| `engineering/control/MASTER_TASK_REGISTER.csv` | 修改 | P04-003 状态 IN_PROGRESS → DONE |

### 2.3 构建产物

| 文件 | 位置 | 说明 |
|---|---|---|
| `orchestrator.exe` | `lib/orchestrator/cpp/orchestrator.exe` (4.1MB) | 主构建产物 |
| `orchestrator.exe` | `build/artifacts/orchestrator.exe` (4.1MB) | 复制到 artifacts 目录供后续任务使用 |
| `test_orchestrator_cli.exe` | `lib/orchestrator/cpp/tests/test_orchestrator_cli.exe` (4.0MB) | 测试可执行文件 |

---

## 3. 测试结果摘要

| 测试套件 | 测试数 | 断言数 | 通过 | 失败 | 状态 |
|---|---|---|---|---|---|
| Part 1: 交互式 REPL 命令 | 11 | 30+ | 11 | 0 | PASS |
| Part 2: 单次命令执行 | 11 | 25+ | 11 | 0 | PASS |
| Part 3: 断点续传 | 6 | 28+ | 6 | 0 | PASS |
| Part 4: DLL 加载失败降级 | 6 | 20+ | 6 | 0 | PASS |
| Part 5: 日志系统集成 | 6 | 35+ | 6 | 0 | PASS |
| Part 6: P04-001 CLI request | 12 | 40+ | 12 | 0 | PASS |
| Part 7: P04-002 JSONL 事件 | 7 | 41 | 7 | 0 | PASS |
| Part 8: P04-003 capabilities + inspect | 13 | 88 | 13 | 0 | PASS |
| **总计** | **72** | **317** | **317** | **0** | **PASS** |

**回归状态**: 0 退化 (Part 1-7 既有测试全通过, P04-001/P04-002 功能未变)

---

## 4. 验收点证据映射

| 验收点 | 证据文件 | 验证方法 |
|---|---|---|
| capabilities 输出 modules 数组 | `capabilities_output.json` | 含 10 个模块, 每个含 name/version/capabilities |
| capabilities 输出 stages 数组 | `capabilities_output.json` | 含 8 个 stage (READ_FITS/CALIBRATE/...) |
| capabilities 输出 schema_versions | `capabilities_output.json` | 含 6 个契约版本 (hiss/hcsd/star_det/request/effective_config/jsonl_event) |
| capabilities 含 hiss_format/hcsd_format 路径 | `capabilities_output.json` | 含契约文件路径引用 |
| inspect --hiss 元数据输出 | `inspect_hiss_output.json` | 含 nside=512/nested=true/n_pix=3927/meta_json (WCS+FITS头+drizzle) |
| inspect --hcsd 元数据输出 | `inspect_hcsd_output.json` | 含 nside=32768/n_leaves=49152/n_pix=15522966/meta_json |
| inspect --frame 元数据输出 | `inspect_frame_output.json` | 含 simple=true/keywords (70+ FITS 关键字) |
| HISS_INVALID(25) 错误码触发 | Part 8 测试 7 (test_output.log) | inspect --hiss 无效 magic 退出码=25 |
| HCSD_INVALID(26) 错误码触发 | Part 8 测试 8 (test_output.log) | inspect --hcsd 无效 magic 退出码=26 |
| INPUT_INVALID(28) 错误码触发 | Part 8 测试 9 (test_output.log) | inspect --frame 无效 FITS 退出码=28 |
| JSONL 输出格式 (result + completed) | `inspect_*_output.json` | 每个文件 2 行 JSONL: result + completed 事件 |
| stdout/stderr 严格分离 | `*.stderr.log` | stderr 仅含日志, 不含 JSONL 行 |
| 互斥分发优先级 | Part 8 测试 13 (test_output.log) | --hiss 优先于 --hcsd |
| 向后兼容 (P04-001/P04-002) | Part 6-7 测试全通过 | inspect --request 与 capabilities 既有功能未变 |

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
cd "f:\Astro dev\Astro CS Normalization Database\lib\orchestrator\cpp"
.\tests\test_orchestrator_cli.exe
```

### 5.3 捕获命令输出

```powershell
$exe = ".\orchestrator.exe"
$evDir = "..\..\..\engineering\evidence\P04-003"

# capabilities
& $exe capabilities 2>"$evDir\capabilities.stderr.log" > "$evDir\capabilities_output.json"

# inspect --hiss (P00-003 baseline)
& $exe inspect --hiss "..\..\..\engineering\evidence\P00-003\output\stage1_baseline.hiss" 2>"$evDir\inspect_hiss.stderr.log" > "$evDir\inspect_hiss_output.json"

# inspect --hcsd (P00-003 baseline)
& $exe inspect --hcsd "..\..\..\engineering\evidence\P00-003\output\stage2_baseline.hcsd" 2>"$evDir\inspect_hcsd.stderr.log" > "$evDir\inspect_hcsd_output.json"

# inspect --frame (真实 FITS)
& $exe inspect --frame "..\..\..\testdata\Victory_Nebula_T4_Flying_Dutchman\lights\Victory_Nebula_mosaic1_flying_dutchman-20250204@035646-180S-Lum.fts" 2>"$evDir\inspect_frame.stderr.log" > "$evDir\inspect_frame_output.json"
```

### 5.4 复制构建产物

```powershell
Copy-Item "lib\orchestrator\cpp\orchestrator.exe" "build\artifacts\orchestrator.exe" -Force
```

---

## 6. 证据完整性

- 所有证据文件位于 `engineering/evidence/P04-003/`
- 代码变更位于 `lib/orchestrator/cpp/` 与 `engineering/control/`
- 测试可重复执行 (无随机性, 仅依赖 P00-003 baseline HISS/HCSD 与 testdata FITS 文件)
- inspect 命令输出样本来自实际 orchestrator.exe 运行 (非手工构造)
- capabilities 输出含 10 个模块的真实版本号查询结果 (calibration 返回真实版本, 其余 unknown)
- 测试覆盖 13 个用例, 88 个断言, 包括成功路径、错误路径、互斥分发、JSONL 有效性、stdout/stderr 分离

---

**索引完成日期**: 2026-07-25
**子 Agent**: P04-003
