# 代码修改地图

预计重点代码面：

- `lib/plate_solve/cpp/ipv/`：标准 WCS/SIP 导出和匹配对诊断；
- `lib/orchestrator/`：阶段数据、CLI 事件、provenance 和真实进度；
- `lib/photometric_calib/`：投影/唯一匹配/诊断，不承担坐标补丁；
- `lib/snr_estimator/` 与 Drizzle adapter：完整 WCS/SIP 和 HISS SNR；
- `lib/calibration/`：T1–T4 元数据解析和 Master resolver；
- `lib/astro_image_io/`：HCSD 持久读句柄、批量 leaf API、兼容测试；
- `lib/healpix_db/healpix_stack/`：重叠图、梯度指标、真实 SNR²、进度拆分；
- `lib/healpix_db/healpix_browser_qt/`：异步 Tile、LRU、GPU Renderer、性能 trace。

任何模块修改前先由相应任务冻结接口；避免 Orchestrator、Photometric 和 Drizzle 各自实现一套 WCS 变换。

---

## Stage C 工程锚定 (V19R3 True Final Freeze, 只读不改代码)

本节增补 Stage C 的接口映射与三阶段落盘路径；原 8 项重点面描述不删。上节为执行期代码面地图，本节为接口锚定与文档落盘地图。

### 接口映射 (Public API → Module, 以 `lib/*/include/*.h` 为准)

| 前缀 | 模块 | 头 | 典型函数/类型 |
|---|---|---|---|
| `ac_*` | calibration | `astro_calibration.h` | `ac_generate_master_{bias,dark,flat}[_f64]` `ac_calibrate_frame[_f64]` `ac_correct_frame[_f64]` `ac::optimize_dark_k` |
| `sdet_*` | star_detector | `star_detector.h` | `sdet_create/destroy` `sdet_detect[_ex,_ex_f64,_debug]` |
| `dpsf_*` | dynamic_psf | `dynamic_psf.h` | `dpsf_fit[_batch,_batch_f,_batch_f32,_batch_f64,_batch_d]` `DPSFFitResult` |
| `ipv_*` | plate_solve/ipv | `ipv_api.h` | `ipv_solve{,_from_memory,_from_detections_v1,_from_memory_with_callback[_d]}` `IpvWcsResult` |
| `pc_*` | photometric_calib | `photometric_calib.h` | `pc_calibrate_simple[_with_gaia,_f64,_with_gaia_v2,_with_gaia_f64_v2]` `PcMatchRecord` |
| `snr_*` | snr_estimator | `snr_estimator.h` | `snr_{phot_cal_quality,psf_fit_quality,noise_model_v1[_f64,_fill,_free],noise_scale_law,estimate[_f64],extract_model[_v2,_v3],free_model*}` `NoiseWeightModelV1` |
| `gaia_client_*` | gaia_xpsd_client | `src/gaia_client.h` | `gaia_client_create[_ex]/destroy` `gaia_client_cone_search[_for_solver,_with_spectrum/_photometry]` |
| `hp_drizzle_*` | healpix_drizzle | `hp_drizzle_api.h` `drizzle_engine.h` | `hp_drizzle_{fits_to_ahpx,run,run_hips,reverse_run}` `DrizzleConfig` `TileAccumulatorT<Scalar>` |
| `aio_*` `hiss_*` | astro_image_io | `astro_image_io.h` `aio_hips.h` `aio_hips_reader.h` `aio_pipeline*.h` `aio_upm.h` `hiss_format.h` | `aio_{read,write_fits,ahpx_*,hips_*,pipeline_*,upm_*}` `aio_hips_{product_begin,write_*_tile,finalize,open,read_tile_*,read_snr_catalog,close}` `aio_upm_{write_sparse,open,dense_*}` |
| `astrocs::healpix` `astrocs::crypto` | common | `healpix/healpix_core.h` `crypto/sha256.h` `astro_scalar.h` `precision_context.h` | `ang2pix_nest/pix2ang_nest/parent_nest/child_nest/query_disc/neighbors` `sha256_hex/Sha256` |
| `BrowserBackend` `STFEngine` | healpix_browser_qt | `healpix_browser_core.h` | `BrowserBackend::{open_file,read_tile_*,query_pixel}` `STFEngine::{get_preset,mtf,auto_stretch}` |
| `p2_*` | phase2 | `astro/phase2/*.h` (8头) | `p2_{coverage_build,sample_controls,frame_id,upm_build/_geo/_save/_open/_calibrate_block,block_plan,reject_plan_resolve,collect_candidate_stack,reject_stack_ex,integrate_pixel,...}` `P2RejectionMethod/Reason/Status` `P2IntegrateStatus` |
| orchestrator | orchestrator/cpp | `orchestrator.h` `json_config.h` 等 | `Orchestrator::{run_stage1,run_stage2,set_stage1_config,init_dlls}` `PipelineStageV2` `PrecisionMode` `AstroCsExitCode(0..10,20..28)` |
| `acr::*` | acr | `core/api/scheduler/*` | `register_phase2_acr_kernels` |

完整 13 模块 + common/browser/acr 清单与逐函数契约见 `docs/API_REFERENCE.md`；错误码/追溯 ID 与 `docs/architecture/ERROR_MODEL.md` / `docs/TRACEABILITY.csv` 一致。

### 三阶段落盘路径

- **Stage A** `reports/science_doc_review.md` — `docs/science/*.md` 10+2 篇科学定义冻结核验，`tools/docs_machine_consistency.py 9/9` + `tools/config_consistency_check.py 0 mismatches`，P0=0。
- **Stage B** `reports/algorithm_doc_review.md` — `工程控制/docs 00/05/07/08/09/10/11/17/18/19/24/25` + `PHASE2_IMPLEMENTATION/INTERFACE_FREEZE` + `docs/algorithms/*` 11 篇算法契约核验，P0=0。
- **Stage C** `docs/ARCHITECTURE.md` (133 行 Engineering Anchor, §1-8 权威链/概览/分层/模块表/数据流/压缩接口映射/configs/机检/追溯) + `docs/API_REFERENCE.md` (13 模块 + common/browser/acr 全量清单) + `reports/engineering_doc_review.md` (模块-接口-函数清单汇总 + 待改接口/命名不一致清单 + 机检结果) — 本文件增补与上述 3 份互引。

机检门禁: 每阶段 `tools/docs_machine_consistency.py` (S8 gate) + `tools/config_consistency_check.py` 通过；超时 600s。
