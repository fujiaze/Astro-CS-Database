# 变更 claim：DOC-204-AUTHORITY-STATUS-001 —— 权威链与状态字段清理（S08/U01/U02/U03 + 登记表 + owner）

- 控制包：RELEASE-03 / 任务 **DOC-204**
- 日期：2026-09-20
- 依据（最高权威）：`ASTROCS_DESIGN.md` §0.1（只有一份权威链）、§0.2（详细文档层地位：不另立权威链、**登记表与映射表不得写状态字段**）、§11（**状态必须现场计算**；**登记声明的路径必须真实存在**；**禁止用「本目录为空」掩盖已存在的实现**；门禁字面量如实）、§11.4（状态阶梯唯一口径）、§12（版本与发布权）
- 依据（控制包）：`工程控制/RELEASE-03/GAP_AUDIT.md` §2.1 V22、§2.6 U01/U02/U03、`TASK_LIST.md` §2 A 类 DOC-204、`tasks/DOC-204.md`
- 依据（提取报告）：`run/RELEASE-02/design-merge/DESIGN-DRAFT.md` §3.3 S08；`IFC-01.md` §5 Y1–Y4、§6-3 口径①/②/③；`CTR-01.md` §2.A A1–A4；`ARCH-01.md` A8
- 状态：**已落地**（文档侧）；**零代码改动**（本任务未改任何 `lib/**` 生产源码；仅改 `lib/infrastructure/{observability,pipeline,benchmark}/PENDING.md` 三份**失实声明**文档，属任务书明列的「前台扩展」文件域）
- 影响类：**术语 / 登记 / 失实声明级**（**不改**公式、常数、默认容差、SCI/ALG 冻结定义、数值结论；**不改** `ci/checks.json`；**不挂 waiver**；**不删检查器**）

---

## 1 S08 / U01 —— 「本目录为空」类失实声明清零

| 文件 | 原状（失实） | 实测 | 改为 |
|---|---|---|---|
| `lib/infrastructure/observability/PENDING.md:3` | 「本目录为空，未含任何实现」 | `git ls-files` = **13** 个跟踪文件（logging/ 3、monitoring/ 6、probes/ 3 + 本文件）；磁盘 20 | 如实登记目录内容清单 + 待办（INT-001 三分迁移）+ 已知缺口（缺 README/module.yaml） |
| `lib/infrastructure/pipeline/PENDING.md:3` | 同上 | **62** 个跟踪文件（orchestrator/ 50、module_loader/ 5、合同/工具 7） | 同上 |
| `lib/infrastructure/benchmark/PENDING.md:3` | 同上 | **38** 个跟踪文件（backend_host/ 27、cpu/ 10） | 同上 |

- 三份文件同时删去 `状态：PENDING` 自证式措辞，改为「迁移登记（INT-001）」+ 「**本文件不作模块状态声明**：模块状态一律由 `tools/quality/check_module_map.py` 现场计算（§11.4 词表）」。
- **全仓同类扫描**（`grep -rn "本目录为空|目录为空|暂无内容|empty directory" docs/ README.md lib/**/PENDING.md`）：本任务前命中 **4** 处，域内 3 处已清零；第 4 处 `lib/infrastructure/hips_browser/PENDING.md:3` **在本任务文件域之外**（任务书「前台扩展」仅授权三份）⇒ **只登记不改**，交前台一行订正。
- 扩面扫描 `未含任何实现|尚未迁移|无任何实现|没有任何实现` 与英文 `empty directory|no implementation`：除上述 4 处外**无同类失实**（`types.h` 的「纯宏头: 无任何实现」与 `windows_pdh_etw.py` 的「PDH/ETW 尚未实现」均为**如实**陈述）。

## 2 U02 —— 被引为权威但不存在的文档（IFC-01 §6-3 口径①，6 处逐条处置）

