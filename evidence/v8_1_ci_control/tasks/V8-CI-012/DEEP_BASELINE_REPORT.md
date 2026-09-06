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

---

# V8-CI-012 定向重验收口轮（观察轮 2，SA-CI-32，mode=read）

- 观察对象：workflow_dispatch run **34018610663**（run #18，ci-linux.yml，inputs.profile=linux-deep，head_sha `2ca7361e37fa51c9586214deb23a8bf4a9eb93ca` = 修复轮 1 = 本地 HEAD = 远端 main，双一致）
- 观察窗口：2026-09-06T07:14Z ~ 07:55Z（控制节点 Linux amd64，仓库原地只读；API 经 `git credential fill` 内联 token，curl 每请求 `--max-time 15`；trace 落盘前逐文件正则断言无凭据）
- **总体判定：REPAIR_ROUND1_VERIFIED_WITH_NEW_CODE_FAIL —— F-D1/F-D3 修复经 hosted 实跑闭环验证（ASan 旗标生效真跑全量 ctest、coverage 工具安装步成功且双检查从 SKIPPED 变实跑）；F-D4 实战暴露 302 凭据转发缺陷（新 F5）；发现新 F6（ci_coverage_runner.py 路径缺陷致 COV-PY 秒败）；时长红线富余不变；push run 71 项回归差集为空；无 RESOURCE_FAIL**

## R2.1 run 终态与时长红线

| 项 | 值 |
|---|---|
| 终态 | completed / **failure**（4F：ASAN/TSAN/COV-CPP/COV-PY） |
| run 窗口 | run_started_at 2026-09-06T07:14:28Z → updated 07:26:51Z = **12m23s** |
| job 101446926625 | 07:14:31Z → 07:26:50Z = **739s = 12m19s** |
| 6h 红线占用 | 739s / 21600s = **3.42%**，余量 20861s（最慢检查 DEEP-SAN-ASAN 221.983s） |
| 步骤 | checkout/bootstrap/select_profile/**Install deep coverage tools=success（F-D3 首跑即过）**/checks(failure)/diag/upload 全链就位 |
| 同 SHA push run | 34018605310（#17）：07:14:21→07:29:07（14m46s），71 项 48P/21F/2T/0S，与 d416fc9d 基线逐 id verdict 差集**空** → 无回归 |

## R2.2 deep 7 项对照表（基线轮 d416fc9d vs 轮 2 2ca7361e）

| # | id | 基线轮 verdict / exit / 时长 | 轮 2 verdict / exit / 时长 | 判读 |
|---|---|---|---|---|
| 1 | BUILD-GCC-RELEASE | PASS / 0 / 122.506s | **PASS** / 0 / 119.404s（gcc 13.3.0） | 回归 ✓（峰值 RSS 849,384 KiB 与基线 848,876 同量级） |
| 2 | DEEP-CLANG-BUILD | PASS / 0 / 83.032s | **PASS** / 0 / 91.205s（clang 18.1.3） | 回归 ✓ |
| 3 | DEEP-SAN-ASAN | FAIL / 2 / 110.147s（CMake 错配早退，0 ctest） | **FAIL** / 2 / 221.983s（**真跑全量**：55/57 通过） | **F-D1 修复验证成功**（旗标生效）；残留 2 失败见 R2.3 |
| 4 | DEEP-SAN-TSAN | FAIL / 2 / 192.572s（3/57） | **FAIL** / 2 / 200.930s（54/57，3 失败：core_pipeline / io_reentrant / cpu001_selftest_avx512） | **F-D2 维持已知失败**，逐 id 与基线一致；无 sanitizer 报告 |
| 5 | DEEP-COV-CPP | SKIPPED(waivable) / llvm-profdata 缺失 | **FAIL** / 2 / 81.589s（install 生效；插桩构建成功；ccov 实跑 ctest 55/57 后 ccov 目标 Error 8） | 前进：工具链生效；失败=C++ 测试既有失败阻断报告步（见 R2.4） |
| 6 | DEEP-COV-PY | SKIPPED(waivable) / pytest 缺失 | **FAIL** / 1 / 0.253s（pytest/pytest-cov 就位后**新缺陷暴露**：`relative_to` ValueError 秒败） | 新 F6（见 R2.4） |
| 7 | DEEP-COMPLEXITY | PASS / 0 / 0.339s | **PASS** / 0 / 0.343s（375 files / 139,881 lines / functions≈1858 / max_cyclo 1048） | 回归 ✓ |
| 汇总 | | FAIL 3P/2F/2S(waivable) | **FAIL 3P/4F/0S**（fail_detail FAIL=4） | SKIPPED 清零 = coverage 门首次投产 |

## R2.3 ASan 真跑结果逐条（F-D1 修复后）

- 旗标验证：cmake-configure 行带 `-DASTROCS_SANITIZE_RUNTIME=ON`（CMakeLists qa-sanitize 生成条件对齐），`qa-sanitize` 目标成功执行到 ctest 全量；对照基线轮 `No rule to make target 'qa-sanitize'` 早退（110s 内 0 测试）→ **修复前 0 ctest vs 修复后 57 ctest，违例面首次真实暴露**。
- ctest 终局：`96% tests passed, 2 tests failed out of 57`，FAILED：`5 - core_pipeline`、`35 - cpu001_selftest_avx512`（`--output-on-failure` 下无 AddressSanitizer/UBSan/LeakSanitizer 运行时签名；`ASAN_OPTIONS=detect_leaks=1:abort_on_error=1` 生效，无 abort/exit 化违例）→ **无 sanitizer 违例报告，0/2 属功能性失败**。
- 资源：cpu_avg 185.41%、peak RSS 749,764 KiB、peak PSS 626,824 KiB、threads_max 19、timed_out=false、1045 采样 —— 无 OOM/泄漏信号。
- **归属（12_FAILURE_POLICY）**：
  - `core_pipeline`（57 项中 #5）→ **CODE_FAIL**：ASan Debug 全量下功能性失败，与 TSan/COV-CPP 分支同 id 交叉复现（非 sanitizer 环境特异），finding 五元组＝{scope: ASan 真跑全量 ctest；observed: core_pipeline Failed；expected: PASS；evidence: run 34018610663 log DEEP-SAN-ASAN.log；disposition: 修复队列}；
  - `cpu001_selftest_avx512`（#35）→ **CODE_FAIL**：hosted runner CPU 特征相关自检失败（AVX-512 探测在 ubuntu-24.04 hosted CPU 上环境依赖），三分支（ASan/TSan/COV-CPP）同 id 复现 → 同入修复队列；
  - LSan 零报告（detect_leaks=1 无泄漏输出）；Ubsan 零 `runtime error:`；**RESOURCE_FAIL 零命中**。

## R2.4 coverage 首批实跑结果

- **install 步生效确认**：hosted step 5「Install deep coverage tools」success（apt llvm-18 / python3-pytest / python3-pytest-cov，`/usr/lib/llvm-18/bin` 入 PATH）；双检查从 SKIPPED(waivable) 变实跑 → F-D3 修复验证成功。
- **DEEP-COV-CPP**：llvm-profdata/llvm-cov 就位（ccov 目标 CMake find_program 过），GNU 13.3.0 `-fprofile-instr-generate -fcoverage-mapping` 插桩构建 81.5s 完成（cpu_avg 182.88%、peak RSS 858,676 KiB），ccov 目标 ctest 实跑 55/57（同 core_pipeline / cpu001_selftest_avx512 两失败）→ `Error 8` → `CMakeFiles/ccov.dir/all Error 2` → driver exit 2；**qa_coverage_report.sh（llvm-profdata merge + llvm-cov report/export → coverage.json）未到达 → 行/分支覆盖率数值本轮未产出**；失败归属＝C++ 测试既有失败阻断（CODE_FAIL 传导），非 coverage 工具链缺陷。
- **DEEP-COV-PY**：pytest/pytest-cov 就位后 0.253s 即败——`ci_coverage_runner.py:62` 对非仓库子路径输出目录调 `Path.relative_to(REPO)` 抛 `ValueError: 'run/ci/coverage-py' is not in the subpath of '/home/runner/work/.../Astro-CS-Database'`（hosted 以相对路径 `run/ci/coverage-py` 调用）→ **新 F6**（CODE_FAIL，属首轮实跑暴露的脚本缺陷；本地复现同构成立）。
- **coverage 数值基线状态**：C++/Python 双项数值均**未产出**（COV-CPP 被 ctest 失败阻断在报告步之前；COV-PY 被 F6 阻断在 pytest 启动之前）→ 冻结基线表数值列维持 F-D3 状态「pending」，附本轮阻断证据；后续清障依赖 core_pipeline/cpu001_selftest_avx512 归零与 F6 修复。

## R2.5 verify_remote_run.py 实战（F-D4 首跑）

