# AUD-001-A4 — infrastructure 7 模块 + 横向条款（设计-实现差异审计）

- 任务：AUD-001-A4（分片：aio / cli / runtime / observability / benchmark / gaia_xpsd_client / hips_browser + 横向 config/、契约 schema、CI 检查器）
- 工作目录：`/workspace/Astro CS Database`（仓库根，只读审计，未改任何源码/测试/配置/文档）
- 对照权威：`ASTROCS_DESIGN.md`（§3.5/§6/§7.1/§8/§9）、`ENGINEERING_SPEC.md §4/§8`、`docs/plugins/infrastructure/17..23`、`docs/design/UNIFIED_MODEL.md`、`docs/ci/01_CHECKS.md`、`docs/architecture/**`、`docs/modules/MODULE_MAP.yaml`、`config/**`、`contracts/**`、`ci/**`
- 证据纪律：每条差距给出 文件:行 或「可复跑命令 + 观测输出片段」；文档↔代码/文档↔文档冲突标 UNRESOLVED，不自行裁决。
- 分级：P0 阻断发布 / P1 发布前应修 / P2 可登记遗留。

## 1. 覆盖清单

| 模块 | 审计结论（含「无差距」） | 差距号 |
|---|---|---|
| aio（`lib/infrastructure/aio`） | 原子提交（tmp→flush→fsync→原子 rename）实现完整且双平台（`aio_atomic_file.h:60-140`）；但缺 `module.yaml` 与模块级 `CMakeLists.txt`，entrypoint 名与 MODULE_MAP 期望不符，doc §5 的 `cache_mb` 无运行期旋钮。七件套 3/7 缺 | A4-01, A4-02, A4-03 |
| cli（`lib/infrastructure/cli`） | 命令树/退出码/取消/JSONL 纪律实现良好（`command_tree.h:43-88`、`exit_codes.h:6-17`、`jsonl.h:50-90`）；但**预检页只实现 correct/error 两级，橘色 optimize 无任何产生点**，与 §3.5/doc18 §4 三级要求冲突；schema 链接悬空 | A4-04, A4-05, A4-06, A4-07, A4-08 |
| runtime（scheduler + pipeline） | DAG/取消/内存回压/线程预算注入有真实实现（`scheduler.cpp:15-70,339`、`executor.cpp:37-99`、`module_adapters.cpp:233-263`）；但 **locality-aware 编排、流式共享缓存、调度指标（空转率/命中率/搬运量）全缺**；且 scheduler per-run 池与 executor 共享池并存 ≈2×budget | A4-09, A4-10, A4-11, A4-12, A4-13, A4-14 |
| observability（`lib/infrastructure/observability`） | 监控器/日志事件实现真实存在并被 `tools/monitoring` 与 `tests/monitoring` 引用；但不在 CMake 构建图、无 README/module.yaml/公开头/共址测试；PENDING.md 声称「本目录为空」与实际 17 文件矛盾 | A4-15, A4-16, A4-17 |
| benchmark（`lib/infrastructure/benchmark`） | `benchmark` 命令按设计把 profile 写安装目录（`commands.cpp:1879-1930`，`install_dir/cpu_profile.json`）；但模块本体缺 README/module.yaml/CMake target/共址测试，且存在与「安装目录」相矛盾且无调用者的 `default_profile_path_v1` | A4-18, A4-19, A4-20 |
| gaia_xpsd_client（`lib/infrastructure/gaia_xpsd_client`） | 进程内 block/query 缓存带 LRU+字节预算（`gaia_client.c:125-149,506-800`）；但 **HEALPix 瓦片键规范化、磁盘二级缓存、request coalescing、邻接预取、对应配置键全部缺失**；entrypoint 为 noop | A4-21, A4-22, A4-23, A4-24, A4-25, A4-26, A4-27, A4-28 |
| hips_browser（`lib/infrastructure/hips_browser`） | 走 aio 读 HiPS（`browser_backend.cpp:201-550` 用 `aio_hiss_*`）；但同目录另写了一份手搓 FITS reader（`hips_browser_backend.cpp:49-93`）违反「读侧复用 aio，不复制 reader」；PENDING.md 同样过时 | A4-29, A4-30, A4-31, A4-32 |
| 横向 config/ 分离 | `config/filters.json` 逐字转录可验证（sha256 实测一致），`defaults.json` 无硬件旋钮、无零点键（三配置分离成立）；但 `config_registry.json#plugin_knobs` 未随新文档包更新，新增键全部漏登；行数自述 96 vs 实际 95 | A4-33, A4-34, A4-35 |
| 横向 契约 schema | UNIFIED_MODEL §2 的 14 个 canonical 对象 schema 齐全、`$id` 唯一，`tests/contracts` 63 测试全绿（无差距）；但 4 处插件文档引用的 schema 文件不存在 | A4-36, A4-37 |
| 横向 CI 检查器 | `ci/checks.json` 注册 46 项、`docs/ci/01_CHECKS.md §2` 列 45 项，双向不一致（CHK-REGISTRY-DOC-SYNC 实测红）；CHK-MODULE-MANIFEST 实测 FAIL（负责人已裁决临时红，如实记数）；ACR dormant 双检查器互相矛盾 | A4-38, A4-39, A4-40, A4-41 |

