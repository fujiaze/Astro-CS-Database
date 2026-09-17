# RT 轴审计报告（调度/线程/资源/观测）— ROOT-004 扩展轴

- **轴名**：RT（调度器/线程预算/资源门/cpu_profile/观测）
- **基线**：开工 HEAD=main=origin/main=`900916fb0dfe93e21bd36c09908a6cc679de5256`（三 SHA 相等，按父指令更新后的基线政策开工）；收工 HEAD=`d414c3e0`（并行线推进中，origin/main≠HEAD）。
- **证据抖动声明**：审计期间并行线在工作树**未提交**重构 `cli/commands.cpp / cli/parser.ts→command_tree.h / ci/checks.json`（CLI-001 命令树迁移进行中，git diff --stat 显示 commands.cpp -392/+? 行级别改动）。本报告全部行号已按【当前工作树实况】二次重锚（旧锚点如 923→794 已更正）；受影响条目=RT-001/002/004/005/006（cli 域）。工作树脏 70 项，属已知并行执行线情况，证据均如实记录本轮真跑输出。
- **方法**：读 DESIGN §6.2/§6.3/§7/§8/§9、ENGINEERING_SPEC §10、docs/plugins/infrastructure/19~21、docs/ci/01/03；grep 定量线程点；resource_gate.h vs run_monitored.py 逐行比对；benchmark/profile_store 消费链追线；run 产物/事件落盘追线；对照 GAP_AUDIT/TASK_LIST/idmap.csv。
- **摘要（≤10 行）**：①生产路径无裸写 `num_threads(字面量)`/`std::thread` 私池（15 处字面量全部在测试/工具 fixture），模块经租约注入 OMP ICV（module_adapters RAII/calibration/cosmetic/gaia/p3），ACR 不入构建，「一进程一调度器+单预算源」在 lib/core 面基本收口（旧双实现在树未被构建引用）。②利用率门分母当前树已两处统一为「已分配容量」（granted 观测优先 / min(allocated,effective)），M5a-G-001/002 的 80/75/50 与 1 核分母**未复现**。③但冻结门**无任何红灯面**：进程内默认 record-only（SO-05 恒 PENDING_OWNER_SIGNOFF），外挂门 CI 注册表零 `--gate-workers/--gate-required`，CHK-RESOURCE（P0）不存在于 ci/checks.json → exit 10 交付合同事实失效（RT-001，P0）。④cpu_profile 链断在消费端：run 预算=affinity 主机容量，`--cpu-profile` 旗标无消费者，安装目录 profile 无自动读取（RT-002，P1）。⑤观测合同偏离 §9 七件套：无 run-plan.json/run-trace.jsonl/artifact-manifest.json/run-summary.json 实名，事件仅 `--events-jsonl` 打 stdout 无落盘，phase2 不写运行图（RT-003/004/005，P1×3）。
- **发现计数**：**P0=1 ｜ P1=4 ｜ P2=2**（共 7 条）

## 发现表

### RT-01｜P0｜资源利用率/内存增长门禁全树无红灯面（exit 10 交付合同失效）

- **定位**：`cli/commands.cpp:969-998`（判定块）+`cli/resource_gate.h:222-232`（GateEnforcement 默认值）+`ci/run.py:71-77` + `ci/checks.json`
- **违反条款**：ASTROCS_DESIGN.md §6.3 退出码表（:336「10=资源利用率或内存增长门禁失败」，:323 称退出码表唯一源）；docs/plugins/infrastructure/19_runtime.md §7（:45「资源利用率门禁失败 → exit 10」）；docs/ci/01_CHECKS.md §2（:38 CHK-RESOURCE＝内存/线程/利用率门禁，P0 硬门）。
- **当前证据**（本轮真跑）：
  - 命令：`sed -n '971p' cli/commands.cpp` → 输出：`// 以 rc=10 阻塞; --strict-resource-gate 才恢复旧的 error + RESOURCE(10) 语义。`
  - 命令：`grep -c -- "--gate-required\|--gate-workers" ci/checks.json` → 输出：0（python 判读：`gate flags: False`，注册表 147 项仅 CHK-ROOT-CLEAN/CHK-MODULE-MANIFEST 两个 CHK-*，无 CHK-RESOURCE）
  - 命令：`grep -n "first10s_cancel : nullptr" cli/commands.cpp` → 输出：737（默认不接协作取消；first-10s 快速失败同样仅 strict 生效）
  - SO-05 恒挂：`cli/v6_runtime_contract.h:338 kSo05Status = "PENDING_OWNER_SIGNOFF"`；`ci/tests/test_ci001_failclosed.py:121` 自述「CI 注册表当前零 --gate-required」。