- 命令：`GITHUB_TOKEN=<credential fill 内联> python3 ci/verify_remote_run.py --sha 2ca7361e37fa51c9586214deb23a8bf4a9eb93ca --workflows linux-ci --profile linux-deep` → **exit 2**，输出 `API_UNAVAILABLE: artifact 下载非 2xx（status=401，artifact=linux-ci-2ca7361e37fa51c9586214deb23a8bf4a9eb93ca…）`（如实落档 `logs/round2/verify_remote_run_invocation_round2.txt` 与 `.json`；token 零落盘）。
- 结构级验证全部通过：workflow 键 "linux-ci" 词集唯一指认 ci-linux.yml ✓；run 选取 dispatch>push 命中 34018610663 ✓；head_sha 匹配 ✓；status=completed ✓；jobs conclusion 核验正确读出 failure ✓；artifacts 列表与 CI_RESULT.zip 内 profile=linux-deep 一致性本体正确（手动同路径 curl 带 token 下载同 artifact 200，解包 CI_RESULT.profile=linux-deep ✓）。
- **根因实证（新 F5）**：artifact zip 端点 302 → `productionresultssa18.blob.core.windows.net`；脚本 `_http_get_bytes` 经 urllib 默认重定向跟随，**Authorization 头跨主机转发至 Azure 存储端点 → 401**。三连在线实验：①第一跳 302（Location host=blob.core.windows.net）；②默认跟随（脚本同路径）=401；③第二跳剥离 Authorization = 200（235,079 bytes）。→ 修复方向（后续 repair 轮）：二跳手动跟随或 302 时剥离凭据头；单测 fixture 补 302 用例。
- 脚本其余行为（含 exit 2 归类、报错文案）符合设计契约；缺陷仅限重定向凭据转发，未发现其他偏差。

## R2.6 资源红线复核（6 项监控检查）

| id | exit | dur s | cpu_avg % | peak RSS KiB | peak PSS KiB | thr max | timed_out |
|---|---|---|---|---|---|---|---|
| BUILD-GCC-RELEASE | 0 | 119.3 | 211.41 | 849,384 | 801,702 | 13 | false |
| DEEP-CLANG-BUILD | 0 | 91.1 | 177.05 | 515,356 | 419,700 | 14 | false |
| DEEP-SAN-ASAN | 2 | 221.9 | 185.41 | 749,764 | 626,824 | 19 | false |
| DEEP-SAN-TSAN | 2 | 200.9 | 186.95 | 564,584 | 528,242 | 19 | false |
| DEEP-COV-CPP | 2 | 81.5 | 182.88 | 858,676 | 813,131 | 16 | false |
| DEEP-COV-PY | 1 | 0.2 | — | 3,212 | 731 | 1 | false |

hosted host_probe 与基线同形（4 核 affinity / 16 GiB / available ≥15 GiB）；全部 timed_out=false；峰值内存 DEEP-COV-CPP 858,676 KiB（838 MiB）取代 BUILD-GCC-RELEASE 成为最耗内存检查 ≪ 16 GiB；**最慢检查 = DEEP-SAN-ASAN 221.983s**；RESOURCE_FAIL 零命中。

## R2.7 本轮 FINDINGS 增量

| id | severity | 归属 | 内容 | 状态 |
|---|---|---|---|---|
| **F5** | HIGH | CODE_FAIL（ci/verify_remote_run.py） | artifact zip 302 重定向跨主机转发 Authorization → Azure 端点 401（三连实验实证，R2.5）；单测 fixture 无 302 用例 | 修复队列（F-D4 收口残留） |
| **F6** | HIGH | CODE_FAIL（tools/quality/ci_coverage_runner.py:62） | 非仓库子路径输出目录 `relative_to(REPO)` ValueError 秒败（hosted 以相对路径 run/ci/coverage-py 调用；pytest/pytest-cov 就位后首轮实跑暴露） | 修复队列 |
| F7 | INFO | ENV/时序 | 首查 run artifacts API total_count=0（上传步已 success），数分钟后重查=1 —— artifact 索引最终一致延迟；对 verify_remote_run 的 artifact 证据步骤构成时序风险 | 记录（不修） |
| — | — | — | F-D1（ASan 旗标）→ **已验证修复生效**；F-D2 维持（TSan 3/57 与基线逐 id 一致）；F-D3 → **已验证修复生效**（install 步 success，SKIPPED 清零）；F-D4 → 结构验证通过但实战暴露 F5 | — |

## R2.8 证据清单（本轮增量）

- `logs/round2/api/`：runs_head2ca7361e_page1.json、run_34018610663.json、jobs_34018610663.json（含 inprogress 快照）、artifacts_34018610663.json、joblog_101446926625.log、artifacts_push_34018605310.json、jobs_push_34018605310.json（全部经脱敏断言 CLEAN）
- `logs/round2/linux_ci_2ca7361e.zip`（dispatch artifact 原始包）+ `artifact_2ca7361e/`（解包：CI_RESULT + 7 checks + 6 检查 log 含 .zst 全量）
- `logs/round2/linux_ci_push_2ca7361e.zip` + `artifact_push_2ca7361e_CI_RESULT.json`（回归对照）
- `logs/round2/verify_remote_run_invocation_round2.txt/.json`（F-D4 实战 exit 2 原始输出）
- artifact 解包路径差异记录：轮 2 包内根为 `2ca7361e37fa/20260906T071449Z-924e6fe9/`（基线轮为扁平结构），CI_RESULT.run_id=20260906T071449Z-924e6fe9。

## R3. 修复轮 2（F5/F6/COV-CPP；SA-CI-32；base=2ca7361e 原地修复，零 git 写操作）

### R3.1 三处修复 diff 摘要

| # | 目标 | 根因（精化） | 修复 | 单测 |
|---|---|---|---|---|
| F5 | ci/verify_remote_run.py | artifact zip 端点 302 → `*.blob.core.windows.net` 跨主机；urllib 默认 HTTPRedirectHandler 自动跟随并原样转发 Authorization → Azure 端点 401（轮 2 R2.5 三连实验实证） | `_http_get_bytes` 自实现重定向循环：301/302/307 手动跟随；同主机保留全部请求头；跨主机（netloc 比对）剥离 Authorization 再跟随；上限 5 跳，耗尽按最后 3xx 返回（调用方按非 2xx 归 exit 2）；Location 相对路径 urljoin；`_http_get` 维持 urllib 默认（api.github.com JSON 端点无跨主机重定向需求）；凭据纪律不变：token 仅经 `_build_headers()` 内存进 headers dict，零落盘 | `test_verify_remote_run.py` 新增 `_FakeRedirectOpener` 可注入 transport + 4 用例：跨主机 302 二跳无 Authorization（首跳有）/同主机 302 保留/5 跳上限（6 请求后按 3xx → exit 2）/无 token 全程无头 |
| F6 | tools/quality/ci_coverage_runner.py | L50 校验步把 `relative_to` 的返回值（仓库相对路径）赋回 `out_dir`，而 L62/63（--cov-report 组装）与 L94/95（outputs 登记）再对其调 `relative_to(REPO)`（绝对）→ 必然 ValueError；任何相对路径调用都触发（hosted 报错 self=`run/ci/coverage-py` + traceback L62 完全吻合），非"仓库外路径" | 校验步不再回写，`out_dir` 全程绝对；仓库外仍受控 exit 2（契约不变：outputs 登记 + mutates_workspace=false 依赖，不放松）；新增 `_as_repo_rel`：仓库内子路径 → 仓库相对 posix（报告落点与 checks.json outputs 串一致）、非子路径退化 `os.path.relpath`、仍失败给绝对 posix——永不抛 ValueError；不吞异常 | `test_deep_profiles.py` +3：hosted 形态进程内断言（argv 含 `--cov-report=xml:run/ci/coverage-py/coverage.xml`、cwd、COVERAGE_FILE）/端到端 hosted argv 形态（无 pytest exit 1、有 pytest 4/5，均非 2 且无 ValueError/Traceback）/`../` 逃逸仍 exit 2 且零副作用 |
| COV-CPP | tools/quality/deep_ci_driver.py | ccov 自定义目标第 3 步 ctest 失败（既有 core_pipeline/cpu001_selftest_avx512 → Error 8）中断第 4 步 qa_coverage_report.sh（profdata merge + llvm-cov report/export → coverage.json）；driver 层 `cmd_coverage_cpp` 在 ccov-target 非零时早退 → 连 profraw 归集都跳过 → 覆盖率数值永不产出 | `cmd_coverage_cpp` 增第 4 步 `coverage-merge-report`（直接调 cmake/qa_coverage_report.sh，同参 `<build_dir> <source_dir>`，timeout 300）；`_run_steps` 增 `stop_on_failure` 参数（默认 True 保持其他子命令原语义；coverage-cpp 传 False）——失败后继续，rc 恒记首个失败步骤退出码（verdict 仍 FAIL，12_FAILURE_POLICY 归属不变）；归集后缀扩 `.profraw`；输出 JSON 增 verdict 字段 | `TestCoverageCppDriver` 3 用例：ccov-target=8 → merge/report 照跑 + .profraw/.profdata/coverage.json 归集 + rc=8/FAIL；全过 rc=0/PASS（正向回归）；merge 自身失败 rc=1/FAIL（报告不可得如实上报） |

### R3.2 验收实测（逐条）

