# 复核件主张汇总（第②层判定 + 定级 + 小节标题）

## 复核-AUD201.md （覆盖成稿：AUD-201-测光核验.md）

- **V1 —— 被正本否定的通量式仍在库里且"被活跃调用"** ｜判定 `PASS_DEGRADED`｜定级 `-`
  - 判定原文：降级**（"存活"确认；"可达产品路径"推翻；"星等计两次"的机理与 0.459 dex 数值判待证）
  - 点名对象：lib/include/

- **V2 —— λ 因子是色项：其"25 倍裕度排除备择假设"的论证** ｜判定 `VOID`｜定级 `-`
  - 判定原文：推翻**（推翻的是"25× 裕度"这条**论证**；λ 因子本身的正确性由别的路径成立，正本无需订正）
  - 点名对象：eng/packaging/config/filters.json

- **V3 —— 现行锚的分箱轴使判据失去判别力** ｜判定 `PASS_DEGRADED`｜定级 `-`
  - 判定原文：降级**（"色项型缺陷在星等分箱下趋零"我独立复现；但"判据无证据资格"结论过宽，且成稿给的整改路径无效）
  - 点名对象：(未抽出)

- **V4 —— 常数"出处"登记为配置键（死键）＋ 权威回链行号大面积指错** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认**（四条子claim 全部独立复算成立；"造假 vs 漂移"的裁决见下，成稿未做这一层而我做了）
  - 点名对象：docs/science/PHOTOMETRY.md, eng/ci/prod_wiring_baseline.json, eng/packaging/config/defaults.json, eng/tests/config/check_cfg002_registry.py, eng/tests/config/test_cfg001_contracts.py

- **V5 —— 零离散度的假绿通道（`sigma_residual`）** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认**（我独立构造可达、判据实读放行；并给成稿补三条它没有的事实，其中一条把证据级别从"构造"抬到"库内实跑日志"）
  - 点名对象：docs/science/PHOTOMETRY.md, eng/ci/fixtures/provenance/, eng/contracts/schemas/, eng/tests/validation/release02/fix_p1_photometry_apply/qf_oracle_stderr.txt, lib/infrastructure/pipeline/orchestrator/memory.md

- **V6 —— 一条科学门的归档读数超冻结容差（生产质心 p95）** ｜判定 `PASS_DEGRADED`｜定级 `-`
  - 判定原文：降级**（读数、跟踪性、台账措辞我全部复现；但"该读数属一条科学门"这一定性不成立 ⇒ 现状既不能判红也不能判绿）
  - 点名对象：artifacts/ci/, artifacts/evidence/audit-2026-01/FIX_LEDGER.csv, artifacts/evidence/governance-01/, docs/algorithms/GATES_AND_TOLERANCES.md, docs/science/STAR_DETECTION.md, eng/ci/checks.json, lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/evidence/gate2_result.json, lib/algorithms/psf/tests/p1psf/CMakeLists.txt, lib/algorithms/psf/tests/p1psf/p1psf_centroid_gate.cpp, lib/infrastructure/pipeline/orchestrator/cpp/include/star_coord_contract.h, lib/photometric_calib, lib/photometric_calib/

## 复核-AUD202-补.md （覆盖成稿：AUD-202-SNR核验.md）

- **V5 量纲不齐的噪声式 ＋ 对插值算子恒为 0 的"重建误差"判据** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认（②③）**；① 的量纲部分**确认**，但其"数值偏 1.03–3.72 倍"的归因须**降级/订正**（偏差主要由缺 `ΣP²` 贡献，不是缺 `1/g`）；
  - 点名对象：docs/plugins/algorithms_phase1/07_noise_snr.md, docs/plugins/algorithms_phase2/13_integration.md, lib/algorithms/integration/v6/src/weight_chain.cpp, lib/algorithms/noise_snr/cpp/src/snr_science.cpp, lib/infrastructure/scheduler/src/module_adapters.cpp, 实验/shared/synthetic/noise_model.py