- **影响**：CPU-heavy 跑成单核/低利用率/内存泄漏也 rc=0 过门，发布门（P0 CHK-RESOURCE）与 §6.3 退出码合同双失效。
- **整改建议**（最小改动面）：SO-05 由负责人二选一：(a) 签字→默认 enforcement 切回 Enforced（改 `strict_resource_gate_arg` 默认值一处）；(b) 裁决维持 record-only→修订 DESIGN §6.3/19_runtime.md §7 并注明裁决号。无论哪种，在 ci/checks.json 注册 CHK-RESOURCE（REAL-001 重计算包，用 `run_monitored.py --gate-workers`，能红能绿）。
- **文件域**：cli/、ci/、ASTROCS_DESIGN.md §6.3（走文档变更流程）
- **验收门**：`python3 - <<'PY'` 式单命令：`grep -c '"--gate-workers"\|"--gate-required"' ci/checks.json` ≥1 且 `python3 ci/run.py --check <CHK-RESOURCE-ID> 2>&1 | tail -1` 存在；或注入低利用 shim 断言 rc=10（tests 域既有用 --strict 断言可平移为默认断言）。
- **GAP/任务关系**：GAP-015 同源（资源门双实现/CI 绑定旧文档）不删条——本条为「门失效实况」升级记录；归属 OBS-001（TASK_LIST L5）+CI-001。
- **旧清单同源**：M5b-C-005（退出码三处互不覆盖）部分同源；V21-N-15/16 属同域判据输入（另立 RT-06）。

### RT-02｜P1｜run 线程预算不读 cpu_profile，--cpu-profile 旗标无消费者（benchmark→runtime 链断）

- **定位**：`cli/commands.cpp:1069 / :1280 / :1673`（budget=cli_affinity_cpu_count()）+`cli/commands.cpp:173-179`（`--cpu-profile` 唯一消费者=config show-effective）+`lib/backend_host/profile_store.cpp:181`（default_profile_path_v1 全树无生产消费者）
- **违反条款**：ASTROCS_DESIGN.md §8（:412 benchmark 生成 cpu_profile「输出到安装目录」供运行用；:413 单一线程预算源）与 §6.2（:313「后续运行时自动读取」）；docs/plugins/infrastructure/19_runtime.md §5（:31 workers「来自 cpu_profile，不得硬编码」）；20_benchmark.md §6（:38「runtime 读取安装目录 profile 决定线程/ISA」）。
- **当前证据**：
  - 命令：`grep -rn "load_profile_checked\|default_profile_path" cli lib/core --include='*.cpp' --include='*.h'` → 输出：（空，无匹配）
  - 命令：`grep -n '"--cpu-profile"' cli/commands.cpp` → 输出：173/175/179 三行（全在 show-effective 展示路径）
  - 命令：`grep -n "const uint32_t budget" cli/commands.cpp` → 输出：`const uint32_t budget = cli_affinity_cpu_count();` ×3（phase2/3/1 均为主机 affinity 容量，非 profile 推导）
- **说明**：并行线 WIP 已把 benchmark 输出从 CWD 默认值改为安装目录（`cli/commands.cpp:2419 install_dir + "/cpu_profile.json"`，开工时 HEAD 版本为 `"cpu_profile.json"` CWD 相对）——写端契约已修，**读端仍全断**；本条以工作树实况立条。
- **影响**：benchmark 实测出的 workers/block/ISA 从不生效，「profile 决定并行」合同为空转；旗标存在但 no-op 属 facade 嫌疑（AGENTS §5）。
- **整改建议**：phase run 入口取 `default_profile_path_v1()`（或 --cpu-profile）→ `load_profile_checked_v1`（哈希校验）→ 预算=min(profile.workers, affinity)，无 profile 走保守并行并 warn（20_benchmark.md §4/§7 已给行为规格）。
- **文件域**：cli/commands.cpp、lib/backend_host（复用，不改科学面）
- **验收门**：`grep -c "load_profile_checked_v1" cli/commands.cpp` ≥1 + 既有 tests/backend/test_cpu_profile.py 扩「run 用 profile workers」断言后 `python3 -m pytest tests/backend/test_cpu_profile.py` 绿。
- **GAP/任务关系**：GAP-015 同源不删条；归属 CPU-001（前置 RT-001）。
- **旧清单同源**：idmap 无 cpu_profile 消费链条目（本轮新立）。