| 项 | 结果 |
|---|---|
| validate_registry --strict | exit 0；checks=80，errors=[]，verdict=PASS |
| unittest（ci/tests discover） | **Ran 275 tests — OK**（26.3s）；265 基线 → 275（+10 = F5 4 + F6 3 + COV-CPP 3），0 失败/错误/跳过 |
| plan-only 计数 | fast=57 / linux-main=71 / linux-deep=7 / windows-main=61，全部不变，exit 0 |
| verify_remote_run --offline | exit 0；`PASS (mode=offline, workflows=1)` |
| git diff --stat | 本轮 tracked 修改恰为 ci/verify_remote_run.py（+62/-x）、tools/quality/ci_coverage_runner.py、tools/quality/deep_ci_driver.py、ci/tests/test_deep_profiles.py、ci/tests/test_verify_remote_run.py 五文件 + 本轮证据（DEEP_BASELINE_REPORT.md/TASK_RESULT.json/CHANGED_FILES.txt/logs/repair_round2.log）；ci/checks.json 与 .github/workflows 零改动；其余 M/D（dist zip 删除、PRODUCTION_EXECUTION_INVENTORY.csv、V8-CI-010 evidence）为任务起点快照既有基线噪声，本轮零触碰 |
| 临时产物 | run/ci/coverage-py、run/ci/test-f6-*、run/ci/test-covcpp-*、outside-cov-escape（首轮失败用例遗留）均已清理并复验零副作用 |

执行日志：`logs/repair_round2.log`（修复前基线/三处修复/验收/diff 范围全记录）。

### R3.3 hosted 第三轮预期（供下一观察轮对照）

| id | 预期 | 数值产出路径 |
|---|---|---|
| DEEP-COV-CPP | 仍 **FAIL**（exit=ccov 首个失败步骤码，非 0——两既有业务失败未修，FAIL 判定正确）但不再是"报告步被阻断"：ctest Error 8 后 driver 继续 `coverage-merge-report`（qa_coverage_report.sh：llvm-profdata merge → llvm-cov report/export → coverage.json），产物归集 `run/ci/coverage-cpp/`（coverage.json + astrocs.profdata + *.profraw）随 CI_RESULT.zip 上传；覆盖率行/分支数值**首次产出**（对 lib+cli，失败测试未覆盖部分如实缺省） | `run/ci/coverage-cpp/coverage.json` |
| DEEP-COV-PY | F6 修复后 pytest 实跑（不再 0.25s 秒败）：退出码=pytest 透传（tests 全量在 pytest-cov 下若全过则 PASS 且产出 coverage.xml/json；若存在既有失败如实 FAIL）；报告落 `run/ci/coverage-py/`（hosted 相对路径调用已验证可达报告步） | `run/ci/coverage-py/coverage.xml`、`coverage-summary.json` |
| verify_remote_run | 应 **exit 1** 而非 2：F5 修复后 artifact zip 经 302 跨主机剥离 Authorization 下载成功 → 结构级核验全部走通 → run/jobs conclusion=failure 如实读出 → 归类为"有失败 verdict"（jobs 仍 FAIL 属正确判定，exit 1 = 脚本工作正常）；exit 2 仅应出现在网络/下载/非 2xx 等环境问题 | — |
| 其余 5 项 | 无预期变化（BUILD/CLANG/SAN 两项/COMPLEXITY 均为业务域或已稳定路径） | — |

### R3.4 遗留风险

1. COV-CPP 数值首产的对照面：coverage.json 由 qa_coverage_report.sh 对 `tests/unit/*_test` 全体可执行 merge 产出，失败测试（core_pipeline/cpu001_selftest_avx512）覆盖部分缺省——数值口径受既有失败集影响，基线数值须与失败清单一同解读。
2. `.profraw` 全量归集使 artifact 体积上升（hosted 57 测试 + %p-%m 命名）；若超 artifact 2 GiB 红线需再收口（当前预估远低于红线，首轮实跑核实）。
3. verify_remote_run exit 1 的 CI_RESULT 比对仍以人工/单测覆盖为主（302 真实跨主机路径无本地真网验证；fixture transport 已按轮 2 三连实验的线上行为建模）。
4. F6 修复未放松"仓库外输出目录 exit 2"契约；若后续需要仓库外报告落点，属控制面契约变更，须显式决策。
5. TSan 3 失败与 COV-CPP/ASan 的 2 失败（core_pipeline/cpu001_selftest_avx512）维持业务域登记，不在本轮修复面。

---

# 观察轮 3（终验轮，SA-CI-32，mode=read-only）

- 源 SHA：`485b8b86d7130f067e85b90aa04da36407983801`（修复轮 2 提交，= 本地 HEAD = 远端 main）
- 观察窗口：2026-09-06T08:15Z 起跑 ~ 08:45Z 完成核验；API 同轮 1/2 纪律（`git credential fill` 内联 token、curl `--max-time 15`、轮询 60s 间隔上限 120 分钟，实际第 12 次 iter 命中 completed）
- **总体判定：V8-CI-012_R3_COVERAGE_BASELINE_STILL_PENDING —— deep 端到端 7/7 实跑（COV-CPP/COV-PY 从 SKIPPED(waivable) 转 FAIL，prerequisite 全过）；修复轮 2 三处修复全部 hosted 生效（step5 安装步 success、stop_on_failure=False 报告步照跑、F6 pytest 真跑、verify_remote_run 实战 exit 1）；但 C++ coverage 数值与 Python coverage 数值仍双双未产出（profraw 生成链断裂 + pytest 收集期 exit 2），verdict FAIL（3P/4F/0S）如实；机器基线其余口径（资源/时长/complexity/回归差集）全部冻结**

## R4.1 run 终态与时长

| 项 | 值 |
|---|---|
| dispatch run（主对象） | 34021360340，run #20，workflow_dispatch，inputs.profile=linux-deep，head_sha=485b8b86d713 ✓（预期一致） |
| 终态 | completed / **failure**（08:15:44Z → 08:28:05Z，run 窗口 **741s = 12m21s**） |
| job | 101454437511（linux），08:15:48Z → 08:28:05Z = **737s = 12m17s**，GitHub Actions hosted |
| 步骤链 | 1 Set up ✓ → 2 Checkout exact SHA ✓（joblog L99 拉取 485b8b86）→ 3 Bootstrap ✓（L150 `bootstrap: PASS (6 items)`）→ 4 Select profile ✓（L153 `select_profile.py --requested "linux-deep"`）→ **5 Install deep coverage tools ✓（L252 `deep coverage tools ready: pytest=/usr/bin/pytest, llvm-18 bin=/usr/lib/llvm-18/bin`；llvm-18 + python3-pytest 7.4.4 + pytest-cov 4.1.0 落装）** → 6 Run registered checks **failure**（L254 `python3 ci/run.py --profile "linux-deep"`；verdict=FAIL 3P/4F/0S 透传）→ 7 Collect diagnostics ✓ → 8 Upload evidence ✓ |
| artifact | 9985775765 `linux-ci-485b8b86d7130f067e85b90aa04da36407983801`，237,669 B，21 members（CI_RESULT + 7 checks + 12 logs + BOOTSTRAP_DIAG；**无 coverage 目录**，见 R4.3/R4.4） |
| summary | `verdict=FAIL total=7 pass=3 fail=4 known_fail=0 skipped_waivable=0`（对照轮 1：3P/2F/2S → 3P/4F/0S，COV 双项按预期转 FAIL） |

同 SHA 同窗并发 run（head_sha 过滤 page-1 全量）：push #17 = 34021356429（**ci-windows.yml** push，08:19:23 completed/failure，artifact windows-ci-* 160,651 B）；push **#19 = 34021356525（ci-linux.yml push，回归对照主对象）** 08:15:39→08:30:10 completed/failure，artifact 223,686 B / 215 members，CI_RESULT 71 项 **48P/21F/2T/0S**；#16 = 34021526493（workflow_run 触发类 workflow，08:19:25→08:19:41 failure，16s，口径外旁支仅记录）。

## R4.2 修复轮 2 三处修复域 hosted 闭环判定

| 修复域 | hosted 表现 | 判定 |
|---|---|---|
| ci-linux.yml「Install deep coverage tools」步 | step5 success；llvm-profdata/llvm-cov（/usr/lib/llvm-18/bin）+ pytest/pytest-cov 就位（joblog L167-252）；COV 双检查 prerequisite 全过（`prerequisite.ok=true`，轮 1 为 `llvm-profdata 不在 PATH`） | ✓ 生效 |
| COV-CPP `stop_on_failure=False` | ctest 55/57（Error 8 → gmake Error 2）后 **`coverage-merge-report` 步照跑**（log 明确出现 qa_coverage_report.sh 的报错与 driver 收尾行），driver rc 恒记首个失败步 = 2，`copied_outputs: [], verdict: "FAIL"`（修复轮 2 新增 verdict 字段） | ✓ 生效（机制层） |
| F6 `ci_coverage_runner.py` 路径契约 | 不再 0.253s ValueError 秒败：pytest 真跑 **2.842s**（monitor 采样 pids=2、io_read 15MB），coverage-summary JSON 产出且 outputs 落点 `run/ci/coverage-py/coverage.xml|coverage.json` 全程 repo-relative；pytest argv 形态含 `--cov=lib --cov=cli --cov=tools --cov-branch --cov-report=xml:... --cov-report=json:...`（覆盖面契约兑现） | ✓ 生效（机制层） |
| verify_remote_run.py F5 | 实战 **exit 1**（预期）：`profile_evidence=OK(observed=linux-deep)` —— artifact zip 经 302 跨主机剥凭据下载成功、CI_RESULT.profile 双证；run/jobs conclusion=failure 如实读出 → FAIL 判定 | ✓ 生效（实战闭环） |

