# AstroCS Architecture (Engineering Anchor — Stage C)

> 阶段: Stage C 只读锚定 (V19R3 True Final Freeze) — `lib/*/include/*.h` `lib/*/src/*.cpp` `CMakeLists.txt` `lib/orchestrator/configs/stage1_*.json` `lib/phase2/configs/*.json` `tools/*.py` 为唯一输入；本文件不改代码，仅锚定职责/接口/签名/错误/线程/所有权与科学/算法追溯 ID。机检见 `tools/docs_machine_consistency.py 9/9` + `tools/config_consistency_check.py 0 mismatches`。

权威链: `Wiki(核心约束) → Science(L1) → Algorithm(L2) → Architecture(L3, 本文) → Standards(L4) → Modules(L5) → Source → Test → Diagnostics → Release`；与 `docs/README-DOCS.md` L0-L5 及 `docs/validation/SCIENCE_FREEZE.md` 一致，矛盾以 Wiki 为准。

## 1. System Overview

AstroCS 将单帧 CCD 图像校准并标准化到天球 HiPS 数据库，完成 Phase1 单帧预处理与 Phase2 多帧统一光度集成：

```text
FITS/XISF Light + Master(Bias/Dark/Flat) + Gaia DR3SP
  → astro_image_io(aio_read) → calibration(ac_*) → star_detector(sdet_*)
  → dynamic_psf(dpsf_*) → ipv(ipv_*) → photometric_calib(pc_*)
  → snr_estimator(snr_*) → healpix_drizzle(hp_drizzle_*) + astro_image_io(aio_hips_*)
  → Phase1 HiPS(signal/support/snr/variance/ivar)  [orchestrator.exe <stage1.json>]
  → coverage/sampler/UPM/rejection/integration → Mosaic HiPS
                                                   [astrocs-stage2 <stage2.json>]
  → healpix_browser_qt(只消费不解释科学数据)
```

运行时基座: `lib/acr`(CPU reference + CUDA bridge，`acr_kernels` 幂等注册，科学语义不变)；I/O 唯一入口 `lib/astro_image_io`；通用权威 `lib/common`(healpix_core + sha256)。

## 2. Layering & Dependency Rules

```
orchestrator / stage2 CLI / browser                Application
        phase2 (coverage/sampler/UPM/rejection/integration/block)
        scientific DLLs (calibration, star_detector, dynamic_psf, ipv, photometric, snr, drizzle)
        astro_image_io (FITS/XISF/HiPS/ahpx/compression/UPM+pipeline)
        common (healpix_core, sha256, astro_scalar, precision_context)
```

规则见 `docs/architecture/DEPENDENCY_RULES.md`: 无环；`common` 不依赖业务层；`healpix_drizzle` 依赖 `common/healpix_core`(历史重复已去重至单一实现，见 `docs/algorithms/HEALPIX_MAPPING.md` B4-01)；`astro_image_io` 不依赖 orchestrator/phase2；浏览器仅经 `aio_hips_reader`/`browser_backend` 只读 HiPS。

## 3. Module Map (13 shipping modules + tools)

