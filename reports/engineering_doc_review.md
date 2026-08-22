# Engineering Doc Review — Stage C (V19R3 True Final Freeze)

Date: 2026-08-22  
Scope: `docs/ARCHITECTURE.md` (133 行 Engineering Anchor, §1-8) + `docs/API_REFERENCE.md` + `工程控制/docs/18_CODE_CHANGE_MAP.md` + `lib/*/include/*.h` `lib/orchestrator/configs/*` `lib/phase2/configs/*` `tools/*.py` 全量只读锚定  
Tool: `tools/docs_machine_consistency.py` (S8 gate, 9 checks) + `tools/config_consistency_check.py` + `tools/api_doc_consistency.py` + 人工逐头文件签名/参数/错误码/线程/所有权核验  
Commit: Stage C 单一目的提交 `docs(architecture): stage C engineering anchor — ARCHITECTURE + API_REFERENCE + CODE_CHANGE_MAP [stage C]` (不改 `lib/**/src`)

## 结论总览

| 项 | 结论 |
|---|---|
| docs_machine_consistency | **PASS 9/9** |
| config_consistency_check | **PASS mismatches=[]** |
| P0 工程文档与代码不一致 | **0** |
| P1 待改代码的接口/命名不一致 (仅清单, 本阶段不改代码) | **5** (见 §3) |
| docs/README-DOCS.md L0-L5 一致性 | 通过 (L3 = ARCHITECTURE.md + architecture/* + API_REFERENCE.md + 18_CODE_CHANGE_MAP §Stage C 增补) |
| ARCHITECTURE.md 完整性 | **通过** 133 行 §1-8 全覆盖，无需补全 (权威链/概览/分层/模块表/数据流/压缩接口映射/configs/机检/追溯) |
| API_REFERENCE.md | **新建** 13 shipping modules + common/browser/acr 全量清单，错误码与 ERROR_MODEL.md 全集合一致 |

## 1. 模块-接口-函数清单汇总 (压缩, 全量见 `docs/API_REFERENCE.md`)

| 模块 | 头文件 | 前缀/类型 | 函数/类型数(节选) | 要点 |
|---|---|---|---|---|
| common | `healpix_core.h` `sha256.h` `astro_scalar.h` `precision_context.h` | `astrocs::healpix` `astrocs::crypto` | `ang2pix_nest/pix2ang_nest/nested_local_to_xy/parent_nest/child_nest/query_disc/neighbors` `sha256_hex/Sha256` `AstroScalarType/PrecisionContext` | NESTED 唯一实现 header-only；`common` 不依赖业务层 |
| astro_image_io | `astro_image_io.h` `aio_hips.h` `aio_hips_reader.h` `aio_pipeline.h` `aio_pipeline_engine.h` `aio_upm.h` `hiss_format.h` `aio_ahpx_format.h` | `aio_*` `hiss_*` | ~70 函数 + `PipelineFrame/AioBlock/AstroSphereTileView/AioUpmSparse/Dense` 等 | 唯一 I/O 入口；HiPS `signal/support/snr/variance/ivar` `512 tile` NESTED；`aio_upm_read_all_dynamic new char[]/delete[]` `thread_local g_upm_error` |
| calibration | `astro_calibration.h` | `ac_*` | 12 函数 + `ac::optimize_dark_k` | `AC_OK/-1/-2/-3`；`_f64` 真双精度仅 `calibrate_f64` |
| star_detector | `star_detector.h` | `sdet_*` | 6 函数 + `SDetParams` | `owned(handle)` `owned(*out)` 配 `sdet_free_*`；`sdet_detect_ex_f64` 不降级 |
| dynamic_psf | `dynamic_psf.h` | `dpsf_*` | 7 函数 + `DPSFFitResult/DPSFFitParams` | Moffat4 β=4 `MOFFAT4_FWHM_FACTOR=1.230310` |
| plate_solve/ipv | `ipv_api.h` (+14头) | `ipv_*` | 13 函数 + `IpvWcsResult/IpvParams` | `ipv_solve_from_detections_v1` `with_callback[_d]` `get_last_inliers` 权威缓存 |
| photometric_calib | `photometric_calib.h` | `pc_*` | 6 函数 + `PhotometricDiag/PcMatchRecord` | `borrowed(gaia_client_handle)`；Tukey `c=4.685` |
| snr_estimator | `snr_estimator.h` | `snr_*` | 22 函数 + `PhotometricCalibrationQuality/PsfFitQualityRow/NoiseWeightModelV1/SnrModelV2/V3` | 三层模型 `1.482602218505602/0.7316727929211932` 冻结；`NoiseWeightModelV1 free` |
| gaia_xpsd_client | `src/gaia_client.h` | `gaia_client_*` | 10 函数 + `GaiaStar/GaiaSpectrumStar/GaiaDbType` | RA 环绕 `cos(dec)` 极区 `C=π/2/C45` |
| healpix_drizzle | `hp_drizzle_api.h` `drizzle_engine.h` 等 | `hp_drizzle_*` `drizzle::*` | 6 函数 + `DrizzleConfig/TileAccumulatorT<Scalar>/PixelAccumulator/DrizzleStats` | `sumVarNum=Σv w² variance=sumVarNum/D²` `k_corr=1.4` |
| healpix_browser_qt | `healpix_browser_core.h` | `BrowserBackend` `STFEngine` | 2 类 + `HealpixMath/GLRenderer` | 只读 HiPS 不解释科学数据 |
| phase2 | `astro/phase2/*.h` (8头) | `p2_*` | ~35 函数 + `P2CoverageResult/P2ControlObservation/P2UpmBuildConfig/P2RejectionMethod/Reason/Status/P2IntegrateStatus` | `p2_upm_build/_geo/_calibrate_block` `p2_reject_stack_ex` `p2_integrate_pixel` `weight_mode=2` |
| orchestrator | `orchestrator.h` 等(7头) | `Orchestrator` | `Orchestrator::run_stage1/2` `Stage1Config/parse_stage1_config` `PipelineStageV2` `PrecisionMode` `AstroCsExitCode(0..10,20..28)` `DllLoader` | `DllLoader` 纯 C ABI 跨界；`stage1.schema.json v1.1` |
| acr | `core/api/scheduler/*` | `acr::*` | `register_phase2_acr_kernels` `TaskDescriptor/KernelRegistry` | 幂等注册 |

三件套互引: `ARCHITECTURE.md §5` 压缩映射 → `API_REFERENCE.md` 全量 → 本报告汇总；签名/错误码/追溯 ID 以头文件注释 + `docs/architecture/ERROR_MODEL.md` + `docs/TRACEABILITY.csv` 为准。

## 2. 机检结果 (本版实测, 超时 600s)

```
tools/docs_machine_consistency.py  PASS 9/9
  config_weight_mode_ivar                — weight_mode=2(ivar) 默认与 stage2_common.cpp 一致
  frame_id_contract_exact                — DATA-FRAME-ID-001 exact
  error_taxonomy_exit_codes              — AstroCsExitCode name+value 全集合 == orchestrator.h 0..10
  integration_status_full_set            — P2_INTEGRATE_* 0..4 == INTEGRATION_ALGORITHMS.md
  rejection_status_full_set              — P2_REASON_*/P2_STATUS_* == REJECTION_ALGORITHMS.md
  stage_ids_docs_vs_orchestrator         — P1.*/P2.* == orchestrator stage_name_v2
  snr_constants                          — 1.4826022185/0.7316728 == noise_model.cpp
  product_contracts                      — signal/support/variance/ivar == aio_hips.h flags
  drizzle_variance_formula               — sumVarNum/D² == drizzle_engine.h

tools/config_consistency_check.py  PASS mismatches=[]
  stage1: precision/gaia_data_dir/nside/pixfrac 与 schema/默认值一致
  stage2: model/reject/profile/normalization/large_scale/typed params 与 stage2_common.cpp 单一来源一致

tools/api_doc_consistency.py  (V17 冻结校验, 供参考)
  需关注: PUBLIC_API.md/api_inventory.md 对 p2_* 新接口的收录完整性 (历史遗留, 本阶段不改 docs/contracts)
```

`docs_machine_consistency` 与 `config_consistency` 为 Stage C 门禁；通过即满足 `ARCHITECTURE.md §7` S8 gate。

## 3. 待改代码的接口/命名不一致清单 (本阶段不改代码，仅清单)

> 约束: `lib/**/src` 禁止修改；下表为只读审计发现的命名/签名不一致，建议下一阶段在冻结窗口外以 ADR 统一。

| ID | 级别 | 位置 | 现状 | 建议 (不改代码, 仅清单) |
|---|---|---|---|---|
| ENG-C-01 | P1 | `lib/common` 公共头路径 `healpix_core.h` vs `lib/common/healpix/healpix_core.h` | `docs/modules/common.md` 缺失(L5 未建)，且头路径在部分 doc 中简写为 `healpix_core.h` 与实际 `healpix/healpix_core.h` 前缀不一致 | 补 `docs/modules/common.md` 并统一 `healpix/healpix_core.h` 前缀 |
| ENG-C-02 | P1 | `lib/gaia_xpsd_client` 头文件 `src/gaia_client.h` vs 预期 `include/gaia_client.h` | 唯一模块将公共头置于 `src/` 而非 `include/`，与其它 12 模块 `include/*.h` 布局不一致 | 迁 `src/gaia_client.h` → `include/gaia_client.h` 并保留 `src/` 转发头兼容 |
| ENG-C-03 | P1 | `healpix_drizzle` 头文件直接置于模块根而非 `include/` | `hp_drizzle_api.h`/`drizzle_engine.h` 等 7 头位于 `lib/healpix_db/healpix_drizzle/` 根，与 `lib/*/include/*.h` 约定不一致 | 迁 `healpix_drizzle/*.h` → `include/` 并更新 `Makefile`/`CMakeLists` 的 `-I` |
| ENG-C-04 | P1 | `lib/common` 双重职责 `healpix_core` + `sha256` 但 `Makefile` 仅 header-only 描述 | `crypto/sha256.cpp` 为编译单元却在 `lib/common/Makefile` 中仅作 header-only 描述，易误导为纯头库 | 在 `MODULE_MAP.md` 与 `lib/common/Makefile` 明确 `crypto/sha256` 为静态编译单元 |
| ENG-C-05 | P1 | `phase2` 公共头 `astro/phase2/*.h` 命名空间 `P2_*` vs 目录 `astro/phase2` | 目录含 `astro/` 前缀但头保护宏与 CMake 暴露路径偶见 `phase2/` 简写，文档中两写法混用 | 统一文档引用为 `astro/phase2/*.h` 并约束 `P2_API` 前缀唯一 |

> 说明: 以上均不影响 `docs_machine_consistency.py 9/9` 与 `config_consistency_check.py` 通过；`api_doc_consistency.py` 报告的 `PUBLIC_API.md` 收录缺口属 Stage B 冻结遗留，不属本阶段接口签名错误。

## 4. 与 L0-L5 / 三阶段落盘一致性

- **L0-L5**: `docs/README-DOCS.md` L3 = `docs/architecture/*.md` + `docs/ARCHITECTURE.md`(Engineering Anchor, 本文 Stage C 主锚) + `docs/API_REFERENCE.md`(接口契约视图) + `工程控制/docs/18_CODE_CHANGE_MAP.md`(Stage C 增补)；L5 `docs/modules/*.md` 13 份与 §5/附录互引；`docs/TRACEABILITY.csv` 76 行 SCI-/ALG-/DATA-/ENG- 全 VERIFIED。
- **三阶段落盘**: Stage A `reports/science_doc_review.md`(10+2 PASS P0=0) → Stage B `reports/algorithm_doc_review.md`(11/11 PASS P0=0) → Stage C `docs/ARCHITECTURE.md` + `docs/API_REFERENCE.md` + `工程控制/docs/18_CODE_CHANGE_MAP.md` (Stage C 增补) + 本报告；单目的提交见 git log。

## 5. 本阶段修改 (最小修改, 不改 `lib/**/src`)

- **保留** `docs/ARCHITECTURE.md` 133 行 (§1-8 全覆盖) 不做语义修改。
- **新建** `docs/API_REFERENCE.md` — 13 模块 + common/browser/acr 全量清单 (与 §5 压缩映射一致，错误码与 `ERROR_MODEL.md` 全集合一致)。
- **增补** `工程控制/docs/18_CODE_CHANGE_MAP.md` — 新增 “Stage C 工程锚定” 一节：接口映射表 + 三阶段落盘路径 + 机检门禁；原 8 项重点面不删。
- **新建** 本报告 `reports/engineering_doc_review.md`。

## 6. 遗留风险

- 待改清单 ENG-C-01..05 需 ADR 排期，当前不阻塞冻结。
- `healpix_stack` 已归档 `archive/legacy` 不重建，`phase2+astro_image_io` 为当前 HCSD 权威 (见 `docs/architecture/MODULE_MAP.md`)。
- 性能基线 `G1-G10` 与浏览器异步/LRU/GPU 仍以 `docs/architecture/PERFORMANCE_MODEL.md` 为准，本阶段未重测全量基准 (按 Stage C 只读约束)。

---
*产出: `docs/ARCHITECTURE.md` + `docs/API_REFERENCE.md` + `工程控制/docs/18_CODE_CHANGE_MAP.md` + 本报告；机检 `9/9` + `mismatches=[]`；commit 单一目的，不改 `lib/**/src`。*