- **V6 归档头条数字取自单次实现值** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认**（"头条取自单次 MC 实现值"成立；"最大偏差无人判"成立）。
  - 点名对象：实验/absolute-snr/REPORT_paper.md, 实验/absolute-snr/code/b2_noise_terms.py, 实验/absolute-snr/results/b2_noise_terms.json, 实验/absolute-snr/results/ctest_evidence.md

## 复核-AUD202.md （覆盖成稿：AUD-202-SNR核验.md）

- **V1 权重在信号维退化为常数** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认**（结论成立，但两臂的定性不同，须拆开定级——成稿把它们捆在一条头条里偏重）
  - 点名对象：(未抽出)

- **V2 稀疏层对象与合同冻结口径不是同一个** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认（定性从"写错对象"收窄为"生产根本没有该对象 + 消费侧按已作废的相对语义实现"）
  - 点名对象：docs/science/CONTROL_WEIGHT_SNR.md, eng/ci/ledgers/dead_config_keys.json, eng/contracts/schemas/unified/sparse_snr_layer.schema.json, 实验/absolute-snr/docs/snr-propagation-design.md

- **V3 两篇 FROZEN 科学文档相互排斥（工包要求我下判断，不上呈）** ｜判定 `VOID`｜定级 `-`
  - 判定原文：推翻（"两篇相互排斥、须负责人裁决"这个定性不成立）
  - 点名对象：docs/design/UNIFIED_MODEL.md, docs/plugins/algorithms_phase1/07_noise_snr.md, docs/science/PHASE2_UPM.md

- **V4 `snr_path` 死键与三臂对象错配** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认（并补两条成稿没给的硬证据：结果件里 `snr` 出现 0 次；dense 两条生产者都不可达）
  - 点名对象：eng/ci/ledgers/dead_config_keys.json, eng/ci/prod_wiring_baseline.json, eng/tools/run_forward_drizzle.py, lib/algorithms/noise_snr/, lib/infrastructure/scheduler/, 实验/absolute-snr/code/b3_domain_map.py, 实验/absolute-snr/results/b3_domain_map.json

## 复核-AUD203.md （覆盖成稿：AUD-203-天光无缝核验.md）

- **V1 常数天光块下控制点权重不归零** ｜判定 `PASS`｜定级 `P1`
  - 判定原文：确认
  - 点名对象：docs/algorithms/PHASE2_SAMPLER.md, docs/science/PHASE2_UPM.md, lib/algorithms/coverage/src/sampler.cpp, lib/algorithms/integration/v6/src/phase2_integrate.cpp

- **V2 出厂接缝门与验收证据用的不是同一个统计量** ｜判定 `PASS`｜定级 `P1`
  - 判定原文：确认
  - 点名对象：docs/science/PHASE2_UPM.md, eng/tools/e2e/seam_footprint.py, 实验/additive-sky-seamless/README.md, 实验/additive-sky-seamless/code/sci_c_common.py

- **V3 `additive_mode` 注释与代码默认打架** ｜判定 `PASS`｜定级 `P2`
  - 判定原文：确认
  - 点名对象：docs/modules/phase2_upm.md, docs/science/PHASE2_UPM.md, eng/packaging/config/config_registry.json, eng/tests/unit/v6_p2_sky/, lib/algorithms/coverage/src/upm.cpp, lib/infrastructure/scheduler/src/module_adapters.cpp

- **V4 `tolerance_relative` 两处取值不同** ｜判定 `PASS`｜定级 `P2`
  - 判定原文：确认
  - 点名对象：docs/algorithms/PHASE2_UPM_IMPL.md, docs/science/PHASE2_UPM.md, lib/infrastructure/cli/commands.cpp, lib/infrastructure/scheduler/src/module_adapters.cpp, lib/phase2_session/p2_session.cpp