> 说明：`runtime` 在 MODULE_MAP 中登记为独立模块（`target_dir: lib/infrastructure/runtime`），但实际实现落在 `lib/infrastructure/scheduler/` + `lib/infrastructure/pipeline/`（`scheduler` 承载 DAG/预算/执行/取消/checkpoint，`pipeline` 承载 typed DAG/artifact/orchestrator）。本分片按「runtime = scheduler+pipeline」合并审计，并把目录归属冲突单列为 A4-11（UNRESOLVED）。

## 2. 差距明细

| # | 模块 | 差距类型 | 级别 | 文档条款引用 | 现状复现路径（文件:行 或 命令+输出） | 影响面 | 建议归属 |
|---|---|---|---|---|---|---|---|
| A4-01 | aio | 缺口 | P1 | `ENGINEERING_SPEC.md:37-43` §4 第 1/2/5 项；`ASTROCS_DESIGN.md:349-411` §7.1/§7.3 | `python3 tools/quality/check_module_map.py --json-out /tmp/mm.json --quiet` → `{"module":"aio","code":"missing_module_yaml","detail":"module.yaml 缺失：lib/infrastructure/aio/module.yaml"}`、`missing_cmake_target ... lib/infrastructure/aio/CMakeLists.txt（target=astrocs_infra_aio）`；实际 target 定义在根 `CMakeLists.txt:369`（`astrocs_aio`） | aio 无法作为可独立调度模块被注册/安装；七件套缺 2 件 | INT-001 / MOD-001 |
| A4-02 | aio | 漂移 | P1 | `ENGINEERING_SPEC.md:40` §4 第 4 项「单一 entrypoint」 | `/tmp/mm.json` → `{"module":"aio","code":"missing_implementation","detail":"target_dir 下无 entrypoint astrocs_module_query_v1 的生产实现（src/**）"}`；实际生产 ABI 入口为 `lib/infrastructure/aio/src/aio_abi.cpp`（`aio_abi_query_v1`），与 MODULE_MAP 期望的 `astrocs_module_query_v1` 不同名 | 模块注册/加载按 MODULE_MAP 判「未实现」；ABI 校验面口径不统一 | MOD-001 |
| A4-03 | aio | 漂移 | P2 | `docs/plugins/infrastructure/17_aio.md:29-33` §5（`cache_mb`/`fsync`/`checksum`） | `grep -rn "cache_mb" lib/infrastructure/aio` 无命中；`aio_atomic_file.h:112-130` 无条件 fsync（无开关）；checksum 由 vendored CFITSIO 写（`include/aio_hips.h:171,205`），无运行期开关。`config/config_registry.json` 将三键登记为 `plugin_doc/contracts_doc` finding=none | doc §5 的 3 个配置项在实现侧没有对应运行期旋钮；`cache_mb` 尤其无任何读点 | DOC-002 / 负责人裁决 |
| A4-04 | cli | 缺口 | P1 | `ASTROCS_DESIGN.md:168-190` §3.5「检查结果三级 … 橘色 optimize」；`docs/plugins/infrastructure/18_cli.md:22`；同文 `:55`「有 optimize 三种展示」 | `lib/infrastructure/cli/subcommand.h:108-143`：`calibration_checks` 只产生 `correct`/`error`，`precheck_config` 只产生 `correct`/`error`；`session_commands.h:198-204` `render_checks` 虽支持 `optimize` 文案，但全仓 grep `\"optimize\"` 在 CLI 预检路径 0 个产生点（仅 `scheduler/src/module_adapters.cpp:1818`、`phase1_session/p1_session.cpp:403` 的 stage 统计） | 暗场-亮场超容差（`config/defaults.json` `calibration.dark_light_exposure_tolerance=5s` 约束「超容差→预检橘色 optimize」）不会出现橘色提示；§3.5 三级页面只落地两级 | CLI-001 / DOC-002 |
| A4-05 | cli | 缺口 | P1 | `docs/plugins/infrastructure/18_cli.md:20`（引用 `cli_output.schema.json`、`events.schema.json`）；`ENGINEERING_SPEC.md:43` §4 第 7 项 | `ls contracts/schemas/cli_output.schema.json contracts/schemas/events.schema.json` → 两文件均 MISSING；`/tmp/mm.json` → `cli: dangling_schema_link contracts/schemas/cli_output.schema.json`、`... events.schema.json` | CLI 输出/事件合同的「唯一事实源」缺位；API-DOCS/CHK-CONTRACT-REF 悬空引用 | DOC-001 / CLI-001 |
| A4-06 | cli | 漂移 | P2 | `docs/modules/MODULE_MAP.yaml` cli 条目（`module_id: astrocs.infra.cli`、`entrypoint: main`） | `/tmp/mm.json` → `module_yaml_value_mismatch module.yaml module_id='astrocs.infrastructure.cli' 与映射表期望 'astrocs.infra.cli'`；`entrypoint='astrocs::cli::cmd::run' 与映射表期望 'main'` | 模块清单与 module.yaml 命名空间/入口不一致 | MOD-001 |
| A4-07 | cli | 违规 | P1 | `ENGINEERING_SPEC.md:41` §4 第 5 项；`lib/infrastructure/cli/CMakeLists.txt:1-24`（BLD-002 自述「非产品事实源」） | `/tmp/mm.json` → `cli: missing_cmake_target lib/infrastructure/cli/CMakeLists.txt 中无 add_library/add_executable(astrocs)`；实际唯一可执行在根 `CMakeLists.txt:701 add_executable(astrocs ...)`，cli/CMakeLists.txt 只声明 INTERFACE target | MODULE_MAP 判红；「一模块一 CMake target」与「唯一 project()/唯一 add_executable」两条规则打架 | 负责人裁决（BLD-002 vs §4.5） |
| A4-08 | cli | 缺口 | P2 | `ENGINEERING_SPEC.md:39,42` §4 第 3/6 项 | `/tmp/mm.json` → `cli: header_missing_abi_version 公开头缺 abi_version/struct_size`、`missing_co_located_tests 共址测试缺失或为空：lib/infrastructure/cli/tests`、`contract_refs=false` | CLI 模块七件套 3/7 缺 | CLI-001 / TST-001 |
| A4-09 | runtime | 缺口 | P1 | `docs/plugins/infrastructure/19_runtime.md:41-45` §4.2；`ASTROCS_DESIGN.md:420` §8「编排连续性与数据局部性」 | `grep -rn "schedule_policy\|locality_first\|locality-aware\|locality_aware" lib/ include/ config/ contracts/` → **0 命中**；`grep -rni "localit\|data_location\|block_chain\|depth_first" lib/infrastructure/scheduler lib/infrastructure/pipeline` → 仅 `module_adapters.cpp:477` 的 `cli_affinity_cpu_count` 注释（非编排）。scheduler 是「依赖就绪队列 + 有界 worker 池」（`scheduler.cpp:131-341`），无「同一块连续节点一次走完 / 块间流水 / work stealing」逻辑 | 最高设计 §8 的 headline 语义无实现；PERF-001 的 L2 编排证据（上下文切换/块重载对比）无对象 | PERF-001 / INT-001 |
| A4-10 | runtime | 缺口 | P1 | `docs/plugins/infrastructure/19_runtime.md:49-53` §4.3、`:69`（`cache_budget_mb`）；`ASTROCS_DESIGN.md:419` §8「内存极简化…受字节预算与 LRU 约束」 | `grep -rn "cache_budget_mb\|cache_budget" lib/ include/ config/` → 0 命中；`grep -rn "schedule_policy" lib/` → 0。scheduler 只有整机 `memory_limit_bytes` 回压（`scheduler.cpp:25,172-194`、`include/astrocs/core/scheduler.h:45`），无「进程内只读共享缓存 + 字节预算 + LRU」层 | 流式内存管理只落地「内存回压」，缓存分层/字节预算/LRU 未实现 | INT-001 / PERF-001 |
| A4-11 | runtime | 缺口 | P1 | `docs/plugins/infrastructure/19_runtime.md:17` §3（输出含 `run-trace(JSONL)`、`artifact-manifest`、`run-summary`、`run-graph`）；`ASTROCS_DESIGN.md:433` §9（上述 JSON 形态「当前状态为 NOT_IMPLEMENTED」） | 实测 CLI 只落 `resource_timeseries.csv`/`resource_summary.json`/`worker_balance.csv`/`astrocs_run_*.json`/`run_context.json`/graph 渲染目录（`lib/infrastructure/cli/commands.cpp:147-150,300-442,690-691,898`）；无 `run-trace.jsonl`/`artifact-manifest.json`/`run-summary.json` 写出点 | 插件文档 §3 与最高设计 §9 对同一批 run 产物的状态表述不一致；下游 trace 消费无对象 | UNRESOLVED（见 §3） |
| A4-12 | runtime | 违规 | P1 | `ASTROCS_DESIGN.md:418` §8「一个进程只有一个资源调度器与线程预算源」 | `python3 tools/arch/check_thread_budget.py` 输出：`lib/infrastructure/scheduler/src/scheduler.cpp:339: thread_pool [run() 期 per-run 池 x1 (budget_ 个线程): … 但与 executor 池并存 -> 同 run 线程上界≈2×budget(RT-001 步骤2 单一 executor 未收编; W3-R2-008 佐证)]`；`executor.cpp:39/217` 另有 CPU-heavy + I/O 两个长期池 | 同 run 峰值线程 ≈2×预算，与「单一线程预算源」字面冲突（检查器以「已登记」放行，未消解） | INT-001（RT-001 步骤2） |
| A4-13 | runtime | 缺口 | P1 | `docs/plugins/infrastructure/19_runtime.md:17,45` §3/§4.2（worker 空转率、缓存命中率、数据搬运量、上下文切换、RSS 峰值）；`ASTROCS_DESIGN.md:421` §8 | `lib/infrastructure/cli/commands.cpp:458-483` `emit_resource_summary` 仅含 `n_samples/wall_seconds/avg_equivalent_cores/peak_equivalent_cores/peak_rss_bytes/rss_slope/total_read_bytes/total_write_bytes/total_ctx_switches/max_threads/sample_overhead_ms`；`grep -n "idle\|worker_idle\|cache_hit\|migration" resource_recorder.h resource_events.h` → 0 命中 | 调度指标 4 项中只落地「上下文切换/RSS 峰值」；空转率、缓存命中率、数据搬运量无记录点 | PERF-001 / INT-001 |
| A4-14 | runtime | 漂移 | P2 | `docs/plugins/infrastructure/19_runtime.md:63-70` §5（`workers/block/checkpoint/memory_limit/cache_budget_mb/schedule_policy`） | `config/config_registry.json#plugin_knobs` 对 `19_runtime` 只登记到 line 34（`checkpoint`/`memory_limit`），doc 现行 `:69-70` 的 `cache_budget_mb`/`schedule_policy` 未登记（见 A4-33） | 新增配置键无归属登记，与 CFG-002 登记册脱节 | CFG-002 |
| A4-15 | observability | 缺口 | P1 | `ENGINEERING_SPEC.md:37-43` §4 七件套 | `/tmp/mm.json` observability 条目：`missing_readme`、`missing_module_yaml`、`missing_public_header`、`missing_implementation`、`missing_cmake_target`、`missing_co_located_tests`、`product_unit_missing`（items 全 false，仅 plugin_doc=true）；`grep -n "observability" CMakeLists.txt` → 0 命中 | 模块七件套 0/7；不在构建图，无法作为独立模块安装/注册 | INT-001 |
| A4-16 | observability | 过时 | P1 | `ENGINEERING_SPEC.md:88-116` §7/§8（文档不得用陈旧状态冒充）；`cmake/ARCH-001-migration-manifest.md §4`（「infrastructure/observability 为空（仅 PENDING.md）」） | `lib/infrastructure/observability/PENDING.md:4` 写「本目录为空，未含任何实现」；实测 `find lib/infrastructure/observability -type f | grep -v third_party | wc -l` = **17**（`monitoring/monitor.py`、`logging/log_event.py` 等），且 `tools/monitoring/verify_monitor_csv.py:30`、`tests/monitoring/test_log_contract.py:29` 正在 import 它 | PENDING/迁移清单与现状相反，误导归属判定（ARCH-001 仍标 PENDING） | INT-001 / DOC-002 |
| A4-17 | observability | 缺口 | P1 | `docs/plugins/infrastructure/21_observability.md:17` §3（`run-graph.json` 等）；`ASTROCS_DESIGN.md:433` §9 | `grep -n "events.schema.json\|run_.schema.json" /tmp/mm.json` → observability 的 `schema_link_glob_unverifiable`（`contracts/schemas/events.schema.json`、`run_*.schema.json`）；实际 run 图由外部脚本 `tools/quality/gen_run_graphs.py`（`commands.cpp:423`）渲染，JSON 形态 run-graph 未实现 | 观测产物合同（events/run_*）在 `contracts/schemas/` 无实体，glob 引用不可证 | DOC-001 |
| A4-18 | benchmark | 缺口 | P1 | `ENGINEERING_SPEC.md:37-43` §4；`docs/plugins/infrastructure/20_benchmark.md:17,31` | `/tmp/mm.json` benchmark：`missing_readme`、`missing_module_yaml`、`missing_implementation`、`missing_cmake_target`、`missing_co_located_tests`、`product_unit_missing`；MODULE_MAP status=`CONTRACT_READY`、implemented=false | 七件套 1/7（仅公开头+contract_refs）；profile 生成能力以算法面存在但模块未登记 | MOD-001 |
| A4-19 | benchmark | 无主 | P2 | `docs/plugins/infrastructure/20_benchmark.md:17,31` §3/§5「缓存位置：程序安装目录（固定）」；`docs/design/UNIFIED_MODEL.md §3` | `lib/infrastructure/benchmark/backend_host/profile_store.cpp:181-205 default_profile_path_v1()` 解析 `LOCALAPPDATA/AstroCS/cpu_profile.json`（Win）/ `$XDG_DATA_HOME/AstroCS/cpu_profile.json`（Linux）；`grep -rn "default_profile_path_v1" lib/ include/ tools/` → 仅声明/定义，**无调用者**；生产写点在 `cli/commands.cpp:1922 install_dir + "/cpu_profile.json"` | 存在一条与文档冲突且无人调用的默认路径 API（无主），未来接线会踩到口径分叉 | CPU-005 / DOC-002 |
| A4-20 | benchmark | 缺口 | P2 | `docs/plugins/infrastructure/20_benchmark.md:33` §5（`repeats`） | `config/config_registry.json#plugin_knobs`：`{"module":"20_benchmark","field":"repeats","registration":"none","finding":"unregistered"}`（`20_benchmark.md:33`） | benchmark 自身旋钮无默认登记 | 负责人裁决 |
| A4-21 | gaia | 缺口 | P1 | `docs/plugins/infrastructure/22_gaia_xpsd_client.md:24,58` §4.1/§5（HEALPix 瓦片规范化缓存键 + `tile_scheme`） | `grep -rn "tile_scheme" lib/ include/ config/ contracts/` → **0 命中**；`gaia_client.c:692-740 query_cache_lookup` 用**精确** `ra/dec/radius/mag_low/mag_high/db_type/file_count` 做键，无瓦片包含/重叠复用 | 重叠视场无法复用；文档核心新语义未实现 | INT-001 |
| A4-22 | gaia | 缺口 | P1 | `docs/plugins/infrastructure/22_gaia_xpsd_client.md:24-26` §4.1（两级缓存：进程内 + 磁盘） | `grep -n "cache_dir\|disk\|persist" gaia_client.c` → 0 命中；进程内缓存存在（`QUERY_CACHE_CAPACITY 64`/`QUERY_CACHE_TTL_SEC 60`/`BLOCK_CACHE_MAX_MEMORY 4GiB`，`gaia_client.c:125-149`），磁盘缓存无实现；`config_registry` 中 `cache_dir` finding=unregistered | 跨 run 缓存复用缺失；磁盘缓存配置项无实现 | INT-001 |
| A4-23 | gaia | 缺口 | P1 | `docs/plugins/infrastructure/22_gaia_xpsd_client.md:28-42` §4.2（request coalescing） | `grep -rni "coalesc\|in_flight\|in-flight" lib/infrastructure/gaia_xpsd_client` → 0 命中（全仓命中仅在 ACR dormant 与第三方） | 并发同键查询会重复发起外部请求；「一次外部请求」验收项无对象 | INT-001 |
| A4-24 | gaia | 缺口 | P1 | `docs/plugins/infrastructure/22_gaia_xpsd_client.md:41,59` §4.2/§5（`prefetch_neighbors`） | `grep -rn "prefetch_neighbors\|prefetch" lib/infrastructure/gaia_xpsd_client` → 0 命中 | 邻接瓦片预取未实现 | INT-001 |
| A4-25 | gaia | 缺口 | P1 | `docs/plugins/infrastructure/22_gaia_xpsd_client.md:18` §3（引用 `contracts/schemas/catalog_query.schema.json`） | `ls contracts/schemas/catalog_query.schema.json` → MISSING；`/tmp/mm.json` → `gaia_xpsd_client: dangling_schema_link contracts/schemas/catalog_query.schema.json` | 查询合同无实体，缓存命中/来源标识无 schema 约束 | DOC-001 |
| A4-26 | gaia | 缺口 | P1 | `ENGINEERING_SPEC.md:39-40` §4 第 3/4 项 | `/tmp/mm.json` → `gaia_xpsd_client: header_missing_abi_version`、`noop_entrypoint entrypoint astrocs_module_query_v1 函数体内零调用：导出符号存在但无可执行路径（判 NOT_IMPLEMENTED）` | 模块注册入口为空骨架，违反「不用 facade/空骨架冒充实现」 | MOD-001 |
| A4-27 | gaia | 漂移 | P2 | `docs/plugins/infrastructure/22_gaia_xpsd_client.md:57` §5（`mem_cache_budget_mb`）；`docs/plugins/infrastructure/19_runtime.md:65` §5（workers 由 profile） | `grep -rn "mem_cache_budget_mb" lib/ include/ config/ contracts/` → 0 命中；缓存预算为编译期宏 `BLOCK_CACHE_MAX_MEMORY 4GiB`（`gaia_client.c:134`）；线程数 `gaia_omp_team_size()`（`:106-114`）无租约时回落 `omp_get_max_threads()` 而非预算 | 内存预算不可配；无租约时线程来源不是统一预算源（有注入通道 `gaia_set_worker_lease`，但回落语义未收敛） | INT-001 |
| A4-28 | gaia | 漂移 | P2 | `docs/plugins/infrastructure/22_gaia_xpsd_client.md:50-61` §5（`catalog/catalog_version/query_limit/timeout`） | `config_registry.json#plugin_knobs`：`22_gaia` 行 `catalog`(finding=gap，跨模块重复默认)、`catalog_version/query_limit/timeout` finding=unregistered；登记行号只到 `22_gaia_xpsd_client.md:32-36`，doc 现行 `:57-59` 新键未登记 | 归属未定，且登记册与现行文档行号错位 | CFG-002 / 负责人裁决 |
| A4-29 | hips_browser | 违规 | P2 | `docs/plugins/infrastructure/23_hips_browser.md:24` §4「读侧复用 aio，不复制 reader」 | `lib/infrastructure/hips_browser/healpix_browser_qt/core/hips_browser_backend.cpp:49-93` 自写 `read_fits_image()`（手搓 2880 字节 header + BITPIX 解析 + 字节序交换），与 aio 读路径并行存在（同文件另用 `aio_hiss_inspect/read_tile_signal`，`browser_backend.cpp:201,508`） | 第二份 FITS reader，存在语义分叉风险；违反「唯一 I/O 边界」 | INT-001 |
| A4-30 | hips_browser | 过时 | P2 | `ENGINEERING_SPEC.md:88-116` §7/§8 | `lib/infrastructure/hips_browser/PENDING.md:4`「本目录为空，未含任何实现」；实测 `find lib/infrastructure/hips_browser -type f | grep -v third_party | wc -l` = **50**（含完整 Qt 浏览器） | 与 observability 同类过时；归属判定被误导 | INT-001 / DOC-002 |
| A4-31 | hips_browser | 缺口 | P2 | `docs/plugins/infrastructure/23_hips_browser.md:17` §3（引用 `contracts/schemas/hips_product.schema.json`） | `ls contracts/schemas/hips_product.schema.json` → MISSING；`/tmp/mm.json` → `hips_browser: dangling_schema_link contracts/schemas/hips_product.schema.json` | 只读侧复用合同缺实体 | DOC-001 |
| A4-32 | hips_browser | 缺口 | P2 | `ENGINEERING_SPEC.md:37-43` §4；`docs/plugins/infrastructure/23_hips_browser.md:6,43`（可选组件、不进 manifest） | `/tmp/mm.json` hips_browser：`missing_readme`、`missing_module_yaml`、`missing_cmake_target`、`missing_co_located_tests`、`header_missing_abi_version`、`missing_implementation`；`product_manifest_exempt`（NOTE，依据 00_INDEX §2 + §7.1）；`grep -n "hips_browser" CMakeLists.txt` → 0 | 可选组件本可低配，但七件套缺口与其「独立可执行/组件」定位矛盾；未进构建图 | INT-001 |
| A4-33 | 横向 config/ | 漂移 | P1 | `config/config_registry.json#plugin_knobs`（CFG-002 登记册）；`docs/plugins/infrastructure/19_runtime.md:69-70`、`22_gaia_xpsd_client.md:57-59` | `config/config_registry.json#plugin_knobs` 对 `19_runtime` 最大行号=34、对 `22_gaia` 最大行号=36；而现行文档在 `:69-70` 与 `:57-59` 新增 `cache_budget_mb/schedule_policy/mem_cache_budget_mb/tile_scheme/prefetch_neighbors`，登记册中 0 条；`grep -rn "cache_budget_mb\|schedule_policy\|tile_scheme\|mem_cache_budget_mb\|prefetch_neighbors" contracts/ config/ ci/` → 0 命中 | 新文档包新增配置语义无登记归属；CHK-CONFIG/CONFIG-CONTRACT 面出现未登记键 | CFG-002 |
| A4-34 | 横向 config/ | 漂移 | P2 | `config/defaults.json:14`、`config/config_registry.json:87` | 两处自述「plugin 级…旋钮归属（96 行）」，实测 `python3 -c "import json;print(len(json.load(open('config/config_registry.json'))['plugin_knobs']))"` → **95**；`config_registry.json:1657 "rows": 95` | 登记册行数自述与实体不符（1 行差） | CFG-002 |
| A4-35 | 横向 config/ | 无差距 | — | `ASTROCS_DESIGN.md:80-89` §2；`docs/design/UNIFIED_MODEL.md §3` | `grep -rni "zeropoint\|zero_point\|零点" config/` → 0 命中；`defaults.json` 51 个 key 中无 worker/thread/isa/cpu/block 类硬件旋钮；`filters.json` 转录 sha256 实测一致：`python3 -c "import hashlib,json;d=json.load(open('config/filters.json'));print(hashlib.sha256(open('lib/algorithms/photometry/data/response_curves/filters.json','rb').read()).hexdigest()==d['transcription']['source_sha256'])"` → `True` | 三配置分离与无零点键要求成立 | — |
| A4-36 | 横向 契约 schema | 无差距 | — | `docs/contracts/UNIFIED_OBJECTS.md §1/§2`（UNIFIED_MODEL §2 的 14 对象 canonical 唯一性） | `python3 -c "import json;print(len(json.load(open('docs/contracts/unified_object_registry.json'))['canonical_object_classes']))"` → **14**，逐对象 `canonical_schema_file` 均存在；`python3 -m unittest discover -s tests/contracts -t tests/contracts` → `Ran 63 tests … OK` | 14 canonical schema 完整性成立 | — |
| A4-37 | 横向 契约 schema | 缺口 | P1 | `ENGINEERING_SPEC.md:43` §4 第 7 项「输入/输出端口引用有效 DATA 合同」 | 四文件实测 MISSING：`catalog_query.schema.json`（22 §3）、`events.schema.json`（18 §3、21 §3）、`cli_output.schema.json`（18 §3）、`hips_product.schema.json`（23 §3）；`run_*.schema.json` 为 glob 引用（`/tmp/mm.json` `schema_link_glob_unverifiable`），实际只有 `run_manifest.schema.json`。事件实体为 `contracts/schemas/jsonl_event_v1.schema.json` | 4 个插件模块的端口合同引用悬空；CHK-CONTRACT-REF 面存在不可证引用 | DOC-001 |
| A4-38 | 横向 CI | 违规 | P1 | `ENGINEERING_SPEC.md:117-129` §8「注册表双向一致」；`docs/ci/01_CHECKS.md §1` | `timeout 200 python3 ci/run_checks.py --check CHK-REGISTRY-DOC-SYNC --quiet; echo EXIT=$?` → `EXIT=1`；`python3 ci/check_registry_doc_sync.py` → `REGISTRY_DOC_SYNC_FAIL: registered_not_documented: ['CHK-E2E-REPRO', 'CHK-EXIT-CONSISTENCY']`；`ci/checks.json` 46 项 vs `docs/ci/01_CHECKS.md §2` 45 项 | 注册表↔文档双向一致门实测红；两个已注册检查项未登记到 §2 | CI-001 / DOC-002 |
| A4-39 | 横向 CI | 违规 | P1 | `ENGINEERING_SPEC.md:117-129` §8；`docs/ci/01_CHECKS.md §2 CHK-MODULE-MANIFEST` | `timeout 600 python3 ci/run_checks.py --check CHK-MODULE-MANIFEST --quiet; echo EXIT=$?` → `EXIT=1`；`verdict=FAIL entries=1 steps=8 pass=7 fail=1`；`python3 tools/quality/check_module_map.py --json-out /tmp/mm.json --quiet` → `summary: {"modules_total":23,"status_counts":{"NOT_IMPLEMENTED":20,"CONTRACT_READY":2,"FAIL":1},"fail_findings":139,"verdict":"FAIL"}`，其中本分片 7 模块子集 45 条（FAIL 38 + NOTE 7；aio 5 / cli 10 / runtime 3 / benchmark 6 / observability 9 / gaia_xpsd_client 4 / hips_browser 8） | 模块 manifest/注册表/构建 target 一致性门红；本分片 7 模块全部落在红面 | 负责人已裁决临时红（如实记数）；修归属见各模块行 |
| A4-40 | 横向 CI | 漂移 | P2 | `docs/ci/01_CHECKS.md §2.1`（退役只减不增、RESERVED 不得注册） | `python3 ci/check_registry_doc_sync.py --self-test` → `EXIT=0 SELFTEST_PASS: 正例差集全空；负例 R1/R2/R3/R4 …均按预期命中`；R3/R4 未报，说明退役/RESERVED 面无回归；唯一红为 R1（见 A4-38） | 检查器负例面健康；结论：本项无额外差距，仅佐证 A4-38 | — |
| A4-41 | 横向 ACR（范围外补充） | UNRESOLVED | P2 | `AGENTS.md §5`「ACR dormant」；`ASTROCS_DESIGN.md §1.3/§8` | `python3 tools/check_legacy_exit.py` → `EXIT=0 LEGACY_EXIT_PASS: … ACR dormant 隔离 (binary=build/astrocs, prod_sources=270)`；但 `python3 lib/infrastructure/acr/ci/check_acr_dormant.py` → `EXIT=1 ACR_DORMANT_FAIL - ACK-ACR-004: ACR/CUDA reference in cmake/install_layout.cmake` | 仓内两个 ACR dormant 检查器结论相反：注册项（CHK-SCI-REF 的 ACR-DORMANT）判绿，模块自带 guard 判红 | 负责人裁决（是否将 acr/ci 检查器纳入注册表） |