### RT-03｜P1｜run 产物七件套按 DESIGN §9 名单缺失，run_*/events schema 合同不存在

- **定位**：`cli/commands.cpp::write_run_graphs`（写 graph/static_graph.json、observed_trace.json、graph_sidecar.json）+`cli/commands.cpp:446`（astrocs_run_<id>.json）+`cli/resource_recorder.h:144,281`（resource_samples.csv/resource_summary.json/worker_balance.csv）+`contracts/schemas/`（目录无 run_*.schema.json、无 events.schema.json，实名 jsonl_event_v1.schema.json）
- **违反条款**：ASTROCS_DESIGN.md §9（:422「每次运行至少生成 run-plan.json、run-graph.json、run-trace.jsonl、resource-timeseries.csv、resource-summary.json、artifact-manifest.json、run-summary.json」）；19_runtime.md §3（:17 参考 `contracts/schemas/run_*.schema.json`）；21_observability.md §2/§3（:11 参考 `contracts/schemas/events.schema.json`）。
- **当前证据**：
  - 命令：`grep -rn "run-plan\.json\|run-trace\.jsonl\|run-graph\.json\|resource-timeseries\.csv\|run-summary\.json\|artifact-manifest\.json" cli lib --include='*.cpp' --include='*.h'` → 输出：0 命中
  - 命令：`find contracts -name "*.schema.json" | grep -Ei "run_|events\.schema"` → 输出：空（仅 jsonl_event_v1.schema.json 在位）
  - run-plan 等价物只存在于独立 `phaseN plan` 命令（run 本身不落 plan）；artifact-manifest 语义并入 run manifest 的 artifacts 数组（commands.cpp:410-438）。
- **影响**：DESIGN 冻结的运行记录合同按名不可寻，消费者（溯源/审计）无法机械核对「每次运行至少生成」条款；插件文档引用悬空 schema 文件。
- **整改建议**：二选一并对齐——(a) 在 DESIGN §9 走变更流程承认现命名映射表（static_graph=run-graph、observed_trace=run-trace、resource_samples=resource-timeseries 等）；(b) 或 CLI 落盘侧改实名+补 run-plan/run-summary。同 commit 补 `contracts/schemas/run_*.schema.json`（或订正 19/21 文档指向 jsonl_event_v1）。
- **文件域**：cli/、contracts/schemas/、docs/plugins/infrastructure/19,21（映射表订正）
- **验收门**：`grep -E "run-plan|run-trace|resource-timeseries" cli/commands.cpp` 有命中，或 DESIGN §9 含命名映射表（单 grep 二选一判绿）。
- **GAP/任务关系**：归属 OBS-001；与 GAP-014/015 域相邻。
- **旧清单同源**：**M5a-C-003 同源不删条**（资源工件名两套且未登记偏差；其「PSS/每线程 CPU/I/O wait 未采集」半条当前树已采集——commands.cpp:981-985 per_thread_cpu_*/io_wait_pct，半条销账）。

### RT-04｜P1｜phase2（mosaic）run 成功路径不写运行图，观测面按 run 缺失

- **定位**：`cli/commands.cpp:1373`（`write_run_graphs(...,{3})`）与 `cli/commands.cpp:1730`（`...,{1})`）——全文件仅此两个调用点；phase2 路径（:1069-1140 区段）仅 write_run_manifest，无图。
- **违反条款**：ASTROCS_DESIGN.md §9（:422「每次运行至少生成 …run-graph…」（1/2/3 同规））；19_runtime.md §3（:16 输出 run-graph）。
- **当前证据**：
  - 命令：`grep -n "write_run_graphs(out_dir" cli/commands.cpp` → 输出：1373、1730 两行（{3} 与 {1}），无 {2} 调用
- **影响**：mosaic 是三板块中唯一无节点级 trace/图产物的阶段，CHK-002 类双向比较（static vs observed）对 phase2 不可执行。
- **整改建议**：phase2 成功与失败收尾各补 `write_run_graphs(out_dir, ev, cfg, cfg_sha, {2});`（best-effort 语义照抄 phase1 注释合同，RT-009/P1-001 同型改动）。
- **文件域**：cli/commands.cpp
- **验收门**：`grep -c "write_run_graphs(out_dir, ev, cfg, cfg_sha, {2})" cli/commands.cpp` =1 且 tests/integration/v6_p2 或 system 测试断言 phase2 出图后 ctest 目标绿。
- **GAP/任务关系**：归属 OBS-001。
- **旧清单同源**：idmap 检索无 phase2 图缺条目（本轮新立；RT-009/P1-001 历史修复只补了 1/3 两相，代码注释自证）。

