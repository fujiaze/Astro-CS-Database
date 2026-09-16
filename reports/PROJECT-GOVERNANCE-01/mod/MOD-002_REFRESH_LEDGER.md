# MOD-002 迁移后引用刷新账本（REFRESH_LEDGER）

> 任务：`工程控制/PROJECT-GOVERNANCE-01/tasks/MOD-002.md`（L2，依赖 ARCH-001 停稳）
> 依据权威：`ASTROCS_DESIGN.md` §7.1/§7.3/§11.3、`ENGINEERING_SPEC.md` §3/§4/§7/§8、
> `cmake/ARCH-001-migration-manifest.md` §1（旧→新唯一事实源）、`docs/plugins/00_INDEX.md` §1/§2
> 口径：`问题扫描/REBASE.md`（四态词表）、`reports/PROJECT-GOVERNANCE-01/root-scan/CONTINUATION.md`（陷阱 20 条）
> 开工基线：`HEAD = 8f0a4c6b`（main；工作树另有 1566 条他线未提交改动，本任务不改其中他域文件）
> 零 git 写：本任务全程未 add/commit/push/branch/stash/reset/checkout；改动全部留在工作区。

---

## 0. 清单交付异常（必须先说明）

任务卡《你的输入》第 1 条指定 `reports/PROJECT-GOVERNANCE-01/root-scan/by_owner/MOD-002.txt`，
**该文件在树中不存在**（`ls by_owner/` 实测 37 个文件，含 MOD-001.txt，无 MOD-002.txt）。
按 `问题扫描/REBASE_TABLE.md` 的「归属」列实测：**MOD-002 = 0 条**（MOD-001 = 39 条）。
⇒ **MOD-002 名下没有逐条清单**；本任务的 39 条口径来自 MOD-001.txt（条目 39 / P0-OPEN 6 与之逐字一致），
按任务卡《额外范围（重要）》作为**并入复检**处理。计数以 MOD-001.txt 为准：**39 = 1（已由 MOD-001 交付解决）+ 29（需在模块图/共址文档修正）+ 1（需权威裁决）+ 8（需转其它任务）**（逐条见 §2）。

---

## 1. 旧→新路径翻译表（ARCH-001 迁移清单 §1，逐条现场核验）

来源：`cmake/ARCH-001-migration-manifest.md` §1（35 条 DONE + 3 条 DEFERRED）。
现场核验命令：`run/PROJECT-GOVERNANCE-01/MOD-002/logs/00_path_existence.log`、
`run/PROJECT-GOVERNANCE-01/MOD-002/logs/06_translation_check.txt`（`bad=0`）。

### 1.1 可翻译（35/35，旧路径已不存在、新路径存在）

| # | 旧路径（迁移前） | 新路径（当前树） | 状态 |
|---|---|---|---|
| 1 | `lib/phase2_rej` | `lib/algorithms/rejection` | DONE |
| 2 | `lib/phase2_samp` | `lib/algorithms/sampling` | DONE |
| 3 | `lib/phase2_upm` | `lib/algorithms/upm` | DONE |
| 4 | `lib/phase3_fits` | `lib/algorithms/fits_output` | DONE |
| 5 | `lib/phase2_int` | `lib/algorithms/integration` | DONE |
| 6 | `lib/phase3_proj` | `lib/algorithms/projection` | DONE |
| 7 | `lib/phase3_rsmp` | `lib/algorithms/resample` | DONE |
| 8 | `lib/common` | `lib/algorithms/shared` | DONE |
| 9 | `lib/drizzle` | `lib/algorithms/drizzle` | DONE |
| 10 | `lib/hips` | `lib/algorithms/drizzle/hips` | DONE |
| 11 | `lib/healpix_db/healpix_drizzle` | `lib/algorithms/drizzle/healpix_drizzle` | DONE |
| 12 | `lib/healpix_db/healpix_browser_qt` | `lib/infrastructure/hips_browser/healpix_browser_qt` | DONE |
| 13 | `lib/astro_image_io` | `lib/infrastructure/aio` | DONE |
| 14 | `lib/io` | `lib/infrastructure/aio/io` | DONE |
| 15 | `lib/orchestrator` | `lib/infrastructure/pipeline/orchestrator` | DONE |
| 16 | `lib/backend_host` | `lib/infrastructure/benchmark/backend_host` | DONE |
| 17 | `lib/gaia_xpsd_client` | `lib/infrastructure/gaia_xpsd_client` | DONE |
| 18 | `lib/calibration` | `lib/algorithms/calibration` | DONE |
| 19 | `lib/cosmetic` | `lib/algorithms/cosmetic` | DONE |
| 20 | `lib/star_detector` | `lib/algorithms/star_detection` | DONE |
| 21 | `lib/dynamic_psf` | `lib/algorithms/psf` | DONE |
| 22 | `lib/plate_solve` | `lib/algorithms/platesolve` | DONE |
| 23 | `lib/photometric_calib` | `lib/algorithms/photometry` | DONE |
| 24 | `lib/snr_estimator` | `lib/algorithms/noise_snr` | DONE |
| 25 | `lib/phase1/noise` | `lib/algorithms/noise_snr/wrapper_phase1` | DONE |
| 26 | `lib/phase1/photometry` | `lib/algorithms/photometry/wrapper_phase1` | DONE |
| 27 | `lib/phase1/stars` | `lib/algorithms/star_detection/wrapper_phase1` | DONE |
| 28 | `lib/phase1/wcs` | `lib/algorithms/platesolve/wrapper_phase1` | DONE |
| 29 | `lib/phase1/v6` | `lib/algorithms/integration/v6_phase1` | DONE |
| 30 | `lib/phase1/tests` | `lib/algorithms/photometry/wrapper_phase1/tests` | DONE |
| 31 | `lib/phase2` | `lib/algorithms/coverage` | DONE |
| 32 | `lib/acr` | `lib/infrastructure/acr` | DONE |
| 33 | `lib/hips_p2` | `lib/algorithms/coverage/hips_p2` | DONE |
| 34 | `lib/core` | `lib/infrastructure/scheduler` | DONE |
| 35 | `lib/healpix_db` | `lib/infrastructure/aio/healpix_db` | DONE |