- **V5 `stalled=2` 无归档 B 级证据** ｜判定 `PASS`｜定级 `P2`
  - 判定原文：确认
  - 点名对象：eng/tests/backend/test_p2003_seam_oracle.py, lib/algorithms/coverage/src/upm.cpp, 实验/additive-sky-seamless/REPORT_paper.md, 实验/additive-sky-seamless/results/c1_additive.json, 实验/m42-realdata/

## 复核-AUD204.md （覆盖成稿：AUD-204-面积交叠核验.md）

- **W1 — 面积闭合是代数恒等，因而"1e-15 闭合"没有证据资格** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认（并订正成稿两处口径）
  - 点名对象：docs/algorithms/DRIZZLE_GEOMETRY.md, lib/algorithms/drizzle/healpix_drizzle/tests/reference_overlap.cpp, lib/algorithms/drizzle/healpix_drizzle/v6_spherical_overlap.h, lib/infrastructure/aio/src/hips/aio_hips_writer.cpp, 实验/healpix-polar/code/chart_native.h, 实验/healpix-polar/code/polar_common.h, 实验/healpix-polar/docs/EXP-07-POLAR.md

- **W2 — 折线缝上的精度平台高于冻结门** ｜判定 `PASS_DEGRADED`｜定级 `-`
  - 判定原文：降级（成稿的两处口径混用；平台量级不可当作几何事实；但另有更大的真实预算违反）
  - 点名对象：docs/KNOWN_LIMITATIONS.md, docs/algorithms/DRIZZLE_GEOMETRY.md, docs/science/DRIZZLE.md, eng/ci/checks.json

- **W3 — `hips_profile=0` 与 `=1` 不等价，误差来自 uint8 支撑格** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认（并补两条成稿未报的更大分支；负责人待裁事项可据此定案）
  - 点名对象：eng/tests/validation/release02/q2_snr_smoothness/realdata/realdata_seam.log, eng/tools/e2e/render_vis.py, eng/tools/e2e/seam_footprint.py, eng/tools/quality/frame_qc_grid.py, lib/algorithms/coverage/src/sampler.cpp

- **W4 — 为 pixfrac 修复作证的那条门，其 parity 断言按构造永不触发** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认（并订正"逐 leaf 分配环节没有任何门"为"没有任何在册门；两件可执行件已在仓但被显式不注册"）
  - 点名对象：eng/tests/, lib/algorithms/drizzle/healpix_drizzle/tests/drizzle_pf_sb_gate.cpp, lib/infrastructure/aio/tests/hiss_experiments

## 复核-DB13.md （覆盖成稿：AUD-101-DB-13.md）

- **W1 ** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认
  - 点名对象：artifacts/evidence/v6/qa-design/data/baseline_matrix.json, artifacts/evidence/v6/qa-design/qa_matrix.json, docs/contracts/DATA_SEMANTICS.md, docs/design/UNIFIED_MODEL.md, docs/science/PSF_SIGNAL_WEIGHT.md, docs/validation/v6/, eng/ci/check_psfsw_retired.py, eng/ci/checks.json, eng/contracts/data/v6_clause_registry_v1.json, eng/contracts/schemas/unified/, eng/tools/monitoring/mem_guard.py, lib/algorithms/coverage/tests/weight_mode_retire_negative_test.cpp, reports/v6/qa-design/data/baseline_matrix.json

- **W2 ** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认（头条数字降级）
  - 点名对象：docs/TRACEABILITY.csv, docs/contracts/INDEX.yaml, docs/modules/registry/astrocs.phase, docs/traceability/, eng/ci/checks.json, eng/contracts/schemas/traceability_matrix.schema.json, eng/tools/monitoring/mem_guard.py, eng/tools/traceability/check_traceability_matrix.py, lib/algorithms/projection/p3_wcs.h, lib/plate_solve/…