## 3. UNRESOLVED

| # | 冲突双方 | 各自原文/证据 | 为何不能自行裁决 |
|---|---|---|---|
| U-1 | 插件文档 `19_runtime.md §3` vs 最高设计 `ASTROCS_DESIGN.md §9` | `19_runtime.md:17` 把 `run-trace(JSONL)/artifact-manifest/run-summary/run-graph` 列为 runtime **输出**；`ASTROCS_DESIGN.md:433` 明确「run-plan.json、run-trace.jsonl、artifact-manifest.json、run-summary.json、run-graph.json（JSON 形态）当前状态为 NOT_IMPLEMENTED」 | 最高设计为最高权威，但 §9 同时授权「状态以 docs/plugins/ 与台账为准」；两文档对同一批产物给出「输出 vs 未实现」相反表述。改哪边都需走变更流程（A4-11 的最终归属） |
| U-2 | `docs/modules/MODULE_MAP.yaml` runtime 条目 vs `ASTROCS_DESIGN.md §7.1` + `cmake/ARCH-001-migration-manifest.md` | MODULE_MAP（`docs/modules/MODULE_MAP.yaml:578-606`）要求 `target_dir: lib/infrastructure/runtime`、`module_id: astrocs.infra.runtime`，note 自述「runtime/ 与 tools/monitoring 不在 ARCH-001 迁移清单内」；而 §7.1（`ASTROCS_DESIGN.md:373-374`）只有 `scheduler/` 与 `pipeline/`，ARCH-001 把 `lib/core` 迁到 `lib/infrastructure/scheduler` | 目录不存在是「注册表期望」与「最高设计目录表 + 迁移清单」三方不一致；是新建 `runtime/` 还是改 MODULE_MAP，属架构归属裁决（A4-11/A4-39） |
| U-3 | `lib/infrastructure/acr/ci/check_acr_dormant.py` vs `tools/check_legacy_exit.py`（CHK-SCI-REF 注册项） | 前者 EXIT=1（ACK-ACR-004，`cmake/install_layout.cmake` 含 ACR/CUDA 引用）；后者 EXIT=0（`ACR dormant 隔离`） | 两个 guard 判据范围不同（install_layout 文本引用 vs 生产符号/构建图），无法判定哪一个才是 ACR dormant 的权威判据；且 ACR 不在本分片 7 模块内（A4-41） |