### RT-05｜P1｜事件流无按 run 落盘（event_dir 合同未实施），JSONL 仅旗标开启时打 stdout

- **定位**：`cli/jsonl.h:77-79`（fputs→stdout）+`cli/commands.cpp:1768`（`events = flags.count("--events-jsonl")`，默认关）；全树无 `run/<run_id>` 事件目录实现
- **违反条款**：docs/plugins/infrastructure/21_observability.md §5（:30 `event_dir` 默认 `run/<run_id>/`）、§7（:39 事件写失败须降级为文件日志并标记——前提是存在文件通道）；ASTROCS_DESIGN.md §9（:422 run-trace.jsonl 为落盘产物）。
- **当前证据**：
  - 命令：`grep -rn "events\.jsonl\|event_dir" cli lib/infrastructure/cli --include='*.cpp' --include='*.h' | grep -v test` → 输出：空
  - 命令：`sed -n '77,79p' cli/jsonl.h` → 输出：`std::fputs(ev.dump().c_str(), stdout);`
- **影响**：不带旗标的 run 零事件证据（崩溃/取消后无 trace 可复盘）；「无日志污染 stdout」达成但「事件走 JSONL 落盘」半边缺失。
- **整改建议**：run 收尾把已发射事件镜像写入 `<output_dir>/events.jsonl`（JsonlEmitter 加 tee 文件通道，旗标语义保留）；或 DESIGN/21 号文档按实况修订并登记偏差。
- **文件域**：cli/jsonl.h、cli/commands.cpp
- **验收门**：跑最小合成 run 后 `test -f <output_dir>/events.jsonl && wc -l <output_dir>/events.jsonl` ≥1（既有 tests/cli 冒烟可挂断言）。
- **GAP/任务关系**：归属 OBS-001；与 RT-03 同 commit 域。
- **旧清单同源**：无（本轮新立）。

### RT-06｜P2｜门禁输入常量与诊断文案不一致（kind/annotation 恒真、LowAvgCores 文案仍写旧分母）