三处修复均达成"机制生效"；数值产出仍被下一层阻断（R4.3/R4.4）。

## R4.3 DEEP-COV-CPP：stop_on_failure=False 生效但 coverage 数值未产出（新瓶颈：profraw 生成链断裂）

实测链（log 原文为证）：
1. `cmake -S . -B run/ci/build-cov -DCMAKE_BUILD_TYPE=Debug -DASTROCS_BUILD_COVERAGE=ON`（无编译器指定）→ configure 成功；
2. `cmake --build run/ci/build-cov --target ccov` → 构建 100% 完成（编译警告同轮 1，无 error）；
3. ctest 全量 57 项：**55 passed, 2 failed out of 57**（失败 = core_pipeline、cpu001_selftest_avx512，与业务域既有清单一致）→ Error 8 → gmake Error 2；
4. **coverage-merge-report 步照跑**（stop_on_failure=False 生效）：`qa_coverage_report.sh run/ci/build-cov …` → `error: run/ci/build-cov/coverage/*.profraw: No such file or directory`；
5. driver 收尾 `copied_outputs: [], verdict: "FAIL"`，rc=2；CI_RESULT `outputs_missing: []`（`run/ci/coverage-cpp/` 与 `build-cov-summary.json` 目录型产物成立，但 **coverage.json / astrocs.profdata 未产出**，artifact 包内无 coverage 目录）。

根因定位（证据链完整）：`CMakeLists.txt` ASTROCS_BUILD_COVERAGE 分支仅 `add_compile_options(-fprofile-instr-generate -fcoverage-mapping)`，**不设 CMAKE_C/CXX_COMPILER=clang**（同文件 qa-sanitize/qa-sanitize-tsan 分支均显式 `-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`，对照成立）；COV-CPP configure 无编译器指定 → hosted 默认 `cc`，本 round log 明确 `The C compiler identification is GNU`（GNU 13.3.0）。`-fprofile-instr-generate/-fcoverage-mapping` 为 clang 专属旗标：GNU 侧未被拒绝但也不产生 clang 运行时插桩语义（本地无 GNU 工具链无法等价复现该静默形态，不影响归属——qa_coverage_report.sh 的 llvm-profdata 合并链只认 clang profile 文件，profraw 零产出与该错配自洽）。**新 finding（F8，本轮登记）：coverage 构建工具链未强制 clang → profraw 生成链断裂 → C++ 行/分支覆盖率数值无法产出。** 修复归属 CMakeLists.txt（源码/构建面），不在 SA-CI-32 观察轮权限内，登记待下轮。

## R4.4 DEEP-COV-PY：F6 生效但 pytest 收集期 exit 2 透传（数值未产出）

实测链：prerequisite ok → pytest 真跑 2.842s（非秒败）→ runner `coverage-summary.json` 产出（outputs 双路径 repo-relative 正确）→ **pytest exit_code=2 透传**（monitor JSON 与 checks JSON 双证）→ FAIL rc=2。pytest exit 2 语义 = 收集期失败（`Interrupted: N errors during collection` / `No tests ran` 类，非断言失败即 exit 1）；hosted `stderr_tail` 为空、runner 不采 pytest stdout → 收集失败的具体模块不可见（观测面缺口，同轮 2 R3.4 遗留第 3 条同族）。本地对照：容器 `/usr/bin/python3` 无 pytest 模块（`-m pytest` 即 exit 2/无收集），hosted 已显式落装 pytest 7.4.4 但收集面仍空转——F6 修复面（路径契约）与 pytest 装配面均已过，**收集期失败是 tests 侧 import/依赖问题（新 finding F9，本轮登记）**，定性需下轮在 hosted 采集 pytest stdout/stderr 后收口。coverage.xml/coverage.json 未产出 → **Python 行/分支覆盖率数值缺位**，artifact 包内无 coverage 目录与此一致。

## R4.5 deep 7 项逐项对照（轮 1 基线 → 轮 2 → 轮 3 终表）

轮 2 为本地修复轮（无 hosted run），verdict 列轮 2 以"修复面预期"记；资源/时长为轮 3 hosted 实测终值。

| # | id | verdict 轮1 | 轮2 | **轮3 终** | exit 轮1→轮3 | CI 时长 s 轮1→轮3 | monitor 时长 s | cpu_avg % | peak_rss KiB | peak_pss KiB | threads_max | samples | timed_out |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | BUILD-GCC-RELEASE | PASS | PASS(预期) | **PASS** | 0→0 | 122.506→**138.984** | 138.884 | 191.02 | 847,672 | 800,096 | 14 | 669 | false |
| 2 | DEEP-CLANG-BUILD | PASS | PASS(预期) | **PASS** | 0→0 | 83.032→**88.694** | 88.628 | 183.84 | 536,324 | 439,345 | 14 | 419 | false |
| 3 | DEEP-SAN-ASAN | FAIL | FAIL(预期) | **FAIL** | 2→2 | 110.147→**211.772** | 211.705 | 186.13 | 739,348 | 616,146 | 19 | 994 | false |
| 4 | DEEP-SAN-TSAN | FAIL | FAIL(预期) | **FAIL** | 2→2 | 192.572→**187.897** | 187.832 | 187.80 | 604,208 | 530,520 | 19 | 885 | false |
| 5 | DEEP-COV-CPP | SKIPPED(waivable) | FAIL(预期) | **FAIL** | null→2 | 0.0→**84.880** | 84.822 | 181.68 | 858,744 | 813,151 | 14 | 411 | false |
| 6 | DEEP-COV-PY | SKIPPED(waivable) | FAIL(预期) | **FAIL** | null→2 | 0.0→**2.898** | 2.842 | 96.19 | 78,268 | 62,242 | 2 | 15 | false |
| 7 | DEEP-COMPLEXITY | PASS | PASS(预期) | **PASS** | 0→0 | 0.339→**0.336** | —(no monitor) | — | — | — | — | — | false |

- ASAN/TSAN 时长上抬（110→212 / 193→188）为修复轮 1 sanitizer 接线生效后的真跑形态（轮 1 是 configure 早败短时长）；失败集逐 id 与轮 1/2 一致：ASAN = {core_pipeline, cpu001_selftest_avx512}、TSAN = {core_pipeline, io_reentrant, cpu001_selftest_avx512}，且 **0 条 sanitizer 运行时报告**（无 AddressSanitizer/ThreadSanitizer/LeakSanitizer SUMMARY 行）→ 维持业务域功能性失败定性，无回归。
- 非 coverage 5 项 verdict 与轮 2 预期**差集为空**；complexity 数值逐字段与轮 1 一致（lib 339 files / 133,486 lines / 1,790 fn_approx / 18,411 branch_tokens，全局 lines 139,881 / fn 1,858 / branch 19,182 / max_file_cyclomatic 1,048 / excluded_total 307 / placeholder=true → 全等）。
- 资源红线：全部 timed_out=false；cpu_avg 96.19-191.02%（COV-PY 单核 pytest 形态 96% 正常，其余双核满载形态 182-191%）；峰值内存 DEEP-COV-CPP 858,744 KiB ≈ 839 MiB ≪ 16 GiB；无 OOM/低利用率/泄漏信号 → **RESOURCE_FAIL 零命中**（三轮一致）。

## R4.6 冻结基线终表（V8-CI-012 机器基线收口版）

### 6.1 deep 7 项最终 verdict（hosted linux-deep，SHA 485b8b86，run 34021360340）

| id | 最终 verdict | exit | CI 时长 s | cpu_avg % | peak_rss KiB | threads_max | 定性 |
|---|---|---|---|---|---|---|---|
| BUILD-GCC-RELEASE | PASS | 0 | 138.984 | 191.02 | 847,672 | 14 | 机器基线冻结 |
| DEEP-CLANG-BUILD | PASS | 0 | 88.694 | 183.84 | 536,324 | 14 | 机器基线冻结 |
| DEEP-SAN-ASAN | FAIL | 2 | 211.772 | 186.13 | 739,348 | 19 | 业务域 2 失败（无 sanitizer 报告） |
| DEEP-SAN-TSAN | FAIL | 2 | 187.897 | 187.80 | 604,208 | 19 | 业务域 3 失败（无 sanitizer 报告） |
| DEEP-COV-CPP | FAIL | 2 | 84.880 | 181.68 | 858,744 | 14 | F8 工具链错配（数值缺位） |
| DEEP-COV-PY | FAIL | 2 | 2.898 | 96.19 | 78,268 | 2 | F9 pytest 收集失败（数值缺位） |
| DEEP-COMPLEXITY | PASS | 0 | 0.336 | — | — | — | 数值基线冻结（与轮 1 全等） |

