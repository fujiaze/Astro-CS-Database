# GATE-501 验收门实测记录（命令 → 实测结果）

生成：RELEASE-05 / GATE-501。日志目录 `run/RELEASE-05/logs/`；
判据函数单一实现点 `eng/ci/monitor_evidence.py`（run.py / run_checks.py 共用）。

## 1. 门禁合理性修复的验收门

| # | 验收门 | 实测命令 | 实测结果 |
|---|---|---|---|
| 1 | L2 性能门真判红（fail-closed） | `python3 eng/ci/check_frozen_gate.py --self-test` | **14/14 通过**：正例绿（合规证据/纯采样留证）；负例红（缺失证据、坏证据、空文件、门不适用、分母未声明、阈值合同缺失） |
| 2 | RELEASE-04 违规证据回放（改前绿→改后红） | `python3 eng/ci/check_frozen_gate.py --replay --json-out run/ci/l2-frozen-gate/replay.json` | **11/11 归档证据判红**；历史 `verdict=pass` 的 **9/9 全部翻红**（各 4 条违规：avg 0.245<0.85、p50 0.039<0.90、达标占比 0.069<0.70、低利用窗 142.0s≥10s） |
| 3 | worker_balance 指标判别力 | `python3 eng/ci/check_worker_balance.py --self-test` | **12/12 通过**：合成负载 A `[25,50,75,100]` 与 B `[100,50,0,25]` 输出**不同**且非常数；恒定列/退化输入(active==runnable)/算法不符/缺失/空表/坏表头/未声明分母全部判红 |
| 4 | 归档退化派生件回放 | `python3 eng/ci/check_worker_balance.py --replay-archived` | **11/11 退化 `*_resource_timeseries.csv`（active==runnable ⇒ 恒 50.0）判红**；11 份权威时间序列按正确算法复算全部非常数（例 real16_w16 distinct=232, min 0.0, max 92.25） |
| 5 | requires_monitor/mutates_workspace 语义落地 | `python3 -B -m unittest discover -s eng/ci/tests -t eng/ci/tests -p test_gate_failclosed_selftest.py` | **8/8 通过**（真实 `run.py` 驱动）：A 缺监控证据→红；B 证据违反冻结判据→红；C 合规→绿；D 纯采样留证→绿；E 请求判定但无 `frozen_gate`→红；F `mutates_workspace` 写出登记面→红；G 写登记 outputs→绿；H 登记输出缺失→红 |
| 6 | 注册表一致性 | `python3 eng/ci/validate_registry.py --registry eng/ci/checks.json --strict` / `python3 eng/ci/check_registry_doc_sync.py` | **PASS**（78 项 / 230 执行单元 / 0 error）；**PASS**（注册项 78 == `docs/ci/01_CHECKS.md` §2 表 78，双向差集空） |
| 7 | 全门禁 fail-closed 普查 | `python3 eng/ci/failclosed_survey.py --json-out run/ci/failclosed-survey/survey.json --md-out artifacts/evidence/release-05/FAILCLOSED_SURVEY.md` | **PASS**：230 个执行单元，226 个有适用注入面且**全部判红**（A 缺失证据→`FAIL(missing_output)`；B 坏证据→`FAIL(monitor_gate_missing)`（11 个 requires_monitor 单元）；C 静默成功→`FAIL(empty_outputs)`），4 个显式无适用面（`SILENT_OK_UNITS` 2 + 无证据面 2） |
| 8 | 普查自身红绿自证 | `python3 eng/ci/failclosed_survey.py --self-test` | **3/3 通过**（含"判定函数恒绿注入必被抓"负例） |
| 9 | runner 契约自测 | `python3 eng/ci/run_checks.py --self-test` | **17/17 通过**（新增 N4 缺输出→红、N5 静默→红、N6/N7 监控证据缺失/违规→红、P3/P4 绿；S6 串并行等价） |
| 10 | PSFSW 静态门 Python 化 | `python3 eng/ci/check_psfsw_retired.py --self-test` | **3/3 通过**（正例绿 / 残留红 / 锚缺失 fail-closed rc=2） |

## 2. 档位与回归

| 档 | 命令 | 实测结果 |
|---|---|---|
| fast（最终态） | `python3 eng/ci/run_checks.py --all --profile fast --json-out run/RELEASE-05/logs/GATE-501_fast_final.json` | 108 步 **101 绿 / 7 红**（59 个注册项，并行 60.5s 档）；7 红全部**域外/并发写者**，见 §3 |
| integration | `python3 eng/ci/run_checks.py --all --profile integration` | 复跑 **5/5 全绿**（含 `RESOURCE-GATE-REAL` 20.3s、`RESOURCE-GATE-REAL-NEG` 20.3s、`CHK-FIX208-*`） |
| ctest 相关子集 | `ctest --test-dir build -R '^(mon001_recorder|mon001_gate|mon002_gate|p1_resource|resource_monitor_quality)$'` | **5/5 通过**（4.83s） |
| 并行等价性（子集） | `run_checks.py --check <14 项> --serial` vs `--jobs 8` | 33 步逐项 `(id, verdict)` **完全一致**，38.8s → 21.0s |
| 并行等价性（全档） | `--all --profile fast --serial` vs `--jobs 8` | 108 步 **107 一致**；唯一差异 `CTEST-REGISTRATION` 经复跑两条道各 2 轮均为 FAIL ⇒ 并发写者所致，非并行性差异 |

