# 检查-S10-architecture（docs/architecture 全部）

> 切片：S10｜面：①科学性 ②行文逻辑 ③跨文档冲突 ④幻觉与锚｜方式：**纯静态检查**（读文件 + grep + 用 eng/ci/cmake_graph.py 做静态闭包复算；**未编译、未跑测试、未执行任何 CI 门**；凡自行复算门逻辑处均标注「静态复算」）。
> 预读基线：独立审计/实验重做/总编对账/检查-修复验证.md（28 PASS / 4 遗留，遗留均不在本切片，未重复上报）；科学争议以 分歧台账.md D-01..D-11 终裁为界（本切片未触及公式/容差/冻结科学定义，未重开任何裁决）。
> 仓库状态：git status --porcelain 干净（HEAD 已提交态问题）。
> 计数：**红 7 / 黄 12 / 绿 0**（无填充项）。

---

## 一、红（7）

### R1
- **严重级**：红
- **文件:行**：docs/architecture/EXECUTION_MODEL.md:50、:52（同文 :77 自引）
- **问题**：退出码数值与「唯一源」直接冲突，且与本文件自身声明自相矛盾。表中写 CANCELLED=10、「超时返 TIMEOUT=9」；:77 又写「生产错误面见 ERROR_MODEL.md（唯一源 exit_codes.h）」。
- **证据**：
  - 唯一源 lib/infrastructure/cli/exit_codes.h:2「本文件是 11 个退出码在仓库内的唯一定义处」；:16 CANCELLED=9（注释含“超时”）、:17 RESOURCE=10、:18 INTERNAL=70；**无 TIMEOUT 码**（TIMEOUT 只是 ERROR_MODEL:7-8 的错误类别）。
  - docs/architecture/ERROR_MODEL.md:26-34 同一张表（OK0/ARGS2/INPUT3/SCIENCE4/BACKEND5/COMPUTE6/IO7/INTEGRITY8/CANCELLED9/RESOURCE10/INTERNAL70），:33-34「本文档与任何下级文档的数值表只有这一套」。
  - 反方核验：CANCELLED=10 / TIMEOUT=9 只存在于 lib/infrastructure/pipeline/orchestrator/cpp/include/orchestrator.h:143 namespace AstroCsExitCode（legacy），而 EXECUTION_MODEL:11-12 自己把 orchestrator 标为「历史保留…不是生产执行层」。
- **建议改法**：按唯一源修正 :50/:52 数值与语义（超时→CANCELLED，或改述为错误类别），或改为「引用 exit_codes.h、不复制数值」；同步复核 ARC-EXEC-007 表述。
- **所属面**：③（docs ↔ docs ↔ 源码两两冲突）

### R2
- **严重级**：红
- **文件:行**：docs/architecture/ERROR_MODEL.md:45（判据声称）＋域外附件 eng/tools/docs_machine_consistency.py:199/:341、docs/TRACEABILITY.csv:13
- **问题**：文档声明的判据与仓内唯一实际门互相否定：文档侧已切 exit_codes.h，机器门与溯源表仍锚 legacy orchestrator.h，**二者不可同时成立**。
- **证据**：
  - ERROR_MODEL.md:45「判据 = 与 exit_codes.h 的 astrocs::ExitCode 枚举逐名逐值一致」——仓内**无任何检查器**实现该比对（eng/ 下 grep astrocs::ExitCode → 0 命中）⇒「文档说有、代码没接」。
  - 实际门 docs_machine_consistency.py:199 SOURCES 要求 ERROR_MODEL.md 含 token AstroCsExitCode（实测 grep -c 该 token = **0**）；:341 error_taxonomy_exit_codes = ("AstroCsExitCode" in tax) and bool(code_exit) and (doc_exit == code_exit)，doc_exit 需从 ERROR_MODEL 解析 legacy 枚举（不存在 → False）；:464-465 工具自带的 ERROR_MODEL 期望片段仍是「AstroCsExitCode / SUCCESS=0…」旧口径。**静态复算**：source_paths_alive（fail-closed）与 error_taxonomy_exit_codes 双红 ⇒ CHK-DOCS-MACHINE-CONSISTENCY（checks.json 中 waivable:false）静态判红。
  - docs/TRACEABILITY.csv:13 ENG-ERR-001 仍写「退出码与 orchestrator.h 全量一致」；orchestrator.h:40 注释亦声称「与 ERROR_MODEL.md 全集合一致…TIMEOUT=9/CANCELLED=10，由 docs_machine_consistency.py error_taxonomy 全集合校验」——与现行 ERROR_MODEL 全面不符。