### 1.2 无法翻译 / 明确不改（3 条 DEFERRED + 3 条清单外）

| 旧路径 | 现状 | 处置 | 归属 |
|---|---|---|---|
| `lib/phase1_session` | 仍存在（3 个 `*_session` 之一） | **不翻译**：§7.3 禁 Session 型模块，删除归 INT-001；本任务只读不改 | INT-001 |
| `lib/phase2_session` | 仍存在 | 同上（`docs/modules/registry/astrocs.phase2.session.md`、`docs/contracts/PUBLIC_API.md`、`docs/contracts/DATA_SEMANTICS.md` 里对它的引用**保持原样**，随删除一并收口） | INT-001 |
| `lib/phase3_session` | 仍存在 | 同上 | INT-001 |
| `cli/` | 仍存在 | 不在 ARCH-001 清单（manifest §5：cli 属「兼容图」，路径同步归 CLI-001/INT-001）；MODULE_MAP 的 `legacy_paths: [cli]` 保留并注明同名 | CLI-001/INT-001 |
| `lib/infrastructure/gaia_xpsd_client` | 与 target_dir 同名 | 迁移前旧路径即 `lib/gaia_xpsd_client`；现名与 target 同名，`legacy_paths` 保留该名并在 note 注明 | — |
| `tools/monitoring`、`runtime/` | 仍存在 | 非 ARCH-001 清单项；observability/runtime 的 legacy 面不再由本任务声明（见 §3） | INT-001 |

### 1.3 旧路径引用实测（工作树，按翻译表复跑）

扫描器：`run/PROJECT-GOVERNANCE-01/MOD-002/scan_old_paths.py`（35 条旧路径最长匹配 + 目录边界；日志 `logs/scan_*.json`）。

| 面 | 改前命中 | 改后命中 | 说明 |
|---|---|---|---|
| `docs/modules/**` | 4（全部是 `lib/phase2_session`） | 0（同上，属 INT-001） | ARCH-001 已就地改写其余；本任务复核零残留 |
| `docs/contracts/**` | 8（全部是 `lib/phase2_session`） | 0（同上，属 INT-001） | 只改引用，不动合同语义 |
| `tools/quality/**`（非 fixtures） | 96 行 / 24 文件 | 3（**仅注释历史键**） | 见 §4 |
| `tools/quality/contracts/fixtures/**` | 1391（CSV 夹具自身数据） | 1391 | **保留**：夹具是测试输入数据，不是路径引用 |
| `tests/quality/**` | 17（`test_module_map.py` 的 LEGACY_PAIRS） | 17 | **保留**：历史键常量，t23 的判据正是「旧目录存在不得算实现」 |
| `tests/**` 其它 | 12 文件（cli/backend/unit/integration） | 12 | **未改**：tests/cli、tests/backend 属 TEST-CLI-SYNC-2 域；其余见 §6 转出项 |

---

## 2. MOD-001 名下 39 条逐条处置表（计数一致）

**计数**：39 = **已由 MOD-001 交付解决 1**（M8a-I-001，仅「目录存在性」子项）+ **需在模块图/共址文档修正 29** + **需权威裁决 1**（M3b-C-01，科学正文口径，本任务不改）+ **需转其它任务 8**。
四态：**39/39 仍 OPEN**（无一转 RESOLVED/VOID/UNVERIFIABLE；逐条真跑证据见 `logs/item_*.log`、`logs/item2_*.log`、`logs/item3_*.log`）。