`RESOURCE-GATE-REAL` 在 integration 档的监控证据（我的 fail-closed 判定未误伤真门）：
`frozen_gate.verdict=pass`、`violations=0`、`recorded=[allocated_capacity_undeclared ...]`
（分母未声明 ⇒ 利用率判据不成立，按契约记 recorded，不判红）。

## 3. fast 档 7 项红灯归因（全部非本任务改动面）

| 红灯 | 根因（证据） | 归属 |
|---|---|---|
| `CTEST-REGISTRATION` | `C3 未注册的 add_test 目标 core_block_frame <- eng/tests/unit/CMakeLists.txt` | 并发写者（eng/tests/unit） |
| `UT-CONTRACTS` | 5 个新 schema 未登记 ownership：`pipeline_block / dual_line_file_domain / perf_gate_criteria / monitor_field_semantics / scheduler_probe_event` | CONTRACT-501（eng/contracts/schemas） |
| `CON-FULL-INTEGRATION` | 子工具 `generate_contract_report` FAIL（同上合同面） | CONTRACT-501 |
| `CON-DOC-SYMBOLS` | `docs/contracts/PERF_GATE_CONTRACT.md` 引用未登记符号 `OPEN_QUESTIONS` | CONTRACT-501/REPORT-501 |
| `ENG-CONSTRAINTS` | `spec7_required_dirs_present: 缺失目录 scheduler` | 既有（ARCH-501 面） |
| `CHK-NO-WEIGHT-MODE` | 命中 `docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json:179` 的 `_retirement_note` | CONTRACT-501 |
| `CHK-AIO-IO-BOUNDARY` | `eng/tests/unit/p1001_real_nodes_test.cpp::test_psf_nonfinite_frame_fail_closed` 未登记 | 并发写者（SCI-506 面） |

**本任务改动引入的新红：0**（改动前后同为这 3 项既有/并发红：ENG-CONSTRAINTS、
CHK-NO-WEIGHT-MODE、CHK-AIO-IO-BOUNDARY；另 4 项在我两次 fast 运行之间由他任务提交落地）。

## 4. 未完成 / 未验证（如实登记）

1. **`utilization_pct` 生产侧算法未修**（域外 `lib/`）：`lib/infrastructure/cli/commands.cpp`
   以 `recorder.set_workers(budget, budget)` 调用、`lib/infrastructure/cli/resource_recorder.h`
   以 `active*100/(active+runnable)` 计算 ⇒ 恒 50.0。本任务在 CI 面建立**判别力**
   （退化/算法不符判红）并登记根因，**未改生产代码**（任务书禁止 `lib/`）。
2. **生产侧 @`record_and_justify` 未翻转**：`eng/tools/monitoring/run_monitored.py` 与
   `eng/contracts/resource_gate_v1.json` 仍写 `record_and_justify`（CONTRACT-501 域）。
   CI 裁决面（本任务）不再采信该字段，D-10 恒真门已堵。
3. **`--self-test` 覆盖面**：78 个注册项中 **22 项**有可执行红绿自测（本任务新增 6 项）；
   其余 56 项**无自测**（既有缺口，本任务以"全门禁三注入普查"覆盖其**证据面** fail-closed 维度，
   未覆盖各自的**内容判据**面）。清单见 `run/RELEASE-05/tmp/selftest_coverage.py` 输出。
4. **`UT-BACKEND`（linux-main 档）红**：`test_isa_variants.py::test_05` 编译
   `backend_loader.cpp` 失败（缺 `-Ilib/infrastructure/aio/src`）—— 改动前同样失败，
   非本任务引入；派单 eng/tests + aio。
5. **`CHK-SCI-REF` 在 `--check@@ 单跑时 `FAIL(missing_output)`**：该 check 的
   `TRACEABILITY` step 只在 linux-main/windows-main 档跑，fast 单跑不产出
   `reports/v19r2/evidence/quality/traceability_check.json` ⇒ 既有档位作用域问题
   （`dirty.violations=[]` 证明与本任务 `mutates_workspace` 改动无关），登记待修。
6. **integration 档一次瞬时红未复现**：首跑 `CHK-FIX208-DISK-GATE` 红
   （`test_03`：`resource_gate_mode` 为 None）；单跑、与 `RESOURCE-GATE-REAL-NEG`
   并发、与 `CHK-FIX208-EVENT-STREAM-DEFAULT` 并发、以及整档复跑**均绿** ⇒ 判定为并发写者
   环境下的瞬时现象，**未定位到确定性根因**（如实登记，不冒充已修）。
7. **`eng/ci/INVENTORY_REPORT.md` 陈旧**（称 70 项检查/0 error，实为 78 项）；域外（报告文本），登记。