- **建议改法**：择一口径收敛（门与溯源表改锚 exit_codes.h，或文档回退）；不得以 waiver 盖红。本报告只登记、未作任何修改。
- **所属面**：④（判据声称无实现）＋③（TRACEABILITY/源码注释 vs 文档）

### R3
- **严重级**：红
- **文件:行**：docs/architecture/execution_options_contract.md:49-55（配置行 schema 指向＋§CLI 覆盖）
- **问题**：合同声称的「唯一入口 acsd 的 CLI 覆盖键」与「生产配置 schema」两处均未接线。
- **证据**：
  - §CLI 覆盖：grep -rn -- "--cpu-workers" lib/infrastructure/cli/ → **rc=1（0 命中）**；parser.cpp 实收 flag 仅 13 个（--cpu-profile/--events-jsonl/--export-mode/--help/--json/--mode/--on-resource-gate/--output/--run-manifest/--strict-resource-gate/--template/--version/--yes），--deterministic 与 --io-workers 同为 0 命中。全仓这三个 flag 只在**非发布工具** lib/algorithms/coverage/tools/stage2.cpp:140/:168/:169/:171（PRODUCTION_EXECUTION_INVENTORY.csv:9 将 astrocs-stage2 分类 tool、非发布）。
  - §schema 指向：grep -rln "execution" eng/contracts/schemas/ → **rc=1（0 命中）**；phase_config_mosaic.schema.json 的 config 段 props = {algorithm_rejection_method, blocks, coverage_index, hips_paths, output_dir, reject, snr_path, upm}，无 execution/worker 键；且该文件 **:31** 明文「硬件字段禁令：ISA/workers/block/CPU 型号等一律不得出现在 phase_config…用 additionalProperties:false 做机器拒绝」——与合同指向相反。
  - 反方核验（代码侧合同属实部分）：stage2_common.cpp:484-491 确实解析 execution.cpu_workers/io_workers 并校验 0..1024（与合同数值一致）；但其唯一非测试调用方是 tools/stage2.cpp:159（非发布工具），生产 acsd 不经此路径。
- **建议改法**：§CLI 覆盖改为「acsd 无此三 flag；覆盖面 = 非发布 astrocs-stage2」或补实现；schema 指向改为真实承载面（或登记 phase_config 不含 execution 段）。
- **所属面**：④（文档说有、代码没接）

### R4
- **严重级**：红
- **文件:行**：docs/architecture/IO_AND_ATOMICITY.md:13-19（R13 缺口「未闭合」）
- **问题**：登记为「HiPS tile 非原子、缺口未闭合」，与现行代码/测试状态**相反**；连带 ARCHITECTURE.md 中「原子发布已全覆盖以缺口闭合为条件」的条件式失效。
- **证据**：
  - aio_hips_writer.cpp:628-630：「原子 tile 写入口…**全部调用点自动经** write_fits_atomic」；:539 定义 write_fits_atomic（tmp → 写体 → 校验 → fsync → 原子改名 → 父目录 fsync，含故障注入）；9 个 tile 调用点 write_fits_image：:1018/:1028/:1082/:1092/:1369/:1377/:1585/:1595/:1747。
  - :445 代码注释自述「**修复前** write_fits_image 先 std::remove(final) 再…」——即已修。
  - 验收测试在位：lib/infrastructure/aio/tests/test_hips_atomic_publish.cpp、eng/tests/cli/test_fix401_hips_atomic.py。
  - 反方核验：文档锚 aio_hips_writer.cpp:330/:399 的 std::remove 实际在 **:334**（write_fits_image_raw）与 **:404**（write_moc_fits_raw），两处是**非 tile** 直写 helper（raw/MOC 面）——残余直写面存在，但与「HiPS tile 非原子」的陈述对象不符，锚本身也漂 4 行。