| 模块 | 路径 | 产物 | 职责 | Public 头 | 构建 |
|---|---|---|---|---|---|
| common | `lib/common` | header-only/static | `healpix_core` NESTED 唯一实现 + `crypto/sha256` + `astro_scalar`/`precision_context` 双精度 ABI | `healpix_core.h` `sha256.h` `astro_scalar.h` `precision_context.h` | `lib/common/Makefile` header-only/静态 |
| astro_image_io | `lib/astro_image_io` | `astro_image_io.dll` | FITS/XISF/HiPS/ahpx/compression/UPM 容器 + `PipelineFrame` 命名块管线 | `astro_image_io.h` `aio_hips.h` `aio_hips_reader.h` `aio_pipeline.h` `aio_pipeline_engine.h` `aio_upm.h` `hiss_format.h` `aio_ahpx_format.h` | `Makefile` (CFITSIO 4.6.4 vendored，`aio_build_config.json`) |
| calibration | `lib/calibration` | `astro_calibration.dll` + `cosmetic_corrector.dll` | 主帧生成/图像校准/坏点修复 | `astro_calibration.h` `cpp/cosmetic_corrector.h` `src/photometry_apply.h` | `lib/calibration/Makefile` + `cpp/Makefile` |
| star_detector | `lib/star_detector` | `star_detector.dll` | 星点检测与质心(`SDetParams`, `sdet_*`) | `star_detector.h` | `lib/star_detector/Makefile` |
| dynamic_psf | `lib/dynamic_psf` | `dynamic_psf.dll` | 动态 PSF 建模(Moffat4 β=4, LM) | `dynamic_psf.h` | `lib/dynamic_psf/Makefile` |
| plate_solve(ipv) | `lib/plate_solve/cpp/ipv` | `ipv_solver.dll` | IPV 星表匹配/plate solve/WCS/SIP(TAN+SIP 2005, Paper II) | `ipv_api.h` `ipv_solver.h` `ipv_wcs.h` `ipv_sip.h` `ipv_types.h` 等14头 | `cpp/ipv` CMake/Make |
| photometric_calib | `lib/photometric_calib` | `photometric_calib.dll` | 测光定标(Fsyn 积分+Tukey IRLS 求 scale) | `photometric_calib.h` | `lib/photometric_calib/cpp/Makefile` |
| snr_estimator | `lib/snr_estimator` | `snr_estimator.dll` | 三层噪声模型: `NoiseWeightModelV1`(blank-sky)/`PsfFitQuality`/`PhotometricCalibrationQuality` | `snr_estimator.h` | `lib/snr_estimator/cpp/Makefile` |
| gaia_xpsd_client | `lib/gaia_xpsd_client` | `gaia_client.dll` | Gaia DR3/DR3SP 锥形查询与 XPSD 解码/缓存 | `gaia_client.h` | `lib/gaia_xpsd_client/Makefile` |
| healpix_drizzle | `lib/healpix_db/healpix_drizzle` | `healpix_drizzle.dll` | 球面 Drizzle(线性重建+Fruchter&Hook, 方差传播 α²v) + 反向 drizzle | `hp_drizzle_api.h` `drizzle_engine.h` `wcs_sip.h` `spherical_overlap.h` 等 | header-only + `healpix_drizzle` Makefile |
| healpix_browser_qt | `lib/healpix_db/healpix_browser_qt` | `healpix_browser_qt.exe` | HiPS 浏览器(Qt6, STF, LOD, GL 3.3) | `healpix_browser_core.h`→`browser_backend.h` `stf_engine.h` `healpix_math.h` `gl_renderer.h` | `CMakeLists.txt` + `Makefile` |
| phase2 | `lib/phase2` | `phase2.a` + `astrocs-stage2.exe` | Phase2 统一光度模型: coverage/sampler/UPM/rejection/integration/block | `astro/phase2/*.h`(8头)+`stage2_common.h` | `lib/phase2/CMakeLists.txt`(C++20) |
| orchestrator | `lib/orchestrator/cpp` | `orchestrator.exe` | Phase1 编排(DllLoader 动态加载，stage1.json 驱动) | `orchestrator.h` `json_config.h` `dll_loader.h` `checkpoint.h` `logger.h` 等 | `lib/orchestrator/cpp/Makefile` (C++17, `-Wl,--stack,33554432`) |
| acr | `lib/acr` | `lib*.a` + `acr_cuda_bridge.dll` | 异构计算抽象(CPU/CUDA)，phase2 ACR kernels | `acr/include/*` `scheduler/*` `backends/cuda/bridge/*` | `lib/acr/CMakeLists.txt` |

`healpix_stack`(`lib/healpix_db/archive/legacy/healpix_stack`) 已归档不重建，当前 HCSD 由 `phase2+astro_image_io` 承担(见 `docs/architecture/MODULE_MAP.md`)；`tools/` 仓根脚本不计入 shipping modules。

完整职责/接口/数据含义/线程/所有权/测试见 `docs/modules/<module>.md`(L5) 与本文 §5 API 一览、`docs/API_REFERENCE.md`。

## 4. Data Flow & Ownership

### 4.1 Phase1 (single-frame)

