# W2-DATA — 第二波·文件级审计：contracts/** + testdata/** + packaging/** + cmake/**

- 分片：W2-DATA（穷尽扫描；四态口径与旧→新权威映射按 `reports/PROJECT-GOVERNANCE-01/root-scan/_tools/SHARD_BRIEF.md` §1/§2）
- 开工 SHA：HEAD=main=`180c8a0a`（origin/main=`b4afc135` 不等；按指令等待≥2 分钟复测一次仍不等，以 HEAD=main 相等为准开工并注明）
- 收工 SHA：证据采集与覆盖清单生成于 HEAD=main=origin/main=`32a5f5f300700b817cfa1ceafe047acaeff4ee9b`（三 SHA 曾一度全等）；本报告落盘时 HEAD=main=`181b4496`、origin/main=`32a5f5f3`（并行线继续提交，扫描期间 HEAD 前进 4 次：678fd51a DATA-001 统一对象合同入库、20d86b79 MOD-001、32a5f5f3 GAP-034、181b4496）；全部发现证据在 32a5f5f3 树复跑成立，且 1–4 号发现所在文件在本次前进中未被改写（git log 范围外）
- 文件域计数：tracked 123（开工点 87 + 扫描中合入 36）+ UNTRACKED 4 = 127，覆盖率 100%；日志 `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/audit/w2_DATA.log`
- 发现 10 条：P0×1、P1×5、P2×4。正面结论（不立条）：域内 112 个 JSON 全部可解析；v6 生产/提案 schema×示例 16/16 PASS；unified 16 例 PASS、4 负例全按 EXPECTED 被拒；testdata/index.json 8/8 数据集帧计数与磁盘一致、27/27 校准母版在位、断言字段逐项命中；`cmake/toolchain/verify_toolchain.py` → TOOLCHAIN_CONTRACT_PASS。

## 一、发现表