### 6.2 coverage 双数值口径（本轮冻结状态）

| 指标 | 数值 | 口径 |
|---|---|---|
| C++ 行覆盖率 | **未产出** | profraw 零产出（F8：GNU cc 下 clang 插桩旗标不生效 → LLVM_PROFILE_FILE 无从落盘）；qa_coverage_report.sh 在 `set -eu` glob 空展开处失败；`astrocs.profdata`/`coverage.json` 均未生成。修复前置：CMake coverage 分支强制 clang 工具链；修复后仍需与失败清单一同解读（ASAN/COV-CPP 共有的 core_pipeline/cpu001_selftest_avx512 2 失败测试覆盖部分缺省） |
| C++ 分支覆盖率 | **未产出** | 同上（同源同口径） |
| Python 行覆盖率 | **未产出** | pytest 收集期 exit 2 透传（F9）；pytest-cov xml/json 报告未写；观测面需下轮采集 pytest stdout/stderr 定性收集失败模块 |
| Python 分支覆盖率 | **未产出** | 同上（argv 已含 `--cov-branch`，契约在、数值待产出） |
| 数值冻结判定 | **COVERAGE_BASELINE_PENDING** | 轮 1 待 F-D3 → 轮 3 待 F8/F9；除 coverage 外全部机器基线口径本轮冻结 |

### 6.3 时长红线

- 最慢检查：**DEEP-SAN-ASAN 211.772s**（CI_RESULT 口径；monitor 口径 211.705s）。
- job 总时长：**737s（12m17s）**；6h 上限（21,600s）占用 **3.4%**，余量 **20,863s ≈ 5h47m**（对照轮 1 job 518s/2.4%——上抬来自 coverage 工具安装步 ~12s 与 COV 双检查真跑 88s）。
- run 窗口 741s；push #19（71 项 main profile）988s；hosted 资源形态三轮一致（4 vCPU affinity / 16 GiB）。

## R4.7 verify_remote_run.py 实战（F5 闭环验证）

- 命令：`python3 ci/verify_remote_run.py --sha 485b8b86d7130f067e85b90aa04da36407983801 --workflows linux-ci --profile linux-deep`（GITHUB_TOKEN 经 `git credential fill` 内联注入环境，未落盘；输出与 exit 落盘 `logs/round3/verify_remote_run_invocation_round3.txt`）。
- 结果：**exit 1（预期达成）**。结构级核验全过：workflow linux-ci 唯一指认、head_sha 匹配、run 唯一（34021360340）、completed、`profile_evidence=OK(observed=linux-deep)`（302 跨主机剥凭据下载 artifact 成功 + CI_RESULT.profile 双证 = F5 修复实战生效）；jobs conclusion=failure 如实读出 → FAIL 判定（理由：job 失败：linux=failure）。exit 2 环境类失败零出现 → F5 无残留。

## R4.8 遗留（登记待后续轮，不在本轮修复面）

1. **F8**：CMake ASTROCS_BUILD_COVERAGE 分支未强制 clang 工具链（对照同文件 sanitizer 分支显式 clang）；hosted GNU cc 下插桩失效 → profraw 零产出。修复归属 CMakeLists.txt（构建面源码）。
2. **F9**：DEEP-COV-PY pytest 收集期 exit 2，具体失败模块因 runner 不采 pytest stdout/stderr 而不可见；需 hosted 采集 pytest 输出后定性（tests 侧依赖/import 面）。
3. F8/F9 修复后的 coverage 数值基线（C++ 行/分支、Python 行/分支）待补测冻结；数值解读仍须与业务域失败清单（core_pipeline/cpu001_selftest_avx512）一同进行。
4. coverage 产物（run/ci/coverage-*/）不在 artifact 打包清单内（本轮 artifact 21 members 无 coverage 目录）；数值产出轮需同步扩 artifact 归集面。
5. TSan 3 失败与 ASan/COV-CPP 共有 2 失败维持业务域登记；UT-BACKEND/UT-CLI TIMEOUT 维持 main-profile 既有登记。

---

# 修复轮 3（F8/F9 coverage 收敛轮）— SA-CI-32

base_sha `485b8b86` 不动；无 git 写操作；执行日志 `logs/repair_round3.log`。
改动面（净）：`tools/quality/deep_ci_driver.py`（+8/−1）、`tools/quality/ci_coverage_runner.py`（+44/−1）、`ci/tests/test_deep_profiles.py`（+96，新增单测 5）。前轮遗留的未提交工作区项（V8-CI-010 证据、zip 变更等）不属于本轮。

## R5.1 F8 修复（profraw 断产根因：coverage 构建工具链未强制 clang）

- 根因复核（与前轮 R4 定性一致）：`cmd_coverage_cpp` configure cache 只传 `-DCMAKE_BUILD_TYPE=Debug -DASTROCS_BUILD_COVERAGE=ON`，无编译器指定 → hosted 默认 GNU cc（轮 3 hosted log `The C compiler identification is GNU 13.3.0`）→ clang 专属 `-fprofile-instr-generate/-fcoverage-mapping` 无插桩语义 → profraw 零产出 → `qa_coverage_report.sh` 合并链断 → C++ 覆盖率数值缺位。
- 修复：coverage-cpp 分支 configure 显式追加 `-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`，与同文件 `cmd_qa_sanitize` sanitizer 分支（L135）及 `build-clang`（L111）同构；SANITIZE 系旗标不误入（单测交叉断言）。
- 单测：`TestCoverageCppDriver.test_coverage_cpp_configure_forces_clang_toolchain`（mock run_step 捕获 configure argv，零落盘零触网；断言 clang 双旗标 + `ASTROCS_BUILD_COVERAGE=ON` 存在、SANITIZE 系旗标缺席）。

## R5.2 F9-a 修复（pytest 失败观测面：summary["pytest_tail"]）

- 缺口复核：runner 失败时只透传 exit code + stderr 尾 8 行（hosted stderr 为空 → 收集期 exit 2 全程黑箱，失败模块不可见）。
- 修复（复用 `ci_windows_driver.collect_error_lines` 思路）：新增 `PYTEST_ERROR_LINE_RE`（ERROR/ERRORS/Interrupted/short test summary/ModuleNotFoundError/ImportError/No module named/cannot import/Traceback/`E   `/`in <module>`）+ `collect_error_lines()`：取 stdout+stderr 合流行中**首个关键词命中行起的连续 50 行**（`PYTEST_TAIL_CAP=50`；保留 import traceback 源码上下文行，如 `    import yaml`——逐行过滤会丢掉 E 行之外的定性依据）；无命中退化原始尾窗（失败时 pytest_tail 永不为空黑箱）；timeout 分支亦采集（bytes 防御性 decode）；成功（rc=0）路径 pytest_tail 恒空，不噪声化。字段落 `summary["pytest_tail"]`（stdout JSON 与落盘 `coverage-summary.json` 同步），`stderr_tail` 语义不变。
- 单测（4）：`test_coverage_runner_failure_collects_pytest_tail`（模拟收集期 exit 2：6 条错误行全入选、30 条噪声不入选）、`test_coverage_runner_pytest_tail_capped_at_50`（60 行错误风暴 → 恰 50 行、保序）、`test_coverage_runner_pytest_tail_fallback_on_unfiltered`（无关键词命中 → 原始尾窗兜底）、`test_coverage_runner_success_has_empty_pytest_tail`（rc=0 → pytest_tail=[]）。本机真实形态实跑（无 pytest）：exit 1 透传 + `pytest_tail=["/usr/bin/python3: No module named pytest"]`（日志 §5），失败分支行为与设计一致。

## R5.3 F9-b 本地定性（收集期 exit 2 复现与归因）

- 本机限制：`/usr/bin/python3` 无 pytest 模块，且无 pip/ensurepip（最小容器）→ `python3 -m pytest --collect-only -q` 无法执行（`--version` 即 `No module named pytest`；证据日志 §4）。替代定性：`run/repair_round3/f9b_collect_probe.py`（仅 stdlib）以与 pytest collection 同构的 import 语义（rootdir 插 `sys.path[0]`，tests 为包形态——`tests/__init__.py` 等 12 个 `__init__.py` 均被 git 跟踪，pytest 将按包形态 import `tests.<域>.<模块>`）对 `tests/**/test_*.py` 全量 105 个模块逐一 `importlib.import_module`；输出落 `logs/repair_round3_f9b_collect_probe.json`。
- 实测（0.2s）：**105 个模块中 98 个可导入、7 个收集失败**，失败原文：