```
FITS/XISF → aio_read → calibration → star_detector → dynamic_psf → ipv
→ photometric_calib → snr_estimator(NoiseWeightModelV1 patch grid, MAD, 平面场)
→ healpix_drizzle(drizzleTiled/DrizzleConfig, TileAccumulatorT<Scalar>) → aio_hips_write*
→ HiPS(signal/support/snr/variance/ivar, 512 tile, NESTED) + Hiss legacy(仅 validation)
→ orchestrator PipelineFrame 命名块容器 + stage 编排 + 诊断/日志
```

### 4.2 Phase2 (multi-frame mosaic)

```
Phase1 HiPS集 → p2_coverage_build(MOC union, target_order=min leaf)
→ p2_sample_controls(background-clean, control_ivar=k_corr·(π/2)·σ²/N)
→ p2_upm_build/_geo(Huber IRLS, 弱零锚, 连通分量 gauge=min frame_id)
→ aio_upm_* 持久化(sparse 权威, dense cache 可选, stale hash拒绝)
→ p2_block_plan → p2_upm_calibrate_block → p2_collect_candidate_stack
→ p2_reject_stack_ex(7方法 typed, eligibility 分层) → p2_large_scale_apply(可选)
→ p2_integrate_pixel(ivar 加权, max support) → aio_hips_write → verify
```

所有权与生命周期见 `docs/architecture/OWNERSHIP_AND_LIFETIME.md`、`docs/contracts/DATA_SEMANTICS.md`、各头文件 `borrowed/owned/optional` 注释：C API 句柄/模型/reader 由调用方 `*_close/free`；输出 buffer 调用方分配传容量；`aio_upm_read_all_dynamic` 返回 `new char[]` 调用方 `delete[]`；`g_upm_error` 为 `thread_local`。线程模型见 `docs/architecture/THREADING_MODEL.md`：编排层顺序 stage + 模块内 OpenMP parallel-for；ACR `work_pool+device_executor`；浏览器主线程+IO 线程；浮点累积顺序固定(确定性)，reduction 顺序文档化。

## 5. Interface Map (Public API → Module → Science/Algorithm Traceability)

全量清单见 `docs/API_REFERENCE.md`(模块-接口-函数清单表) 与 `reports/engineering_doc_review.md`；此处为压缩映射：

