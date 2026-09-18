# W2-DOC3 —— 第二波·文件级审计：docs 其余全部域（含 docs 根）

- 分片：W2-DOC3（穷尽扫描，每文件给出结论；只判定，不修复，零 git 写）
- 开工 SHA：HEAD=main=`180c8a0ad9755e513670c7a1feb8fa215a87e1d7`；`origin/main=b4afc135848d8b401ee36278effcb06699542fec`（三者不等；按规程等约 2 分钟复测仍不等，以 HEAD=main 相等为准开工并注明。并行线会话中持续提交，HEAD 依次经 `23f42ffa`、`01754fab`、`20d86b79`，main 收工时已到 `32a5f5f3`。）
- 收工 SHA：HEAD=main=origin/main=`32a5f5f300700b817cfa1ceafe047acaeff4ee9b`（会话末三 SHA 已追平；并行线持续提交，关键门禁证据在 01754fab→20d86b79 两轮复跑一致）
- 必读口径：AGENTS.md；ASTROCS_DESIGN.md §0（权威链）与 §1.2/§5.3/§6.2/§10.1/§11.3/§12；ENGINEERING_SPEC.md §7/§8；SHARD_BRIEF.md §1/§2
- 域枚举：`git ls-files docs | grep -v ^docs/(science|algorithms|plugins)/` → 收工 **223 个 tracked 文件**（开工 220；会话中并行线将 contracts UNIFIED_OBJECTS.md / unified_object_registry.json / modules MODULE_MAP.yaml 3 个 UNTRACKED 转正提交）（任务列名 17 目录=186 + docs 根 11=197；「docs 其余全部」再含 validation/interfaces/review/references/performance/operations=23，一并穷尽）。开工时 UNTRACKED 5 个：收工时 3 个已被并行线转正（计入 223），余 2 个单列附注不计入 files_total。
- 发现 11 条：P0=0，P1=8，P2=3。对旧账本口径全部按 §1 四态在树内复现 = OPEN（本轮亲测）。

## ① 发现表

字段：ID｜定位 path:line｜违反的最新权威条款｜当前证据（命令+本轮真跑输出≤3行）｜严重度｜影响｜整改建议｜建议文件域｜验收门（单命令）｜同源标注