| ID | 定位 path:line | 违反的最新权威条款 | 当前证据（命令+本轮真跑输出≤3行） | 严重度 | 影响 | 整改建议 | 建议文件域 | 验收门（单命令） | 同源标注 |
|---|---|---|---|---|---|---|---|---|---|
| W2-DATA-1 | cmake/astrocs.product.windows.json.in:12-13（对照 packaging/astrocs.product.json:12-13） | ASTROCS_DESIGN §12「未实现/未验收必须明确报告，不得用…文档声明冒充完成」+§11.3 状态阶梯唯一口径；AGENTS §5 禁 facade/空骨架冒充 | 命令：python3 逐 unit 比较两 manifest；输出：linux{PLATFORM-RUNTIME,PLATFORM-IO}=SKELETON vs win 模板=IMPLEMENTED（units_diff 2 项）；linux note 明言「不伪装实现完成, 维持 SKELETON」；install_layout.cmake:131-137 由该模板 configure_file 生成并安装 Windows 交付 manifest | P0 | Windows 候选出厂即虚标平台 DLL 完成度，验收判定依据失真；SKELETON/IMPLEMENTED 互斥面被击穿 | 模板两 unit status 改 SKELETON（或按 §11.3 平台实测后统一升格）；「双平台 status 集合一致」并入 CHK-PACKAGE | cmake/** packaging/** | 见日志§[9] 的 python3 比对脚本（比较两文件 SKELETON unit_id 集合）→ 输出 True | 第一波 PKG-2 同源（P0，本轮独立复现于新树）；归 PKG-001 |
| W2-DATA-2 | packaging/astrocs.product.json:3；packaging/schemas/astrocs-product.schema.json:12；packaging/install-tree.contract.json:5-6；packaging/schemas/install-tree-contract.schema.json:14；contracts/config/module_dll_contract.schema.json(target_version const)；contracts/schemas/version.schema.json 全文 | ASTROCS_DESIGN §12：Alpha 前程序与代码不含任何版本信息；Alpha 时 --version=0.1alpha；ENGINEERING_SPEC §7 同款 | 命令：grep -rn 0.11.0-alpha packaging cmake contracts；输出：alpha.1 阵营 8 文件 vs alpha.2 阵营 3 文件（compat_map 17 处）；根 VERSION=0.11.0-alpha.2 → .in 注入 @ASTROCS_BASE_VERSION@ 后必然违反 astrocs-product.schema pattern；该 schema 零校验消费者 | P1 | 版本信息进入交付合同链且两派号互斥；「生成物永远过不了自家 schema、schema 又无人执行」＝假门双缺陷 | 按 §12 移除交付合同具体版本号（或负责人裁决走 §0 变更流程）；若保留则单源生成并入 CHK-PACKAGE | packaging/** cmake/** contracts/config/** contracts/schemas/** | grep -rE "0\.11\.0-alpha" packaging cmake contracts \| wc -l → 0 | GAP-017 同源；第一波 PKG-4/PKG-5 同域不删条；归 GOV-001+PKG-001 |
| W2-DATA-3 | packaging/install-tree.contract.json:11；cmake/install_layout.cmake:14,34-38（全文无 config 安装规则）；astrocs.product.windows.json.in:11 | ASTROCS_DESIGN §10.1：唯一可执行 ACSD Cli.exe/acsd_cli，交付=exe+各 dll+schemas+manifest；§3.3：程序根 config/filters.json+defaults.json 必备 | 命令：grep -rni "acsd_cli\|ACSD Cli" packaging cmake contracts；输出：0 命中；install-tree-contract.schema.json:22 install_path 正则 ^(astrocs 锁死旧名；contract 无 CONFIG-* 单元 | P1 | 交付命名合同与最高设计相反；安装树缺全局配置 → 装完不可运行（滤镜匹配/容差无从执行） | 统一命名到 §10.1（或裁决改名）；新增 CONFIG-{DEFAULTS,FILTERS} 安装单元并入白名单与校验 | cmake/** packaging/** | grep -c "acsd" packaging/install-tree.contract.json cmake/install_layout.cmake → ≥2 | GAP-018 同源；第一波 PKG-3 同域不删条；归 PKG-001+CFG-001 |
| W2-DATA-4 | contracts/data/artifact_manifest.schema.json:required(15 项)；contracts/schemas/run_manifest.schema.json(未入库)；contracts/** 无 run-plan/run-graph/run-trace/resource-timeseries/resource-summary/run-summary schema | ASTROCS_DESIGN §9：每次运行至少生成七件记录；「manifest 至少记录：产品类型/schema 版本/软件来源/run ID/输入产品标识/科学配置/单位/坐标 frame/像素·采样语义/算法 ID/模块 build ID/实际 provider/生成时间」 | 命令：python 扫描 contracts 文件名+内容；输出：6 件 NOT_FOUND；artifact_manifest raw.count("units")=0 且 additionalProperties:false；units/coordinate/pixel_semantics/sampling/algorithm_ids/provider 仅存于 v6 provenance schema 而 manifest 无 provenance 挂接 | P1 | 「唯一事实源」对 §9 名录 7 缺 6；已入库 manifest 合同结构性缺 5 类科学追溯字段（evidence/字段缺列典型例） | 补 6 schema（run_manifest 随 CFG-001 入库）；artifact_manifest 增 provenance_ref 必需字段或直接落 §9 字段 | contracts/** | ls contracts/schemas/run_plan.schema.json contracts/schemas/run_graph.schema.json contracts/schemas/run_summary.schema.json → rc=0 | 第一波 AIO-4（§9 生产者面）同源、GAP-007 同域；归 AIO-001+DATA-001 |
| W2-DATA-5 | contracts/schemas/traceability_matrix.schema.json:22-50（有 src_path/test_path、无 evidence_path；authoring_task/owner const 锁 DOC-001/SA-QA-29） | ASTROCS_DESIGN §11.3 状态阶梯（VERIFIED=可核事实）+§12；ENGINEERING_SPEC §8 检查器覆盖追溯 | 命令：grep -c evidence_path contracts/schemas/traceability_matrix.schema.json → 0；python 解析 docs/traceability/TRACEABILITY_MATRIX.json → evidence_status VERIFIED:6/MISSING:24（30 模块）；checker:66 EVID 层锚=None、:241-250 C7 仅解析 src/test 路径 | P1 | EVIDENCE 层 VERIFIED 无路径可解析＝追溯门对最强声明零校验；authoring const 使矩阵不可跨任务复用 | schema 增 evidence_path（VERIFIED 必填、存在且 tracked），checker EVID 锚改该列；authoring_* 放宽为字符串 | contracts/schemas/** tools/traceability/** | grep -c evidence_path contracts/schemas/traceability_matrix.schema.json → ≥2 | 旧账本 M6b-G-001（问题扫描 G_GOV_GATE/p0，当时 3/27）同源，本轮复现 6/24；归 GOV-001+CI-001 |
| W2-DATA-6 | packaging/schemas/dependency-lock.schema.json | ENGINEERING_SPEC §8 每项检查能红能绿；DESIGN §12 发布包 provenance 通过 | 命令：grep -rn dependency-lock.schema tools/ ci/ cmake/ scripts/ packaging/gen_sbom_input.py；输出：0 命中（gen_sbom 只做语义比对不应用 schema） | P2 | schema 无机器消费者＝注册表校验缺位（本域其余 schema 均已对账有消费者） | gen_sbom_input.py 或 CHK-PACKAGE 里用 jsonschema 校验 dependency-lock.json | packaging/** ci/** | grep -c "dependency-lock.schema" packaging/gen_sbom_input.py → ≥1 | 新发现；归 PKG-001 |
| W2-DATA-7 | contracts/data/aio_abi_contract_v1.json(implementations.path)；contracts/data/artifact_types.registry.json(doc_ref)；contracts/data/examples/v6/provenance.example.json 与 proposals 同件(k_corr.calibration.script)；packaging/licenses/LICENSE-INDEX.txt(来源行) | ENGINEERING_SPEC §7 run/ 不入库、§8 删除/重命名无悬空引用；DESIGN §9 来源链；SHARD_BRIEF §2（aio 迁移后旧路径） | 命令：python 解析 contracts 全部字符串路径逐条 exists；输出：broken=aio→lib/astro_image_io/src/aio_abi.cpp（目录不存在）、registry→docs/interfaces/data/DATA-001_ARTIFACT_CONTRACT.md（不存在）、examples provenance→run/v6/upm/kcorr_calib.py（gitignored 且不存在）；LICENSE-INDEX 同源 lib/astro_image_io | P1 | 类型登记表「语义与 content 见…§3」不可达；合同示例把校准可复现性寄托 gitignored run/ → 干净树不可复算 | 全部重锚现存路径；kcorr 标定脚本入 tests/oracle 或 tools/；DATA-001 文档补回或改指现行权威 | contracts/** packaging/** | 复跑日志§[8] 死引用扫描 → broken 列表为空 | GAP-002/GAP-007 同域；归 DATA-001+DOC-001 |
| W2-DATA-8 | 域内 34 命中文件（-l 清单见日志§[F8]），代表：contracts/schemas/v6/** 与 proposals/**「宪章 §4.3/§6.3/§18.3」、contracts/schemas/unified/provenance.schema.json:宪章§18.3（DATA-001 新入库仍引）、aio_abi/phase2_uncertainty constitution_ref、install-tree-contract.schema/preset-contract/windows README 的 03_TARGET、09_WINDOWS_TOOLCHAIN_LOCK、13_DATA_PIPELINE 等 | SHARD_BRIEF §2：ASTROCS_PROJECT_CONSTITUTION/旧编号文档不在最新权威链（替代：DESIGN/ENGINEERING_SPEC/UNIFIED_MODEL 对应节）；DESIGN §0 | 命令：grep -rlE "ASTROCS-CONSTITUTION-001\|宪章 §\|03_TARGET\|09_WINDOWS" contracts packaging cmake；输出：34 文件；ls docs/design/03_TARGET_PRODUCT_AND_ARCHITECTURE.md → 不存在（09 同） | P2 | 活动合同仍把旧宪章/已删编号文档挂为判据（内容多与现行权威重合，故 P2 但机器读者按 §2 映射会落空） | 逐处改挂最新权威节号；DATA-001 新文件同步订正 | contracts/** packaging/** cmake/** | 复跑同一 grep → 0 文件 | GAP-002/GAP-019 同域；第一波 DOC 族同型；归 DOC-001 |
| W2-DATA-9 | 65 文件（覆盖清单 FINDING:W2-DATA-9 之 14 个为主批，另与 -8 交叠）；典型 jsonl_event_v1.schema.json:4、cpu_profile:4、hardware_inspect:4、v6×10 description 前缀、cfitsio_sources.cmake:2、qa_coverage_report.sh:2、win32_pthread_shim/*.h:2、version/compat $id 风格与缺 $id | ENGINEERING_SPEC §2 注释禁令（任务编号/审计流水/历史版本号），DESIGN §12（助记符不入代码与产物） | 命令：grep -rlE "BENCH-00\|SCHEMA-INTEGRATE-001\|CLI-00\|QA-00\|WIN-00\|BLD-00\|DOC-001" contracts packaging cmake；输出：38 文件；另 $id 四风格并存（.invalid×2、astrocs://、astrocs:、缺省×1） | P2 | 合同元数据被审计流水污染；$id 命名域分裂不利稳定引用（同型批量归并一条） | 清理 title/description/头注中的任务号与旧编号节号；$id 统一一种 scheme 并补 version.schema 的 $id | contracts/** cmake/** packaging/** | grep -rcE "(BENCH|SCHEMA-INTEGRATE|QA-0|WIN-0|BLD-0|DOC-001)" contracts/schemas/*.json contracts/config/*.json 的 title/description 字段 → 0 | 第一波 I_DOC_HYGIENE 同族；归 DOC-001 |
| W2-DATA-10 | contracts/schemas/phase_config_{normalize,mosaic,export}.schema.json（工作树存在、HEAD 无；run_manifest 归 W2-DATA-4） | ASTROCS_DESIGN §3.3/§6.1（三命令 JSON 配置合同 + --template 是交付合同本体）；ENGINEERING_SPEC §4 合同先行 | 命令：git ls-files -o --exclude-standard contracts；输出：phase_config_export/mosaic/normalize 3 行；tracked 代码域 grep 引用 → 0（消费者 tests/config 同为未跟踪，WIP 自洽） | P2 | 「唯一事实源」暂不可自干净 clone 复现；系 CFG-001 并行线预存集合（未入库前非已提交面破口） | CFG-001 线随其 tests/config 原子提交；BASE-001 登记边界 | contracts/** | git ls-files contracts/schemas \| grep -c "phase_config\|run_manifest" → 4 | 新发现（预存登记性质）；归 CFG-001+BASE-001 |

## 二、覆盖清单（机器可解析：每行 `<相对路径>\t<VERDICT>`，untracked 行追加 `\tUNTRACKED`；127/127）

```
cmake/astrocs.product.windows.json.in	FINDING:W2-DATA-1
cmake/cfitsio_sources.cmake	FINDING:W2-DATA-9
cmake/install_layout.cmake	FINDING:W2-DATA-3
cmake/qa_coverage_report.sh	FINDING:W2-DATA-9
cmake/toolchain/verify_toolchain.py	FINDING:W2-DATA-8
cmake/win32_pthread_shim/pthread.h	FINDING:W2-DATA-9
cmake/win32_pthread_shim/unistd.h	FINDING:W2-DATA-9
contracts/config/cli_modules_list.schema.json	FINDING:W2-DATA-9
contracts/config/cli_selftest.schema.json	FINDING:W2-DATA-9
contracts/config/module_dll_contract.schema.json	FINDING:W2-DATA-2
contracts/config/module_lifecycle_contract.schema.json	FINDING:W2-DATA-2
contracts/data/aio_abi_contract_v1.json	FINDING:W2-DATA-7
contracts/data/artifact_manifest.schema.json	FINDING:W2-DATA-4
contracts/data/artifact_types.registry.json	FINDING:W2-DATA-7
contracts/data/examples/external_fixture_hips.example.json	OK
contracts/data/examples/frame_hips_manifest.example.json	OK
contracts/data/examples/phase1_product_v1.example.json	OK
contracts/data/examples/phase2_mosaic_v1.example.json	OK
contracts/data/examples/phase3_planar_fits_v1.example.json	OK
contracts/data/examples/v6/covariance.example.json	OK
contracts/data/examples/v6/effective-psf.example.json	OK
contracts/data/examples/v6/phase3.example.json	OK
contracts/data/examples/v6/point-information.example.json	OK
contracts/data/examples/v6/provenance.example.json	FINDING:W2-DATA-7
contracts/data/examples/v6/psf.example.json	OK
contracts/data/examples/v6/psfsw.example.json	OK
contracts/data/examples/v6/signal.example.json	OK
contracts/data/examples/v6/units.example.json	OK
contracts/data/examples/v6/weight-mode.example.json	OK
contracts/data/phase2_uncertainty_rejection_provenance_v1.json	FINDING:W2-DATA-8
contracts/data/phase_product_exchange.schema.json	OK
contracts/data/phase_product_exchange_matrix.json	OK
contracts/data/unified_object_compatibility_map_v1.json	FINDING:W2-DATA-2
contracts/data/v6_data_dictionary_v1.json	FINDING:W2-DATA-8
contracts/data/v6_migration_map_v1.json	FINDING:W2-DATA-9
contracts/data/v6_weight_vocabulary_v1.json	FINDING:W2-DATA-9
contracts/proposals/v6/data/astrocs.v6.covariance.v1.schema.json	FINDING:W2-DATA-8
contracts/proposals/v6/data/astrocs.v6.data-design-catalog.v1.json	FINDING:W2-DATA-8
contracts/proposals/v6/data/astrocs.v6.effective-psf.v1.schema.json	FINDING:W2-DATA-8
contracts/proposals/v6/data/astrocs.v6.phase3.v1.schema.json	FINDING:W2-DATA-8
contracts/proposals/v6/data/astrocs.v6.point-information.v1.schema.json	FINDING:W2-DATA-8
contracts/proposals/v6/data/astrocs.v6.provenance.v1.schema.json	FINDING:W2-DATA-8
contracts/proposals/v6/data/astrocs.v6.psf.v1.schema.json	FINDING:W2-DATA-8
contracts/proposals/v6/data/astrocs.v6.psfsw.v1.schema.json	FINDING:W2-DATA-8
contracts/proposals/v6/data/astrocs.v6.signal.v1.schema.json	FINDING:W2-DATA-8
contracts/proposals/v6/data/astrocs.v6.units.v1.schema.json	FINDING:W2-DATA-9
contracts/proposals/v6/data/astrocs.v6.weight-mode.v1.schema.json	FINDING:W2-DATA-8
contracts/proposals/v6/data/examples/effective-psf.example.json	OK
contracts/proposals/v6/data/examples/phase3.example.json	OK
contracts/proposals/v6/data/examples/point-information.example.json	OK
contracts/proposals/v6/data/examples/provenance.example.json	FINDING:W2-DATA-7
contracts/proposals/v6/data/examples/psfsw.example.json	OK
contracts/proposals/v6/data/examples/signal.example.json	OK
contracts/schemas/contract_index.schema.json	FINDING:W2-DATA-9
contracts/schemas/cpu_profile.schema.json	FINDING:W2-DATA-9
contracts/schemas/hardware_inspect.schema.json	FINDING:W2-DATA-9
contracts/schemas/jsonl_event_v1.schema.json	FINDING:W2-DATA-9
contracts/schemas/task_result.schema.json	OK
contracts/schemas/traceability_matrix.schema.json	FINDING:W2-DATA-5
contracts/schemas/unified/coverage.schema.json	OK
contracts/schemas/unified/depth_m5.schema.json	OK
contracts/schemas/unified/examples/coverage.example.json	OK
contracts/schemas/unified/examples/depth_m5.example.json	OK
contracts/schemas/unified/examples/frame_snr.example.json	OK
contracts/schemas/unified/examples/ivar.example.json	OK
contracts/schemas/unified/examples/point_information.example.json	OK
contracts/schemas/unified/examples/provenance.example.json	OK
contracts/schemas/unified/examples/psfsw_robust_weight.example.json	OK
contracts/schemas/unified/examples/rejection.example.json	OK
contracts/schemas/unified/examples/signal.example.json	OK
contracts/schemas/unified/examples/signal_flux.example.json	OK
contracts/schemas/unified/examples/source_snr.example.json	OK
contracts/schemas/unified/examples/sparse_snr_layer.example.json	OK
contracts/schemas/unified/examples/support.example.json	OK
contracts/schemas/unified/examples/validity.example.json	OK
contracts/schemas/unified/examples/variance.example.json	OK
contracts/schemas/unified/frame_snr.schema.json	OK
contracts/schemas/unified/ivar.schema.json	OK
contracts/schemas/unified/negative/EXPECTED.json	OK
contracts/schemas/unified/negative/n1_bare_weight.schema-violation.json	OK
contracts/schemas/unified/negative/n2_source_snr_as_variance.schema-violation.json	OK
contracts/schemas/unified/negative/n3_coverage_as_rejection.schema-violation.json	OK
contracts/schemas/unified/negative/n4_source_snr_into_variance_port.schema-violation.json	OK
contracts/schemas/unified/point_information.schema.json	OK
contracts/schemas/unified/port_contract.schema.json	OK
contracts/schemas/unified/provenance.schema.json	FINDING:W2-DATA-8
contracts/schemas/unified/psfsw_robust_weight.schema.json	OK
contracts/schemas/unified/rejection.schema.json	OK
contracts/schemas/unified/signal.schema.json	OK
contracts/schemas/unified/source_snr.schema.json	OK
contracts/schemas/unified/sparse_snr_layer.schema.json	OK
contracts/schemas/unified/support.schema.json	OK
contracts/schemas/unified/validity.schema.json	OK
contracts/schemas/unified/variance.schema.json	OK
contracts/schemas/v6/astrocs.v6.covariance.v1.schema.json	FINDING:W2-DATA-8
contracts/schemas/v6/astrocs.v6.effective-psf.v1.schema.json	FINDING:W2-DATA-8
contracts/schemas/v6/astrocs.v6.phase3.v1.schema.json	FINDING:W2-DATA-8
contracts/schemas/v6/astrocs.v6.point-information.v1.schema.json	FINDING:W2-DATA-8
contracts/schemas/v6/astrocs.v6.provenance.v1.schema.json	FINDING:W2-DATA-8
contracts/schemas/v6/astrocs.v6.psf.v1.schema.json	FINDING:W2-DATA-8
contracts/schemas/v6/astrocs.v6.psfsw.v1.schema.json	FINDING:W2-DATA-8
contracts/schemas/v6/astrocs.v6.signal.v1.schema.json	FINDING:W2-DATA-8
contracts/schemas/v6/astrocs.v6.units.v1.schema.json	FINDING:W2-DATA-8
contracts/schemas/v6/astrocs.v6.weight-mode.v1.schema.json	FINDING:W2-DATA-8
contracts/schemas/version.schema.json	FINDING:W2-DATA-2
packaging/README.md	FINDING:W2-DATA-8
packaging/astrocs.product.json	FINDING:W2-DATA-2
packaging/dependency-lock.json	FINDING:W2-DATA-2
packaging/gen_sbom_input.py	FINDING:W2-DATA-8
packaging/install-tree.contract.json	FINDING:W2-DATA-3
packaging/launch/demo_gc.json	OK
packaging/launch/start_browser.ps1	OK
packaging/licenses/CFITSIO_LICENSE.txt	OK
packaging/licenses/LICENSE-INDEX.txt	FINDING:W2-DATA-7
packaging/licenses/nlohmann_json.MIT.txt	OK
packaging/schemas/astrocs-product.schema.json	FINDING:W2-DATA-2
packaging/schemas/dependency-lock.schema.json	FINDING:W2-DATA-6
packaging/schemas/install-tree-contract.schema.json	FINDING:W2-DATA-2
packaging/schemas/preset-contract.json	FINDING:W2-DATA-8
packaging/verify_install_tree.py	FINDING:W2-DATA-9
packaging/windows/.vsconfig	OK
packaging/windows/README.md	FINDING:W2-DATA-8
testdata/index.json	OK
contracts/schemas/phase_config_export.schema.json	FINDING:W2-DATA-10	UNTRACKED
contracts/schemas/phase_config_mosaic.schema.json	FINDING:W2-DATA-10	UNTRACKED
contracts/schemas/phase_config_normalize.schema.json	FINDING:W2-DATA-10	UNTRACKED
contracts/schemas/run_manifest.schema.json	FINDING:W2-DATA-4	UNTRACKED
```

files_total: 127（tracked 123 + untracked 4；开工点 tracked=87）
verdict_counts: FINDING:W2-DATA-1=1 FINDING:W2-DATA-10=3 FINDING:W2-DATA-2=8 FINDING:W2-DATA-3=2 FINDING:W2-DATA-4=2 FINDING:W2-DATA-5=1 FINDING:W2-DATA-6=1 FINDING:W2-DATA-7=5 FINDING:W2-DATA-8=28 FINDING:W2-DATA-9=14 OK=62
枚举命令: `git ls-files contracts testdata packaging cmake` ＋ `git ls-files -o --exclude-standard contracts testdata packaging cmake`（工作目录 "/workspace/Astro CS Database"，收工点 32a5f5f3）