| # | 被引对象 | 引用处（本任务域内） | 实测 | 处置 |
|---|---|---|---|---|
| 1 | `tasks/03_RUNTIME_DATA_IO_TASKS.md` | `docs/architecture/observability/{STRUCTURED_LOGGING_CONTRACT:155, RUN_GRAPH_CONTRACT:17,167, RESOURCE_MONITORING_CONTRACT:18,181}`、`docs/interfaces/io/IO_00{1,2,3}:7`、`docs/interfaces/data/DATA-00{2:246,3:9,4:140}` | `tasks/` 目录不存在；`git log --all -- tasks/03_...` **零提交** | 改「**来源已删除/不可考**」+ **替代**（`ASTROCS_DESIGN.md` §6.3/§9 + 本文件 + `docs/DOCUMENT_INDEX.yaml` 登记） |
| 2 | `05_FIXED_SUBAGENT_BINDINGS.yaml` | `docs/interfaces/io/IO_00{1,2,3}:7` | git 跟踪面零命中；仅存 `run/scif2001/shadow/engineering/control/archive/2026-09-09_superseded_V8.1_CI_CONTROL_20260905/05_FIXED_SUBAGENT_BINDINGS.yaml`（gitignore 非规范面） | 同上（并如实写明仅存 run/ 历史归档副本） |
| 3 | `13_DATA_PIPELINE_AND_ARTIFACT_STANDARD`（§1） | `docs/interfaces/data/DATA-003:18` | git 跟踪面零命中（连 `.md` 后缀都没有的引用形态） | 标「来源已删除/不可考」+ 替代 `ASTROCS_DESIGN.md` §9 + `docs/architecture/ARCHITECTURE.md` |
| 4 | `DIAGNOSTICS_STANDARD` | `docs/standards/ERROR_HANDLING_STANDARD.md:20` | 实际文件 = `docs/standards/LOGGING_DIAGNOSTICS_STANDARD.md` | **改引用为可解析来源**（订正为实际文件名） |
| 5 | `lib/infrastructure/aio/io/include/astrocs/io/io_module_api_v1.h` | `docs/interfaces/io/IO_001:30`（列为在位文件） | **全仓零命中**（find 含 `run/`） | 标「**该文件不存在**，如实登记为缺口」+ 现行 io ABI 面 = `fits_stream_v1.h` + `hips_input_v1.h`（均在位） |
| 6 | `astrocs/contracts/artifact_abi_v1.h` | `docs/interfaces/io/IO_001:196` | **路径写法问题**：实际在 `include/astrocs/contracts/artifact_abi_v1.h`（在位） | **改引用为可解析来源**（补 `include/` 前缀） |

- 同族追加（口径①未列但同类，**本任务一并订正**）：`14_RUNTIME_SCHEDULER_AND_TRACE_STANDARD.md`（`STRUCTURED_LOGGING_CONTRACT.md:156`）——git 跟踪面零命中 ⇒ 同 #3 处置。
- **域外**（只登记不改）：`tests/io/test_fits_stream_contract.py:4` 同引 `tasks/03_RUNTIME_DATA_IO_TASKS.md`（`tests/**` 不在文件域）。

## 3 U03 —— 不存在路径的引用

### 3.1 `lib/` 代码注释 40 处（**代码不在本任务域 ⇒ 只产出逐条清单，交前台派单**）

- 复现命令（口径：`git ls-files lib`，排除 `third_party`；正则抓 `docs|contracts|tasks|engineering` 前缀路径，逐条 `os.path.exists`）：
  `python3 /var/tmp/astrocs/u03_scan.py`（脚本与完整输出：`run/RELEASE-03/logs/DOC-204-u03-lib-missing-paths.log`）
- 实测：**40 个不同路径 / 89 处引用**（与 IFC-01 §6-3 口径②「40 处」一致）。
- 逐条清单（路径 | 处数 | 首三处位置 | 建议改法）见 §3.2 表。

### 3.2 逐条清单与建议改法