- **建议改法**：R13 行改为「tile 已 temp+校验+原子改名（负例测试承载判据）；残余直写面 = raw/MOC helper，另行登记」，或把缺口对象改指真正的非原子面；同步解除上层条件式。
- **所属面**：②（状态陈述与实测相反）＋④（锚错位）

### R5
- **严重级**：红
- **文件:行**：docs/architecture/observability/RESOURCE_MONITORING_CONTRACT.md:43-53（§3.0「唯一口径」强制消歧）
- **问题**：自称唯一口径的消歧段**自相矛盾**：表格与前条 bullet 明确 21 列合同属 monitor_timeseries.csv、本节以下「CSV」均指该工件，末条 bullet 却写「本节的 21 列合同只覆盖 resource_timeseries.csv 上」（该工件为 20 列）；首句亦语法残缺（「任何消费方读取 monitor_timeseries.csv 时 resource_timeseries.csv 名列另一份工件」），读者无法判定 21 列归属。
- **证据**：同文表格两行（resource_timeseries.csv → resource_recorder.h:260-266（20 列）；monitor_timeseries.csv → §3.1（21 列＋seed 行＋行指纹链））；:49「本合同的 CSV 工件名固定为 monitor_timeseries.csv（本节以下所有「CSV」均指该工件）」；:50-53 末 bullet 反向。反方核验：实现侧无歧义——monitor.py CONTRACT_COLUMNS 20 键 + row_fingerprint = 21 列（顺序逐列一致，salt astrocs-log002-v1）；resource_recorder.h:289-294 为 20 列（文档 :47 锚 :260-266 亦漂行）。
- **建议改法**：末 bullet 改为「21 列合同只覆盖 monitor_timeseries.csv；resource_timeseries.csv 另有 20 列合同（resource_recorder.h）」，修残句，锚改 :289-294。
- **所属面**：②（同段自相矛盾）

### R6
- **严重级**：红
- **文件:行**：docs/architecture/ISA_VARIANTS.md:69
- **问题**：引用**不存在**的测量工件并据此宣称「完整性…已提供 Linux 侧完整测量证据」，且该缺位无任何登记（幻觉锚）。
- **证据**：ls artifacts/evidence/prerelease-v5/ → 仅 **ISA-001 / ISA-002 / ISA-003**；artifacts/evidence/prerelease-v5/ISA-004/MEASUREMENTS.csv **不存在**；全仓对该路径引用仅此 1 处；docs/KNOWN_LIMITATIONS.md:37（第 21 条）只登记 ISA-005 与 TASK_LEDGER 缺位，**未登记 ISA-004**。
- **建议改法**：补测量工件入库，或把 §1.6 的「完整性/完整测量在案」改为如实缺位登记并入 KNOWN_LIMITATIONS。
- **所属面**：④（文档说有、仓库没有）

### R7
- **严重级**：红
- **文件:行**：docs/architecture/THREADING_MODEL.md:124（ARC-004 确定性锚点 4+1）；同组锚被 THREAD_BUDGET_ARCH.md:34（FROZEN、自称「线程架构权威」）与 EXECUTION_MODEL.md:19 重复引用
- **问题**：**冻结合同的验收锚 100% 失效**——4/4 drizzle 锚＋upm 锚全部指向空行/无关语句，三份文档互相引用同一组死锚，且无任何门覆盖行锚可解析性。
- **证据**：
  - drizzle_engine.cpp:1662 / :1751 / :1834 / :1843 —— **四行全为空行**（逐行 sed 打印为空）。
  - upm.cpp:495 = "const std::size_t comp = comp_frames.size();"（与「compute_raw raw_w 归一」无关）；compute_raw 实际定义在 **:605**（使用点 :739）。
  - 反方核验（语义未被证伪）：仓内现存归约只有 drizzle_engine.cpp:2279 reduction(+:n_quick,n_fully,n_dropin,n_sh) 与 :2078「不使用 reduction」注释；"reduction(+:nSourcePixels" 全仓 0 命中。check_thread_budget.py 的 REGISTERED 注记已使用**现行**锚（module_adapters :240-:262 ScopedOmpWorkerInjection）——代码门已迁移、文档冻结锚未迁移。另：THREAD_BUDGET_ARCH.md:41 所称登记表名 THREAD_BUDGET_EXEMPT 在 checker 中实为 EXEMPT（:148）与 REGISTERED 两表，无该标识（语义一致、名不符）。