| # | 失败模块 | import 错误原文 | 判定 |
|---|---|---|---|
| 1 | tests.backend.test_cpu_profile | `FileNotFoundError: [Errno 2] No such file or directory: '.../工程控制/RELEASE_V5/AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828/schemas/cpu_profile.schema.json'` | **CODE_FAIL**（业务域登记） |
| 2 | tests.backend.test_p2003_seam_oracle | `ModuleNotFoundError: No module named 'numpy'` | **ENV_FAIL**（依赖缺失） |
| 3 | tests.backend.test_p3005_fits_output | 同上 | ENV_FAIL |
| 4 | tests.backend.test_p3006_production_pipeline | 同上 | ENV_FAIL |
| 5 | tests.io.test_fits_stream_contract | 同上 | ENV_FAIL |
| 6 | tests.io.test_hips_input_contract | 同上 | ENV_FAIL |
| 7 | tests.io.test_hips_output_contract | 同上 | ENV_FAIL |

- 归因说明：
  1. **ENV_FAIL（6/7，主因）**：6 个测试模块（backend 3 + io 3，另有 fixture 脚本 `tests/io/hips_output_fixture.py`）顶层 `import numpy`。hosted 安装步（F-D3）按 policy 明确"仅安装验证工具，不联网拉任何业务依赖"（llvm-18 / python3-pytest / python3-pytest-cov），numpy 不在安装面 → hosted 收集期对这 6 个模块必然 ModuleNotFoundError。**裁决建议（本机不改 hosted 安装面 yml）**：DEEP-COV-PY 解除前置需在 `ci-linux.yml` deep coverage 安装步补 `python3-numpy`（ubuntu-24.04 包名）或部署任务提供等价依赖源；属 install 步/凭据补充项，`ci/checks.json` 不改（prerequisite 探测不变，waiver 解除靠安装面补齐）。
  2. **CODE_FAIL（1/7，登记不修）**：`tests/backend/test_cpu_profile.py` L8-11 在**模块顶层**（import 期，即 pytest collection 期）执行 `json.load(open(工程控制/RELEASE_V5/.../schemas/cpu_profile.schema.json))`。该 V5 控制包目录路径本机不存在（现工作区为 V6.1 布局 `工程控制/AstroCS_V6_1_REWORK_CONTROL_20260831/`），且 `git ls-files 工程控制` = 0（整目录不被 git 跟踪）→ hosted git checkout 上该路径**结构性缺失**，收集错误必然复现。两重缺陷叠加：①收集期做文件 IO（import 炸）；②硬编码过时控制包资产路径。归属业务域（tests/backend 域 finding 登记），不在本轮 CI 接入面修复范围。
  3. hosted 收集期 exit 2 与本探针形态自洽：pytest 收集期 ≥1 个 error 即 `Interrupted: N errors during collection` → exit 2；hosted 预期 N≥7（numpy 6 + schema 1）。hosted 实际 N 与逐模块原文待第四轮 pytest_tail 采集后闭环（本探针为本机等价定性，非 hosted 原文）。
- 业务失败登记不变：core_pipeline / cpu001_selftest_avx512 / core_pipeline(TSan 3 id) 维持原登记；本探针发现的 7 个收集失败为其上游同源（收集期失败 → 这些模块内测试从未被执行，coverage 数值缺位的测试面组成部分）。

## R5.4 验收实测（日志 logs/repair_round3.log）

| 项 | 结果 |
|---|---|
| `validate_registry.py --registry ci/checks.json --strict` | exit 0，80 checks，errors=0，verdict PASS |
| unittest 全量（ci/tests discover） | **Ran 280 tests — OK**（26.5s）；275 基线 → 280（+5 = F8 1 + F9-a 4），0 失败/错误/跳过 |
| plan-only 数值口径 | fast=57 / linux-main=71 / linux-deep=7 / windows-main=61（四 profile 与基线全等） |
| git diff --stat（本轮净改动） | 仅 `deep_ci_driver.py`（+8/−1）、`ci_coverage_runner.py`（+44/−1）、`test_deep_profiles.py`（+96）；证据文件本轮新增（DEEP_BASELINE_REPORT.md / TASK_RESULT.json / logs/repair_round3*）；无业务源码、无 yml、无 checks.json 改动；HEAD=485b8b86 未动，零 git 写操作 |
| 失败分支实跑（本机） | ci_coverage_runner exit 1 + pytest_tail 捕获 No module named pytest（§5）；coverage-cpp 路径 mock 单测 rc=0/8/1 三形态不回归 |

## R5.5 hosted 第四轮预期（coverage 双数值产出条件）

- **DEEP-COV-CPP（F8 修复后）**：configure 步将带 `-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++` → hosted 编译器识别为 Clang（18.x，与 `llvm-18` 安装步同代）→ `-fprofile-instr-generate/-fcoverage-mapping` 生效 → `LLVM_PROFILE_FILE` 落盘 profraw → `qa_coverage_report.sh` profdata 合并 + llvm-cov export 链打通 → `build/coverage/astrocs.profdata` 与 `coverage.json` 产出，driver 归集 `*.profraw + coverage.json + astrocs.profdata` 到 `run/ci/coverage-cpp` → **C++ 行/分支覆盖率双数值预期产出**。预期 verdict：仍 **FAIL**（ccov 内 ctest 既有业务失败 core_pipeline / cpu001_selftest_avx512 → Error 8 非零透传，verdict FAIL 但数值照常产出——轮 2 修复语义）；若 clang-18 与 profdata 版本错配或 `qa_coverage_report.sh` 另有断点则维持 FAIL 且数值缺位，输出尾窗可定位。
- **DEEP-COV-PY（F9-a/F9-b 定性后）**：runner 失败时必落 `pytest_tail` → hosted 收集失败定性从黑箱变白箱。数值产出前提（二选一，均为 hosted 侧动作，本机不改）：①安装步补 numpy（6 个 numpy 模块恢复收集）+ 处理 test_cpu_profile 收集错误（业务域修复或收集豁免）→ 105−1=104 模块全收集 → pytest-cov xml/json 报告写出 → **Python 行/分支覆盖率双数值产出**；预期 verdict 仍 **FAIL**（其余既有测试失败透传，exit 1）；②不补依赖 → 仍 exit 2，但 `pytest_tail` 将带出 `ModuleNotFoundError: No module named 'numpy'` 等逐模块原文（N≥7），F9 定性闭环、数值继续缺位。hosted 实际收集错误数 N 与本探针预测（7）的对照即第四轮 F9 验证点。
- 数值冻结判定维持 **COVERAGE_BASELINE_PENDING**：待第四轮 hosted 双检查实跑后按 R5.5 条件收口；数值解读仍与业务域失败清单（core_pipeline / cpu001_selftest_avx512）一同进行（失败测试覆盖部分缺省，数值口径注明）。

## R5.6 遗留风险（登记）

1. **test_cpu_profile 收集错误为结构性**：hosted checkout 永无 `工程控制/`（git 不跟踪）→ 只要该模块在收集根内，DEEP-COV-PY 收集期至少 1 error 持续存在，直至业务域修复（路径改 V6.1 布局或资产入库或收集豁免）。补 numpy 只能把 7 降到 1，**单靠依赖补齐不能让 pytest 收集期归零** → Python coverage 数值产出必须叠加业务域修复（第四轮起为业务域任务前置）。
2. **CI yml 安装面未改**（裁决边界）：numpy 缺项只登记不修，DEEP-COV-PY 第四轮若仍不补 `python3-numpy`，预期收集错误从 7 → 1（仅 test_cpu_profile），pytest_tail 应精确印证；此时 Python 数值仍缺位。
3. **clang 版本耦合**：coverage 构建改 clang-18 后，业务失败清单可能较 GNU 轮漂移（不同编译器行为差异）；core_pipeline / cpu001_selftest_avx512 既有失败归属判定在第四轮复核一次，防止 F8 修复引入新失败被误归业务域。
4. pytest_tail cap 50 行：若 hosted 收集错误行极多（>50 行窗）可能截断部分模块原文；已保序取首个命中行起窗，配合探针预测清单（7 模块名已知）可人工对账，风险可控。
5. 前轮遗留 4（coverage 产物不在 artifact 打包清单）不变：数值产出轮需同步扩 artifact 归集面，否则 hosted 数值仍需经 CI_RESULT JSON 间接读取。

---

# 观察轮 4（终局轮）— SA-CI-32，mode=read

观察窗口 2026-09-06T09:05Z ~ 09:35Z；仓库原地只读（HEAD=52e71804 本地=远端 main=观察对象三一致）；API 经 `git credential fill` 内联 token（仅内存，零落盘），curl 全部 `--max-time 15`（artifact 下载 300s）；执行要点 `/tmp/r4/poll_state.log`（脱敏后归档 logs/round4/api/）。

## R6.1 run 发现与终态