| 路径 | 处数 | 位置（首三） | 建议改法 |
|---|---|---|---|
| `contracts/schemas/v6/phase_config.schema.json` | 1 | `lib/infrastructure/cli/module.yaml:25` | 改指 `contracts/schemas/phase_config_{normalize,mosaic,export}.schema.json`（三命令同构块结构，§9.71 裁决 2） |
| `docs/24_WCS_VALIDATION_V2_SPEC.md` | 2 | `lib/algorithms/platesolve/cpp/ipv/include/ipv_api.h:238`; `ipv_solver.h:236` | 标「旧控制包规格已删除」+ 替代 `docs/algorithms/PLATESOLVE.md` |
| `docs/25_AUTHORITATIVE_MATCH_PAIR_CONTRACT.md` | 2 | 同上 | 同上（+ `docs/contracts/DATA_SEMANTICS.md` §18） |
| `docs/ADR-00*.md`（glob） | 1 | `lib/infrastructure/acr/README.md:96` | 改指 `lib/infrastructure/acr/docs/ADR-*.md`（实际 ADR 在模块内 docs/） |
| `docs/DESIGN_IMPL_GAP.md` | 2 | `lib/algorithms/photometry/memory.md:336`; `lib/infrastructure/pipeline/orchestrator/memory.md:42` | 标「已删除」+ 替代 `docs/contracts/*` 合同面 |
| `docs/HEALPIX_FORMAT_SPEC.md` | 1 | `lib/infrastructure/aio/memory.md:95` | 改指 `lib/infrastructure/aio/docs/HEALPIX_FORMAT_SPEC.md`（在位） |
| `docs/PIPELINE_OVERVIEW.md` | 1 | `lib/infrastructure/pipeline/orchestrator/memory.md:43` | 改指 `docs/owner/PIPELINE_OVERVIEW.md`（活动版唯一） |
| `docs/PROJECT_OVERVIEW.md` | 1 | `lib/infrastructure/pipeline/orchestrator/memory.md:41` | 标「旧 L0 副本已退役」+ 替代 `docs/owner/*` |
| `docs/algorithm.md` | 3 | `lib/algorithms/photometry/README.md:192`; `memory.md:117`; `module.yaml:121` | 改指 `lib/algorithms/photometry/docs/algorithm.md`（在位） |
| `docs/architecture.md` | 1 | `lib/algorithms/photometry/memory.md:118` | 改指 `lib/algorithms/photometry/docs/architecture.md` 或 `docs/architecture/ARCHITECTURE.md` |
| `docs/algorithms/v6/phase3/ALG-P3-001_{SPEC,…}.md` | 1 | `lib/algorithms/resample/V6_PHASE3_RSMP_IMPL.md:6` | **假阳性**（花括号展开记法，三个文件均在位）⇒ 可保持 |
| `docs/architecture/CPU_ADAPTIVE_V1.md` | 1 | `lib/algorithms/coverage/README.md:87` | 标「已删除」+ 替代 `docs/architecture/cpu/CPU_001_CAPABILITY_PROBE.md` |
| `docs/architecture/cpu/ARCH_CONTRACTS.md` | 2 | `lib/algorithms/projection/README.md:53`; `resample/README.md:32` | 改指 `docs/contracts/ARCH-001.md`（本任务已对 `docs/modules/**` 侧同名引用做同样订正） |
| `docs/architecture/cpu/CPU_002_BASELINE_PROVIDER.md` | 1 | `lib/infrastructure/benchmark/cpu/baseline/include/astrocs/cpu/baseline_provider_v1.h:12` | 现存 cpu 页 = `CPU_001_CAPABILITY_PROBE.md`/`CPU_003_AVX2_PROVIDER.md` ⇒ 改指在位页或标「已删除」 |
| `docs/audit-report.md` / `docs/dependency-lock.json` / `docs/forbidden-paths.md` | 3 | `lib/infrastructure/acr/README.md:93/94/95` | 均改指 `lib/infrastructure/acr/docs/<同名>`（在位） |
| `docs/cuda_bridge_build.md` | 1 | `lib/infrastructure/acr/backends/cuda/bridge/acr_cuda_bridge.h:8` | 标「已删除」+ 替代 `docs/architecture/BUILD_GRAPH.md` |
| `docs/interfaces/io/IO_003_ATOMIC_OUTPUT_` | 1 | `lib/algorithms/drizzle/hips/include/astrocs/hips/publish.h:4` | **假阳性**（注释内换行截断，实际文件在位）⇒ 可保持 |
| `docs/refactor` | 1 | `lib/phase1_session/memory.md:120` | **假阳性**（散文，非路径）⇒ 可保持 |
| `docs/science/GATES_AND_TOLERANCES.md` | 3 | `lib/algorithms/psf/tests/p1psf/CMakeLists.txt:101`; `p1psf_centroid_gate.cpp:21`; `star_coord_contract.h:33` | 改指 `docs/algorithms/GATES_AND_TOLERANCES.md`（在位） |
| `docs/stage1_fix/00_COMMON_CONTRACTS.md` | 8 | `lib/infrastructure/aio/src/hiss_stream_writer.cpp:6`; `hiss_stream_writer.h:10`; `hiss_tile_model.cpp:6` … | 标「旧 stage1_fix 控制包已删除」+ 替代 `docs/contracts/DATA_SEMANTICS.md` §12 + `docs/standards/IO_STANDARD.md` |
| `docs/stage1_fix/spec.md` | 9 | `lib/infrastructure/aio/src/hiss_stream_writer.cpp:7`; `hiss_stream_writer.h:11`; `hiss_tile_model.cpp:7` … | 同上 |
| `docs/stage1_fix/tasks.md` | 1 | `lib/infrastructure/aio/tests/test_tile_model.cpp:4` | 同上 |
| `docs/superpowers/plans/2026-07-13-cpp-qt-browser.md` | 2 | `…/healpix_browser_qt/README.md:129`; `memory.md:11` | 标「外部工具产物，不在本仓」+ 替代 `lib/infrastructure/hips_browser/healpix_browser_qt/README.md` |
| `docs/superpowers/specs/2026-07-13-cpp-qt-browser-core-design.md` | 5 | `…/core/browser_backend.h:6`; `gl_renderer.cpp:9`; `gl_renderer.h:8` … | 同上 |
| `docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md` | 11 | `…/CMakeLists.txt:6`; `app/main.cpp:5`; `app/main_window.cpp:5` … | 同上 |
| `docs/superpowers/specs/2026-07-14-sphere-view-tangent-plane-navigation.md` | 1 | `…/widgets/sphere_view.h:6` | 同上 |
| `docs/superpowers/specs/2026-07-16-healpix-db-legacy-archive.md` | 1 | `lib/infrastructure/aio/healpix_db/memory.md:269` | 同上 |
| `docs/superpowers/specs/2026-07-18-gradient-2d-archive.md` | 1 | `lib/infrastructure/pipeline/orchestrator/memory.md:17` | 同上 |
| `docs/traceability/TRACEABILITY_MATRIX` | 2 | `lib/algorithms/drizzle/hips/memory.md:34`; `drizzle/memory.md:27` | **假阳性**（前缀写法，`.csv`/`.json` 均在位）⇒ 可保持 |
| `engineering/contracts/config_parameter_registry.csv` | 1 | `lib/infrastructure/pipeline/orchestrator/memory.md:624` | `engineering/` 已清空（旧控制包世代）⇒ 标「已删除」+ 替代 `config/config_registry.json` |
| `engineering/contracts/error_code_registry.csv` | 1 | `lib/infrastructure/pipeline/orchestrator/cpp/include/orchestrator.h:112` | 同上 + 替代 `lib/infrastructure/cli/exit_codes.h`（退出码唯一源） |
| `engineering/evidence/P03-002` / `P03-003` | 2 | `lib/infrastructure/pipeline/orchestrator/memory.md:623/642` | 同上（历史证据目录已删） |
| `tasks/02_ABI_BUILD_CLI_TASKS.md` | 2 | `lib/infrastructure/pipeline/module_loader/module_registry.h:8`; `secure_loader.h:7` | 标「旧控制包任务规格已删除」+ 替代 `docs/architecture/abi/*` |
| `tasks/03_RUNTIME_DATA_IO_TASKS.md` | 8 | `lib/infrastructure/aio/io/hips_output_store.py:4`; `…/production_store.py:5`; `…/provenance.py:5` … | 同上 + 替代 `docs/interfaces/io/*`（本任务已订正 docs 侧 9 处） |
| `tasks/04_CPU_RESOURCE_TASKS.md` | 1 | `lib/infrastructure/acr/ci/check_acr_dormant.py:8` | 同上 + 替代 `docs/architecture/EXECUTION_MODEL.md` |
| `tasks/05_PHASE1_TASKS.md` | 3 | `lib/phase1_session/tests/p1sess/CMakeLists.txt:7`; `p1sess_test_main.hpp:6`; `p1sess_tests_units.cpp:4` | 同上 + 替代 `docs/modules/phase1_session.md` |
| `tasks/acr` | 1 | `lib/infrastructure/acr/docs/forbidden-paths.md:37` | **假阳性**（散文截断）⇒ 可保持 |

