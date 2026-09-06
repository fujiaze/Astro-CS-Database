# V8-CI-012 首次 hosted Linux deep CI 基线（观察轮，SA-CI-32，mode=read）

- 派发单：`evidence/v8_1_ci_control/dispatch/V8-CI-012.json`（dispatched_utc 2026-09-06T05:55:42Z，timeout_s 21600）
- 源 SHA：`d416fc9df7767b6a407994048203667a0ef8cbaa`（= 本地 HEAD，= 远端 main，双一致）
- 观察窗口：2026-09-06T05:55Z ~ 06:30Z（控制节点 Linux amd64，仓库原地只读；API 经 `git credential fill` 内联 token，curl 全部 `--max-time 15`）
- **总体判定：DEEP_RUN_COMPLETED_WITH_FAILURE —— 首次 deep CI 端到端跑通（bootstrap→select_profile→7 项 plan 全注册→3 PASS→上传成功），但 verdict=FAIL（3P/2F/2S）；时长红线大幅富余；失败归属全部落定，无 RESOURCE_FAIL；coverage/complexity 机器基线按"仅记录数值/状态"冻结**

## 1. 三个 run 的终态与时长

| run | id | # | event | SHA | profile | 终态 | run_started_at | completed_at | job 时长 |
|---|---|---|---|---|---|---|---|---|---|
| **dispatch run（主对象）** | 34015193694 | 16 | workflow_dispatch（inputs.profile=linux-deep） | d416fc9d | **linux-deep** | completed / **failure** | 2026-09-06T05:55:51Z | 2026-09-06T06:04:33Z | **8m38s**（job 101437630810：05:55:54→06:04:32 = 518s） |
| push run（同 SHA，随派发 chore 触发） | 34015190353 | 15 | push | d416fc9d | linux-main | completed / **failure** | 2026-09-06T05:55:46Z | 2026-09-06T06:25:04Z | 14m53s（job 101439198734：06:10:10→06:25:03；含排队） |
| 参照 push run（V8-CI-010 终态 SHA） | 34015182811 | 14 | push | ff4d2066 | linux-main | completed / **failure** | 2026-09-06T05:55:36Z | 2026-09-06T06:10:09Z | 14m29s（job 101437603644：05:55:39→06:10:08） |

注：dispatch run 与 push run 首次并发排队（concurrency group 按 event 分离，未互踩）；push run 实际起跑 06:10:10。

## 2. deep run 核验（逐项落表）

### 2.1 select_profile
- hosted step 4「Select profile」执行 `python3 ci/select_profile.py --event "$GITHUB_EVENT_NAME" --requested "linux-deep"`（job log L151-153，`$GITHUB_EVENT_NAME=workflow_dispatch`）；
- step 5 实际命令行为 `python3 ci/run.py --profile "linux-deep"`（job log L155-156）；CI_RESULT.json `profile: "linux-deep"` ✓；
- 本地复现 `ci/select_profile.py --event workflow_dispatch --requested linux-deep` → `profile=linux-deep`，exit 0 ✓。

### 2.2 实跑 check 数 = 7 = linux-deep plan 基线 ✓
- 本地 `python3 ci/run.py --profile linux-deep --plan-only` @d416fc9d：selected_count=7（logs/plan_linux-deep_local_d416fc9d.json）；
- hosted CI_RESULT：total=7，selection.selected_by 全部 `profile:linux-deep`，id 集合与本地 plan 逐一相同 ✓。

### 2.3 逐项 verdict / 资源表（duration 取 CI_RESULT；monitor 数字取各检查 log 内嵌 resource_monitor JSON）