| Public 前缀 | 模块 | 头 | 典型函数/类型(节选) | 科学/算法追溯 ID |
|---|---|---|---|---|
| `ac_*` | calibration | `astro_calibration.h` | `ac_generate_master_{bias,dark,flat}[_f64]` `ac_calibrate_frame[_f64]` `ac_correct_frame[_f64]` `ac::optimize_dark_k` | SCI-CAL-001, `docs/science/CALIBRATION.md` `docs/algorithms/CALIBRATION_ALGORITHMS.md` |
| `sdet_*` | star_detector | `star_detector.h` | `sdet_create/destroy` `sdet_detect[_ex,_ex_f64,_debug]` `SDetParams` | SCI-PSF/star, `STAR_PSF_ALGORITHMS.md` |
| `dpsf_*` | dynamic_psf | `dynamic_psf.h` | `dpsf_fit[_batch,_batch_f,_batch_f32,_batch_f64,_batch_d]` `DPSFFitResult` `DPSFFitParams` | SCI-PSF-*, `PSF.md` `STAR_PSF_ALGORITHMS.md` (Moffat4 β=4 `MOFFAT4_FWHM_FACTOR=1.230310`) |
| `ipv_*` | plate_solve/ipv | `ipv_api.h` | `ipv_solve{,_from_memory,_from_detections_v1,_from_memory_with_callback[_d]}` `ipv_get_last_inlier{,s}_count` `IpvWcsResult` `IpvParams` | SCI-AST-001, `ASTROMETRY.md` `PLATESOLVE.md` 05/24/25 |
| `pc_*` | photometric_calib | `photometric_calib.h` | `pc_calibrate_simple[_with_gaia,_f64,_with_gaia_v2,_with_gaia_f64_v2]` `PcMatchRecord` `PhotometricDiag` | SCI-PHOT-001, `PHOTOMETRY.md` `PHOTOMETRIC_FIT.md` |
| `snr_*` | snr_estimator | `snr_estimator.h` | `snr_{phot_cal_quality,psf_fit_quality,noise_model_v1[_f64,_fill,_free],noise_scale_law,noise_gain_variance,estimate[_f64],extract_model[_v2,_v3],free_model[_v2,_v3]}` `NoiseWeightModelV1` `SnrQualityFlagBits` | SCI-NOISE-001..015, `NOISE_MODEL.md` `NOISE_ESTIMATION.md` 常数 `1.482602218505602`/`0.7316727929211932` 冻结 |
| `gaia_client_*` | gaia_xpsd_client | `gaia_client.h` | `gaia_client_create[_ex]/destroy` `gaia_client_cone_search[_for_solver,_with_spectrum/_photometry]` | SCI-AST/GAIA, `GAIA_QUERY.md` (RA 环绕/C45 polar prune) |
| `hp_drizzle_*` | healpix_drizzle | `hp_drizzle_api.h` `drizzle_engine.h` | `hp_drizzle_{fits_to_ahpx,run,run_hips,reverse_run}` `DrizzleConfig` `PixelAccumulator` `TileAccumulatorT<Scalar>` `DrizzleStats` `compute_auto_nside` | SCI-DRZ-001/014, `DRIZZLE.md` `DRIZZLE_GEOMETRY.md` `sumVarNum/D²` `k_corr=1.4` |
| `aio_*` `hiss_*` | astro_image_io | `astro_image_io.h` `aio_hips.h` `aio_hips_reader.h` `aio_pipeline*.h` `aio_upm.h` `hiss_format.h` 等 | `aio_{read,write_fits,compress,ahpx_*,set_precision_mode,alloc,pipeline_*}` `aio_hips_{product_begin,write_*_tile,finalize}` `aio_hips_{open,read_tile_*,read_snr_catalog,close}` `aio_upm_{write_sparse,open,read_*,dense_*}` `HissWriter/Reader` `HissGridSpec` | SCI-DRZ/SCI-UPM/DATA-HIPS-*, `DRIZZLE.md` `PHASE2_UPM.md` `HEALPIX_MAPPING.md` |
| `astrocs::healpix` `astrocs::crypto` | common | `healpix_core.h` `sha256.h` `astro_scalar.h` `precision_context.h` | `ang2pix_nest/pix2ang_nest/nested_local_to_xy/xy_to_nested_local/parent_nest/child_nest/query_disc/neighbors` `sha256_hex/Sha256` `AstroScalarType/PrecisionContext` | SCI-DRZ/SCI-UPM DATA-FRAME-ID-001, `HEALPIX_MAPPING.md` |
| `BrowserBackend` `STFEngine` `HealpixMath` | healpix_browser_qt | `healpix_browser_core.h` 等 | `BrowserBackend::{open_file,load_hiss,read_tile_*,query_pixel,ud_grade}` `STFEngine::{get_preset,mtf,auto_stretch}` | `15_HCSD_LOD_AND_FORMAT_EVOLUTION.md` BROWSER 规格 |
| `p2_*` | phase2 | `astro/phase2/*.h` | `p2_{coverage_build/free,sample_controls,frame_id,stats_median/mad,upm_build/_geo/_save/_open/_calibrate_block,_raw_weight,_normalized_weights,block_plan,reject_plan_resolve,rejection_semantic_id,eligibility_filter,collect_candidate_stack,reject_stack_ex,large_scale_apply,reject_stack,integrate_pixel,validate_candidate_weights}` `P2CoverageResult` `P2ControlObservation` `P2UpmBuildConfig` `P2RejectionMethod/Reason/Status` 等 | SCI-UPM-001..010/PERSIST/WEIGHT, `PHASE2_UPM.md` `UPM_SOLVER.md` `REJECTION.md` `INTEGRATION.md` 等 |
| orchestrator | orchestrator/cpp | `orchestrator.h` `json_config.h` 等 | `Orchestrator::{run_stage1,run_stage2,set_stage1_config,init_dlls,request_cancel}` `PipelineStageV2` `PrecisionMode` `AstroCsExitCode(0..10,20..28)` `Stage1Config/parse_stage1_config` | 全链编排，`ERROR_MODEL.md` 全集合一致(机检) |
| `acr::*` | acr | `core/api/scheduler/*` | `register_phase2_acr_kernels` `TaskDescriptor` `KernelRegistry` | `ACR-IVAR-001`, `docs/modules/acr.md` |