> 汇总：**真缺口 28 条**（须改引用或标已删除）、**假阳性 12 条**（花括号展开 / 换行截断 / 前缀写法 / 散文）。`lib/**` 不在本任务文件域 ⇒ **本表只交前台派单，未改一行代码**。

### 3.3 `docs/` 侧引用（**本任务域内，直接改**）

| 文件 | 原状 | 改为 |
|---|---|---|
| `docs/architecture/abi/ABI_003_SECURE_LOADER.md:18` | 引 `tasks/02_ABI_BUILD_CLI_TASKS.md` | 标「来源已删除/不可考」+ 替代 `ASTROCS_DESIGN.md` §7.3/§8 + 本文件 |
| `docs/modules/phase3_proj.md:30`、`phase3_rsmp.md:33` | `ARCH-001` 指针 = `docs/architecture/cpu/ARCH_CONTRACTS.md`（不存在） | 改指 `docs/contracts/ARCH-001.md`（在位）；旧路径**去反引号**留痕（`CON-DOC-SYMBOLS` 只判反引号 token ⇒ 不制造假红） |
| `docs/modules/phase3_proj.md:94`、`phase3_rsmp.md:104` | 引 `docs/modules/phase3_session.md`（未建） | 标「未建：Session 型模块按 §7.3 不迁移、待删除」+ 替代 `docs/contracts/RT-001.md` + registry 页 |
| `docs/standards/ERROR_HANDLING_STANDARD.md:20` | 引 `DIAGNOSTICS_STANDARD` | 改指 `docs/standards/LOGGING_DIAGNOSTICS_STANDARD.md` |