- **dispatch run 首查未出现**：push 后 3 分钟（09:05Z 起）head_sha=52e71804 下仅 push run #21（34023649656，event=push，09:05:19Z 起跑）。与轮 1「随派发 chore 同批触发 dispatch」形态不同（本轮 push 为主提交，无伴随 dispatch）。
- **SA 经 REST API 主动触发**（token 域内使用，非 git 写操作）：`POST /actions/workflows/ci-linux.yml/dispatches {ref:main, inputs.profile:linux-deep}` → HTTP 204；90s 后 dispatch run #22 出现（logs/round4/api/runs_head52e_page1.json）。
- dispatch run（主对象）：**34023949880** #22 workflow_dispatch / linux-deep，head_sha 52e718048312fa05c694c5a32e7dec52263bf0cc ✓ → completed / **failure**；run 窗口 09:11:36→09:23:36Z = **720s**；job 101461486015：09:11:39→09:23:35Z = **716s（11m56s）**。
- push run（回归对照）：34023649656 #21 push / linux-main，completed / **failure**；run 窗口 873s。
- 6h 红线（21,600s）：job 716s 占 **3.3%**，余量 **20,884s ≈ 5h48m**（轮 3 为 737s/3.4%——本轮 coverage 工具安装步复用、时长形态稳定）。

## R6.2 deep 7 项终表（SHA 52e71804，run 34023949880）

| # | id | verdict | exit | CI 时长 s | monitor 时长 s | cpu_avg % | peak_rss KiB | threads_max | 对照轮 3 |
|---|---|---|---|---|---|---|---|---|---|
| 1 | BUILD-GCC-RELEASE | **PASS** | 0 | 124.545 | 124.480 | 188.55 | 850,440 | 14 | 同 PASS ✓ |
| 2 | DEEP-CLANG-BUILD | **PASS** | 0 | 81.632 | 81.574 | 178.78 | 533,388 | 14 | 同 PASS ✓ |
| 3 | DEEP-SAN-ASAN | **FAIL** | 2 | 202.039 | 201.976 | 184.81 | 730,848 | 19 | 同 FAIL(2) ✓ |
| 4 | DEEP-SAN-TSAN | **FAIL** | 2 | 189.183 | 189.120 | 186.65 | 586,152 | 19 | 同 FAIL(3) ✓ |
| 5 | DEEP-COV-CPP | **FAIL** | 2 | 86.729 | 86.670 | 199.63 | 525,492 | 12 | FAIL 保持（性质变：F8 修复生效，profraw 仍零产出） |
| 6 | DEEP-COV-PY | **FAIL** | 2 | 2.895 | 2.840 | 95.88 | 77,332 | 2 | FAIL 保持（性质变：pytest_tail 白箱化生效，收集错误原文可读） |
| 7 | DEEP-COMPLEXITY | **PASS** | 0 | 0.337 | — | — | — | — | 同 PASS，数值与轮 1 全等 ✓ |

- 汇总：verdict=FAIL total=7 pass=3 fail=4 known_fail=0 skipped_waivable=0（轮 3 同为 3P/4F；两项 coverage 由 SKIPPED(waivable)→FAIL 的转变发生在轮 2，本轮维持）。
- **非 coverage 5 项 verdict/exit 差集为空**；ASan 失败原文 `5 - core_pipeline (Failed)` / `35 - cpu001_selftest_avx512 (Failed)`，`Error 8` 透传，0 条 sanitizer SUMMARY → 与轮 1-3 业务域定性一致、无漂移；TSan 3 失败（core_pipeline×2 + cpu001_selftest_avx512）同前轮全等，0 条 sanitizer 报告。
- complexity 数值逐字段与轮 1 冻结值全等（global lines 139,881 / fn 1,858 / branch 19,182 / max_file_cyclomatic 1,048 / excluded 307 / placeholder=true）。
- 资源红线：全部 timed_out=false；cpu_avg 95.88-199.63%（COV-PY 单核 96% 正常，其余双核满载形态）；峰值内存 BUILD-GCC-RELEASE 850,440 KiB ≈ 830 MiB ≪ 16 GiB；无 OOM/低利用率/泄漏信号 → **RESOURCE_FAIL 零命中（四轮一致）**。
- push run #21 回归对照：71 项 48P/21F/2T（TIMEOUT=UT-BACKEND/UT-CLI 既有登记），逐项 verdict/exit 与轮 3 push 对照 **差集为空** ✓。

## R6.3 coverage 双数值终判

### DEEP-COV-CPP（F8 修复验证 + 新断点定位）

证据链（logs/DEEP-COV-CPP.log 全量，见 logs/round4/artifact_52e71804/）：
1. **clang 工具链强制生效**：`The C/CXX compiler identification is Clang 18.1.3`；configure 参数含 `ASTROCS_BUILD_COVERAGE=ON`；无 GNU 识别行（轮 3 根因消除）✓。
2. **插桩构建全绿**：57/57 lib 目标 100% Built，0 warning（`-fprofile-instr-generate -fcoverage-mapping` 旗标在编译行可见）✓。
3. **ctest 实跑**：96% tests passed, **2 failed out of 57**（core_pipeline / cpu001_selftest_avx512 = 既有业务域清单）→ `Error 8` → **verdict FAIL 为正确语义**（业务失败透传，非 coverage 机制故障）✓。
4. **profraw 仍零产出 → 数值缺位（第 4 轮）**：`llvm-profdata merge` 报 `run/ci/build-cov/coverage/*.profraw: No such file or directory`；`copied_outputs=[]`；artifact 23 members 无 coverage 目录。
5. **新断点定性 F8-b（观察结论，不属本轮修复面）**：coverage 插桩旗标加于根 CMakeLists.txt L522-523（`if(ASTROCS_BUILD_COVERAGE)` 块），而 `add_subdirectory(tests/unit)` 在 L501 先行执行 → lib 目标带插桩编译、`tests/unit/*_test` 可执行**未经插桩链接**（无 `__llvm_profile` runtime）→ 测试进程不落 profraw。F8（工具链未强制 clang）与 F8-b（旗标作用域在 add_subdirectory(tests/unit) 之前）是两个独立缺陷；本轮只修掉了前者。
- **verdict：FAIL（预期达成）**——业务 ctest Error 8 透传语义正确；C++ 行/分支覆盖数值：**未产出**。

### DEEP-COV-PY（F9-a/F9-b 修复验证 + F9-b 探针对账）

- **pytest_tail 白箱化生效**（F9-a 实战验证）：runner summary `pytest_tail` 50 项（cap 50 生效）、`exit_code=2`、stderr_tail 空（pytest 错误走 stdout）；collect_error_lines 正则命中起窗（首行 `==== ERRORS ====`），经 resource_monitor stdout 尾窗 84 行完整归档。
- **窗内实证收集错误 6 处原文**：
  1. `tests/backend/test_cpu_profile.py:8` — FileNotFoundError: `工程控制/RELEASE_V5/.../schemas/cpu_profile.schema.json`（**CODE_FAIL，结构性**：hosted checkout 永无 `工程控制/`，与 F9-b 预测定性逐字一致）；
  2-6. `test_p2003_seam_oracle` / `test_p3005_fits_output` / `test_p3006_production_pipeline` / `test_fits_stream_contract` / `test_hips_input_contract` — 全部 `ModuleNotFoundError: No module named 'numpy'`（ENV_FAIL）。
- **F9-b 对账**：探针预测 7 失败（6 numpy + 1 cpu_profile）；窗内命中 5 numpy + 1 cpu_profile；第 6 个 numpy（test_hips_output_contract，探针序末位）与 pytest `Interrupted: N errors` 汇总行落在 cap 50 项窗外 → **方向一致（numpy 未补 → ENV_FAIL 留存），精确 N=7 待 CI 安装面补 python3-numpy 后由同窗复核**。
- 数值预期达成情况：numpy 未补（裁决边界维持：CI yml 安装面不属本轮修复面）→ Python 行/分支覆盖数值 **未产出**（pytest 收集期 exit 2，pytest-cov xml/json 未写）；argv 契约（--cov=lib,cli,tools --cov-branch --cov-report=xml/json）逐参数在位 ✓。
- **verdict：FAIL（预期达成）**；阻断清单冻结为 **numpy×6（ENV_FAIL）+ test_cpu_profile×1（CODE_FAIL，业务域）**。

### COVERAGE_BASELINE 终态（轮 4 冻结口径）

| 指标 | 数值 | 状态 |
|---|---|---|
| C++ 行覆盖率 | 未产出（第 4 轮） | **FROZEN_CPP_PENDING**：机制链已收敛至单点 F8-b（tests/unit 插桩作用域）；修复后数值可产出，解读须与业务失败清单（core_pipeline / cpu001_selftest_avx512，clang-18 下与 GNU 轮一致无漂移）同框 |
| C++ 分支覆盖率 | 未产出 | 同上同口径 |
| Python 行覆盖率 | 未产出 | **PENDING_PY**：双前置 = CI 安装面补 python3-numpy（6 项 ENV_FAIL 归零）+ 业务域 test_cpu_profile 路径修复（结构性 CODE_FAIL） |
| Python 分支覆盖率 | 未产出 | 同上（argv 契约在位） |

## R6.4 verify_remote_run.py 实战（第 2 次）