- **建议改法**：ARC-004 锚更新到现行行（drizzle 固定序合并/计数归约现行点、upm compute_raw :605 与归一实现行），并加一道锚可解析的静态检查；登记表名与 checker 对齐。
- **所属面**：④（幻觉/死锚）——注：这是证据链断裂，**不是**「归约顺序」语义被证伪，不触及 D-* 科学裁决。

---

## 二、黄（12）

### Y1
- **严重级**：黄｜**文件:行**：docs/architecture/ISA_VARIANTS.md:53 vs 同文 :55-70（§1.6）
- **问题**：同文件先声明「ISA-004(AVX512)/ISA-005(BMI2/POPCNT) 属 Windows 域，**本机不评估**」，随后 §1.6 在 Linux 完整评估 ISA-004 并给出逐 kernel 表与判定。
- **证据**：:53 原句 vs :55「## 1.6 ISA-004 AVX512 复测与判定(vm-bj)」、:56「按任务规则**可以**在 Linux 完整验证…本机支持→必须验证」；反方核验：变体源 lib/infrastructure/benchmark/backend_host/avx512_backend.cpp 存在，矛盾纯属文档内部口径。
- **建议改法**：删 :53 的「本机不评估」，或改为「Windows 复验仍在 WIN-003 域」。
- **所属面**：②

### Y2
- **严重级**：黄｜**文件:行**：docs/architecture/ISA_BIT_MANIP_VARIANTS.md:41
- **问题**：「## 4 完整性」引用不存在的 artifacts/evidence/prerelease-v5/ISA-005/MEASUREMENTS.csv，未并列仓库已有的缺位登记，与 docs/KNOWN_LIMITATIONS.md:37 冲突。
- **证据**：文件不存在（ls 报错）；KNOWN_LIMITATIONS:37「**验收证据文件缺位**：ISA-005/MEASUREMENTS.csv…」已如实登记。反方核验：本文 :35-37 已自述 NOT_APPLICABLE、不写空 DLL——结论保守，冲突仅在完整性声明未同步缺位口径。
- **建议改法**：:41 加「（缺位，见 KNOWN_LIMITATIONS 第 21 条）」或删除该完整性指涉。
- **所属面**：②（跨文档口径）＋④

### Y3
- **严重级**：黄｜**文件:行**：docs/architecture/observability/STRUCTURED_LOGGING_CONTRACT.md:29
- **问题**：§1.1 表把运行事件 kind 枚举复制为 5 个，唯一源 protocol.h 登记 **10 个**；同段还声明「运行事件流…唯一以 protocol.h 为源…本合同**不复制**其字段表」——既复制又不完整，与自身声明矛盾。
- **证据**：文档行 progress / resource / artifact / backend / final；lib/infrastructure/cli/protocol.h:43-56 registered_event_kinds_v1 = 上述 5 ＋ stage_start / stage_end / graph / resource_gate / v6_mode_route；:123-135 未登记 kind fail-closed 拒发；:38「10 类开放 kind 全登记」。
- **建议改法**：删除表中枚举列（改为指向 protocol.h），或补全 10 项并注明唯一源。
- **所属面**：③（文档 vs 唯一源代码）＋②