| # | id | verdict | exit | CI_RESULT 时长 s | monitor 时长 s | cpu_avg % | peak_rss KiB | peak_pss KiB | threads_max | samples | timed_out |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | BUILD-GCC-RELEASE | **PASS** | 0 | 122.506 | 122.442 | 190.83 | 848,876 | 801,394 | 16 | 592 | false |
| 2 | DEEP-CLANG-BUILD | **PASS** | 0 | 83.032 | 82.974 | 185.94 | 552,692 | 455,740 | 12 | 393 | false |
| 3 | DEEP-SAN-ASAN | **FAIL** | 2 | 110.147 | 110.087 | 189.55 | 721,020 | 615,078 | 14 | 518 | false |
| 4 | DEEP-SAN-TSAN | **FAIL** | 2 | 192.572 | 192.508 | 185.84 | 622,360 | 528,102 | 19 | 906 | false |
| 5 | DEEP-COV-CPP | **SKIPPED(waivable)** | null | 0.0 | — | — | — | — | — | — | false |
| 6 | DEEP-COV-PY | **SKIPPED(waivable)** | null | 0.0 | — | — | — | — | — | — | false |
| 7 | DEEP-COMPLEXITY | **PASS** | 0 | 0.339 | —（requires_monitor=false） | — | — | — | — | — | false |

- 汇总行（CI_RESULT.summary）：`verdict=FAIL total=7 pass=3 fail=2 known_fail=0 skipped_waivable=2`；job log L159 同读数，run.py exit 1。
- SKIP 原因（CI_RESULT/checks JSON 原文）：DEEP-COV-CPP「prerequisite 未满足（waivable）：依赖工具不在 PATH：llvm-profdata」；DEEP-COV-PY「依赖工具不在 PATH：pytest」。
- 资源语义：hosted runner host_probe cpu_logical=4 / effective_cpu_cores=4.0（affinity）、mem_total 16 GiB / available ≥15.0 GiB；cpu_avg 186-191%（≈2 核满载）为构建期正常形态；峰值内存 BUILD-GCC-RELEASE 829 MiB ≪ 16 GiB；**全部 timed_out=false，无 OOM/低利用率/泄漏信号 → RESOURCE_FAIL 零命中**。
- 工具链读数：DEEP-CLANG-BUILD log「The C/CXX compiler identification is Clang **18.1.3**」（与 V8-CI-010 报告该镜像批次观测 18.1.3 一致）；BUILD-GCC-RELEASE 全图构建 `[100%] Built target astrocs`，warnings 与轮 2-5 同源（未升级）。

## 3. 最慢检查

- **DEEP-SAN-TSAN：192.572s**（7 项实跑最慢；clang TSan 全图构建 + ctest 57 用例）。
- 次慢 BUILD-GCC-RELEASE 122.506s；检查窗口合计（started 05:56:00 → finished 06:04:30）= 510s，其中 4 项构建/测试占 508.3s。
- 单检查最长预算 3600s（5 项）/1800s（COV-PY）/600s（COMPLEXITY），最慢实跑仅用预算 5.4%。

## 4. sanitizer/science 失败归属（12_FAILURE_POLICY 四类；五元组：id/SHA/命令/预期/实际）