- **W3 ** ｜判定 `PASS`｜定级 `P1`
  - 判定原文：确认（范围与严重度上修）
  - 点名对象：docs/algorithms/PHASE2_REJECTION.md, docs/contracts/PUBLIC_API.md, docs/design/PHASE2_DETAILED_DESIGN.md, docs/modules/phase2_rej.md, docs/modules/registry/astrocs.phase2.reject.md, docs/plugins/algorithms_phase2/12_rejection.md, docs/science/REJECTION.md, docs/traceability/TRACEABILITY_MATRIX.csv, docs/traceability/TRACEABILITY_MATRIX.json, eng/tests/unit/p2002_unc_rej_prov_test.cpp, eng/tests/unit/p2_rejection_test.cpp, eng/tools/monitoring/mem_guard.py, lib/algorithms/coverage/src/rejection.cpp

- **W4 ** ｜判定 `PASS`｜定级 `P1`
  - 判定原文：确认（可结案，不必上呈）
  - 点名对象：docs/GLOSSARY.md, docs/algorithms/PHASE2_SAMPLER.md, docs/contracts/DATA_SEMANTICS.md, docs/contracts/PUBLIC_API.md, docs/science/CONTROL_WEIGHT_SNR.md, docs/science/CONTROL_WEIGHT_SNR.md#153, eng/ci/check_no_weight_mode.py, lib/algorithms/coverage/include/astro/phase2/upm.h, lib/algorithms/coverage/tests/synthetic_gate.cpp

## 复核-合同层.md （覆盖成稿：AUD-101-DB-19.md, AUD-101-DB-20.md, AUD-101-DB-11.md）

- **W1 `pixel_area_power` 两侧取值** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认（但成稿的事实面不完整——冲突不是"说明层 vs 机器层"，而是"机器层内部两个事实源同名互斥"）
  - 点名对象：docs/contracts/DATA_SEMANTICS.md, docs/contracts/unified_object_registry.json, eng/ci/check_product_contract.py, eng/contracts/, eng/contracts/data/v6_clause_registry_v1.json, eng/contracts/schemas/product_family_field_constraints.schema.json, eng/contracts/schemas/unified/examples/{variance,ivar}.example.json, eng/contracts/schemas/unified/variance.schema.json, eng/contracts/schemas/unified/{provenance,coverage,rejection,support,validity}.schema.json, eng/tests/contracts/test_unified_object_contract.py, eng/tests/unit/v6_p1_drz/v6_p1_drz_test.cpp, lib/algorithms/drizzle/healpix_drizzle/v6_drizzle_science.cpp, lib/algorithms/resample/p3_rsmp_units.cpp, lib/infrastructure/aio/v6/src/v6_bunit.cpp, lib/infrastructure/aio/v6/src/v6_provenance.cpp

- **W2 `§9.73 A44` 的锚宿主是否真的无处可寻** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认存在（引用悬空成立）＋ 定性降级（不是"依据丢失"，是"锚号/出处失效"）；另确认一条成稿未见的机器面：该引用面处在三道锚门的结构性盲区
  - 点名对象：docs/algorithms/anchors/check_doc_line_anchors.py, docs/algorithms/anchors/unresolved_registry.json, docs/design/UNIFIED_MODEL.md, docs/research/SNR_WEIGHT_RESEARCH_PACK.md, eng/ci/check_no_weight_mode.py, eng/ci/check_registration_anchors.py, eng/ci/ledgers/registration_anchor_ledger.json, eng/contracts/, eng/contracts/data/examples/, eng/lib, reports/v6/contract-review/02_CONFLICT_AND_GAP_AUDIT.md, 工程控制/RELEASE-02/GAP_AUDIT.md, 工程控制/RELEASE-05/GAP_AUDIT.md