返回/错误码/线程/所有权等逐函数契约见各头文件注释与 `docs/API_REFERENCE.md`；错误码与 `docs/architecture/ERROR_MODEL.md`(AstroCsExitCode 0-10 进程码+20-28 numeric_code+100 base) 全集合一致(工具 `tools/docs_machine_consistency.py` error_taxonomy 全量校验)，stage IDs `P1.*/P2.*` 与 `ERROR_MODEL.md` 一致。

## 6. Configs & Tooling

- Stage1: `lib/orchestrator/configs/stage1.schema.json`(v1.1) + `stage1.template.json`(pixfrac 默认 0.8, 生产默认收缩滴落) + `stage1_gc_panel{1,2,3}_Red.json`(32=11+11+10, panel1↔panel2/panel2↔panel3 连通, GC 三面板 pixfrac=1.0 无收缩分支用于最大覆盖)；`precision` fp32/fp64、`gaia_data_dir` 必填、`nside` auto/explicit、`pixfrac` (0,1] 默认 0.8 / GC 1.0 分支见 `lib/orchestrator/configs/`；校验 `validate_stage1_schema` (nlohmann-json-schema-validator v2.4.0，`orchestrator.h` 锚点)。
- Stage2: `lib/phase2/configs/stage2_*.json`(inputs/model/integration/output/diagnostics)；`model` control 网格/sigma_floor/support_power/use_ivar_weight 等，`reject_method/profile/normalization/large_scale/typed params` 冻结(V17)，`weight_mode` 2=ivar 默认，`acr_route` auto；默认值单一来源 `lib/phase2/src/stage2_common.cpp`，与 `docs/development/CONFIG_SCHEMA.md` + `tools/config_consistency_check.py` 一致(`mismatches=[]`)。
- 工具: `tools/docs_machine_consistency.py`(9 checks，见 §7)，`config_consistency_check.py`，`api_doc_consistency.py`，`no_legacy_production_reference.py` 等。

## 7. Machine Consistency (S8 gate, 本版实测)

```
tools/docs_machine_consistency.py PASS 9/9
  config_weight_mode_ivar, frame_id_contract_exact(DATA-FRAME-ID-001 exact),
  error_taxonomy_exit_codes(AstroCsExitCode name+value 全集合==orchestrator.h 0-10),
  integration_status_full_set(P2_INTEGRATE_*==INTEGRATION_ALGORITHMS.md),
  rejection_status_full_set(P2_REASON_*/P2_STATUS_*==REJECTION_ALGORITHMS.md),
  stage_ids_docs_vs_orchestrator, snr_constants(1.4826022185/0.7316728),
  product_contracts(signal/support/variance/ivar), drizzle_variance_formula(sumVarNum)
tools/config_consistency_check.py PASS mismatches=[]
```

## 8. Traceability

- 科学 → 算法 → 工程映射见 `docs/TRACEABILITY.csv`(76 行，SCI-/ALG-/DATA-/ENG- 全 VERIFIED，`MUST` family `docs/TRACEABILITY_family.json` + `gen_v19_source_snapshot` 绑定实现符号/测试)；Stage C 由 `reports/science_doc_review.md`(10+2 PASS P0=0) + `reports/algorithm_doc_review.md`(11/11 PASS P0=0) 延续，本文件为工程锚定增量。
- 模块级追溯见 `docs/modules/*.md`(13 份 L5) 与本文件 §5/附录。

---
*Stage C 产出: 本文件 + `docs/API_REFERENCE.md` + `工程控制/docs/18_CODE_CHANGE_MAP.md` + `reports/engineering_doc_review.md`；单目的 commit，超时 600s，不改 `lib/**/src` 代码(纯文档同步)。*