| finding | 判定 | 五元组要点 |
|---|---|---|
| **F-D1** DEEP-SAN-ASAN exit 2 | **CODE_FAIL**（构建接线域） | id=DEEP-SAN-ASAN；SHA=d416fc9df7767b6a407994048203667a0ef8cbaa；命令=`python3 tools/quality/deep_ci_driver.py qa-sanitize --build-dir run/ci/build-qa-asan --output run/ci/build-qa-asan-summary.json`；预期=`qa-sanitize` 自定义目标存在（clang ASan+UBSan 构建+ctest）；实际=clang 正常全图构建 100% 后，qa-target 步 `gmake: *** No rule to make target 'qa-sanitize'.  Stop.` exit 2。根因（源码级坐实，本地复读）：`deep_ci_driver.py cmd_qa_sanitize` ASan 分支传 `-DASTROCS_ENABLE_SANITIZERS=ON`（QA-002 编译旗标），而 CMakeLists.txt L511-563 自定义目标要求 `-DASTROCS_SANITIZE_RUNTIME=ON`——选项错配致目标从未定义。TSan 分支传 `-DASTROCS_SANITIZE_THREAD=ON`（正确）故 target 存在，交叉印证。最小复现：任意 clang host 配置 `-DASTROCS_ENABLE_SANITIZERS=ON` 后 `cmake --build --target qa-sanitize`。责任模块：tools/quality/deep_ci_driver.py（或 CMakeLists 增加别名分支）。 |
| **F-D2** DEEP-SAN-TSAN exit 2 | **CODE_FAIL**（sanitizer 域测试，非资源） | id=DEEP-SAN-TSAN；SHA 同上；命令=`... deep_ci_driver.py qa-sanitize-tsan --build-dir run/ci/build-qa-tsan ...`；预期=ctest 全量 0 失败；实际=ctest 3/57 失败 exit 8→gmake 链 Error 8/2。失败用例：**core_pipeline、io_reentrant、cpu001_selftest_avx512**（Debug+TSan）。log 全文 ThreadSanitizer 运行时报告 **0 条** → 无 data race 证据，判功能性失败而非违例失败；CPU 185.84%/RSS 608 MiB/无超时 → 排除 RESOURCE_FAIL。与 linux-main 既有 UT 失败族（UT-IO/UT-CPU-AVX512 等）的 id 不重合，同根因与否待修复轮本地复现裁决。 |
| **F-D3** DEEP-COV-CPP/PY 双 SKIPPED | **ENV_FAIL**（缺工具，waivable 语义内） | ubuntu-24.04 hosted 镜像（本批次）无 llvm-profdata（llvm-18 工具链未入 PATH）与 pytest。waivable 使 run.py 不判 FAIL（机制按合同工作），但 deep 基线目的下 coverage 数值缺位。修复路径：ci/bootstrap.py 增加 pytest/llvm-profdata 安装步，或 CI 专用 venv。 |
| **F-D4** verify_remote_run.py 缺失 | **CODE_FAIL**（工具链引用与仓库不符） | 派发单 ledger_command=`python3 ci/verify_remote_run.py --sha HEAD --workflows linux-ci --profile linux-deep`，但该脚本全仓不存在（find/glob 0 命中；ci/ 仅有 verify_actions_lock/verify_toolchain/verify_workspace_adoption）。实际执行输出如实落档 logs/api/verify_remote_run_invocation.txt（python3 exit 2, FileNotFoundError）。等效核验以手工 GitHub API 完成（本报告全部数字来源）。 |

coverage/complexity 按派发单纪律**不判成败**：仅冻结数值/状态（见 §5）。BOOTSTRAP_DIAG.json（step 6 产物）bootstrap_exit_code=0，为诊断步复跑读数，与本轮失败归属无关。

## 5. 冻结基线数值清单（机器基线，首测；后续 deep run 以此对照）

**A. 构建/测试资源基线（12_FAILURE_POLICY 资源三数字 + 附加维度）**

| id | duration s | cpu_avg % | peak_rss KiB | threads_max |
|---|---|---|---|---|
| BUILD-GCC-RELEASE | 122.442 | 190.83 | 848,876 | 16 |
| DEEP-CLANG-BUILD | 82.974 | 185.94 | 552,692 | 12 |
| DEEP-SAN-ASAN（含未成 test 步） | 110.087 | 189.55 | 721,020 | 14 |
| DEEP-SAN-TSAN（含 ctest 3F/57） | 192.508 | 185.84 | 622,360 | 19 |

**B. complexity 基线（DEEP-COMPLEXITY PASS，占位测量器 placeholder=true 正则近似，阈值未冻结——合同允许首测只记录）**

| 指标 | 数值 |
|---|---|
| files / lines | 375 / 139,881 |
| functions_approx / branch_tokens | 1,858 / 19,182 |
| max_file_cyclomatic | 1,048（lib/orchestrator/cpp/src/orchestrator.cpp） |
| top5 | orchestrator.cpp 1048 / sdet_api.cpp 510 / rejection.cpp 499 / gaia_client.c 483 / nanoflann.hpp 482 |
| excluded_total | 307 |