## 4. 复核命令清单

> 全部在仓库根 `/workspace/Astro CS Database` 执行；均为只读（除检查器自身写 `run/ci/**`、`/tmp/**` 临时输出外不改仓库）。

```bash
# —— 0) 目录/清单基线 ——
ls lib/infrastructure/                      # 实际目录：aio cli scheduler pipeline observability benchmark gaia_xpsd_client hips_browser acr
find lib -name module.yaml | sort            # 20 份；aio/scheduler/pipeline/observability/benchmark/hips_browser 无

# —— 1) 横向 CI 双向一致 + 模块七件套 ——
python3 ci/check_registry_doc_sync.py                                   # REGISTRY_DOC_SYNC_FAIL: registered_not_documented ['CHK-E2E-REPRO','CHK-EXIT-CONSISTENCY']
python3 ci/check_registry_doc_sync.py --self-test                       # SELFTEST_PASS（负例面健康）
python3 ci/run_checks.py --check CHK-REGISTRY-DOC-SYNC --quiet; echo EXIT=$?   # EXIT=1 verdict=FAIL
python3 ci/run_checks.py --check CHK-MODULE-MANIFEST --quiet; echo EXIT=$?     # EXIT=1 verdict=FAIL steps=8 pass=7 fail=1
python3 tools/quality/check_module_map.py --json-out /tmp/mm.json --quiet; echo EXIT=$?  # EXIT=1 fail_findings=139
python3 tools/check_module_readmes.py                                   # DOC-003_PASS（只覆盖 5 个模块）

# —— 2) §8 线程预算 / 硬编码 workers 逐点核 ——
python3 tools/arch/check_thread_budget.py                               # THREAD_BUDGET_CHECK_PASS 扫描537 未登记0 硬编码0 已登记31（含 scheduler:339 双池≈2×budget、aio_pipeline_engine:518 待收编）
python3 tools/check_serial_hardcode.py                                  # QA-002_PASS 无 workers=1/nside/OMP 硬编码；3 处租约登记
python3 tools/quality/check_serial_heavy.py                             # SERIAL_HEAVY_PASS
grep -rn "hardware_concurrency" lib/ include/ | grep -v third_party     # 逐点：仅 orchestrator.cpp:507/642、parser.cpp:243（指纹）、benchmark、acr、nanoflann（文件级豁免）
grep -rn "#pragma omp" lib/infrastructure/gaia_xpsd_client/src/gaia_client.c | wc -l   # 23 行（22 条 pragma + 1 注释）；线程源 gaia_omp_team_size() gaia_client.c:106-114
grep -rn "schedule_policy|locality_first|cache_budget_mb" lib/ include/ config/ contracts/   # 0 命中（A4-09/A4-10）
grep -rn "tile_scheme|mem_cache_budget_mb|prefetch_neighbors" lib/ include/ config/ contracts/  # 0 命中（A4-21/A4-24/A4-27）

# —— 3) G-RES-01 文档 ↔ 契约 ↔ 实现 ——
cat contracts/resource_gate_v1.json                     # 阈值 2 / 10s / 60% / 85% / 90% / 0.70 / 32 MiB/s / 10 core·s
sed -n '43,110p' docs/plugins/infrastructure/21_observability.md   # §8 判据表逐条与上文件一致
grep -n "resource_gate_thresholds_generated" CMakeLists.txt        # :88-89 configure_file；:703 进 include
grep -n "kPerSampleUtilizationMinPercent|kQueueLowUtilizationPercent|growth" lib/infrastructure/cli/resource_gate.h  # 实现侧只引用契约常量
grep -n "worker_idle|cache_hit|migration" lib/infrastructure/cli/resource_recorder.h lib/infrastructure/cli/resource_events.h  # 0 命中（A4-13）

# —— 4) config/ 分离 + 登记册 ——
grep -rni "zeropoint|zero_point|零点" config/          # 0 命中（A4-35 无差距）
python3 -c "import json;print(len(json.load(open('config/config_registry.json'))['plugin_knobs']))"   # 95（自述 96，A4-34）
python3 -c "import hashlib,json;d=json.load(open('config/filters.json'));print(hashlib.sha256(open('lib/algorithms/photometry/data/response_curves/filters.json','rb').read()).hexdigest()==d['transcription']['source_sha256'])"  # True

# —— 5) 14 canonical 契约 schema ——
python3 -c "import json;print(len(json.load(open('docs/contracts/unified_object_registry.json'))['canonical_object_classes']))"   # 14
python3 -m unittest discover -s tests/contracts -t tests/contracts       # Ran 63 tests OK
ls contracts/schemas/{catalog_query,events,cli_output,hips_product}.schema.json   # 均 MISSING（A4-37）

# —— 6) 逐模块定位证据 ——
sed -n '108,143p' lib/infrastructure/cli/subcommand.h                   # 预检只产 correct/error（A4-04）
sed -n '198,205p' lib/infrastructure/cli/session_commands.h             # render_checks 有 optimize 文案但无产生点
sed -n '692,740p' lib/infrastructure/gaia_xpsd_client/src/gaia_client.c # 精确键，无瓦片规范化（A4-21）
sed -n '49,93p'  lib/infrastructure/hips_browser/healpix_browser_qt/core/hips_browser_backend.cpp  # 自写 FITS reader（A4-29）
sed -n '181,205p' lib/infrastructure/benchmark/backend_host/profile_store.cpp  # XDG/LOCALAPPDATA 默认路径（A4-19）
head -5 lib/infrastructure/{pipeline,observability,benchmark,hips_browser}/PENDING.md  # 均称「本目录为空」（A4-16/A4-30）
python3 tools/check_aio_ownership.py                                    # AIO-OWN-002_PASS；aio 唯一读边界成立
python3 tools/check_legacy_exit.py                                      # LEGACY_EXIT_PASS（ACR dormant，注册面）
python3 lib/infrastructure/acr/ci/check_acr_dormant.py; echo EXIT=$?    # EXIT=1 ACK-ACR-004（A4-41）
```

## 5. 只读声明

本分片仅创建本报告文件 `run/RELEASE-01/audit/AUD-A4-infrastructure.md`；未修改任何源码/测试/配置/文档/ci，未执行任何 git 写操作。检查器运行产生的 `run/ci/**` 与 `/tmp/**` 输出不属本分片报告交付物。