- **W3 配置合同的数值快照与事实源全面失配（`docs/contracts/CONFIG_CONTRACT.md`）** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认（成稿列出的四组差异逐条复算全部成立）＋ 定级上调（性质不是"抄了旧快照"，是"越位成第二套登记面"，且其复制对象自身也已漂移；按实现逐位复算，在册门 `UT-CONFIG`
  - 点名对象：docs/contracts/CONFIG_CONTRACT.md, eng/ci/ID_MIGRATION_MAP.md, eng/ci/checks.json, eng/contracts/config/module_dll_contract.schema.json, eng/contracts/schemas/, eng/packaging/config/config_registry.json, eng/packaging/config/defaults.json#field_count, eng/tests/cli/, eng/tests/config/, eng/tests/config/check_cfg002_registry.py, eng/tests/conformance/noop/README.md, eng/tools/monitoring/mem_guard.py, eng/tools/quality/check_path_domain_anchors.py

- **W4 `must_contain` 与唯一源互斥（落盘形态合同批次主结论）** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认（互斥成立、触发面缺口成立）＋ 处置方案降级（成稿给的"换成指针判据"会净失一项现行判据强度，引入新 fail-open；须改为双向判据）
  - 点名对象：docs/ci/CI_SPEC.md, docs/contracts/CONFIG_CONTRACT.md, docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md, docs/design/PHASE1_DETAILED_DESIGN.md, docs/design/PHASE2_DETAILED_DESIGN.md, docs/design/PHASE3_DETAILED_DESIGN.md, docs/design/PRODUCT_STORAGE_FORM.md, docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md, docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md, eng/ci/checks.json, eng/contracts/schemas/hips_storage_form.schema.json, eng/tools/hipsform/check_hips_storage_form.py, eng/tools/monitoring/mem_guard.py

- **W5 裸形态是否允许没有产品级索引（三方冲突）** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认（冲突成立且比成稿说的更硬——不是"三方"而是"五处口径 + 同一判据两侧相反"，其中合同自身条款互斥到不可同时满足）
  - 点名对象：docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md, docs/design/PRODUCT_STORAGE_FORM.md, eng/ci/ledgers/dead_config_keys.json, eng/contracts/schemas/hips_storage_form.schema.json, eng/tools/, eng/tools/hipsform/, eng/tools/hipsform/check_hips_storage_form.py, eng/tools/monitoring/mem_guard.py, lib/infrastructure/cli/session_commands.h

## 复核-架构接线.md （覆盖成稿：AUD-401-架构对齐.md）

- **W1 · 三阶段调度器只有 1/3 接在生产上，生产由单一通用 `Scheduler` 跑三阶段** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认**（成稿 AUD401-005 的事实面与计数全部独立复现成功；我的定级理由与改法方向与成稿不同，见"与成稿差异"）
  - 点名对象：eng/tests/, eng/tests/unit/block_flow_test.cpp, eng/tests/unit/mosaic_window_test.cpp, eng/tests/unit/normalize_workflow_test.cpp, eng/tools/arch/check_p3_export_stream_prod.py, lib/include/astrocs/core/export_stream.h, lib/include/astrocs/core/mosaic_window.h, lib/include/astrocs/core/normalize_workflow.h, lib/include/astrocs/core/scheduler.h, lib/infrastructure/cli/runtime_client.cpp, lib/infrastructure/scheduler/src/module_adapters.cpp, lib/infrastructure/scheduler/src/runtime.cpp

- **W2 · 模块 ID 是两套命名空间且交集为零** ｜判定 `PASS`｜定级 `P2`
  - 判定原文：确认（事实面）＋ 降级（定性与定级）**：交集为零、两侧计数、双向全差集**全部独立复现**；但成稿把短名侧说成"无任何机器门核对"，实测**该侧有门**（只是没有跨面比对的门），
  - 点名对象：docs/modules/registry/, eng/ci/check_registry_ir_parity.py, eng/cmake/astrocs.product.windows.json.in, eng/packaging/astrocs.product.json, eng/tests/abi/mod001_install_load_check.py, eng/tests/conformance/, eng/tests/conformance/{echo,noop}/module.yaml, lib/algorithms/calibration/module.yaml, lib/algorithms/cosmetic/module.yaml, lib/algorithms/coverage/hips_p2/module.yaml, lib/algorithms/coverage/module.yaml, lib/algorithms/drizzle/hips/module.yaml, lib/algorithms/drizzle/module.yaml, lib/algorithms/fits_output/module.yaml, lib/algorithms/integration/module.yaml, lib/algorithms/noise_snr/module.yaml, lib/algorithms/photometry/module.yaml, lib/algorithms/platesolve/module.yaml, lib/algorithms/projection/module.yaml, lib/algorithms/psf/module.yaml, lib/algorithms/rejection/module.yaml, lib/algorithms/resample/module.yaml, lib/algorithms/sampling/module.yaml, lib/algorithms/star_detection/module.yaml