**C. coverage 基线状态**：DEEP-COV-CPP=**NOT_RUN**（llvm-profdata 缺失）；DEEP-COV-PY=**NOT_RUN**（pytest 缺失）——无数值可冻结，冻结的是「首测未运行 + 缺失工具名」状态；修复 F-D3 后下一 deep run 才产生数值基线。

**D. deep run 端到端基线**：CI_RESULT 检查窗口 510s；job 518s；7 项分布 3P/2F/2S(waivable)；host_probe 4 vCPU / 16 GiB。

## 6. 时长红线（< 6h runner 上限）

- job started_at 2026-09-06T05:55:54Z → completed_at 06:04:32Z，**差值 518s（8m38s）**；
- 6h=21,600s，**余量 21,082s（用时 2.4%）**；workflow `timeout-minutes: 330`（19,800s）同样大幅富余；
- 结论：linux-deep 当前形态（2 项 sanitizer 未真正执行 test 主链）远未逼近上限；**但本轮并非 deep 全链上限读数**——F-D1/F-D3 修复后 ASan 全量 ctest + 两项 coverage 实跑将显著加时，届时需重测本红线（粗估仍 <1h）。

## 7. push run 参照（71 项分布对照 V8-CI-010 轮 5）

- 34015190353（d416fc9d，linux-main）：`verdict=FAIL total=71 pass=48 fail=2(FAIL)+2(TIMEOUT) skipped_waivable=0` = **48P / 21F / 2T / 0S**，与轮 5 基线分布一致；
- **逐 id×verdict 差集 = 空**（脚本比对 CI_RESULT.json：ff4d2066 run#14、d416fc9d run#15、轮 5 run#13 三方完全一致）→ 派发单「差集非空才展开分析」条件未触发，**无回归**；
- UT-BACKEND/UT-CLI 仍为 TIMEOUT（F-R2-10 预算族），21 FAIL 集合与轮 2-5 判定一致（16 项共享族 + UT-IO/UT-CPU-AVX512）。

## 8. verify_remote_run.py 实际结果

- 如实执行：`python3 ci/verify_remote_run.py --sha d416fc9df7767b6a407994048203667a0ef8cbaa --workflows linux-ci --profile linux-deep` → `can't open file ... [Errno 2] No such file or directory`（exit 2）；脚本在 d416fc9d 工作树与全仓均不存在（不臆造输出），归 F-D4（CODE_FAIL），记录见 `logs/api/verify_remote_run_invocation.txt`。等效核验（runs/jobs/artifacts API + artifact 实体解包）已完整执行并落 trace。

## 9. 验收与凭据纪律（本轮实测）

- `git status --porcelain`：相对任务起点快照（92 行既有遗留，与 V8-CI-010 轮 5 记录一致）**唯一增量为 `?? evidence/v8_1_ci_control/tasks/V8-CI-012/`**——恰为派发 allowed_paths，无越界。
- `python3 -m unittest discover -s ci/tests -p 'test_*.py'`：**Ran 239 tests — OK**（26.1s）。
- 凭据：token 仅在 poll/get_jobs/curl 命令内经 `git credential fill` 内联读取，零落盘；每 HTTP 请求 `--max-time 15`；落盘前凭据特征断言（token 前缀四式 / Bearer 头 / password 字段模式，详见 /tmp 工具脚本）对 logs/ 全部 37 文件 **0 命中**。
- 证据实体：logs/api/（12 文件：3 run trace + 2 jobs trace + 3 artifacts trace + dispatch job 全量日志 + invocation 记录）、logs/artifact_d416fc9d/（CI_RESULT.json、BOOTSTRAP_DIAG.json、checks×7、logs×10 原文+7 log.zst）、logs/artifact_ff4d2066/ 与 logs/artifact_push_d416fc9d/（CI_RESULT.json）、3 个 artifact 原始 zip、plan_linux-deep_local_d416fc9d.json、SOURCE_MANIFEST.json（观察域：CI 链 16 源文件 @d416fc9d SHA256/字段登记，参照 V8-CI-006 语义；hosted 打包本身不含 SOURCE_MANIFEST——V8-CI-002 打包仅收 artifacts/ci/）。