- 复测：`python3 tools/quality/contracts/check_doc_symbols.py` 的 `DOC-BAD-FILE` 由 **4 → 2**（消除本任务域内 2 条；余 2 条属他任务：`docs/contracts/DATA_ARTIFACTS.md` 的 PSFSW 退役面、`docs/modules/registry/astrocs.phase2.write.md` 的 `p2_write_descriptor` 未登记符号——后者修法已由 `DOC-203-PREREQ-Q2Q4Q6Q8-SYNC-001.md` §133 给出：在 `docs/architecture/doc_symbol_namespaces.json` 登记该 token + evidence）。

## 4 登记表零状态字段（逐表核实）

| 表 | 状态字段实测 | 处置 | 依据 |
|---|---|---|---|
| `docs/modules/MODULE_MAP.yaml` | **零**（`grep -n 'status:\|状态：'` 空；仅 `status_vocabulary`/`status_vocabulary_authority` 两个**词表**键，由 `tools/quality/check_module_map.py` 现场算状态） | **无需改**（表头已明写「不含任何模块状态声明…禁止在本表写 status 字段」） | §0.2/§11 |
| `docs/standards/STANDARDS_REGISTRY.md` | 原 `:4` `doc_status: ACTIVE_NORMATIVE`（**无任何检查器读取** ⇒ 自证） | **删除**，改为「文档活动分类以 `docs/DOCUMENT_INDEX.yaml` + `check_doc_index.py` 为准」；文末追加「附：状态字段口径」说明两处保留列的机器判据 | §0.2/§11 |
| `docs/architecture/execution_inventory.csv` | 原第 10 列 `production_reachable`（手写、**无生成器、无检查器、无消费者** ⇒ 纯自证快照） | **删除该列**；其证据文本并入 `evidence` 列并注明「生产可达性一律由 `tools/arch/build_production_execution_inventory.py` 现场计算」；13 列 → 12 列 | §0.2/§11 |
| `docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv` | `classification`/`production_reachable` | **保留 + 登记**：该文件是 `tools/arch/build_production_execution_inventory.py` 的**生成物**（状态现场计算），被 `tests/arch/test_inventory.py`、`tests/arch/test_single_cli.py`、`ci/check_spec_named_impl.py` 消费 ⇒ **手改即破坏生成器幂等与既有门**；属「状态由检查器现场计算」的合规形态 | §0.2 但书（现场计算） |
| `docs/DOCUMENT_INDEX.yaml` | 334 条 per-entry `status` | **保留 + 表内注明 + 登记**：`DOC-INDEX`（`tools/doccheck/check_doc_index.py --strict`，`ci/checks.json` 内 `waivable:false`）**要求**该键，缺键即 `status_legal` 红。表头规则 1 已补注：「该字段是**文档活动分类**、由检查器现场校验，**不是** §11.4 交付状态阶梯——§0.2 禁的是后者」。**要连它一起清，须先改 `tools/doccheck/check_doc_index.py`（本任务文件域外）** | §0.2 + §11「门禁字面量如实」 |