### Y4
- **严重级**：黄｜**文件:行**：docs/architecture/EXECUTION_MODEL.md:18、:32、:59
- **问题**：§1/§2/§5 三处证据与实现不符。
- **证据**：
  - :18 证据列 "calibrator.cpp: OpenMP 16"：该字符串不存在；grep "16" 仅命中 **:30 历史注释**「多线程固定 16 线程（开发环境 16 核）」，所有 #pragma omp parallel for 均无 thread 子句；生产并发由预算租约注入（module_entry.cpp:949 ac_set_num_threads((int)leased) 等）——「最大并发 16」既无源又与「不硬编码线程」铁律相抵。
  - :32 机器判据名 check_execution_contracts.py::EXEC-AIO-READ-NO-GLOBAL-LOCK：该文件（eng/tools/quality/contracts/check_execution_contracts.py）**无此标识**；实际 35 个 EXEC-* 标识为 EXEC-AIO-READ-GLOBAL-LOCK、EXEC-SMP-GLOBAL-READ-LOCK、EXEC-AIO-LOCK-UNOBSERVABLE 等。
  - :59 "CPU buffers | BufferBinding caller-owned"：BufferBinding 只存在于 **ACR DORMANT 域**（lib/infrastructure/acr/include/astro/compute/kernel_registry.hpp:59、acr_kernels.cpp），生产 CPU 面无此类型；同表 aio_hio_free 属实（aio_healpix_io.cpp:1785）。
  - 反方核验：:32 的读路径线程模型语义与 cfitsio 现状相符（cfileio.c:1499 fits_already_open，:1544 为函数内注释 "threads cannot share the same FITSfile pointer"，落点可接受）；:20 sampler.cpp:924-954 属实（:933/:941 next_c）。
- **建议改法**：证据列改指向真实注入点、真实判据名、真实 CPU buffer 类型。
- **所属面**：④

### Y5
- **严重级**：黄｜**文件:行**：docs/architecture/ERROR_MODEL.md:54
- **问题**：ERR-P2-UPM-001 「见 upm.cpp:~890」——:890 与 frames 校验无关；且与 docs/science/PHASE2_UPM.md:282（upm.cpp:1304-1567）给的是第三套锚。
- **证据**：frames 唯一/类型/C 行数校验实际在 p2_upm_open（upm.cpp:1506 起），畸形拒绝路径注释约 :1606；三处锚互不一致。
- **建议改法**：统一为 p2_upm_open 现行行区间（三处文档同一写法）。
- **所属面**：④

### Y6
- **严重级**：黄｜**文件:行**：docs/architecture/MODULE_MAP.md:73、:74、:102
- **问题**：§2 证据锚三处失准、§4 一处漂行；表格事实本身成立。
- **证据**：
  - :73 module_adapters.cpp:3777-3793 实为 star-psf 降级逻辑；唯一 executor 注册实际在 **:289-313**（shared_work_executor，:246-256 RT-001 编译归属注记）。
  - :74 :4257/:4282/:4309 实为 PSF 统计 JSON 字段与 PSF-FAST 注释；三 Phase 节点表实际在 **:15462 p1_nodes[] / :15487 p2_nodes[] / :15514 p3_nodes[]**。
  - :102 「根 CMakeLists.txt:17 ACR 默认 OFF」——add_subdirectory(export) 已在 :18，选项亦在 :18。
  - 反方核验（通过项）：module.h:94 astrocs::ModuleRegistry、module_registry.c:570、secure_loader.c:369/:558/:612、p1001_real_nodes 注册于 eng/tests/unit/CMakeLists.txt:428-434、lib/phase1_session|phase2_session|phase3_session 目录均在——§1.1 与 §2 事实性声明成立。
- **建议改法**：更新三处行锚。
- **所属面**：④

### Y7
- **严重级**：黄｜**文件:行**：跨文档行锚漂移打包
- **问题/证据**：
  - CACHE_POLICY.md:15：spherical_overlap.h:303 → 类实际在 **:321**、容量常量 **:323**（默认 8192 与文档数值一致）。
  - OWNERSHIP_AND_LIFETIME.md:19：aio_upm.cpp:~448「unique_ptr guard」与 :283「std::unique_ptr<AioUpmDense> guard(d)」——实测 :283 为 float 转换、:448 为 fclose，均非所指语句。
  - OWNERSHIP_AND_LIFETIME.md:20：aio_upm.h:61 实为 open-sparse 注释（delete[] 由调用方负责的规则本身无误）。
  - IO_AND_ATOMICITY.md:12：aio_upm.cpp:60-97 temp+rename → 实际区间 **:84-105**（temp 写＋rename 语义属实）。
  - IO_AND_ATOMICITY.md:33：aio_pipeline.h:205-247 → 六个非生产符号实际分布 **:232-264**（4/6 落在所给区间内）。
- **建议改法**：按实测行更新（数值/语义均无需改）。
- **所属面**：④