## 10. 遗留风险与后续修复建议（移交前台/修复队列）

1. **F-D1（P1，阻塞 ASan 门）**：deep_ci_driver.py ASan 分支改传 `-DASTROCS_SANITIZE_RUNTIME=ON`（或 CMakeLists 为 QA-002 旗标增加同名自定义目标别名）；建议同补 driver 单测：断言 qa-sanitize 模式 cache 含 SANITIZE_RUNTIME。验收：下一 deep run DEEP-SAN-ASAN exit 0 或给出真实 sanitizer 违例报告。
2. **F-D2（P1）**：TSan Debug 下 core_pipeline/io_reentrant/cpu001_selftest_avx512 三用例失败需本地 clang+TSan 复现定位（疑似 Debug 断言/初始化序问题而非 race）；修复后 DEEP-SAN-TSAN 才具备"sanitizer 门"语义。
3. **F-D3（P2，阻塞 coverage 基线）**：bootstrap 增加 pytest + llvm-profdata（或 pinned llvm-18 工具链入 PATH）；产出后重冻结 coverage 数值基线。
4. **F-D4（P2）**：补齐 `ci/verify_remote_run.py`（sha/workflows/profile 参数面按派发单合同）或修订控制包 ledger_command 引用；使后续观察轮验收命令可执行。
5. **时长红线复测（P3）**：F-D1/D3 修复后重跑 deep，采集全链（ASan 全量 ctest + coverage×2）时长与资源，更新本基线表。
6. **观察口径备注**：本轮 deep run 实为"7 项注册全执行、2 项 waivable 跳过"的最深可达形态；DEEP-* 预期集合中 coverage 双项未产生数值，机器基线冻结存在已知缺口（F-D3 闭环后补齐）。

**观察轮判定：PASS_OBSERVED（基线冻结达成）——首次 hosted deep CI 端到端完成、7 项逐项核验落表、失败四类归属清零含糊项（2 CODE_FAIL + 1 ENV_FAIL + 1 CODE_FAIL 文档域）、时长红线 2.4% 占用、参照 run 无回归；coverage 数值基线缺口与两项 CODE_FAIL 已带五元组移交修复队列。**

---

# 修复轮 1（SA-CI-32，2026-09-06，接入域三项；mode=repair，本仓库容器原地）

对第 10 节 F-D1/D3/D4（接入域）完成修复 + 单测 + 验收；F-D2（业务域）按派发只登记不修（见 11.4）。执行明细全文：`logs/repair_round1.log`。

## 11.1 F-D1（P1，一行语义修复 + 注释 + 单测）

- 根因实读闭环：driver ASan 分支外层 configure 传 `ENABLE_SANITIZERS`，而 qa-sanitize 自定义目标仅由 `SANITIZE_RUNTIME` 生成（CMakeLists L511-513）；UBSan 共存无需独立旗标——目标内部重配置传 `ENABLE_SANITIZERS=ON` 走 QA-002 接线（L536-549，`-fsanitize=address,undefined`），与 TSan 分支同构（L542-543）。
- 修复：`tools/quality/deep_ci_driver.py` cmd_qa_sanitize ASan 分支改传 `-DASTROCS_SANITIZE_RUNTIME=ON`（+8/-1，含两层接线注释）。
- 单测（注入 run_step 捕获 argv，不真跑 cmake）：qa-sanitize cache 含 `SANITIZE_RUNTIME=ON` 且不含 `ENABLE_SANITIZERS`/`SANITIZE_THREAD`、目标步为 qa-sanitize；TSan 分支对称回归（仍 `SANITIZE_THREAD=ON`）。

## 11.2 F-D3（P2，ENV_FAIL 安装项）