- 其它扫描到的登记/映射表：`docs/traceability/TRACEABILITY_MATRIX.{csv,json}`（含 `VERIFIED`/`MISSING` 列）**属 DOC-202/域外移交面，本任务只登记不改**（其检查器 `tools/traceability/check_traceability_matrix.py` 亦在域外）。
- **config 锚点守恒**：`python3 tests/config/check_cfg002_registry.py` → `CFG002-01 … drift=0`、`CFG002-09 anchors=47 … stale_exceptions=[]`、`CFG002-08 anchors=5`、`CFG002-11 token_anchors=8` ⇒ **DRIFTED=0**（本任务未触碰 `docs/plugins/**`、`docs/science/**`、`docs/contracts/CONFIG_CONTRACT.md`，且所有文档编辑**逐行等长**）。

## 5 权威链清理（§0.1）

- 域内（`docs/**` 除 contracts/algorithms/science/design/api/development）`唯一权威` 命中：**60 行 → 3 行**。
- 改写口径：`唯一权威签名头` → `签名头正本`；`唯一权威生产源` → `生产源正本`；`唯一权威=…` → `权威源=…`；其余 `唯一权威` → `权威源`；`VERSION` 两处 → `唯一来源`（保留「唯一源」语义，去掉自我授权措辞）；`ARCHITECTURE.md`/`SCIENCE_OVERVIEW.md` 的**引文**改为不复述禁令字样。
- 改动面 25 文件：`docs/modules/**`（12 页 + registry 8 页）、`docs/traceability/TRACEABILITY_MATRIX.{csv,json}`、`docs/DOCUMENT_INDEX.yaml:257`、`docs/VERSIONING.md`、`docs/architecture/ARCHITECTURE.md`、`docs/governance/VERSION_NAMESPACES.md`、`docs/owner/SCIENCE_OVERVIEW.md`。全部**逐行等长**（JSON/CSV 结构已复验可解析）。
- **残留 3 行（如实登记，不改）**：`docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md:1941`、`docs/archive/review/SCIENCE_OVERVIEW.md:8`、`docs/review/SCIENCE_OVERVIEW.md:8` —— 三处均在 **ARCHIVED_NON_NORMATIVE 归档/退役快照**内；按「归档快照不可改写」保留原样，其内容不构成现行权威链声明。

## 6 `docs/owner/**` 状态字段 与 `docs/DOCUMENT_INDEX.yaml` 失实注释

| 文件 | 原状 | 改为 |
|---|---|---|
| `docs/owner/{SCIENCE_OVERVIEW,PIPELINE_OVERVIEW,ARCHITECTURE_OVERVIEW,CHANGE_REVIEW,RELEASE_STATUS}.md` 第 4 行、`PROJECT_SPEC.md:4` | `> 状态：ACTIVE_NORMATIVE（…）` 自证式文档状态 | 「文档活动分类：以 `docs/DOCUMENT_INDEX.yaml` 登记为准（`check_doc_index.py` 现场校验；本文不自证状态）」 |
| `docs/owner/PIPELINE_OVERVIEW.md:65,91,117` | `状态：`IMPLEMENTED` —— …` 自证模块状态 | 「现场计算（`tools/quality/check_module_map.py`；状态词依 §11.4，不在本文自证）—— …」（**事实与实测命令/rc 原样保留**） |
| `docs/DOCUMENT_INDEX.yaml:554` | 备注「机器检查 …（**C1-C7**）」而注册表正文已 **C1–C9**（IFC-01 §6-4） | 订正为 **C1-C9** |

## 7 验收证据（命令 + rc）