| ID | 原类别 | 原优先级 | MOD-002 当前树复检结论 | 路径翻译情况 | 处置 | 归属/去向 |
|---|---|---|---|---|---|---|
| M2a-F-1 | F_TEST_GAP | P0 | OPEN：drizzle 5 个 Oracle TU 在 CMakeLists/*.cmake 注册 0 命中（rc=1）；DRIZZLE.md:85 仍点名 candidate_oracle_test 9003 例 | lib/healpix_db/healpix_drizzle/tests → lib/algorithms/drizzle/healpix_drizzle/tests | **需转其它任务** | P1-001（测试注册面） |
| M2b-F-01 | F_TEST_GAP | P0 | OPEN：test_healpix_oracle 零注册（rc=1）；极点跳过 test_healpix_oracle.cpp:88 仍在 | lib/common/healpix/tests → lib/algorithms/shared/healpix/tests | **需转其它任务** | P1-001（测试注册面） |
| M3b-C-01 | C_DOC_CODE_GAP | P0 | OPEN：docs/science/STAR_DETECTION.md:13「不引入 0.5px 网格量化损失」仍在；docs/KNOWN_LIMITATIONS.md 中 PSF-001 命中 0 | lib/photometric_calib → lib/algorithms/photometry（已迁） | **需权威裁决（本任务不改；科学正文/文档口径）** | MOD-001 / 负责人裁决 |
| M3b-C-02 | C_DOC_CODE_GAP | P0 | OPEN：module_adapters.cpp:1616 phase1::StarDetector det(5.0)、:1663 「: 99.0」逐字复现 | lib/core/src/module_adapters.cpp → lib/infrastructure/scheduler/src/module_adapters.cpp | **需在模块图/共址文档修正** | MOD-001 |
| M5b-G-06 | G_GOV_GATE | P0 | OPEN：packaging/astrocs.product.json units=10、sha256 全 null；根与 cli 构建面 acs_secure_loader 命中 0 | 不涉迁移路径 | **需转其它任务** | INT-001 / PKG-001（装配与清单面） |
| V11-N-05 | G_GOV_GATE | P0 | OPEN：acs_negotiate_v1 唯一定义在 tests/abi/abi002_lifecycle_probe.c；生产面零命中（rc=1） | 不涉迁移路径 | **需转其它任务** | INT-001（ABI 装配面） |
| M2a-C-4 | C_DOC_CODE_GAP | P1 | OPEN：lib/algorithms/drizzle/README.md:9 仍称「本目录仅合同文件、无源码」，而 src/module_entry.cpp 与 CMakeLists.txt:61 add_library(astrocs_p1_drizzle SHARED) 在位；module.yaml:34 entrypoint: MISSING | lib/drizzle → lib/algorithms/drizzle（共址文档需改） | **需在模块图/共址文档修正** | MOD-001（本轮 MOD-002 执行） |
| M2a-C-5 | C_DOC_CODE_GAP | P1 | OPEN：gaia README:13/:137 仍称「尚未存在 / 无 CMake target」，而 CMakeLists.txt:15 add_library(astrocs_catalog_gaia SHARED)、tests/unit/CMakeLists.txt:859 add_test(NAME gaia_cat_unit) 在位 | lib/gaia_xpsd_client → lib/infrastructure/gaia_xpsd_client | **需在模块图/共址文档修正** | MOD-001 |
| M2b-C-02 | C_DOC_CODE_GAP | P1 | OPEN：hips README:24 仍称「全仓库无 astrocs_p1_hips_writer 目标」，而 hips/CMakeLists.txt:43 已建、product.json:16 MOD-P1-HIPSW=IMPLEMENTED | lib/hips → lib/algorithms/drizzle/hips | **需在模块图/共址文档修正** | MOD-001 |
| M2b-C-03 | C_DOC_CODE_GAP | P1 | OPEN：hips/CMakeLists.txt:13-15 错误注释与 :33-40 RESCUE 更正段并存 | lib/hips → lib/algorithms/drizzle/hips | **需转其它任务** | INT-001（CMake 注释面） |
| M3-C-008 | C_DOC_CODE_GAP | P1 | OPEN：descriptor astrocs.phase1.calibration(:563) 与 module.yaml:15 astrocs.p1.calibration 两拼写并存 | lib/core/src → lib/infrastructure/scheduler/src；lib/calibration → lib/algorithms/calibration | **需在模块图/共址文档修正** | MOD-001 / MOD-002（模块图 ID 归一） |
| M5b-C-04 | C_DOC_CODE_GAP | P1 | OPEN：lifecycle_v1.h:73 DOUBLE_DESTROY 承诺与 module_api_v1.h:107 void (*destroy) 互斥；state 仍普通 int | lib/calibration/src/module_entry.cpp → lib/algorithms/calibration/src/module_entry.cpp | **需转其它任务** | INT-001（ABI 语义面） |
| M5b-G-18 | G_GOV_GATE | P1 | OPEN：registry/astrocs.phase3.resample2.md production 身份与 TRACEABILITY_MATRIX.csv 行 25 十二格 MISSING 并存 | registry 页已随迁移改写 | **需在模块图/共址文档修正** | MOD-001 / DOC-001（注册表与追溯面） |
| M5b-G-20 | G_GOV_GATE | P1 | OPEN：tools/check_module_readmes.py 5 项硬编码（旧 lib/phase1/{stars,wcs,photometry,noise}/README.md）已刷新为新路径；module.yaml 实测 20、registry 页 26 的集合对账缺口仍在 | lib/phase1/{stars,wcs,photometry,noise} → lib/algorithms/*/wrapper_phase1（检查器常量已刷新，见 §4.3） | **需在模块图/共址文档修正** | MOD-001（集合对账后继） |
| M8a-C-002 | C_DOC_CODE_GAP | P1 | OPEN：cosmetic README:9 / hips README:24 / calibration module.yaml:21 三例五源互斥仍在 | 三处路径均已迁（lib/algorithms/{cosmetic,drizzle/hips,calibration}） | **需在模块图/共址文档修正** | MOD-001 |
| M8a-C-005 | C_DOC_CODE_GAP | P1 | OPEN：module.yaml module_id 与 descriptor 交集 0（复算：manifest 21 vs descriptor 22） | lib/core/src → lib/infrastructure/scheduler/src | **需在模块图/共址文档修正** | MOD-001 / MOD-002（模块图 ID 归一） |
| M8a-E-001 | E_TRACE_BREAK | P1 | OPEN：ALG-P2-COV-001 仍只在 module_adapters.cpp:874 + coverage/{module.yaml:32,README.md:174}；INDEX.yaml 只登记 ALG-COV-001(:73) | lib/core/src → lib/infrastructure/scheduler/src；lib/phase2 → lib/algorithms/coverage | **需在模块图/共址文档修正** | MOD-001 / DATA-001 |
| M8a-G-006 | G_GOV_GATE | P1 | OPEN：check_module_readmes.py 只覆 5 份（P1-003/004/005 在 INDEX.yaml 命中 0）；检查器常量已刷新 | 旧路径曾致 5 份 README 全部 missing；现已 rc=0 | **需在模块图/共址文档修正** | MOD-001（覆盖面后继） |
| M8a-G-007 | G_GOV_GATE | P1 | OPEN：gen_module_readmes.py 零接线（checks.json 命中 0）、26 页 status 全 ACTIVE（不在 §11.3 阶梯） | 生成器 docstring 源路径已刷新为 lib/infrastructure/scheduler | **需在模块图/共址文档修正** | MOD-001 / CI-003 |
| V12-N-11 | A_SCI_DEF | P1 | OPEN：200000 仍 1 具名 + 2 处字面复制（gaia_client.c / module_entry.c / ipv_types.h） | lib/gaia_xpsd_client/src → lib/infrastructure/gaia_xpsd_client/src；lib/plate_solve/cpp/ipv/include → lib/algorithms/platesolve/cpp/ipv/include | **需在模块图/共址文档修正** | MOD-001（P2-002 常量单点化） |
| V14-N-02 | G_GOV_GATE | P1 | OPEN：modules/conformance/noop/module.yaml:11 module_status: SKELETON 与两清单 MOD-NOOP status=IMPLEMENTED、echo 零登记并存 | 不涉迁移 | **需转其它任务** | INT-001 / PKG-001（清单一致性） |
| V14-N-04 | G_GOV_GATE | P1 | OPEN：ALG-P2-COV-001 三处已迁至 lib/algorithms/coverage/**；ALG-004 仍在 backend_host/backend_table.inc:21 与 module_adapters.cpp:808 | 站点全部已迁（本行即翻译结果） | **需在模块图/共址文档修正** | MOD-001 / DATA-001 |
| V14-N-08 | G_GOV_GATE | P1 | OPEN：gen_module_readmes.py 无条件覆盖 registry（10 页含「事实修订」）、零接线 | 源路径已迁（同上） | **需在模块图/共址文档修正** | MOD-001 / CI-003 |
| V21-N-13 | C_ALG_IMPL | P1 | OPEN：14 个 manifest 键回读面仍在 module_adapters.cpp 之外零命中 | lib/core/src → lib/infrastructure/scheduler/src | **需在模块图/共址文档修正** | MOD-001（OBS-001 观测面） |
| W1-N-03 | G_GOV_GATE | P1 | OPEN：lib/algorithms/noise_snr/CMakeLists.txt:25 注释仍称「由根 CMakeLists.txt 注册」，根 :210 明载断链解除裁决；tests/unit/CMakeLists.txt:1176 门卫恒假 | lib/snr_estimator → lib/algorithms/noise_snr | **需在模块图/共址文档修正** | MOD-001 / CI-002 |
| W6-N-04 | C_ALG_IMPL | P1 | OPEN：tracked h/hpp 347、含 typedef struct 47、带 struct_size 13（点名结构仍缺） | 头文件随模块迁移（本行即翻译结果） | **需在模块图/共址文档修正** | MOD-001（INT-001 ABI 面） |
| M2a-C-13 | C_DOC_CODE_GAP | P2 | OPEN：gaia_client_get_spectrum_params(:2684) 仍无 if (!client) 守卫（同族 6 处有） | lib/gaia_xpsd_client/src → lib/infrastructure/gaia_xpsd_client/src | **需在模块图/共址文档修正** | MOD-001 |
| M2b-C-05 | C_DOC_CODE_GAP | P2 | OPEN：hips/include/astrocs/hips/types.h:35 编号错位注释仍在；HIPS_WRITER.md 编号整体错位一格 | lib/hips/types.h → lib/algorithms/drizzle/hips/include/astrocs/hips/types.h | **需在模块图/共址文档修正** | MOD-001 |
| M4-G-02 | G_GOV_GATE | P2 | OPEN：module_adapters.cpp:582 astrocs.phase2.resample 假身份 Session 仍在注册表；registry 页仍 tracked | lib/core/src → lib/infrastructure/scheduler/src | **需在模块图/共址文档修正** | MOD-001 / INT-001 |
| M6a-I-004 | I_DOC_HYGIENE | P2 | OPEN：lib/algorithms/calibration/cpp/cosmetic_corrector.cpp 无 legacy 声明、9 处 fprintf(stderr；inventory:88 仍标 production,yes | lib/calibration/cpp → lib/algorithms/calibration/cpp（路径已变、结论不变） | **需在模块图/共址文档修正** | MOD-001 / INT-001 |
| M6b-E-007 | E_TRACE_BREAK | P2 | OPEN：modules/services/io/ 仍 4 文件（3/4 清单不符）；docs/interfaces/data/DATA-001_ARTIFACT_CONTRACT.md 仍不存在 | 不涉迁移 | **需转其它任务** | INT-001（残体清退） |
| M8-E-001 | E_TRACE_BREAK | P2 | OPEN：coverage/hips_p2/ 仍零源文件；coverage/CMakeLists.txt:129 astrocs-stage2 EXCLUDE_FROM_ALL；tools/stage2.cpp 实测 1782 行 vs README 自称 1762 行 | lib/hips_p2 → lib/algorithms/coverage/hips_p2；lib/phase2 → lib/algorithms/coverage | **需在模块图/共址文档修正** | MOD-001（行锚修正） |
| M8a-C-008 | C_DOC_CODE_GAP | P2 | OPEN：providers/cpu/common/README.md:65-70 无 acs_cap_classify_v1（头 :127 有）；README:3 仍标 FROZEN（不在 §11.3 阶梯） | 不涉迁移 | **需在模块图/共址文档修正** | MOD-001 |
| M8a-E-004 | E_TRACE_BREAK | P2 | OPEN：cosmetic README 仍称 include/astro_calibration.h；docs/architecture/cpu/ARCH_CONTRACTS.md 缺失而 phase3_proj/rsmp README 仍引；hips README:172 相对引 tests/hiss_write_probe.cpp 实测 MISSING | 全部已迁（本行即翻译结果） | **需在模块图/共址文档修正** | MOD-001（悬空引用修正） |
| M8a-I-001 | I_DOC_HYGIENE | P2 | OPEN（**部分子项已消**）：docs/plugins/00_INDEX.md:16/:22 与 MODULE_MAP.yaml:37/:55 以 lib/algorithms/ 为目标源码根——迁移后该目录**已存在**（子项「目标根不存在」消解）；「每子库 README」硬义务主体缺口仍在 | lib/algorithms 现为真实根（ARCH-001 翻译完成） | **已由 MOD-001 交付解决（目录存在性子项）** | MOD-001（映射门交付已消解该子项；条文缺口留 MOD-001 名下） |
| V14-N-07 | G_GOV_GATE | P2 | OPEN：snr_estimator.h:240 snr_source_snr_f64 在 lib/algorithms/noise_snr/module.yaml 命中 0 | lib/snr_estimator/cpp/include → lib/algorithms/noise_snr/cpp/include | **需在模块图/共址文档修正** | MOD-001（module.yaml 登记面） |
| V20-N-07 | C_ALG_IMPL | P2 | OPEN：build_out_manifest 出参 err 仅在签名出现（区间内 err 引用 1 次）、(void)what 仍在 :778 | lib/calibration/src → lib/algorithms/calibration/src | **需在模块图/共址文档修正** | MOD-001 |
| V21-N-04 | C_ALG_IMPL | P2 | OPEN：echo 模块 aerr/rerr 成员访问 False（仍声明+memset+传址） | 不涉迁移（modules/conformance/echo） | **需在模块图/共址文档修正** | MOD-001 |
| V5-N-01 | C_DOC_CODE_GAP | P2 | OPEN：catalog_dir 路径预算检查散落执行点（module_entry.c:104 等） | lib/gaia_xpsd_client/src → lib/infrastructure/gaia_xpsd_client/src | **需在模块图/共址文档修正** | MOD-001 |

**计数（脚本保证一致）**：总计 **39** = 已由 MOD-001 交付解决 **1** + 需在模块图/共址文档修正 **30** + 需转其它任务 **8**。

转出的 8 条：M2a-F-1→P1-001、M2b-F-01→P1-001、M5b-G-06→INT-001 / PKG-001、V11-N-05→INT-001、M2b-C-03→INT-001、M5b-C-04→INT-001、V14-N-02→INT-001 / PKG-001、M6b-E-007→INT-001。

---

## 3. P0-OPEN 逐条结论（6/6）

| ID | 结论 | 一句话理由（附本轮实跑证据路径） |
|---|---|---|
| M3b-C-01 | **仍 OPEN**，不在 MOD-002 域 | `docs/science/STAR_DETECTION.md:13` 与 `docs/KNOWN_LIMITATIONS.md`（PSF-001 = 0）互斥文本逐字复现；涉 SCI 正文 ⇒ 只登记（`logs/item_M3b-C-01_*.log`） |
| M3b-C-02 | **仍 OPEN** | `lib/infrastructure/scheduler/src/module_adapters.cpp:1616` `phase1::StarDetector det(5.0)`、`:1663 : 99.0` 逐字复现（`logs/item_M3b-C-02_*.log`） |
| M2a-F-1 | **仍 OPEN** | drizzle 5 个 Oracle TU 在 `CMakeLists/*.cmake` 注册 0 命中（rc=1）；`docs/science/DRIZZLE.md:85` 仍点名 `candidate_oracle_test` 9003 例（`logs/item_M2a-F-1_*.log`） |
| M2b-F-01 | **仍 OPEN** | `test_healpix_oracle` 在 `ci/tests/lib` 注册 0 命中（rc=1）；极点跳过 `lib/algorithms/shared/healpix/tests/test_healpix_oracle.cpp:88` 仍在（`logs/item_M2b-F-01_*.log`） |
| M5b-G-06 | **仍 OPEN** | `packaging/astrocs.product.json` units=10 / sha256-null=10；根与 cli 构建面 `acs_secure_loader` 命中 0（`logs/item_M5b-G-06_*.log`） |
| V11-N-05 | **仍 OPEN** | `acs_negotiate_v1` 唯一定义在 `tests/abi/abi002_lifecycle_probe.c`；生产面（lib/runtime/cli/modules/providers）零命中（rc=1）（`logs/item_V11-N-05_*.log`） |

**P0 归属结论**：6 条 P0-OPEN 全部与「迁移后引用刷新」无关（无一因 ARCH-001 迁移而消失或新增），
应按原四态留在各自归属任务：**MOD-001 名下 3 条**（M3b-C-01/C-02、M5b-G-06）+ **P1-001 名下 2 条**（M2a-F-1、M2b-F-01）+ **INT-001 1 条**（V11-N-05）。

---

## 4. 改动清单 + 每处「改前红 → 改后绿」证据

### 4.1 `docs/modules/MODULE_MAP.yaml`（legacy_paths 与实际迁移结果一致）

| 项 | 改前 | 改后 | 证据 |
|---|---|---|---|
| 17 个算法的 `legacy_paths` | 误写为**迁移后 target_dir 自身**（如 calibration 写 `[lib/algorithms/calibration]`） | 迁移前旧路径（`[lib/calibration]`…） | `MODULE_MAP.yaml.before` 对照；`logs/07_legacy_map_before.txt` |
| drizzle `legacy_paths` | `[lib/algorithms/drizzle/healpix_drizzle, lib/algorithms/drizzle/hips]` | `[lib/healpix_db/healpix_drizzle, lib/hips]` | 同上 |
| coverage / integration / noise_snr / aio | 只写 target_dir 或漏项 | 补 `lib/phase2`+`lib/hips_p2` / `lib/phase2_int`+`lib/phase1/v6` / `lib/snr_estimator`+`lib/phase1/noise` / `lib/astro_image_io`+`lib/io`+`lib/healpix_db` | 同上 |
| runtime / benchmark | `[runtime, lib/infrastructure/scheduler, lib/infrastructure/pipeline/orchestrator]` / `[]` | `[lib/core]`（清单内）/ `[lib/backend_host]`（清单内） | 同上 |
| observability | `[tools/monitoring]`（非清单项） | `[]` + note 说明清单内无对应旧目录 | 同上 |
| 21 条 `note` | 与 ARCH-001 后事实不符（如「迁移目标目录由 ARCH-001 建立」） | 写明旧路径、迁移条目号与「已迁入 target_dir」；cli/gaia 注明不在清单 | 同上 |
| 表头诚实性约定 | 「旧命名目录（lib/algorithms/star_detection 等）」表述失效 | 新增「legacy_paths 保存迁移前旧路径，禁止写 target_dir 自身」硬约定 | 见文件 `# 诚实性约定` 段 |

**改前红 → 改后绿（同一命令、同一门）**

| 指标 | 改前 | 改后 |
|---|---|---|
| `check_module_map.py` rc | 1 | 1（**如实仍红**：153 条真实 FAIL 全是 target_dir 实现缺口，非本任务可修） |
| `modules`/`ids_unique` | 23/23 | 23/23 |
| `fail_findings` | 153 | **153（未减少、未新增）** |
| `note_findings` | 26 | **7**（`legacy_paths_present` 21 → 2） |
| `legacy_paths_present` 明细 | 21 条，且**全部指向 target_dir 自身**（把历史键退化成自证） | 2 条，均为迁移清单外同名历史面（`cli`、`lib/infrastructure/gaia_xpsd_client`） |

差异取证：`logs/baseline_module_map.json` vs `logs/g1_after_maponly.json`（逐 finding 集合差 = 只少 19 条 legacy_paths_present，无新增）。

### 4.2 检查器旧路径常量刷新（tools/quality/** + tests/quality/**）

| 文件 | 改前症状（复跑实测） | 改后 |
|---|---|---|
| `tools/quality/check_serial_heavy.py` | rc=0 **假绿**：`lib/phase2/src/{sampler,upm}.cpp` 与 `lib/astro_image_io/...` 读空串（0 字符），Phase2 并行判据整段空跑 | rc=0 真绿：sampler 63477 字符、upm 106560 字符（`std::thread` 命中 3/7），allowlist 路径存在 |
| `tools/quality/contracts/check_build_graph.py` | rc=1：`lib/phase2/CMakeLists.txt` 缺失 | **rc=0** `passed:true`（findings 空） |
| `tools/quality/contracts/check_config_contracts.py` | rc=1：`lib/phase2/src/stage2_common.cpp`、`lib/phase2/configs` 缺失 | **rc=0** `passed:true` |
| `tools/quality/contracts/check_execution_contracts.py` | rc=0（`lib/astro_image_io/...` 未命中，检查空跑） | rc=0 且 `passed:true`（现读 `lib/infrastructure/aio/src/hips/aio_hips_reader.cpp`） |
| `tools/quality/contracts/check_test_contracts.py` | rc=1：`lib/phase2/tests/synthetic_gate.cpp` 缺失导致 tst 判定失真 | **rc=0** `passed:true, tst_count=36` |
| `tools/quality/contracts/check_doc_symbols.py`（CHK-DANGLING） | rc=124 **超时**（60s 打满） | rc=1，正常跑完 186 篇并给出真实 findings（`snr_v`/`snr_v²` 等 P1，既有红非本任务引入） |
| `tools/quality/check_complexity.py` | rc=0，但 ACR 排除项写 `lib/acr`（迁移后失效 ⇒ ACR dormant 树被计入复杂度） | rc=0，排除 `lib/infrastructure/acr` |
| `tools/quality/check_traceability.py` / `check_serial_heavy.py` 注释 | 描述旧路径 | 注释改为迁移后路径 |
| `tools/quality/known_failures_baseline.py` | 11 处旧路径（`lib/phase2/src`、`lib/core/src`） | 已刷新为 `lib/algorithms/coverage/src`、`lib/infrastructure/scheduler/src`；rc=0、reproduced=17（与改前同） |
| `tools/quality/check_source_inventory.py`（未注册） | 14 处旧模块前缀 | 已刷新为新根 |
| `tools/quality/{v19r3_static,v19r3_traceability,v19r3_audit,build_v19r4_package,extract_cpp_api,gen_source_index_v61,update_audit_status}.py`（未注册、v19 退役面） | 187 处旧路径 | 已刷新（机械替换，仅路径串） |
| `tools/quality/{check_module_map,gen_module_readmes,build_v19r2_package}.py` | 3 行旧路径（注释/docstring/命令字面量） | 已刷新并标注「迁移前路径」 |
| `tests/quality/test_module_map.py` `LEGACY_PAIRS`（17 处） | 旧命名目录常量 | **保留**：t23 判据正是「旧目录存在不得算实现」，改成新路径会让该测试失去意义 |
| `tests/quality/contracts/fixtures/**`（1391 处） | 夹具 CSV 内路径数据 | **保留**：夹具是测试输入数据 |

### 4.3 迁移清单外的注册检查器（同类死引用，硬失败；已机械修复并登记）

| 文件 | 注册项 | 改前 | 改后 |
|---|---|---|---|
| `tools/check_module_readmes.py` | CHK-MODULE-MANIFEST | rc=1：5 个 README 全部 missing（旧 `lib/phase1/*` 路径） | **rc=0** `DOC-003_PASS: 5 模块 README 全含 合同/header/source/test 链接` |
| `tools/check_duplication.py` | CHK-STATIC | rc=1 **崩溃** `FileNotFoundError: lib/phase1/wcs/wcs_tan.cpp` | **rc=0** `QA-004_PASS` |
| `tools/check_warning_suppression.py` | CHK-WARN | rc=1 **崩溃** `FileNotFoundError: lib/phase1/noise/noise_model.cpp` | **rc=0** `QA-001_PASS` |
| `tools/check_serial_hardcode.py` | CHK-RESOURCE | rc=0 但 `PROD_FILES` 6/8 项是旧路径（扫空） | rc=0 且 3 条登记现落到 `lib/algorithms/coverage/src/{upm,sampler}.cpp`、`lib/phase3_session/p3_session.cpp` |
| `tools/check_abi_boundary.py` | CHK-ABI | rc=0 **假绿**：`lib/backend_host/*.h` 命中 0 ⇒ 只扫 1 个头 | rc=0 且 **11 个头**（含 fail-closed：目录缺失即 rc=1） |
| `tools/arch/check_thread_budget.py` | CHK-RESOURCE | rc=0，注释已自述迁移 | **未改**（已自洽） |
| `tools/pack_audit_package.py` | CHK-SECRET-HYGIENE | rc=2 + 自述「已退役」（RETIREMENT_LEDGER） | **未改**，转 RETIRE-001 复核 |

### 4.4 能红能绿（正例 + 负例）双向证据

新增 `tests/quality/test_mod002_migration_refs.py`（**9 用例，全 OK**，日志 `logs/06_unittest_mod002.out`）：

| 用例 | 正例 | 负例 |
|---|---|---|
| T1/T1b legacy_paths 自证防线 | 23 条历史键均不等于/不落在自身 target_dir 下、且迁移清单内旧目录均已不在树中 | — |
| T2 check_module_map | 真实仓库 23/23、真实 FAIL>0（不为绿放宽） | 把 `legacy_paths` 写回现存 `lib/algorithms/calibration` ⇒ **必报 `legacy_paths_present`** |
| T3 check_module_readmes | rc=0 `DOC-003_PASS` | 镜像树里把一条 README 指向不存在文件 ⇒ **rc=1 + `missing`** |
| T4 check_warning_suppression | 真实仓库 rc=0 | 镜像树里给生产源注入 `-w` ⇒ **rc=1 + `QA-001_WARN_VIOLATION`** |
| T5 旧路径零命中 | `docs/modules`、`docs/contracts`（MODULE_MAP 历史键除外）零旧路径 | — |

> 负面测试用「run/ 下镜像树（真目录 + 文件级 symlink）+ 真实仓库只读」实现；
> 首版曾用目录级 symlink 导致写入穿透、`lib/.../noise_model.cpp` 被删，**已用 `git show HEAD:<path>` 逐字还原**
> （sha256 `590f8a1bb9…` 与 HEAD 一致，`git diff` 空），并改为文件级 symlink 后复跑通过。

---

## 5. 验收门复跑（本任务自证；前台可独立复跑）

| 门 | 命令 | 改前 | 改后 | 说明 |
|---|---|---|---|---|
| CHK-MODULE-MANIFEST | `python3 tools/quality/check_module_map.py` | rc=1 / 23-23 / FAIL 153 / NOTE 26 | rc=1 / 23-23 / **FAIL 153** / **NOTE 7** | **如实仍红**：153 条是 target_dir 下实现/CMake/target/清单缺口（ARCH-001 只搬目录），非本任务可修；本任务只消 19 条 legacy NOTE |
| CHK-ROOT-CLEAN | `python3 tools/quality/check_root_cleanliness.py` | rc=1 / violations=1（`evidence` 未登记根条目） | 同（**非本任务域**） | 归 ROOT-001/ROOT-005 |
| 文档索引 | `python3 tools/doccheck/check_doc_index.py --strict` | rc=1 | 同（**非本任务域**） | 归 CI-003（`engineering/control/archive/...` 旧绑定） |
| tests/quality | `python3 -m unittest discover -s tests/quality -t tests/quality` | 129 tests, failures=3 errors=1 skipped=6 | **138 tests, failures=3 errors=1 skipped=6** | +9 全绿，**零新增红**，skipped 未增加 |
| tests/contracts | `python3 -m unittest discover -s tests/contracts -t tests/contracts` | rc=0（63 tests OK） | rc=0（63 tests OK） | 无回归 |
| MOD-002 双向证据 | `python3 -m unittest tests.quality.test_mod002_migration_refs` | — | **9/9 OK** | 正例 + 负例 |

### 5.1 「悬空=0」口径说明（任务卡验收门第 2 条）

任务卡要求「`check_module_map.py` 对真实仓库的判决与**悬空计数**如实给出（允许仍为 NOT_IMPLEMENTED，但**悬空=0** 且证据完整）」。
本任务按门的实际判据拆成三类**如实数字**（不合并、不粉饰）：

| 悬空类 | 判据 | 实测 | 归属 |
|---|---|---|---|
| 目标目录悬空 | `missing_target_dir` | **1**（`runtime` → `lib/infrastructure/runtime` 不存在；其余 22 个 target_dir 均存在） | INT-001（runtime 拆分）；MODULE_MAP 未改 target_dir（§7.1 只给了 scheduler/pipeline/observability，未给 runtime 插槽 ⇒ **不擅自改**，见 §6 裁决项） |
| DATA 合同悬空 | `dangling_data_contract` | **9** | DATA-001（`docs/contracts/INDEX.yaml` 未定义；只改引用不改合同语义） |
| schema 链接悬空 | `dangling_schema_link` | **22** | DATA-001（`contracts/schemas/*` 缺文件） |
| **模块图/共址文档旧路径悬空** | 35 条旧 lib 路径在 `docs/modules`、`docs/contracts`、`tools/quality`、`tests/quality` 的命中 | **0**（3 处为**注释历史键**，非引用） | **本任务（MOD-002）** |

---

## 6. 越界自查与转出项

### 6.1 未改（硬约束）

- `lib/**`、`cli/**`：**零改动**（`git status --porcelain=v1 -- lib cli` 实测空）。
- `ci/**`、`.github/**`：**零改动**。
- `cmake/**`、根 `CMakeLists.txt`：**零改动**（`build_v19r2_package.py` 内的命令字面量属 tools/**，非 CMake 文件）。
- `docs/algorithms/**`、`anchor_contract.json`：**零改动**（ARCH-001 域）。
- `docs/science/**`：**零改动**（科学语义只读）。
- `tests/cli/**`、`tests/backend/**`：**零改动**（TEST-CLI-SYNC-2 域）。
- 合同语义/schema/公式/默认容差：**零改动**。
- 零 git 写；未 add/commit/push/branch/stash/reset/checkout。

### 6.2 转出项（本任务只登记，不抢改）

| # | 对象 | 症状（实测） | 归属 |
|---|---|---|---|
| 1 | `tools/check_l0_docs.py`、`tools/doccheck/check_doc_index.py` | 绑定 ROOT-007 已删的 `engineering/control/archive/.../README_ARCHIVED.md` 与 `REVIEW.md` ⇒ 恒红（`--strict` rc=1） | CI-003（任务卡已点名） |
| 2 | `ci/checks.json` 的 `changed_paths` 含 `lib/phase3_session/**`、`lib/phase2/**`、`lib/plate_solve/**` 等旧 glob | 触发面失效（改新路径不触发对应门） | CI-001/CI-003（**注册项命令禁改**） |
| 3 | `tools/{assemble_v17_review_pkg,gen_v19_evidence,gen_repo_source_manifest,docs_machine_consistency,check_p1_symbol_map,check_p2_symbol_map,config_consistency_check,gen_cfitsio_list,gen_provider_manifests,make_windows_release,validate_cpu_profile}.py`、`tools/quality/{v19r3_*,build_v19r2_package,build_v19r4_package,check_source_inventory,update_audit_status,extract_cpp_api,gen_source_index_v61}` 等**未注册**工具 | 仍含迁移前 lib 路径（已在本任务内把 `tools/quality` 的机械刷新；`tools/` 其余只登记） | RETIRE-001（退役）或 CI-003（重新接线） |
| 4 | `tests/{unit,integration}/**` 12 处旧 lib 路径 | 陈旧常量 | 对应治理任务（非 MOD-002 域） |
| 5 | `docs/modules/registry/astrocs.phase2.session.md`、`docs/contracts/{PUBLIC_API.md,DATA_SEMANTICS.md}` 内 `lib/phase2_session` 引用（12 处） | DEFERRED 目录仍在；删除时一并收口 | INT-001 |
| 6 | 根 `CMakeLists.txt:210` 含 `add_subdirectory(lib/algorithms/noise_snr)` 裁决文本 | 属 INT-001 域 | INT-001 |

### 6.3 需权威裁决（阻塞项，本任务未擅自处理）

| # | 事项 | 依据 | 现状 | 建议 |
|---|---|---|---|---|
| A-1 | MODULE_MAP 的 `runtime` 目标目录 `lib/infrastructure/runtime` **在 `ASTROCS_DESIGN` §7.1 顶层结构中不存在** | §7.1 只有 `scheduler/`、`pipeline/`、`observability/` | 该模块 `missing_target_dir`（唯一 1 条目录悬空），且 `runtime/` 旧目录仍在树中 | 由负责人/MOD-001 决定：`runtime` 是否保留为独立模块（则 §7.1 需增补）或并入 scheduler/observability；**MOD-002 不擅自改 target_dir** |
| A-2 | 39 条 MOD-001 条目在 REBASE 表列 `OPEN` 而 `TASK_LIST.md` 记 MOD-001 = PASS | `CONTROL_PACK_SPEC` §6.3（PASS 仅由前台验收后写入） | 计数矛盾：MOD-001「交付解决」的只有映射门本身，39 条 finding 无一被该交付闭合 | 请前台裁定：MOD-001 的 PASS 是否仅指「映射门建立」；若是，39 条需重新挂账（本任务已给出逐条归属） |
| A-3 | 任务卡输入文件 `by_owner/MOD-002.txt` 不存在，REBASE 表 MOD-002 归属 0 条 | 本文件 §0 | 无法按「逐条清单」验收 MOD-002 | 请前台确认：MOD-002 的验收以本账本 §1/§4 的引用刷新为准，39 条并入按 §2 计数 |
| A-4 | `tools/quality/contracts/fixtures/**` 1391 处旧路径数据 | 夹具即测试输入 | 未改 | 若要求「旧路径零命中」含夹具，请裁决：夹具数据是否算「引用」（本任务判**不算**） |
| A-5 | `tests/quality/test_module_map.py` `LEGACY_PAIRS` 17 处旧路径 | 该常量是 t23 判据（旧目录不得算实现） | 未改 | 若要求零命中，需同时改写 t23 判据（会降低该门意义）⇒ 建议保留 |

---

## 7. 交付物索引

| 交付物 | 路径 |
|---|---|
| 本账本 | `reports/PROJECT-GOVERNANCE-01/mod/MOD-002_REFRESH_LEDGER.md` |
| 自证摘要 | `run/PROJECT-GOVERNANCE-01/MOD-002/自证摘要.md` |
| 扫描器（可复跑） | `run/PROJECT-GOVERNANCE-01/MOD-002/scan_old_paths.py` |
| 机械替换器（可复跑） | `run/PROJECT-GOVERNANCE-01/MOD-002/fix_paths.py` |
| 复检脚本 | `run/PROJECT-GOVERNANCE-01/MOD-002/{recheck_items,recheck_items2,recheck_items3}.py` |
| 日志 | `run/PROJECT-GOVERNANCE-01/MOD-002/logs/`（翻译核验、逐 ID 复检、门复跑、双向证据） |
| 改前快照 | `run/PROJECT-GOVERNANCE-01/MOD-002/MODULE_MAP.yaml.before` |
| 新增测试 | `tests/quality/test_mod002_migration_refs.py` |