- **定位**：`cli/commands.cpp:794`（`g.kind = astrocs::ResKind::Compute;` 三相恒真）+`:807`（`g.has_stage_annotation = true;` 恒真）+`cli/resource_gate.h:441-442`（diag 文案 `" < 0.85*min(workers,cpus)="）与 :245-249 实现（granted_workers 观测优先）不一致；`cli/resource_gate.h:241` 残留 `NEEDS_DECISION(M5a-G-002)` 注释。
- **违反条款**：ASTROCS_DESIGN.md §8（:414 每 heavy 运行自动记录真实观测——恒真标注使 memory/io 类判据与「无标注→P1」分支事实失效）；docs/ci/01_CHECKS.md §2 CHK-RESOURCE 能红能绿精神（判定输入须反映事实）。
- **当前证据**：
  - 命令：`grep -n "ResKind::Compute;\|has_stage_annotation = true" cli/commands.cpp` → 输出：794、807（无条件赋值）
  - 命令：`sed -n '441,442p' cli/resource_gate.h` → 输出：`return "compute 门禁: avg_equivalent_cores " + ...` / `" < 0.85*min(workers,cpus)=" + std::to_string(thr);`
- **影响**：memory/io/mixed 分类门在 CLI 路不可达（与 commands.cpp 注释「io/mem 判据属 benchmark 专用路径」自洽但 DESIGN 未授权收窄）；诊断文案误导分母口径。
- **整改建议**：diag 文案改 `0.85*allocated_capacity`；kind 从配置 `resources.class` 读取（缺省 compute 如实标注）；NEEDS_DECISION 注释随 RT-01 裁决一并销账。
- **文件域**：cli/commands.cpp、cli/resource_gate.h
- **验收门**：`grep -n "min(workers,cpus)" cli/resource_gate.h` → 0 命中；既有 gate 单测 `ctest -R resource_gate` 绿。
- **GAP/任务关系**：归属 OBS-001。
- **旧清单同源**：V21-N-15（annotation 恒真）与 V21-N-16（kind 恒 Compute）**同源不删条**（行号已重锚 912→807、901→794）；M5a-G-001/002 的阈值/分母主体**当前树未复现**（85/60 + 已分配容量已落地）。

### RT-07｜P2｜第二调度器/第二监控实现残留（均不在生产构建，属清运面）

- **定位**：`lib/orchestrator/cpp/src/orchestrator.cpp:5174`（watchdog `std::thread`）+`lib/orchestrator/cpp/tests/*`（set_num_threads(16) 断言）+`runtime/`（顶层 python monitoring/v6_budget 双实现）+`ci/resource_monitor.py:1-12`（自述为 run_monitored.py 桥接 shim）
- **违反条款**：ASTROCS_DESIGN.md §7.1（基建唯一归属 scheduler/observability）、§8（:413 一进程一调度器）。
- **当前证据**：
  - 命令：`grep -c "orchestrator" CMakeLists.txt` → 输出：0（不构建）
  - 命令：`ls runtime` → 输出：`artifact_store core io logging module_loader monitoring pipeline registry v6_budget.py`
- **影响**：可混淆后续任务与检索（README/退出码第二套随 orchestrator 残存）；无运行期危害。
- **整改建议**：随 RT-001 收口任务统一删除或归档至明确 DEPRECATED 标记（含其 README 第二套 ASTROCS_* 码表）；不单独动。
- **文件域**：lib/orchestrator/、runtime/
- **验收门**：`test -d lib/orchestrator -o -d runtime` 非真，或两目录含 DEPRECATED 登记（单命令判绿）。
- **GAP/任务关系**：**GAP-014、GAP-015 同源不删条**（双实现并存——本条补「不在构建」实况）；归属 RT-001。
- **旧清单同源**：M8a-C-001（废弃第二调度器 README+第二退出码表）、W3-R2-008（executor 头注宣称唯一池与同树残存矛盾）同源。

## 合规事实（非发现，供控制面引用）

1. 硬编码线程定量：全仓 C/C++ `num_threads(常量)/omp_set_num_threads(常量)/std::thread(/pthread_create` 命中 214 处中，**生产路径 0 裸值**——字面量集中于 tests（1-worker vs N-worker 一致性，ENG_SPEC §5.1 要求面）与 tools/quality fixture；生产注入口径=module_adapters.cpp:245（RAII 租约→ICV）、calibration/cosmetic module_entry 租约注入、gaia_client.c:106（leased 优先、回退 omp_get_max_threads 环境层）、p3_session.cpp:244（budget.max_workers 租约，禁 hardware_concurrency 自注）。
2. ACR：未出现在根 CMake（add_subdirectory 清单核对），符合 §8「生产不含 ACR/CUDA」。
3. 双资源门数值口径已一致：85%（均值/p50 90%）+60%（10s 窗）+分母=已分配容量（cli: granted_workers 观测优先；wrapper: min(allocated, effective_cpu_cores)，affinity∩cgroup 探针），「1 核分母」旧问题未复现。
4. 退出码唯一源存在：cli/exit_codes.h（BACKEND=5、RESOURCE=10 与 DESIGN §6.3 表一致；5 的接线见 selftest→BACKEND）。

## 收敛顺序建议（RT-001 → CPU-001 → OBS-001）

- **第一步（可并行止血）**：RT-01 属 OBS-001 但 P0——请负责人先裁决 SO-05（签字恢复 rc=10，或修订 DESIGN §6.3 授权 record-only）+ 注册 CHK-RESOURCE；不必等前序任务。
- **第二步 RT-001（L2 调度/预算单一来源）**：现状已≈合规（lib/core 唯一构建中的调度器），实质工作是 RT-07 清运 + 把「预算来源」语义定死：预算=profile.workers（接 CPU-001）而非主机容量。
- **第三步 CPU-001（L5）**：等并行线 CLI-001 WIP 落定命令面（benchmark 旗标 quick/full/--output 取舍正在变）后，实现 run 自动读取安装目录 profile（RT-02），profile→workers/block/ISA 注入单预算源。RT-001 的预算语义与 RT-02 应同波实现：否则 OBS 门分母（granted vs profile 分配）在 RT-001 之后仍会再变。
- **第四步 OBS-001（L5）**：吃 RT-03/04/05/06——七件套命名对齐 + run_*/events schema 补齐 + phase2 图 + 事件落盘 + 门禁输入去恒真。
- 依赖链：RT-001 →（CPU-001 ∥ OBS-001 前段）→ OBS-001 后段；P0 红灯面恢复插队最前。