- 命令：`python3 ci/verify_remote_run.py --sha 52e718048312fa05c694c5a32e7dec52263bf0cc --workflows linux-ci --profile linux-deep`（GITHUB_TOKEN 经 `git credential fill` 内联注入并 export，未落盘）。
- 结果：**exit 1（预期达成）**：workflow 唯一指认、head_sha 匹配、run 唯一（34023949880）、completed、`profile_evidence=OK(observed=linux-deep)`（302 跨主机剥凭据 artifact 下载 + CI_RESULT.profile 双证）、jobs conclusion=failure → FAIL 判定；exit 2 环境类失败零出现。
- 首次执行因调用侧 shell 变量未 export（token 未传至子进程，非脚本缺陷）匿名 401 → exit 2，已在落档文件头如实记录并作废重跑。

## R6.5 轮 4 遗留（登记，不在本轮修复面）

1. **F8-b（新）**：根 CMakeLists.txt coverage 分支 `add_compile_options/add_link_options(-fprofile-instr-generate ...)` 位于 `add_subdirectory(tests/unit)`（L501）之后，`*_test` 可执行未插桩 → profraw 零产出。修复归属构建面（将 tests/unit 的 add_subdirectory 移入插桩作用域内，或对 tests 目标显式补旗标），修复后预期 profraw 产出 + 数值产出（verdict 仍 FAIL，属业务 Error 8 透传）。
2. **F9-c**：pytest_tail cap 50 项致窗内仅见 6/7 收集错误与无 `Interrupted: N errors` 汇总行；精确计数核验需扩窗（cap 50→120）或依赖 numpy 补齐后复核。
3. numpy 缺项（ENV_FAIL×6）与 test_cpu_profile 结构性 CODE_FAIL 维持登记：前者属 CI 安装面任务，后者属业务域任务（V6.1 布局路径 / 资产入库 / 收集豁免三选一）。
4. coverage 产物目录仍不在 artifact 打包清单（run/ci/coverage-*/）→ 数值产出轮需同步扩归集面。

## R6.6 冻结基线终表 v_final（V8-CI-012 收口版）

### 6.6.a deep 7 项最终状态（hosted linux-deep，SHA 52e71804，run 34023949880，2026-09-06）

| id | 最终状态 | 定性 |
|---|---|---|
| BUILD-GCC-RELEASE | PASS（124.5s / 188.55% / 850,440 KiB） | 机器基线冻结 |
| DEEP-CLANG-BUILD | PASS（81.6s / 178.78% / 533,388 KiB） | 机器基线冻结 |
| DEEP-SAN-ASAN | FAIL(2)：core_pipeline、cpu001_selftest_avx512 | 业务域冻结（0 sanitizer 报告，四轮一致） |
| DEEP-SAN-TSAN | FAIL(3)：core_pipeline×2、cpu001_selftest_avx512 | 业务域冻结（0 sanitizer 报告，四轮一致） |
| DEEP-COV-CPP | FAIL，数值未产出 | F8-b 单点待修（tests/unit 插桩作用域）；Error 8 透传语义正确 |
| DEEP-COV-PY | FAIL，数值未产出 | PENDING_PY：numpy×6 ENV_FAIL（CI 安装面）+ test_cpu_profile CODE_FAIL（业务域） |
| DEEP-COMPLEXITY | PASS，数值与轮 1 全等 | 数值基线冻结（lines 139,881 / fn 1,858 / branch 19,182 / max_cc 1,048） |

### 6.6.b 资源与时长红线（四轮一致口径）

- 最慢检查 DEEP-SAN-ASAN 202.039s（CI 口径）；job 716s = 6h 上限 3.3%，余量 20,884s ≈ 5h48m；run 窗口 720s。
- hosted 资源形态：4 vCPU affinity / 16 GiB（host_probe cpu_logical=4 / effective_cpu_cores=4.0 / mem_available ≥15.0 GiB）；全检查 timed_out=false，峰值 rss ≤ 830 MiB → RESOURCE_FAIL 零命中。
- push run（71 项 main profile）873s，48P/21F/2T 与轮 3 差集为空。

### 6.6.c 收口判定

**COVERAGE_BASELINE = FROZEN_CPP_PENDING_PY**：deep 机器基线（5 项 verdict/资源/时长红线/complexity 数值/push 回归差集空）全部冻结收口；coverage 双数值登记为 CPP 单点构建面待修（F8-b）+ PY 双前置（python3-numpy 安装面 + test_cpu_profile 业务域），均属后续独立任务面，不阻塞本观察任务闭环。

## R7 R6.5 遗留执行落地（本地 agent-host 收口轮，SA-CI-32 控制面执行）

前置：用户指令"本地也装上该装的工具链，而不是全靠远端 ci。本地通过再推送"。agent-host（Debian 13，16C/15GiB）补装 cmake 3.31.6（与 hosted 同版）/ gcc,g++ 14.2.0 / clang,clang++ 18.1.8（update-alternatives 无后缀注册）/ ninja 1.12.1 / llvm-profdata,llvm-cov 18.1.8 / python3-numpy,astropy,scipy,yaml,pytest,pytest-cov；V81-ADOPT-004 lock 刷新（ci/toolchain.lock.json，missing_tools 9→ccache）。

### 7.1 修复面（六原子 commit，54a8f9a7..b8a83efa）

| commit | 内容 | 对应登记 |
|---|---|---|
| 54a8f9a7 | F8-b 插桩作用域前移 + F8-c `llvm-cov --sources` | R6.5-1 |
| b0733710 | cpu_profile schema rehome（schemas/ 单一事实源 + 双读取点）+ exe 路径 V6.1 布局修正（R6.5.3 三选一落定） | R6.5-3 |
| 06e288ab | CI 安装面闭包 numpy/astropy/scipy/yaml + F9-c pytest_tail cap 50→120 | R6.5-2/3 |
| b6001684 | V81-ADOPT-004 工具链 lock 刷新（补装后 9 项 missing 清零） | — |
| f2ea0928 | 断链批次 2：test_cli_protocol LEDGER_04 → tracked 副本（sha 17040e7b 双副本一致）+ task_result.schema v2 rehome + checker default 改指（V6_1=v2 代际，archive CV6=v1 不可替代） | R6.5-3 扩展（UT-API/DEEP-COV-PY 面 6 项 setUpClass FileNotFoundError，本地 DEEP-COV-PY 修复前快照实证） |
| b8a83efa | test_version_consistency test_01 过期字面量改单源派生（0.10.0-alpha.2 硬编码 vs VERSION 单源矛盾） | UT-VERSION 1/2 败修复 |

### 7.2 本地验证结果（agent-host 16 核）

- **F8-b/F8-c**：coverage 构建（ASTROCS_BUILD_COVERAGE=ON，clang）profraw **58 个**（hosted 四轮 0）；qa_coverage_report.sh merge/report 通过，coverage.json 产出（132 源文件）；ctest 业务失败集合 = 冻结基线一致（core_pipeline / cpu001_selftest_avx512，Error 8 透传）。
- **test_cpu_profile**：7/7 passed（修复前 import 期 FileNotFoundError）。
- **test_cli_protocol**：6/6 OK（修复前 setUpClass FileNotFoundError ×6）。
- **numpy×6 修复面**：5 测试面 26 passed（import 修复后下一层断言全过）；p3006 13 errors 为测试间耦合（p2003_dbg 产物），业务域既有问题维持登记。
- **DEEP-COMPLEXITY**：五字段与冻结基线全等（lines 139,881 / fn 1,858 / branch 19,182 / max 1,048 / excluded 307）。
- **DEEP-CLANG-BUILD**：本地 PASS 112.7s，资源形态一致（双核 -j2 满载）；F8-b 编译警告集合零变化（预期：旗标仅在 COVERAGE=ON 分支生效）。
- **UT-BACKEND 全套**（126 tests）：9F/16E/7S——hosted 侧同检查四轮全部 TIMEOUT(-9)（2 核 900s 跑不完），运行期面被超时遮蔽；失败构成：abi_loader g++ 探针族 9、aio fixture 自编译族（p200x/p300x setUpClass）12、hardware_inspect V6.1 路径 1、p3006 耦合族内含。**登记为 UT-BACKEND 治理独立任务面（超时/分片/fixture 缓存 + 上述失败集合），不属本轮断链修复面。**

### 7.3 coverage 数值（修复后 DEEP-COV-PY 本地复跑）

（本节数值由后台复跑 bash-28 回填：pytest 收集错误清零后的 coverage 数值与 tail。）

### 7.4 R6.5-4（归集面扩法）决策登记

方案 A（gzip 明细 + 摘要双轨，≈30MB/run）评审通过；实施面 = qa_coverage_report.sh 落盘+gzip、deep_ci_driver.py L181 后缀集合、checks.json outputs 文件级收紧、yml 新增 Upload coverage evidence step——归后续数值产出轮实施（本轮 checks.json 禁改）。

### 7.5 已知边界

- verify_toolchain 防抄袭哨兵 1 项 FAIL：agent-host cmake 3.31.6 与 policy.linux_hosted.cmake（minimum floor）巧合同值，判据（version≠hosted）无法区分实测同值与抄录同值；lock 数据 verbatim 实测，59/60 PASS。哨兵逻辑升级另立任务。
- 工具链 lock 的 policy_file_sha256 记录工作树值（apt_packages 扩充后），与最终提交的 policy 一致性随 commit 固化。