| 命令 | 结果 |
|---|---|
| `grep -rn "本目录为空\|目录为空\|暂无内容" docs/ README.md lib/infrastructure/*/PENDING.md` | 域内 **0**；余 1 行 `lib/infrastructure/hips_browser/PENDING.md:3`（文件域外，已登记） |
| `grep -rn "唯一权威" docs/ \| grep -v ASTROCS_DESIGN` | 域内 **3 行**，全部在归档/退役快照（§5） |
| `python3 ci/run_checks.py --all --profile fast` | 本任务前 `pass=78 fail=8`（日志 `run/RELEASE-03/logs/DOC-204-baseline-runchecks.log`）→ 本任务后 **两次复跑均 `pass=80 fail=6`**（`DOC-204-runchecks.log` / `DOC-204-runchecks-final2.log`）；**失败集合 ⊆ 基线失败集合，零新增红**。注：中间一次复跑出现 `CHK-UNIT/UT-VERSION` 瞬时红，单独复跑 `python3 -B -m unittest discover -s tests/version -t tests/version` = `Ran 31 tests … OK` rc=0（并发他线写入竞态，非本任务引入） |
| `python3 tests/config/check_cfg002_registry.py --self-test` | `CFG002_SUMMARY checks=11 pass=11 fail=0 selftest=PASS`；`SELF_TEST PASS injections=21 problems=0`；rc=0 |
| `python3 tests/config/check_cfg002_registry.py` | `drift=0`、`stale_exceptions=[]`、anchors 5/47/8 ⇒ **DRIFTED=0**；rc=0 |
| `python3 ci/check_no_weight_mode.py` | `CHK-NO-WEIGHT-MODE_PASS: files=346 lines=57794`；rc=0 |
| `python3 docs/standards/checks/check_standards_registry.py --root .` | `STANDARDS_REGISTRY_PASS`；rc=0（DOC-202 转绿后**未回退**） |
| `python3 -m pytest tests/config -q` | `72 passed`；rc=0（DOC-202 基线 72） |
| `python3 tools/doccheck/check_doc_index.py --strict` | `DOC_INDEX_PASS`；rc=0 |
| `python3 tools/check_l0_docs.py` | `DOC_002_L0_PASS`；rc=0 |

## 8 残留与域外（交前台）

1. `lib/infrastructure/hips_browser/PENDING.md:3` 同一失实声明（文件域外，一行可改）；
2. `lib/**` 代码注释 40 条不存在路径（§3.2 清单，代码域外）；
3. `MODULE_MAP.yaml` **S09 复核结论：未闭合** —— 声明路径 164 条中 **53 条不存在**（20 条 `schema_links` + 33 条 `readme/module_yaml/target_file/co_located_tests`）；DOC-202 claim §5 已把「逐条创建/修正」移交 **MOD-002 / 各模块落地任务** ⇒ 本任务**只复核不改**；
4. `docs/DOCUMENT_INDEX.yaml` 334 条 `status`：要按 §0.2 彻底清零，须先改 `tools/doccheck/check_doc_index.py`（+ `ci/checks.json` 的 DOC-INDEX 项）⇒ **门禁改造，域外**；
5. `docs/traceability/TRACEABILITY_MATRIX.{csv,json}` 的 `VERIFIED`/`MISSING` 列：登记表状态字段，域外（检查器 `tools/traceability/`）；
6. `CHK-DANGLING` 余 2 条：`docs/contracts/DATA_ARTIFACTS.md`（PSFSW 退役面，他任务在飞）、`docs/modules/registry/astrocs.phase2.write.md` 的 `p2_write_descriptor`（修法见 DOC-203 claim §133）；
7. 归档/退役快照内 3 行「唯一权威」（§5）；
8. 既有红 6 项（`CHK-MODULE-MANIFEST`/`CHK-SCI-REF`/`CHK-CONTRACT-TEST`/`CHK-DANGLING`/`CHK-CONFIG-DEFAULTS`/`CHK-SPEC-NAMED-IMPL-ON-PROD-PATH`）均有「本任务前已红」日志证据，未由本任务引入。

## 9 自检

- 本任务**未改**任何科学公式、常数、默认容差、SCI/ALG 冻结定义（`docs/science/**`、`docs/algorithms/**` 零改动）；
- 本任务**未改** `ci/checks.json`、**未挂 waiver**、**未删检查器**、**未动 git**；
- 本任务所有文档编辑**逐行等长**（`git diff --numstat` 各文件 added==deleted），`config_registry.json` 锚点 DRIFTED=0；
- 所有外部命令带 `timeout`，日志落 `run/RELEASE-03/logs/DOC-204-*.log`，临时环境 `TMPDIR=/var/tmp/astrocs`。