- **coverage 工具需求清单（实读结论）**：DEEP-COV-CPP = `cmake`+`llvm-profdata`+`llvm-cov`（CMakeLists L516-521 find_program 硬依赖，gcov/lcov 不在依赖面）；DEEP-COV-PY = `pytest` + `pytest-cov` 插件（ci_coverage_runner.py L57-66 以 `--cov=lib/cli/tools` 运行；插件无可执行名，按 run.py L443-445 的 shutil.which 探测机制**不登记**进 prerequisite_tools，否则引入永久 SKIPPED 新缺陷）。
- `ci-linux.yml`：新增步 **Install deep coverage tools**（Select profile 之后、Run registered checks 之前），`if: profile == 'linux-deep'`；`timeout 240 apt-get update -qq` + `timeout 480 apt-get install -y -qq --no-install-recommends llvm-18 python3-pytest python3-pytest-cov`；llvm-18 无后缀二进制落点 `/usr/lib/llvm-18/bin` 写入 GITHUB_PATH（本步内 test -x 落点核验）；仅装验证工具、不拉业务依赖。版本对齐 clang-18 时代包名（hosted clang 18.1.3 同代，ubuntu-24.04 noble 默认 LLVM 18）。
- `ci/toolchain.policy.json`：linux_hosted 增补 `deep_coverage_tools` 登记（apt_packages/path_tools/path_tool_source/module_tools/consumers/observed_from，minimum floor 语义）；bootstrap check_linux 按名读键、未知键透传（--json 实测不受影响）。
- `ci/checks.json`：**零改动**（既有 prerequisite_tools 与实读需求一致；两个追加试验均撤销，理由见 logs/repair_round1.log §2）。
- 效果：DEEP-COV-* 两条 SKIPPED(waivable) 的理由（"依赖工具不在 PATH：llvm-profdata / pytest"）在下一次 hosted deep run 中应消除 → 状态转 PASS 或真实覆盖率/测试失败。

## 11.3 F-D4（P2，新建 ci/verify_remote_run.py）

- stdlib-only；CLI 合同：`--sha <40hex> --workflows <name>... [--profile <id>] [--timeout 15] [--repo owner/repo] [--offline] [--json]`；workflow 键唯一指认（id/path/文件名/主干/name 全名精确 → 词集包含唯一匹配，台账写法 `linux-ci` 唯一指认 ci-linux.yml）；run 选取 dispatch > push、同 event 取最新；核验 run 存在 + head_sha 匹配 + completed + jobs 全 success；`--profile` 经 artifact zip（内存解压）取 CI_RESULT.json 核对 profile 字段（summary 仅透传）。
- Exit code：0=PASS；1=失败 verdict（run 缺失/未完成/sha 不符/job 失败/profile 证据不一致）；2=API/环境（网络、非 2xx、仓库不可解析、artifact 下载失败）；3=用法错误（参数形态、键无法唯一解析）——与 verify_actions_lock 同风格。`--offline` 仅校验参数与输出契约。
- 凭据纪律：仅环境 GITHUB_TOKEN → Authorization 内联；零落盘（无文件写入）；token 不进输出。传输接缝 `_http_get/_http_get_bytes` 可注入。
- 单测 22 例（fixture 假 API、零真网络）：离线 5 / 在线 10 / API·用法 5 / 凭据纪律 2；CLI 冒烟：offline PASS exit 0、坏 sha 中文用法错误 exit 3。在线核验待修复项入 main 后按台账命令实测（本容器不出网）。

## 11.4 F-D2（业务域，只登记不修）

TSan 三用例（core_pipeline/io_reentrant/cpu001_selftest_avx512）归属业务域，本轮未触碰；登记维持第 10 节第 2 条原文。

## 11.5 验收实测（逐条）