- **W3 · 唯一执行器池靠文本包含走私 ＋ 生产里私建线程池 ＋ 第二资源预算源** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认（三条事实全复现）＋ 补充（我另找到 2 条成稿未记的实质事实，其中 1 条把风险等级顶高、1 条把成稿押注的理由压低）
  - 点名对象：eng/tests/unit/CMakeLists.txt, eng/tests/unit/drizzle_adapter_test.cpp, eng/tools/arch/check_thread_budget.py, lib/algorithms/coverage/include/astro/phase2/execution_options.h, lib/algorithms/coverage/include/astro/phase2/stage2_common.h, lib/algorithms/coverage/src/sampler.cpp, lib/algorithms/coverage/src/stage2_common.cpp, lib/algorithms/coverage/src/upm.cpp, lib/include/astrocs/core/context.h, lib/infrastructure/acr/scheduler/dispatcher.cpp, lib/infrastructure/benchmark/backend_host/baseline_kernels_impl.inc, lib/infrastructure/cli/commands.cpp, lib/infrastructure/scheduler/src/executor.cpp, lib/infrastructure/scheduler/src/export_stream.cpp, lib/infrastructure/scheduler/src/module_adapters.cpp, lib/infrastructure/scheduler/src/mosaic_window.cpp, lib/infrastructure/scheduler/src/normalize_workflow.cpp, lib/infrastructure/scheduler/src/scheduler.cpp, lib/phase3_session/p3_session.cpp