### Y8
- **严重级**：黄｜**文件:行**：docs/architecture/production_call_paths_stage1.csv:3-8、execution_inventory.csv（全表）；被 ARCHITECTURE.md §6 引为证据
- **问题**：两份「生产执行语义存量证据」的入口符号与配置门，在唯一生产入口上不可达/不存在。
- **证据**：**静态复算**（eng/ci/cmake_graph.py）：production_entry=acsd，production_closure = **31 target / 135 源**，不含 lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp（astrocs_infra_orchestrator 无任何链接方）；Orchestrator::run_stage_* 调用方仅在 orchestrator 自身与测试内。config_gate "stage1.calibrate.enabled" 等键在 lib/eng/docs 中 **0 命中**；生产 schema phase_config_normalize.schema.json 实际门为 cosmetic.enabled（:225）、photometry fit.enabled（:353）。反方核验：stage1 行 2 已注明「orchestrator 历史登记…非生产入口」，BUILD_GRAPH.md:82-83 亦把 astrocs-stage2 / orchestrator_legacy_cli 列为非生产——**部分自述**；但行 3-8 与 execution_inventory 无同类标注，evidence 列仍写「生产可达性 yes」。
- **建议改法**：为行 3-8 与 execution_inventory 加 DORMANT/历史标注，或按 acsd 实链路（cmd_session → p1/p2/p3_session → module_adapters 节点）重生成。
- **所属面**：④（＋③ 引用面）

### Y9
- **严重级**：黄｜**文件:行**：docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv（24 个路径含 /tests/ 的行；:503-508 acr_boundary 行）
- **问题**：测试文件被标 classification=production、production_reachable=yes；acr_boundary 行的 symbol 列填的是**源码行文本**而非符号。
- **证据**：如 io_writer,test_hips_atomic_publish.cpp,…,production,yes；orchestrator_saturation_wiring_gate.cpp；test_checkpoint.cpp 等 24 行路径含 /tests/ 却标生产可达。生成器 eng/tools/arch/build_production_execution_inventory.py 对测试路径无条件分类（仅 p3_v6_export.cpp 一行带 CLEAN-401 豁免注记，注记自认 7b 类过声明）。:503-508 的 symbol 列内容如 "if (cfg->acr_route != "auto" && cfg->acr_route != "cpu") {"。反方核验：wc -l = 562；:120 acsd 唯一生产 exe 行与 ACR 分类（tool/test=no）正确；24/562 占比小，但字段语义被污染。
- **建议改法**：测试路径行改 classification=test / production_reachable=no（或增独立字段）；symbol 列回归符号名或另设 snippet 列。
- **所属面**：④

### Y10
- **严重级**：黄｜**文件:行**：docs/architecture/ARCHITECTURE.md:13
- **问题**：「acr_route 仅存配置守卫（**ARCH-001 清单 24 处**）」的数字与指名来源对不上。
- **证据**：grep -c acr_route docs/contracts/ARCH-001.md = **0**（该文件连 acr 一词都 0 命中）；全仓最大计数为 PRODUCTION_EXECUTION_INVENTORY.csv **21** 行（grep -c acr_boundary = 21）；lib 源码 acr_route 出现 **13** 处（stage2_common.cpp:3、stage2.cpp:2、routing_test.cpp:5、stage2_common.h:1）。反方核验：!=cpu/auto 显式拒的语义属实（stage2_common.cpp:469-470）。
- **建议改法**：数字落到可指名的清单（如「21 行（PRODUCTION_EXECUTION_INVENTORY acr_boundary）」）。
- **所属面**：④

### Y11
- **严重级**：黄｜**文件:行**：docs/architecture/DEPENDENCY_RULES.md:16
- **问题**：规则「Python 仅限带 NON_PRODUCTION_TOOL_ONLY 标记的测试/研究脚本」与仓库现实、兄弟文档冲突，且无门覆盖。
- **证据**：find lib -name "*.py" = **93**，含标记文件仅 **36**；未标记者含**生产语义模块**：lib/infrastructure/aio/runtime/artifact_store/production_store.py、fits_verify.py、hips_output_store.py、phase_product_exchange_validator.py、pipeline/typed_dag.py、scheduler/v6_budget.py、trace_replay.py。而 MODULE_MAP.md:80 恰把 artifact_store（含 validator/production_store）列为「三阶段产品交换 CONTRACT_READY」证据。eng 侧对该标记的引用只是工具脚本自标注（gen_audit_pack.py 等），**无任何检查器扫描 lib 下 Python 的标记**。
- **建议改法**：规则改为「测试/研究脚本必须带标记；运行时/合同校验用 Python 属独立域（点名目录）」，或补一道扫描门。
- **所属面**：③（DEPENDENCY_RULES ↔ MODULE_MAP ↔ 现实）