**W2-DOC3-1**｜docs/DOCUMENT_INDEX.yaml:9-10,35,749-757｜ASTROCS_DESIGN §0（唯一最高权威=ASTROCS_DESIGN，旧宪章已下链）；SHARD_BRIEF §2｜索引头注+active 段仍登记 `ASTROCS_PROJECT_CONSTITUTION.md`=ACTIVE_NORMATIVE「唯一最高约束」；`python3 tools/doccheck/check_doc_index.py` 本轮输出 `verdict=DOC_INDEX_FAIL`（fails: control_archive_dir_readme、docs_fully_covered 未覆盖 30）；python 解析实测 index=305 条中 6 条指向已删文件（宪章/旧约束/CHANGELOG.md/REVIEW.md/engineering/control/archive/×2，均于 `01db973b`(ROOT-007) 删除）、docs/** 未登记 43 个（docs/ci 全部 5、docs/plugins 全部 23、docs/review 全部 5、UNIFIED_MODEL、索引自身等）｜P1｜机器索引整体失效且把已删旧宪章写回最高权威位；doccheck 下游判定失真｜按 §0 权威链重建 active/archived 段、删 6 条 dangling、补登 43 个、旧宪章条目降 archived 注「非链上权威」｜docs 根+全 docs｜`python3 tools/doccheck/check_doc_index.py` verdict=DOC_INDEX_PASS｜GAP_AUDIT「DOC-INDEX rc=1」同源；第一波 GOV 165+ 的索引侧逐文件清单见本报告附表
**W2-DOC3-2**｜活动面 27 文件（行号例：docs/design/PHASE1_DETAILED_DESIGN.md:5；docs/owner/PROJECT_SPEC.md:5,9；docs/owner/ARCHITECTURE_OVERVIEW.md:8,93；docs/owner/PIPELINE_OVERVIEW.md:8,110,133；docs/owner/RELEASE_STATUS.md:11,62；docs/owner/SCIENCE_OVERVIEW.md:79,101；docs/owner/CHANGE_REVIEW.md:111；docs/review/ARCHITECTURE_OVERVIEW.md:6、PIPELINE_OVERVIEW.md:6、SCIENCE_OVERVIEW.md:56；docs/architecture/MODULE_MAP.md:33；architecture/abi/ABI_003:4、cpu/CPU_001:4、CPU_003:5、observability 3 篇、execution_options_contract；interfaces/data 3 篇、interfaces/io 3 篇；docs/api/MANIFEST_VERIFY_V1.md:35；docs/validation/v6/QA_MATRIX.md:4,30,816、ORACLE_AND_ZERO_CASE_POLICY.md:16）｜ASTROCS_DESIGN §0；ENGINEERING_SPEC §8（删除/重命名无悬空引用）；SHARD_BRIEF §2（旧宪章/旧约束/旧控制包废止）｜`grep -rn` 本轮实跑命中 27 文件 45 处（旧宪章/旧约束/宪章§/tasks/NN_/工程控制 AstroCS_* 已删包；被引文件 `git ls-files` 零命中、磁盘不存在，删除 commit=01db973b）｜P1｜活动文档把已删旧权威当上位（PROJECT_SPEC「根宪章是最高约束」、owner/ARCH「旧约束=ACTIVE_NORMATIVE」为权威倒置直陈；SCIENCE/CHANGE 仍以宪章 §18.1「四投影」为现行判据，冲突设计 §5.3 八投影）｜逐文件上位改指链上文档（三 Phase→§1.2、投影→§5.3、控制包→CONTROL_PACK_SPEC §4 现势路径）；自标 ARCHIVED 的快照不计不修｜docs 各活动子集｜对本条文件集执行五旧样式 `grep -rln` 输出为空｜第一波 GOV 轴 165+ 的本域逐文件清单（互补）；GAP-002 根面同族
**W2-DOC3-3**｜docs/governance/VERSION_NAMESPACES.md:18,55-59,77-85,99；docs/VERSIONING.md:5-16；docs/review 5 篇与 docs/owner 5 篇头部「目标产品 0.11.0-alpha.2（根 VERSION 唯一源）」｜ASTROCS_DESIGN §12+ENGINEERING_SPEC §7（Alpha 前程序与产物零版本；VERSION/CHANGELOG 仅内部助记不进发布）｜`python3 tools/check_l0_docs.py` → `DOC-002_L0_VIOLATION: REVIEW.md missing` rc=1（DOC-L0 注册于 ci/checks.json → 门禁红）；`grep -rl 0.11.0` 命中 owner×5/review×5/VERSION_NAMESPACES/VERSIONING/DOCUMENT_INDEX；被绑 REVIEW.md、CHANGELOG.md 已删（01db973b）｜P1｜ACTIVE_NORMATIVE 版本治理文档把版本注入 CLI/CMake/打包写成现行合同（§4 生成链整表），与零版本纪律相反；L0 检查器绑已删对象致注册门恒红｜VERSION_NAMESPACES/VERSIONING 按 §12 重写（版本=内部助记）或下链；DOC-L0 与 checks.json 注册项改绑在位对象（CI 线协同）｜docs 根、docs/governance、docs/review、docs/owner｜`python3 tools/check_l0_docs.py` rc=0 且 VERSION_NAMESPACES 不再把版本注入链列为现行合同｜GAP_AUDIT「注册检查绑旧治理文本（VERSION-CONSISTENCY --expected 0.11.0-alpha.2）」同源（注册表侧归 CI 线）
**W2-DOC3-4**｜docs/ci/01_CHECKS.md §2（29 行清单）/§5、03_GATES.md:5、CI_SPEC.md:53｜ENGINEERING_SPEC §8（ci/checks.json=唯一注册表）；ASTROCS_DESIGN §0（docs/ci=链上 rank⑤ 须可执行）｜python 比对实跑：§2 的 29 个 ID 中 **26 个不在 ci/checks.json（146 项）**，注册表 CHK-* 仅 2（CHK-MODULE-MANIFEST/CHK-ROOT-CLEAN）；§5 运行命令 `python3 ci/run_checks.py` 之文件仅 untracked 存在于工作树（`git ls-files ci/run_checks.py` 空）；`ci/exemptions.json`（§1/03_GATES/CI_SPEC 的豁免唯一登记处）磁盘 MISSING｜P1｜链上 CI 规范与唯一注册表非同一 ID 空间；运行入口与豁免登记处悬空→门禁「能红能绿」不可核｜收敛二选一：§2 重写为注册表实 ID（门禁级/正负例齐），或注册表迁 CHK-*；豁免登记处改在位路径并同步三处｜docs/ci（ci/ 注册表侧移交 CI 线）｜脚本比对「§2 ID 集 ⊆ checks.json ID 集」缺数=0 且豁免路径 `test -f` 通过｜GAP_AUDIT §「145/146 项、CHK-* 不属同一空间」同源（本条=docs 侧逐文件）
**W2-DOC3-5**｜docs/standards/STANDARDS_REGISTRY.md:3-9,32,64,242；docs/standards/RELEASE_STANDARD.md:3；docs/standards/DOCUMENTATION_STANDARD.md（CHANGELOG 义务）｜ASTROCS_DESIGN §5.3（首批冻结八投影 TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA）+§0+ENGINEERING_SPEC §7/§12｜grep 实跑：头注「上游权威: ASTROCS_PROJECT_CONSTITUTION.md §19/§7.3（冻结四投影）/§18」；:64「registry v1 恰四行」；`python3 docs/standards/checks/check_standards_registry.py` → `verdict=STANDARDS_REGISTRY_FAIL`（C3/C4 证据路径不存在×10：lib/gaia_xpsd_client/src/gaia_client.c、lib/healpix_db/healpix_drizzle/tests/*、lib/common/healpix/*、lib/astro_image_io/src/hips/*；C6 悬空 STD-F6）；RELEASE_STANDARD:3 令同步已删 CHANGELOG.md｜P1｜自称唯一冻结注册表 ACTIVE_NORMATIVE，权威依据被 §5.3/§0 替换且自家检查器红（该检查未注册 checks.json，红不入 CI=静默）｜上游改 §5.3+附录B；补 STG/MOL/CEA/ZEA 域行与在位证据锚；证据路径随新树刷新；删 CHANGELOG 同步义务｜docs/standards｜`python3 docs/standards/checks/check_standards_registry.py` verdict=STANDARDS_REGISTRY_PASS 且头注不含旧宪章｜SHARD_BRIEF §2「旧冻结注册表非链上权威」同源；与 DOC3-2 四投影残留互证
**W2-DOC3-6**｜docs/README-DOCS.md:6,9,12,16-20；docs/ARCHITECTURE.md:5,22,34,55,129,133；docs/API_REFERENCE.md:5,12-29；docs/TROUBLESHOOTING.md:1,11-16,27-33；docs/DEVELOPER_GUIDE.md:1,13-31,43；docs/KNOWN_LIMITATIONS.md:1-18；docs/RELEASE_STATUS.md:3-16｜ASTROCS_DESIGN §0（冲突以最高设计为准——「矛盾以 Wiki 为准」=权威倒置）、§6.1/§10.1、§11.3/§12（状态阶梯；未验收不得声明）；ENGINEERING_SPEC §8（活动文档无陈旧状态）｜命令面逐条实测 MISSING：CHANGELOG.md、docs/history/、工程控制/docs/18_CODE_CHANGE_MAP.md、docs/18_CODE_CHANGE_MAP.md、docs/TRACEABILITY_family.json、docs/algorithms/INTEGRATION.md、docs/algorithms/REJECTION.md、docs/science/HEALPIX_MAPPING.md、V19R8 三件套（30_WIKI_TO_CODE_QUALITY_V19R8_SPEC.md 等 3 文件）、lib/healpix_db/healpix_drizzle；ARCHITECTURE.md:129 称 TRACEABILITY.csv「76 行全 VERIFIED」而 `wc -l`=64；RELEASE_STATUS/KNOWN_LIMITATIONS 以 V19R8「94/95 DONE、PRE_RELEASE_ENGINEERING_FOUNDATION=PASS」现时口吻陈述｜P1｜L0/L3 入口层整体仍是 V19R8 世代：权威链叙述与 §0 相反、当前状态陈述与树不符、照抄命令面即败（GLOSSARY 锚点抽查全在位，不在本条）｜六文按新设计集重写或显式降 ARCHIVED 下链（并入 TASK_LIST WIKI-001/DOC-001 处置最省）｜docs 根｜`grep -rln` 检索「矛盾以 Wiki 为准|76 行|docs/history|工程控制/docs/18」于 docs/*.md 输出为空，且 README-DOCS/TROUBLESHOOTING 反引号路径存在性脚本 rc=0｜TASK_LIST「WIKI-001」「DOC-001」=本条执行任务；第一波 P0_RECHECK「历史状态冒充」族同源
**W2-DOC3-7**｜38 文件：docs/architecture 10 篇（BUILD_GRAPH/CACHE_POLICY/DATA_FLOW/DEPENDENCY_RULES/ERROR_MODEL/IO_AND_ATOMICITY/OWNERSHIP_AND_LIFETIME/PIPELINE/PHASE3_MODULE_ARCH/THREADING_MODEL）+MODULE_MAP；docs/modules 22 篇（astro_image_io/common/gaia_xpsd_client/healpix_drizzle/hips_p2/orchestrator/phase2_int/phase2_rej/phase2_samp/phase2_upm/phase3_fits/phase3_proj/phase3_rsmp/plate_solve/star_detector + registry 10 篇）；docs/api/PHASE1_API_V1.md；docs/development/TESTING.md；docs/TRACEABILITY.csv｜ENGINEERING_SPEC §7/§8；ASTROCS_DESIGN §7.1/§11.3｜存在性批量实测 MISSING：lib/astro_image_io、lib/common、lib/orchestrator、lib/drizzle、lib/hips、lib/healpix_db/healpix_drizzle、lib/phase2_int、lib/phase2_samp、lib/phase2_upm、lib/phase3_proj、lib/phase3_rsmp、include/star_detector.h、include/snr_estimator.h、lib/gaia_xpsd_client/src/gaia_client.c（find 全空）；而 ACTIVE/FROZEN 文档以现时口吻引用并挂 IMPLEMENTED 状态词（MODULE_MAP.md:42 astro_image_io「IMPLEMENTED」、:50 phase3_proj「恰四行」）｜P1｜架构/L5 文档把已删代码树写成现状；与新树 lib/algorithms+lib/infrastructure 及 docs/plugins 23 篇（rank⑥）断裂｜按 docs/plugins+在途 MODULE_MAP.yaml 重建/迁移 L5，旧文移 docs/archive 打 ARCHIVED；TRACEABILITY.csv 同批｜docs/modules、docs/architecture、docs/development、docs/api、docs 根 csv｜对列名目录执行 `grep -rlnE` 已删 lib 树正则输出为空（或命中文件已入 archive）｜第一波 ARCH/AIO 线旧树问题的文档侧投影；UNTRACKED docs/modules/MODULE_MAP.yaml 为其修复载体
**W2-DOC3-8**｜docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv、api_inventory.csv、execution_inventory.csv、production_call_paths_stage2.csv；docs/audit/doc_classification.csv、inventory.csv、risk_verification_T012.csv；docs/references/SCIENTIFIC_REFERENCES.md:63｜ENGINEERING_SPEC §7/§8（生成物不冒充现状、无悬空引用）｜python 悬空扫描实测：PRODUCTION_EXECUTION_INVENTORY.csv 42 条不存在路径、api_inventory.csv 9、execution_inventory.csv 4、stage2.csv 1、audit/inventory.csv 14、doc_classification.csv 15（docs/history/** 全不在）、SCIENTIFIC_REFERENCES 1 死链（docs/ImageWeighting/ImageWeighting.html）｜P2｜过期机器快照滞留活动目录冒充 current evidence；按表索址必败｜重生成并写 baseline SHA，或移 docs/archive；修死链｜docs/architecture、docs/audit、docs/references｜逐 csv 首列 path 存在性循环零 MISS（或已不在活动域）｜与 DOC3-7 同根；GAP-013 旧树并存的文档侧
**W2-DOC3-9**｜docs/api/CLI_PROTOCOL_V1.md:1,4,11-18；docs/architecture/ARCHITECTURE.md:1,8；docs/ci/04_ARTIFACTS.md:9-10,30｜ASTROCS_DESIGN §10.1/§6.2（唯一入口名 `ACSD Cli.exe`/`acsd_cli`）｜sed 实测 §10.1 表冻结入口名；CLI_PROTOCOL（CLI-001 刚订）命令树整段可执行名用 `astrocs`；头注「权威=控制包 04；两者冲突以 04 为准」而仓内无该控制包文档（工程控制/ 仅 PROJECT-GOVERNANCE-01、SCI-RES-01）；ARCHITECTURE.md 同「用户入口 astrocs CLI」；04_ARTIFACTS 产物名 astrocs-linux、`astrocs doctor`｜P1｜交付合同面命令前缀与发行名口径二义+对不在仓的外包文档设冲突优先权（§0 倒置残留）｜入口名改 acsd_cli/ACSD Cli.exe（或按 §6.2 别名规则明确并同步 help golden）；删「以 04 为准」改「上位=ASTROCS_DESIGN §6/§10」；04_ARTIFACTS 命名同步｜docs/api、docs/architecture、docs/ci｜`python3 tools/check_api_docs.py` rc=0 且 CLI_PROTOCOL 头注不含「以 04 为准」｜CLI-001（23f42ffa）在途线产物，命令前缀 (a) 或正被并行收敛——本轮按现状登记
**W2-DOC3-10**｜docs/validation/SCIENCE_FREEZE.md:3-14｜ASTROCS_DESIGN §11.3/§12（状态阶梯唯一口径；未复验不得声明 PASS/P0P1=0）｜head 实跑：文首现称「V17 Round0-6 clean-tree 终验完成、G1-G10 全 PASS、known P0/P1=0、FOUNDATION_FINAL_FREEZE=PASS」，无 ARCHIVED 标记、不在索引 archived 段；其对象（lib/phase3_proj 等）已删（DOC3-7 实测）｜P2｜旧世代冻结/全绿声明无标记滞留活动目录，易被当现状引用｜文首加历史快照声明或移 docs/archive；状态按 §11.3 复核后重登｜docs/validation｜`head -5` 含 ARCHIVED/历史声明即闭｜第一波 P0_RECHECK「历史状态冒充」族同源
**W2-DOC3-11**｜docs/development/CONFIG_SCHEMA.md:19-21｜docs/design/UNIFIED_MODEL.md §2 + docs/TRACEABILITY.csv:2（SCI-UPM-WEIGHT-001「禁止 star-SNR/support^p 乘因子」）+ docs/validation/v6/P0_GATE_FAMILY.md P0-04（退休 support×snr²/weight_mode=0/auto 不得成合法规格）；ASTROCS_DESIGN §4.3｜sed 实测：自称「单一事实来源」的 CONFIG_SCHEMA 仍列 `snr_weight_mode(snr2_normalized)`、rejection `auto`、`profile(wbpp_current alias|…)` 为合法取值｜P2（若判为迁移窗口法源可升）｜退役权重语义在「单一事实来源」文档中保留法源外观（R10 反向固化风险）｜标「仅 legacy lib/phase2 窗口，禁入新产品配置」或按 §3.3 config 新合同重写（CFG-001 域协同）｜docs/development + config｜`grep -n snr2_normalized docs/development/CONFIG_SCHEMA.md` 为空或同行带 legacy-only 标注｜v6 缺陷账本 R10/P0-04 族同源；迁移窗口归属待负责人裁决（冲突已在树内复现，故不判 UNVERIFIABLE）

> 在途注记（收工前实测）：并行线正在执行 lib 目录迁移（INT-001/ARCH-001 世代切换）——git status 索引内已 staged 大批 R 改名：lib/calibration→lib/algorithms/calibration、lib/snr_estimator→lib/algorithms/noise_snr、lib/photometric_calib→lib/algorithms/photometry、lib/healpix_db/healpix_drizzle→lib/algorithms/drizzle/healpix_drizzle、lib/astro_image_io→lib/infrastructure/aio、lib/orchestrator→lib/infrastructure/pipeline/orchestrator、lib/backend_host→lib/infrastructure/benchmark/backend_host、lib/gaia_xpsd_client→lib/infrastructure/gaia_xpsd_client、lib/healpix_db/healpix_browser_qt→lib/infrastructure/hips_browser/healpix_browser_qt 等。本域发现（DOC3-2/5/7/8/11 的路径证据）据此定性为「旧路径引用已随树迁移而失配」：无论迁移提交落地前后，活动文档均需改指新落位，验收门（grep 旧路径为空）不变。

### 活动面旧权威/已删路径引用 逐文件清零计数（W2-DOC3-2 附表）

口径：旧宪章（ASTROCS_PROJECT_CONSTITUTION / 「宪章§」）、旧约束（AstroCS_ENGINEERING_CONSTRAINTS）、已删任务书（tasks/NN_）、已删控制包（工程控制/AstroCS_*）、engineering/control、工程控制/ACTIVITY_STATE.md。archive/standards/traceability/contracts/quality 本体按任务口径除外；自标 ARCHIVED 快照不计。

DOCUMENT_INDEX.yaml=17（6 类 dangling+工程控制2+ACTIVITY_STATE3+engineering/control6）｜owner/PROJECT_SPEC=2｜owner/ARCHITECTURE_OVERVIEW=2｜owner/PIPELINE_OVERVIEW=3｜owner/RELEASE_STATUS=2｜owner/SCIENCE_OVERVIEW=2｜owner/CHANGE_REVIEW=1｜review/ARCHITECTURE=1｜review/PIPELINE=1｜review/SCIENCE=1｜design/PHASE1_DETAILED=1｜architecture/MODULE_MAP=1｜architecture/abi/ABI_003=1｜architecture/cpu/CPU_001=1｜architecture/cpu/CPU_003=1｜architecture/observability STRUCTURED=1、RUN_GRAPH=2、RESOURCE=2｜architecture/execution_options_contract=1｜interfaces/data DATA-002=2、DATA-003=2、DATA-004=3｜interfaces/io IO_001=2、IO_002=2、IO_003=2｜api/MANIFEST_VERIFY=1（api/CLI_PROTOCOL=2 另计 DOC3-9）｜validation/v6 QA_MATRIX=3、ORACLE_POLICY=1｜governance/VERSION_NAMESPACES=2（engineering/control，主计 DOC3-3）｜**合计 27 文件 45 处**（另 standards 本体 8+21 处计入 DOC3-5，不计活动面；docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md 2 处系自标 ARCHIVED 快照，不计）。

## ② 覆盖清单

（每行 ``相对路径<TAB>VERDICT``；223 个 tracked 文件逐一列出；OK=已读无发现（小文件批量过+旧权威/悬空路径/状态词三门 grep 实测）；同型问题归并，FINDING 复用同一 ID。）

docs/API_REFERENCE.md	FINDING:W2-DOC3-6
docs/ARCHITECTURE.md	FINDING:W2-DOC3-6
docs/DEVELOPER_GUIDE.md	FINDING:W2-DOC3-6
docs/DOCUMENT_INDEX.yaml	FINDING:W2-DOC3-1
docs/GLOSSARY.md	OK
docs/KNOWN_LIMITATIONS.md	FINDING:W2-DOC3-6
docs/README-DOCS.md	FINDING:W2-DOC3-6
docs/RELEASE_STATUS.md	FINDING:W2-DOC3-6
docs/TRACEABILITY.csv	FINDING:W2-DOC3-7
docs/TROUBLESHOOTING.md	FINDING:W2-DOC3-6
docs/VERSIONING.md	FINDING:W2-DOC3-3
docs/api/CLI_PROTOCOL_V1.md	FINDING:W2-DOC3-9
docs/api/COMMON_ABI_V1.md	OK
docs/api/MANIFEST_VERIFY_V1.md	FINDING:W2-DOC3-2
docs/api/PHASE1_API_V1.md	FINDING:W2-DOC3-7
docs/api/PHASE2_API_V1.md	OK
docs/api/PHASE3_API_V1.md	OK
docs/architecture/ARCHITECTURE.md	FINDING:W2-DOC3-9
docs/architecture/ASYNC_IO_CONTRACT.md	OK
docs/architecture/BUILD_GRAPH.md	FINDING:W2-DOC3-7
docs/architecture/CACHE_POLICY.md	FINDING:W2-DOC3-7
docs/architecture/COMPATIBILITY_POLICY.md	OK
docs/architecture/CPU_BACKEND_ARCH.md	OK
docs/architecture/DATA_FLOW.md	FINDING:W2-DOC3-7
docs/architecture/DEPENDENCY_RULES.md	FINDING:W2-DOC3-7
docs/architecture/ERROR_MODEL.md	FINDING:W2-DOC3-7
docs/architecture/EXECUTION_MODEL.md	OK
docs/architecture/IO_AND_ATOMICITY.md	FINDING:W2-DOC3-7
docs/architecture/ISA_BIT_MANIP_VARIANTS.md	OK
docs/architecture/ISA_VARIANTS.md	OK
docs/architecture/MODULE_MAP.md	FINDING:W2-DOC3-2
docs/architecture/OWNERSHIP_AND_LIFETIME.md	FINDING:W2-DOC3-7
docs/architecture/PERFORMANCE_MODEL.md	OK
docs/architecture/PHASE3_MODULE_ARCH.md	FINDING:W2-DOC3-7
docs/architecture/PIPELINE.md	FINDING:W2-DOC3-7
docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv	FINDING:W2-DOC3-8
docs/architecture/THREADING_MODEL.md	FINDING:W2-DOC3-7
docs/architecture/THREAD_BUDGET_ARCH.md	OK
docs/architecture/abi/ABI_003_SECURE_LOADER.md	FINDING:W2-DOC3-2
docs/architecture/api_inventory.csv	FINDING:W2-DOC3-8
docs/architecture/cpu/CPU_001_CAPABILITY_PROBE.md	FINDING:W2-DOC3-2
docs/architecture/cpu/CPU_003_AVX2_PROVIDER.md	FINDING:W2-DOC3-2
docs/architecture/execution_inventory.csv	FINDING:W2-DOC3-8
docs/architecture/execution_options_contract.md	FINDING:W2-DOC3-2
docs/architecture/observability/RESOURCE_MONITORING_CONTRACT.md	FINDING:W2-DOC3-2
docs/architecture/observability/RUN_GRAPH_CONTRACT.md	FINDING:W2-DOC3-2
docs/architecture/observability/STRUCTURED_LOGGING_CONTRACT.md	FINDING:W2-DOC3-2
docs/architecture/production_call_paths_stage1.csv	OK
docs/architecture/production_call_paths_stage2.csv	FINDING:W2-DOC3-8
docs/archive/PRE_RELEASE_EVIDENCE_INDEX.md	NA:archived
docs/archive/history/README.md	NA:archived
docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md	NA:archived
docs/archive/history/v19/ARCHITECTURE.md	NA:archived
docs/archive/history/v19/BUILD_RELEASE.md	NA:archived
docs/archive/history/v19/CONFIG_REFERENCE.md	NA:archived
docs/archive/history/v19/DATA_CONTRACTS.md	NA:archived
docs/archive/history/v19/DIAGNOSTICS.md	NA:archived
docs/archive/history/v19/DRIZZLE.md	NA:archived
docs/archive/history/v19/ERROR_TAXONOMY.md	NA:archived
docs/archive/history/v19/MODULE_INDEX.md	NA:archived
docs/archive/history/v19/PERFORMANCE.md	NA:archived
docs/archive/history/v19/PHASE2.md	NA:archived
docs/archive/history/v19/README.md	NA:archived
docs/archive/history/v19/SCIENTIFIC_ALGORITHMS.md	NA:archived
docs/archive/history/v19/SNR_NOISE_MODEL.md	NA:archived
docs/archive/history/v19/TESTING_AND_ACCEPTANCE.md	NA:archived
docs/archive/phase2_PRODUCTION_WIRING.md	NA:archived
docs/archive/refactor/CLI_COMMAND_LAYER.md	NA:archived
docs/archive/refactor/CLI_RUN_PRESET.md	NA:archived
docs/archive/refactor/P1_SYMBOL_MAP.md	NA:archived
docs/archive/refactor/P2_SYMBOL_MAP.md	NA:archived
docs/archive/refactor/P3_STATUS.md	NA:archived
docs/archive/review/ARCHITECTURE_OVERVIEW.md	NA:archived
docs/archive/review/CHANGE_REVIEW.md	NA:archived
docs/archive/review/PIPELINE_OVERVIEW.md	NA:archived
docs/archive/review/RELEASE_STATUS.md	NA:archived
docs/archive/review/SCIENCE_OVERVIEW.md	NA:archived
docs/audit/doc_classification.csv	FINDING:W2-DOC3-8
docs/audit/inventory.csv	FINDING:W2-DOC3-8
docs/audit/risk_verification_T012.csv	FINDING:W2-DOC3-8
docs/backlog/NEXT_STAGE.md	OK
docs/browser/HIPS_BROWSER.md	OK
docs/ci/01_CHECKS.md	FINDING:W2-DOC3-4
docs/ci/02_PIPELINE.md	OK
docs/ci/03_GATES.md	FINDING:W2-DOC3-4
docs/ci/04_ARTIFACTS.md	FINDING:W2-DOC3-9
docs/ci/CI_SPEC.md	FINDING:W2-DOC3-4
docs/contracts/API-001.md	OK
docs/contracts/API_CONTRACTS.csv	OK
docs/contracts/ARCH-001.md	OK
docs/contracts/DATA_ARTIFACTS.md	OK
docs/contracts/DATA_SEMANTICS.md	OK
docs/contracts/INDEX.yaml	OK
docs/contracts/PUBLIC_API.md	OK
docs/contracts/RT-001.md	OK
docs/contracts/TEST_MATRIX.md	OK
docs/contracts/UNIFIED_OBJECTS.md	OK
docs/contracts/unified_object_registry.json	OK
docs/contracts/v6/W6_SCHEMA_INTEGRATION.md	OK
docs/contracts/v6/data/00_README.md	OK
docs/contracts/v6/data/01_units_and_bunit.md	OK
docs/contracts/v6/data/02_signal.md	OK
docs/contracts/v6/data/03_covariance.md	OK
docs/contracts/v6/data/04_psf_and_effective_psf.md	OK
docs/contracts/v6/data/05_point_information_and_weight_mode.md	OK
docs/contracts/v6/data/06_psfsw.md	OK
docs/contracts/v6/data/07_provenance.md	OK
docs/contracts/v6/data/08_phase3.md	OK
docs/contracts/v6/data/09_verification.md	OK
docs/contracts/v6/data/10_migration_and_open_items.md	OK
docs/contracts/v6/frozen/00_README.md	OK
docs/contracts/v6/frozen/01_DATA_CONTRACT_FREEZE.md	OK
docs/contracts/v6/frozen/02_WEIGHT_MODE_VOCABULARY.md	OK
docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json	NA:generated
docs/design/PHASE1_DETAILED_DESIGN.md	FINDING:W2-DOC3-2
docs/design/PHASE2_DETAILED_DESIGN.md	OK
docs/design/PHASE3_DETAILED_DESIGN.md	OK
docs/design/UNIFIED_MODEL.md	OK
docs/development/CODE_STYLE.md	OK
docs/development/CONFIG_SCHEMA.md	FINDING:W2-DOC3-11
docs/development/TESTING.md	FINDING:W2-DOC3-7
docs/diagnostics/TROUBLESHOOTING.md	OK
docs/governance/VERSION_NAMESPACES.md	FINDING:W2-DOC3-3
docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md	FINDING:W2-DOC3-2
docs/interfaces/data/DATA-003_PRODUCTION_ARTIFACT_STORE.md	FINDING:W2-DOC3-2
docs/interfaces/data/DATA-004_PRODUCT_PROVENANCE.md	FINDING:W2-DOC3-2
docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md	FINDING:W2-DOC3-2
docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md	FINDING:W2-DOC3-2
docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md	FINDING:W2-DOC3-2
docs/modules/MODULE_MAP.yaml	OK
docs/modules/acr.md	OK
docs/modules/astro_image_io.md	FINDING:W2-DOC3-7
docs/modules/calibration.md	OK
docs/modules/common.md	FINDING:W2-DOC3-7
docs/modules/dynamic_psf.md	OK
docs/modules/gaia_xpsd_client.md	FINDING:W2-DOC3-7
docs/modules/healpix_browser_qt.md	OK
docs/modules/healpix_drizzle.md	FINDING:W2-DOC3-7
docs/modules/hips_p2.md	FINDING:W2-DOC3-7
docs/modules/orchestrator.md	FINDING:W2-DOC3-7
docs/modules/phase1_session.md	OK
docs/modules/phase2.md	OK
docs/modules/phase2_int.md	FINDING:W2-DOC3-7
docs/modules/phase2_rej.md	FINDING:W2-DOC3-7
docs/modules/phase2_samp.md	FINDING:W2-DOC3-7
docs/modules/phase2_upm.md	FINDING:W2-DOC3-7
docs/modules/phase3_fits.md	FINDING:W2-DOC3-7
docs/modules/phase3_proj.md	FINDING:W2-DOC3-7
docs/modules/phase3_rsmp.md	FINDING:W2-DOC3-7
docs/modules/photometric_calib.md	OK
docs/modules/plate_solve.md	FINDING:W2-DOC3-7
docs/modules/registry/astrocs.phase1.calibration.md	OK
docs/modules/registry/astrocs.phase1.cosmetic.md	OK
docs/modules/registry/astrocs.phase1.drizzle.md	FINDING:W2-DOC3-7
docs/modules/registry/astrocs.phase1.hips-writer.md	FINDING:W2-DOC3-7
docs/modules/registry/astrocs.phase1.noise-snr.md	OK
docs/modules/registry/astrocs.phase1.photometry.md	OK
docs/modules/registry/astrocs.phase1.session.md	OK
docs/modules/registry/astrocs.phase1.star-detection.md	OK
docs/modules/registry/astrocs.phase1.star-psf.md	OK
docs/modules/registry/astrocs.phase1.wcs-platesolve.md	OK
docs/modules/registry/astrocs.phase1.writer.md	OK
docs/modules/registry/astrocs.phase2.coverage.md	OK
docs/modules/registry/astrocs.phase2.integrate.md	FINDING:W2-DOC3-7
docs/modules/registry/astrocs.phase2.reject.md	FINDING:W2-DOC3-7
docs/modules/registry/astrocs.phase2.resample.md	OK
docs/modules/registry/astrocs.phase2.sample.md	FINDING:W2-DOC3-7
docs/modules/registry/astrocs.phase2.session.md	OK
docs/modules/registry/astrocs.phase2.upm-apply.md	FINDING:W2-DOC3-7
docs/modules/registry/astrocs.phase2.upm-fit.md	FINDING:W2-DOC3-7
docs/modules/registry/astrocs.phase2.write.md	OK
docs/modules/registry/astrocs.phase3.properties.md	OK
docs/modules/registry/astrocs.phase3.resample.md	OK
docs/modules/registry/astrocs.phase3.resample2.md	FINDING:W2-DOC3-7
docs/modules/registry/astrocs.phase3.verify.md	OK
docs/modules/registry/astrocs.phase3.wcs.md	FINDING:W2-DOC3-7
docs/modules/registry/astrocs.phase3.writer.md	FINDING:W2-DOC3-7
docs/modules/snr_estimator.md	OK
docs/modules/star_detector.md	FINDING:W2-DOC3-7
docs/operations/TOOLCHAIN_AGENT_HOST.md	NA:generated
docs/owner/ARCHITECTURE_OVERVIEW.md	FINDING:W2-DOC3-2
docs/owner/CHANGE_REVIEW.md	FINDING:W2-DOC3-2
docs/owner/PIPELINE_OVERVIEW.md	FINDING:W2-DOC3-2
docs/owner/PROJECT_SPEC.md	FINDING:W2-DOC3-2
docs/owner/RELEASE_STATUS.md	FINDING:W2-DOC3-2
docs/owner/SCIENCE_OVERVIEW.md	FINDING:W2-DOC3-2
docs/performance/BASELINE.md	OK
docs/performance/OPTIMIZATION.md	OK
docs/quality/complexity_baseline_v1.md	OK
docs/quality/coverage_baseline_v1.md	OK
docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md	NA:archived
docs/references/SCIENTIFIC_REFERENCES.md	FINDING:W2-DOC3-8
docs/review/ARCHITECTURE_OVERVIEW.md	FINDING:W2-DOC3-2
docs/review/CHANGE_REVIEW.md	FINDING:W2-DOC3-3
docs/review/PIPELINE_OVERVIEW.md	FINDING:W2-DOC3-2
docs/review/RELEASE_STATUS.md	FINDING:W2-DOC3-3
docs/review/SCIENCE_OVERVIEW.md	FINDING:W2-DOC3-2
docs/standards/API_STANDARD.md	OK
docs/standards/BENCHMARK_STANDARD.md	OK
docs/standards/CODE_STANDARD.md	OK
docs/standards/COMMENT_STANDARD.md	OK
docs/standards/CONCURRENCY_STANDARD.md	OK
docs/standards/C_ABI_STANDARD.md	OK
docs/standards/DOCUMENTATION_STANDARD.md	FINDING:W2-DOC3-5
docs/standards/ERROR_HANDLING_STANDARD.md	OK
docs/standards/IO_STANDARD.md	OK
docs/standards/LOGGING_DIAGNOSTICS_STANDARD.md	OK
docs/standards/NUMERIC_STANDARD.md	OK
docs/standards/RELEASE_STANDARD.md	FINDING:W2-DOC3-5
docs/standards/STANDARDS_REGISTRY.md	FINDING:W2-DOC3-5
docs/standards/TEST_STANDARD.md	OK
docs/standards/checks/check_standards_registry.py	OK
docs/traceability/TRACEABILITY_LAYERS.csv	OK
docs/traceability/TRACEABILITY_MATRIX.csv	OK
docs/traceability/TRACEABILITY_MATRIX.json	OK
docs/traceability/TRACEABILITY_SPEC.md	OK
docs/validation/SCIENCE_FREEZE.md	FINDING:W2-DOC3-10
docs/validation/v6/BASELINE_COMPARISON_MATRIX.md	OK
docs/validation/v6/NEGATIVE_MUTATION_CATALOG.md	OK
docs/validation/v6/ORACLE_AND_ZERO_CASE_POLICY.md	FINDING:W2-DOC3-2
docs/validation/v6/P0_GATE_FAMILY.md	OK
docs/validation/v6/QA_MATRIX.md	FINDING:W2-DOC3-2
docs/validation/v6/README.md	OK

### 附：域内 UNTRACKED 新文件（不计入 files_total）

docs/contracts/CONFIG_CONTRACT.md	OK (UNTRACKED)
docs/contracts/config_separation_anchors.json	OK (UNTRACKED)

### 统计
files_total=223 (tracked；UNTRACKED 另附 2 条不计入)
verdict_counts: OK=96 FINDING=96 NA=31 明细: FINDING:W2-DOC3-1=1 FINDING:W2-DOC3-10=1 FINDING:W2-DOC3-11=1 FINDING:W2-DOC3-2=27 FINDING:W2-DOC3-3=4 FINDING:W2-DOC3-4=3 FINDING:W2-DOC3-5=3 FINDING:W2-DOC3-6=7 FINDING:W2-DOC3-7=38 FINDING:W2-DOC3-8=8 FINDING:W2-DOC3-9=3 NA:archived=29 NA:generated=2 OK=96
枚举命令逐字: git ls-files docs | grep -v '^docs/\(science\|algorithms\|plugins\)/' | sort　＋　UNTRACKED 补充逐字: git status --porcelain docs | grep '^??'