| # | 验收项 | 命令 | 实测 |
|---|---|---|---|
| 1 | registry strict | `python3 ci/validate_registry.py --registry ci/checks.json --strict` | **exit 0**，checks=80，errors=[]，verdict=PASS |
| 2 | unittest 全绿（基线口径 ci/tests） | `python3 -B -m unittest discover -s ci/tests -t ci/tests` | **Ran 265 tests — OK**（26.5s；基线 239 → 265，+26 = driver 2 + verify_remote_run 22 + 安装步/policy 守卫 2） |
| 3 | plan-only 计数不变 | `ci/run.py --profile {fast,linux-main,linux-deep,windows-main} --plan-only` | **57 / 71 / 7 / 61 不变** |
| 4 | actions lock 离线 | `python3 ci/verify_actions_lock.py --offline` | **exit 0**，PASS (offline, 4 entries) |
| 5 | 改动面 | `git status --porcelain` 对照任务起点快照 | 本轮 tracked 修改恰为 **deep_ci_driver.py / ci-linux.yml / toolchain.policy.json / test_deep_profiles.py** 四文件 + untracked **ci/verify_remote_run.py / ci/tests/test_verify_remote_run.py / 本轮证据**；`ci/checks.json` 零改动；其余 M/D（dist zip 删除、INVENTORY.csv、V8-CI-010 evidence）为任务起点既有基线噪声，未触碰 |
| 6 | ci/tests stdout 纯净 | `discover 2>/dev/null \| grep -c '^{}'` | 0（JSON 报告泄漏已修复并回归） |

## 11.6 hosted deep 重跑预期（供下一观察轮对照）

- **DEEP-SAN-ASAN**：FAIL(exit 2, "No rule to make target") 应转 exit 0 真跑——qa-sanitize 目标存在后进入内层 clang Debug 全量 ctest。**可能暴露真实 sanitizer 违例**（ASan/UBSan/LSan 违例 → ctest 非零 → 检查仍 FAIL，但那是 sanitizer 门的本义输出）以及 TSan 同款 Debug 断言类失败（F-D2 同族）；时长预期显著高于基线 110s（基线因目标缺失早退），按 BUILD-GCC-RELEASE 122s + sanitizer 2-3x 量级预判，仍远低于 3500s 监控红线。
- **DEEP-COV-CPP**：SKIPPED(waivable) → 实跑（llvm-profdata/llvm-cov 由安装步补齐）；时长新增插桩全量构建 + ctest，预期 8-15 分钟级（timeout 3500s 富余）。
- **DEEP-COV-PY**：SKIPPED(waivable) → 实跑（pytest + pytest-cov 补齐）；tests 全量 pytest-cov 约 1-2 分钟级（timeout 1700s）。若 Python 测试有既有失败将如实 FAIL（coverage 门以透传退出码为准）。
- **install 步**：deep run 新增 apt 安装 30-90s（仅 linux-deep；main push 零增量）。
- 汇总预期形态：pass 上限 5（BUILD/CLANG/ASAN*/COMPLEXITY + coverage×2 视测试健康状况），TSan 维持 F-D2 已知失败；无 RESOURCE_FAIL 预期（内存峰值最高的 ASan 分支基线 721 MiB ≪ 16 GiB）。

## 11.7 遗留风险

1. verify_remote_run.py 在线路径未在真实 GitHub API 上验证（容器不出网）；artifact zip 下载在匿名模式下可能 401（已在脚本归 exit 2 并提示经 GITHUB_TOKEN），下一观察轮首跑即闭环。
2. llvm-18 包的 llvm-profdata/llvm-cov 落点基于 ubuntu-24.04 noble 包布局（/usr/lib/llvm-18/bin）；镜像批次升级（如 LLVM 19 默认）会使包名漂移——policy observed_from 已按 minimum floor 语义登记，安装步 test -x 失败会显式报错而非静默。
3. pytest-cov 版本随 apt（noble 为 4.1.0 代），`--cov` 参数面与 ci_coverage_runner.py 固定参数兼容性已按现有用法确认，但首次 deep 实跑前无法在本容器真验证 pytest-cov 行为。
4. DEEP-COV-PY 实跑将首次暴露 tests 全量在 pytest-cov 下的真实退出码——若存在环境相关既有失败（本地 tests/api 两例属工程控制缺失/无 cc 噪声），将如实 FAIL，属 coverage 门首次投产的预期信息量。
5. F-D2 未动，DEEP-SAN-TSAN 预期维持 3/57 失败（已知失败基线待业务域修复后重冻结）。
