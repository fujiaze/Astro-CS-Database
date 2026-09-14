# V9 片3-4 · P1（永不红机制 + 覆盖面缺口）

### V9-N-08（P1·机制④新根因）`CON-COMMENTS` 当前是**假绿**而非合规：注释提取正则 `re.S` 使 `//.*` **一路吞到文件尾** ⇒ 一条含"冻结"的注释即可抑制整文件的 V19R2/V19R3 违规
- 判据原文：`re.findall(r'//.*|/\*.*?\*/', text, re.S)` —— **`re.S` 让 `//.*` 跨行吞至文件尾**：`lib/phase2/tests/synthetic_gate.cpp` 的第一条"注释"**跨 222,679 字符（≈整文件剩余）**；而规则是 `if "冻结" not in c` 才判违规 ⇒ **文件后文任意处出现"冻结"二字，整文件被抑制**。
- 复算（`scripts_v9_comments_exact.py`，638 个 lib 源文件逐字复刻）：门口径 `COMMENT-STALE: 0 / verdict PASS`；**按"逐行注释"正确口径复算，同文件 `:5488` 的 `// F-V19R2-UPM-002：…` 确实违规**；`lib` 内 **181/638 文件含"冻结"**，含 V19R2/V19R3 的 **2 个文件 100% 被抑制**。⇒ **邻站若只"删掉那行注释"，门依旧永远不红。**
- 建议：去掉 `re.S`（或 `//.*` 分支改 `[^\n]*`）+ 加一条"整文件被单条注释抑制"的告警。**related** `V15-N-07`（无 `is_open` 前置 fail-open）、`M8-F-015`、簇 1 机制④、`C-16`

### V9-N-09（P1·机制⑤）39 道 `ctest-target` 门**不带 `--fail-if-no-tests`** ⇒ 目标改名/未构建即记 PASS，且本仓有真机直证其退出码为 0
- `tools/quality/deep_ci_driver.py::_ctest_argv` 只在 `--junit` 时追加参数，实际 `argv = ctest -R "^<target>$" --output-on-failure`。⇒ 本仓自带真机直证：`artifacts/ci/.../win-test-summary.json` 记 `"exit_code": 0` + `"output_tail": "… No tests were found!!!"`。
- **叠加两个实测缺口**：(a) 该 39 门里 **20 门在 `artifacts/ci` 全部 50 次跑中零执行证据**（`scripts_v9_evidence.py` 名单）；(b) **`CTEST-PHASE2-GATES` 的 command 有 `--target phase2_.*` 却缺 `ctest_targets` 字段**（`scripts_v9_finalgap.py` → `NO ctest_targets field`）⇒ STD-F7 的"逐目标登记"对它不成立。
- 建议：`_ctest_argv` 无条件加 `--fail-if-no-tests`；给 20 个零证据门补执行面；`CTEST-PHASE2-GATES` 补 `ctest_targets`。**related** `M8-F-004`（在册≠执行）、`FD-F-003`、`A-38`、`V4-N-01`

### V9-N-10（P1·机制②新通道）`ci/run.py:898` 把 **exit 77 无条件判为 `SKIPPED(waivable)`，不看该门的 `waivable`** ⇒ 非豁免门可由 checker 自己 `sys.exit(77)` 从门禁面隐身
- 直证：**`UT-CPU-AVX512` 登记 `waivable=false`**，而 `48ceee59` 那次实际是 `verdict=SKIPPED(waivable), exit=77`；`run.py:1040-1066` 还把它计入 `skipped_waivable` 并让总判定继续写「全部非 waivable 检查 PASS；N 项 SKIPPED(waivable)」。⇒ **`waivable` 字段与 CI 总判定都可被绕过。**
- 建议：77 语义限定为 `waivable=true` 才可用；`waivable=false` 门返回 77 ⇒ 判 `FAIL`（配 `E5`/`R-05`「严禁用 waiver 掩盖红灯」）。
- **related** **`M8-F-009`/`FD-F-003`**（0 输出/SKIP 记 PASS 同族）、簇 1 机制②、`V11`「L23-001 措辞」、`A-38`、`C-21`