### Y12
- **严重级**：黄｜**文件:行**：docs/architecture/ASYNC_IO_CONTRACT.md:78
- **问题**：五个测试名中一个不存在。
- **证据**：lib/algorithms/coverage/tests/async_io_test.cpp 实际 TEST 列表：CapacityDerivedFromMemoryBudget、PushPopRoundTrip、CloseDrainsAndRejectsNew、BackpressureBlocksProducerUntilConsumer、CancelWakesBlockedAndPropagatesError、ReadFailureCancelsAndPropagatesNoDeadlock(:106)、WriteFailureStopsProducerAndPreservesError(:137)、BoundedQueueDeliversAllItemsInOrder(:167)、MultiConsumerProcessesEachItemExactlyOnce(:188)、CancelWakesBlockedConsumerNoDeadlock(:219)——文档写的是 CancelWakesBothBlockedSidesNoDeadlock（**0 命中**）。反方核验：其余 4 个名逐字命中；eng/tests/cli/test_parallel_queue.py 恰 6 用例（test_01..06）与 §10 逐条对应（含 test_06_no_global_serial_lock_construct）。
- **建议改法**：改名对齐 :219。
- **所属面**：④

---

## 三、已查无问题面（逐面覆盖声明）

**①科学性**：本切片 21 md + 5 CSV + 1 JSON 不含公式 / variance-ivar-SNR 定义 / 默认容差 / 冻结科学参数；与 D-01..D-11（含 §2 A-* 裁决）相关的架构侧约束——不硬编码线程/ISA/block、1/N worker 确定性、预算派生并发——已逐条比对：execution_options.h:16/18/24/38/43（gpu_route、hardware_concurrency 为**已登记现状**，合同明列禁令）与 stage2_common.cpp:484-491（0..1024 值域门）一致；PHASE3 默认 sub_block_px=256∈[16,1024]、queue_depth=4∈[1,64] 与 module_adapters.cpp:13552/:13555/:13561/:14731 逐字一致；coverage/CMakeLists.txt:28 P2_ENABLE_OPENMP OFF、ACR DORMANT 与 check_thread_budget.py 的 EXEC-ACR-PROD、check_execution_contracts.py 的 ACR 拒绝逻辑一致。**未发现与科学裁决冲突的新断言**；R7 属证据链问题，按纪律只登记不修。生产面未见硬编码线程数生效路径（Y4 的 16 仅存于历史注释）。

**②行文逻辑**：已通读并确认无内部矛盾者——ARCHITECTURE.md（单入口/三命令/DORMANT 分列）、BUILD_GRAPH.md（C4/C5 判据与两张表自洽）、COMPATIBILITY_POLICY.md（UPM v1/v2 升序自洽；p2_reject_stack 为 COMPAT adapter、生产走 p2_reject_stack_ex 属实，module_adapters.cpp:11111 调用 _ex）、PIPELINE.md 与 DATA_FLOW.md 的 1/20、0/20 命名块覆盖率为**如实自报缺口**（非达成宣称）、ERROR_MODEL.md 的类别/阶段 ID/JSONL 20-29 数字码段、OWNERSHIP 的 close/free 配对规则、ASYNC_IO_CONTRACT §1-§7 语义链、CPU_BACKEND_ARCH（baseline_kernels 单份、六查）。已发现的逻辑问题全部落于 R1/R2/R4/R5/Y1/Y2/Y3/Y11。