- **W4 · §8.4 承诺的加载校验与 CPU provider 不在图，但安装布局仍装 DSO** ｜判定 `MIXED`｜定级 `-`
  - 判定原文：部分确认 + 部分推翻**。四件事里"谁不编译""装出去几份"确认；**"产品内无 `dlopen`"这一条我推翻**——产品里有一条活的动态加载路径，而且它恰好就是 §8.4:6
  - 点名对象：eng/packaging/astrocs.product.json, eng/tests/abi/mod001_install_load_check.py, eng/tests/unit/CMakeLists.txt, eng/tools/gen_backends_manifest.py, lib/algorithms/, lib/algorithms/drizzle/CMakeLists.txt, lib/infrastructure/benchmark/, lib/infrastructure/benchmark/backend_host/backend_loader.cpp, lib/infrastructure/benchmark/cpu/, lib/infrastructure/pipeline/module_loader/{secure_loader.c

## 复核-测光默认阶数.md （覆盖成稿：AUD-101-DB-18.md）

- **三 判定 / 定级 / 与成稿差异 / 缺什么证据** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认（事实链全部成立）＋ 定性与定级须修正
  - 点名对象：docs/plugins/algorithms_phase1/06_photometry.md, eng/packaging/config/, eng/tests/validation/release02/fix_p1_photometry_apply/l4_photometry_fit_snippet.json, eng/tools/e2e/make_vis_configs.py, eng/tools/quality/check_photometry_apply.py, lib/algorithms/photometry/README.md, lib/infrastructure/scheduler/src/module_adapters.cpp

## 复核-结果层与收口层.md （覆盖成稿：AUD-101-DB-17.md, AUD-101-DB-20.md）

- **R1 清包会让写法门怎么样（来自 DB-20 §0.2 反向咬合条 / 条目 I-14 / P1-5）** ｜判定 `PASS`｜定级 `P1`
  - 判定原文：确认（方向 = 判红，不是失明）＋ 补两条成稿未落的独立发现
  - 点名对象：artifacts/evidence/, eng/ci/check_naming_surface.py, eng/ci/checks.json, eng/ci/ledgers/naming_surface.json, eng/contracts/, eng/tools/doccheck/check_doc_hygiene.py, 工程控制/DOC-SELFTEST-01

- **R2 极区报告里四处值在跟踪日志中不存在** ｜判定 `PASS_DEGRADED`｜定级 `-`
  - 判定原文：降级（4 条中 3 条成立；第 4 条推翻。SNAPSHOT 子核 = 确认 47/47 通过）
  - 点名对象：docs/EXP-07-POLAR.md, 实验/healpix-polar/docs/EXP-07-POLAR.md, 实验/healpix-polar/results/t17_extent.csv

- **R3 同一物理量两值与"文档低估缺陷量级"** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认（差因＝口径未声明，非取数错；两侧数值各自可回溯）
  - 点名对象：docs/EXP-05-ABSOLUTE-SNR.md, docs/EXP-06-SNR-PHYS.md, docs/EXP-07-POLAR.md, docs/plugins/algorithms_phase1/07_noise_snr.md, docs/science/CONTROL_WEIGHT_SNR.md, docs/science/NOISE_MODEL.md, eng/tools/doccheck/doc_fact_authority.json, 实验/absolute-snr/README.md, 实验/absolute-snr/results/EXP06_TABLES.md, 实验/absolute-snr/results/b1_sky_scan.json, 实验/absolute-snr/results/b2_noise_terms.json, 实验/healpix-polar/docs/EXP-07-POLAR-摘要.md, 实验/任务书, 工程控制/RELEASE-05/00_README.md

- **R4 手写报告层的字面量与自家 JSON 相矛盾** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认（三处不一致全部复现），但定级按"是否进判据/对外主张"分档；成稿的 1 处对照口径需订正
  - 点名对象：eng/tools/doccheck/doc_fact_authority.json, 实验/additive-sky-seamless/README.md

- **R5 一份"文档"是未填充的骨架却占着 `docs/` 位** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认（占位与 5/7 结论行逐位复现），并补一条比成稿更重的观察
  - 点名对象：docs/smooth-lambda.md, eng/ci/check_doc_unverified_cite.py, eng/tools/doccheck/check_doc_index.py, reports/v19r2/, reports/v19r2/file_audit_inventory.csv, 实验/additive-sky-seamless/REPORT_paper.md, 实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/report.py, 实验/additive-sky-seamless/docs/smooth-lambda.md, 实验/additive-sky-seamless/docs/…, 实验/additive-sky-seamless/results/REVERSE_VERIFY_CANON.md, 实验/shared/REVERSE_VERIFY_MIGRATION.md

## 复核-负责人面与索引.md （覆盖成稿：AUD-101-DB-19.md, AUD-101-DA01-根规范与科学.md）

- **V1 状态登记面自造档位并向全仓传染** ｜判定 `PASS_DEGRADED`｜定级 `-`
  - 判定原文：降级**（"加判据"与"发布结论越界"成立；"自造状态词百级传染"与"闭环"两处口径不成立，需按我实测重写；
  - 点名对象：artifacts/evidence/audit-2026-01/FIX_LEDGER.csv, docs/KNOWN_LIMITATIONS.md, docs/modules/MODULE_MAP.yaml, docs/owner/ARCHITECTURE_OVERVIEW.md, docs/owner/PIPELINE_OVERVIEW.md, docs/owner/RELEASE_STATUS.md, docs/research/COMPRESSION_CODEC_RESEARCH_PACK.md, docs/standards/DOCUMENTATION_STANDARD.md, eng/ci/check_version.py, eng/ci/checks.json, eng/ci/tests/test_ci001_failclosed.py, eng/tests/, eng/tools/quality/check_conclusion_truth.py, lib/algorithms/

- **V2 负责人面两份总览的源码行锚漂约 9000 行** ｜判定 `PASS_DEGRADED`｜定级 `-`
  - 判定原文：降级**（锚确实指到无关内容、真定义点在别处，已我自己定位 3 组；但**成稿给的根因不成立**：
  - 点名对象：docs/algorithms/anchors/anchor_contract.json, docs/algorithms/anchors/check_doc_line_anchors.py, docs/owner/PIPELINE_OVERVIEW.md, docs/owner/RELEASE_STATUS.md, lib/core/src/module_adapters.cpp, lib/core/src/module_adapters.cpp（不存在, lib/infrastructure/scheduler/src/, lib/phase3_proj/module.yaml

- **V3 是否真存在"第二张索引地图"且该删（`docs/audit/doc_classification.csv`）** ｜判定 `PASS_DEGRADED`｜定级 `-`
  - 判定原文：降级**（"争角色"不成立——唯一索引自己已把它判为历史快照并声明权威在本索引；
  - 点名对象：artifacts/evidence/doc-hygiene/baseline.json, docs/API_REFERENCE.md, docs/DOCUMENT_INDEX.yaml, docs/TRACEABILITY.csv, docs/audit/, docs/audit/doc_classification.csv, eng/tools/doccheck/check_doc_index.py, eng/tools/doccheck/dangling_ledger.json, reports/v19r2/source_manifest.csv

- **V4 一条独立审稿结论三档并存无人裁决（SCI-B / I2 fail-closed 对拍）** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认（缺陷成立、该判据的 PASS 资格现在不成立）**，但**"四档并存"的定性要收窄**：
  - 点名对象：artifacts/evidence/audit-2026-01/OWNER_DECISIONS.md, docs/design/PHASE2_DETAILED_DESIGN.md, docs/owner/RELEASE_STATUS.md, docs/plugins/algorithms_phase2/13_integration.md, docs/traceability/, eng/ci/check_config_consumed.py, eng/ci/ledgers/dead_config_keys.json, lib/infrastructure/cli/session_commands.h, 实验/absolute-snr/README.md, 实验/absolute-snr/REPORT_paper.md, 实验/absolute-snr/code/b6_gates_audit.py, 实验/absolute-snr/results/REVIEW.md

## 复核-门禁入口.md （覆盖成稿：AUD-501-门禁现状审计.md）

- **W1 两条执行入口对同一情形判定必然分叉，其中一条可"零执行报绿"** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认（机制与"agent 入口可零执行报绿"两条主链全部成立；成稿的规模数 345/93%、
  - 点名对象：artifacts/ci/, docs/ci/01_CHECKS.md, docs/ci/03_GATES.md, docs/ci/CI_SPEC.md, eng/ci/, eng/ci/checks.json, eng/ci/checks.schema.json, eng/ci/known_failures.json, eng/ci/run.py, eng/ci/run.py（执行, eng/ci/run_checks.py, eng/ci/tests/test_deep_profiles.py, eng/ci/tests/test_runner_selection.py, eng/ci/validate_registry.py

- **W2 那道"全门禁 fail-closed 普查"测的是谁** ｜判定 `PASS`｜定级 `-`
  - 判定原文：确认（数据流、"通过"的充要条件、文档授权面、数值漂移四项全部独立复算成立；
  - 点名对象：artifacts/ci/, artifacts/evidence/, artifacts/evidence/release-05/FAILCLOSED_SURVEY.md, docs/ci/01_CHECKS.md, docs/ci/03_GATES.md, docs/ci/CI_SPEC.md, eng/ci/__pycache__/run_checks, eng/ci/checks.json, eng/ci/failclosed_survey.py, eng/ci/monitor_evidence.py, eng/ci/run.py, eng/ci/run_checks.py, eng/contracts/resource_gate_v1.json, lib/shutil/sys/tempfile