### V9-N-12（P1·**直接推翻"定向复跑全绿"这条自证手段**）`impact_map.json` 只触达 71/130 门 ⇒ **74 门既不在任何 rule 也不在 fallback**，其中含**当前必红的 `DOC-LINE-ANCHORS`**
- 数字：24 rules + 15 fallback ⇒ 触达 71 id；**74/130 不可达**；`fast` 面 7 门不可达（`THREAD-BUDGET`、`WORKFLOW-REGISTRY-BINDING`、`CI-BINDING-TESTS`、**`CTEST-REGISTRATION`**、`KNOWN-FAILURES-BASELINE-VERIFY`、`UT-GAIA-ZLIB`、**`DOC-LINE-ANCHORS`**）；`linux-main` 面 **64 门不可达（含全部 21 个 UT-* 与全部 CTEST-*）**。复算 `scripts_v9_impact2.py`。
- **具体化后果（这是给隔壁的行为纠正，不是抽象缺口）**：控制包 runbook `04_FOREGROUND_AGENT_RUNBOOK.md:54` 让 agent 用 `python3 ci/run.py --profile fast --changed-from HEAD` 做定向复跑 ⇒ **该命令永远选不到 `DOC-LINE-ANCHORS`/`CTEST-REGISTRATION`** ⇒ **「定向复跑全绿」不能证明 fast 绿**，隔壁据此报的"已修"证据链**在此处断裂**。
- 建议：①补齐 `impact_map`；②`--changed-from` 对不可达门**打告警并非零退出**；③工单加硬规则：**定向复跑后必须补跑不可达门**。**related** `M8-F-004`、`V4-N-05`、`C-13`、`V13-N-04`

### V9-N-14（P1·机制③在册无载体 + 9% 采集率）`CI-BINDING-TESTS` 的命令只匹配 **21 个文件中的 2 个、379 个 test-def 中的 33 个（9%）**；`validate_registry.py` 与两个关键工具在登记面**零载体**
- `ci/run.py:103 EMPTY_OUTPUT_SILENCE_EXEMPT` 之外：`ci/validate_registry.py` 在 `checks.json` **0 次**、`.github/workflows` **0 次**，仅 `ci/tests` 引用 15 处；而门命令是 `unittest discover -s ci/tests -p "test_ci001b_*.py"` ⇒ `test_ctest_registration.py`/`test_impact_map.py`/`test_negative_guards.py`/`test_workflow_lock.py`/`test_runner_selection.py` 等 **346 个 def 不进任何门**。另 `tools/quality/check_registration_timeouts.py`（专查 timeout 一致性）与 `tools/traceability/gen_traceability_csv.py`（**TRACEABILITY-MATRIX 的报错信息里让"重跑"它**）在 checks/workflows/ci-tests **三处全零载体**。
- **本时段实测后果**：门数 **121→130**、`DEEP-SAN-ASAN`/`DEEP-COV-CPP` 的 `waivable` 由 `true→false` 翻转，**无任何 schema/一致性门拦截** ⇒ 这正是我此前 `M8-F-001` 改判 PARTIAL 的那条（`validate_registry` 无 CI 执行面）的**完整确认**。
- 建议：discover 模式改 `-p "test_*.py"`（或显式列文件）；给 `validate_registry` 与 `check_registration_timeouts` 各立一门；`waivable` 翻转须进 `E2` 式对账。**related** **`M8-F-001`**、`V4-N-16`、`E2`、`C-18`

### V9-N-15（P1·豁免面被用于**固定化规范违规**）`UT-CLI`（非豁免）把根目录产物写进 `dirty_ignore` ⇒ 门对根目录产物永久失明
- 登记：`dirty_ignore_prefixes: ["astrocs_run_","run/"]` + `dirty_ignore_exact: [resource_samples.csv, resource_summary.json, worker_balance.csv]`（**裸根目录名**）；而根 AGENTS.md 目录规范明令 **CLI 运行产物一律落 `run/cli_runs/`**、「**禁止将运行产物产出到项目根目录**」，实测**根目录 `astrocs_run_*.json` 共 56 个（未跟踪）**。⇒ **违规被写进 ignore 名单而不是被修掉**（豁免面的第三种误用）。
- 建议：口径改 `run/cli_runs/**`；56 个根目录产物归位；`E5` 扩一条「`dirty_ignore` 里出现裸根目录文件名即红」。**related** **`V13-N-08`**（根目录散落 `p8..p11-files.patch`）、`C-15`、簇 1 机制②、`R-05`/`R-13`