**③跨文档冲突**：五权对比（docs/ASTROCS_DESIGN ↔ docs ↔ 实验 ↔ 独立审计/08 ↔ 源码 lib|eng）抽验通过者——单入口不变量三方一致（CMakeLists.txt:981 add_executable(acsd ↔ PRODUCTION_EXECUTION_INVENTORY:120 production/yes ↔ eng/ci/spec_named_impls.json production_entry=acsd）；BUILD_GRAPH 目标集 ↔ 根 CMake **静态复算 31/31 全等、源数 0 偏差、指纹 3/3 命中**（acsd 8a28-c03e-0a23、astrocs_core a537-24d4-cfa9、空集 e3b0-c442-98fc）；install_layout.cmake:104-105 五个科学模块 ↔ MODULE_MAP §1.1；LOG-001/002/003 ↔ 实现（见④）；docs/ci 门注册与各文档所引门名基本对应（例外见 R2、Y4）。已发现冲突全部落于 R1/R2/R3/R5/Y3/Y8/Y11。

**④幻觉与锚**：实开文件核验通过的锚（抽样，均逐行确认）——spherical_overlap.h:321/:323、gaia_client.c:278-282/:851-853、module_registry.c:570、secure_loader.c:369/:558/:612、module.h:94、install_layout.cmake:104-105、execution_options.h:16/18/24/38/43、stage2_common.cpp:484-491、sampler.cpp:924-954（:933/:941 next_c）、cfileio.c:1499（fits_already_open；:1544 为函数内注释）、module_adapters.cpp:240-262/:15462/:15487/:15514（现行节点表）、eng/tests/unit/CMakeLists.txt:428-434；**LOG-001**：schema 15 必含字段＝文档、event 9 枚举＝文档、phase 7 枚举＝文档、log_event.py:54 MAX_LINE_BYTES=4096 / :73 redact / :204 summary / :230 SeqAllocator、log_event_v1.schema.json、check_log_contract.py 均在；**LOG-002**：monitor.py CONTRACT_COLUMNS 20 键 + row_fingerprint = 21 列（顺序逐列一致）、salt astrocs-log002-v1、runner.py HeavyRunGuard、trace_feed.py:57 TraceSnapshotObserver、windows_pdh_etw.py:24/36、verify_monitor_csv.py 均在；**LOG-003**：render_run_graph.py:63-64（VERSION=1.0.0）/:872 GRAPH_CONSISTENT、eng/ci/monitor_evidence.py 语义一致；**ABI_003 / CPU_001 / CPU_003**：secure loader detail 码与 status 面、baseline 单实现、capability probe status 声明抽验一致；check_thread_budget.py 存在且豁免/登记机制与 §5 语义一致（仅表名不符，见 R7 反方核验）；api_inventory.csv 抽样 6 条签名与头文件**逐字一致**（status_codes.h:158-162 四码、aio_pipeline.h:175/:184）；async_io / test_parallel_queue 用例数与名称（除 Y12）一致；PRODUCTION_EXECUTION_INVENTORY wc -l=562 与文档表述相容。**失败锚全部落于 R3/R4/R6/R7/Y4/Y5/Y6/Y7/Y8/Y9/Y10/Y12**。

---

## 四、方法与边界备注

1. **未执行任何构建/测试/CI 门**；R2、Y8 与 BUILD_GRAPH 三处为「静态复算」（读取门逻辑/闭包算法后在只读进程内重算），不是运行门本身。
2. 工具性观察（不影响判级）：本环境 grep 的 BRE 交替 "a\|b" 仅在反斜杠完整保留时生效，JS 字符串里 "pipe 前单反斜杠" 会塌缩为普通竖线导致假阴性；因此本次**所有否定性结论均用单模式或 grep -e A -e B 复验**（--cpu-workers、AstroCsExitCode、execution@schemas、CancelWakesBothBlockedSidesNoDeadlock、reduction(+:nSourcePixels、sub_block_px@resample 等均为单模式复验 0 命中）。
3. 域外（仅登记、不深挖）：docs/TRACEABILITY.csv:13、eng/tools/docs_machine_consistency.py、docs/contracts/ARCH-001.md、docs/KNOWN_LIMITATIONS.md:37——只作为 R2/R6/Y10/Y2 的交叉证据引用。
4. 第三方代码（nanoflann / cfitsio / nlohmann）只做**集成面**核验（cfitsio 的 fits_already_open 与读路径线程陈述），未审其内部实现。
5. git status --porcelain 为空 ⇒ 以上均为已提交态问题；本报告是本切片唯一写入的文件。
