# AstroCS API Reference (Engineering Anchor — Stage C)

> 阶段: Stage C 只读锚定 (V19R3 True Final Freeze) — 唯一输入 `lib/*/include/*.h` `lib/orchestrator/configs/*` `lib/phase2/configs/*` `tools/*.py`；本文件不改代码，仅锚定公共头暴露的函数/类型签名节选、参数/返回值/错误码、线程/所有权契约与追溯 ID。与 `docs/ARCHITECTURE.md §5` 压缩映射一致、与 `docs/architecture/ERROR_MODEL.md` 错误码全集合一致，契约细节以头文件注释为准。

权威链: `Wiki → Science(L1) → Algorithm(L2) → Architecture(L3, ARCHITECTURE.md) → Standards(L4) → Modules(L5) → 本清单(L3 附录, 接口契约视图) → Source → Test`；机检 `tools/docs_machine_consistency.py 9/9` + `tools/config_consistency_check.py 0 mismatches`。

表头: 模块 | 头文件 | 前缀 | 函数/类型(签名节选) | 参数/返回值/错误码要点 | 追溯ID
线程/所有权标记: `borrowed` 调用方不拥有 / `owned` 调用方须释放 / `thread_local` 线程隔离 / `optional(nullptr)` 可空

---

## 1. common (header-only + static, 共享权威)

| 模块 | 头文件 | 前缀 | 函数/类型(签名节选) | 参数/返回值/错误码要点 | 追溯ID |
|---|---|---|---|---|---|
| common | `lib/common/healpix/healpix_core.h` | `astrocs::healpix` | `uint64_t ang2pix_nest(uint32_t nside, double ra, double dec)` / `void pix2ang_nest(uint32_t nside, uint64_t ipix, double& ra, double& dec)` / `nested_local_to_xy / xy_to_nested_local / parent_nest / child_nest / query_disc / neighbors / leaf_to_tile_nest` | 仅 header + `healpix_core.cpp` 实现；`nside` 2^n；`ang2pix` 内归一 `ra` 任意值 `dec∈[-90,90]`；`pix2ang` 越界 `ra=dec=0`；NESTED 唯一实现 (`common/healpix_core` 被 `healpix_drizzle`/`astro_image_io`/`browser` 复用，禁止第二套) | SCI-DRZ/UPM `docs/science/HEALPIX_MAPPING.md` `docs/algorithms/HEALPIX_MAPPING.md` B4-01 去重 |
| common | `lib/common/crypto/sha256.h` | `astrocs::crypto` | `string sha256_hex(const void* data,size_t len)` / `class Sha256 { update(); final_hex(); }` | `sha256_hex` 纯函数；`Sha256` 增量，`final_hex` 后禁止再 `update`；供 `aio_upm` 校验 `source_hash/checksum` 与 `phase2` `model_hash` | DATA-FRAME-ID-001, `docs/architecture/IO_AND_ATOMICITY.md` |
| common | `lib/common/include/astro_scalar.h` | `AstroScalarType` | `enum AstroScalarType:uint8_t {FP32=0,FP64=1}` / `AstroScalarTraits<S>` / `astro_scalar_type_name/size` / `ASTRO_SCALAR_DISPATCH` | `uint8_t` ABI 稳定；`DISPATCH` 运行时→编译时 `float/double` 分发 | SCI-DRZ `docs/science/DRIZZLE.md` 双精度 ABI |
| common | `lib/common/include/precision_context.h` | `PrecisionContext` | `PrecisionContext::instance().set_scalar_type / scalar_type() / is_fp32/is_fp64` | 单例全链路统一精度；仅启动阶段写入、数据阶段只读无锁；默认 `FP32` 历史兼容；跨 DLL 由 `aio_set_precision_mode` 显式传递 | 同上，`lib/orchestrator Stage1Config.precision` |

---

## 2. astro_image_io (`astro_image_io.dll`, 唯一 I/O 入口)

| 模块 | 头文件 | 前缀 | 函数/类型(签名节选) | 参数/返回值/错误码要点 | 追溯ID |
|---|---|---|---|---|---|
| aio | `astro_image_io.h` | `aio_*` | `void aio_set_precision_mode(int is_fp64)` / `AIOImageData* aio_read[_fits/_xisf/_header_only](const char* path)` / `AIOImageMetadata aio_read_metadata` / `float* aio_get_pixel_data / double* aio_get_pixel_data_f64 / uint8_t aio_get_dtype` / `AIOImageMetadata/Geometry/Options aio_get_*` / `aio_free_image_data` | `aio_read*` 返回 `owned(AIOImageData*)` 调用方 `aio_free_image_data`；`aio_get_pixel_data*` `borrowed` 指针随 `AIOImageData` 生命周期；`get_f64` 仅 FP64 模式非 `nullptr`；`aio_write_fits` 经 vendored CFITSIO 4.6.4 含 checksum | SCI-DRZ `docs/science/DRIZZLE.md`；`docs/architecture/OWNERSHIP_AND_LIFETIME.md` |
| aio | `astro_image_io.h` | `aio_*` | `size_t aio_compress/decompress/compress_bound(const void* src,... int codec,int level)` / `int aio_ahpx_{write,read_header,read_pixels,read_snr}(const char* path,...)` | `codec 0=NONE 1=ZSTD 2=LZ4`；`ahpx` 容器 `pixel/weight/snr` 块；`compress_bound` 预分配；`ahpx_read_*` 调用方分配 `out` 传 `capacity` | 同上 |
| aio | `aio_hips.h` | `aio_hips_*` | `enum AioHipsProductFlag {SIGNAL=1,SUPPORT=2,SNR=4,VARIANCE=8,IVAR=16}` / `enum AioHipsDataType {FLOAT32=0,FLOAT64=1}` / `struct AstroSphereTileView {parent_ipix,leaf_order,width,data_type,flux_sum,covered_area,valid_mask,var_num_sum}` / `AioHipsProductSet* aio_hips_product_begin(const char* out_dir,uint32_t nside,... int flags,...)` / `aio_hips_write_{signal_support,variance}_tile / aio_hips_write_snr_points / aio_hips_set_drizzle_provenance / aio_hips_finalize / aio_hips_last_error` | `product_begin` 返回 `owned(AioHipsProductSet*)` 失败 `NULL` 查 `last_error`；`TileView` 各指针 `borrowed(optional)`；`var_num_sum` 可 `NULL` 跳过 `variance/ivar`；`signal=flux_sum/covered_area` `support=covered_area/Acell` `variance=sumVarNum/D²` `ivar=1/variance`；tile `width=512` NESTED | SCI-DRZ `docs/science/DRIZZLE.md` `15_HCSD_LOD_AND_FORMAT_EVOLUTION.md` |
| aio | `aio_hips_reader.h` | `aio_hips_*` | `enum AioHipsProduct {SIGNAL=0,SUPPORT=1,SNR=2,VARIANCE=3,IVAR=4}` / `AioHipsDataset* aio_hips_open(const char* out_dir,int product)` / `aio_hips_{get_properties,tile_count,tile_ipix,read_tile_{f32,f64},read_leaf_{f32,f64},read_tile_datasum,read_snr_catalog,close,last_error}` | `open` 返回 `owned(AioHipsDataset*)` 用 `close` 释放；`read_tile_*` 调用方分配 `512*512` row-major `out`；`read_snr_catalog` 调用方分配 `ra/dec/snr/star_id/quality_flags/photometric_status` 数组；`reader` 为 Browser/HIPS_VERIFY 唯一后端 | 同上 |
| aio | `aio_pipeline.h` / `aio_pipeline_engine.h` | `aio_pipeline_*` | `struct PipelineFrame {blocks,n_blocks,capacity,stages_completed}` / `AioBlock {name[64],type,data,count,dims[4],n_dims}` / `AioBlockType 0..6` / `AioKVEntry {key[64],value[256]}` / `AstroAbiInfo / aio_abi_info / aio_alloc/realloc/free` / `PipelineFrame* aio_pipeline_frame_create()` / `aio_pipeline_frame_destroy / aio_pipeline_{add,find,remove}_block* / aio_save_cache / aio_load_cache` / `PipelineEngine* aio_pipeline_engine_create()` / `aio_pipeline_engine_{register,set_debug,set_auto_free,set_block_drop,run_single,run_batch} / aio_pipeline_stage_name` | `frame_create` `owned(PipelineFrame*)` 配 `destroy`；块 `data` 由 `aio_alloc` 分配、由 `frame` 拥有 (`add_block_move` 转移)；`PipelineStageHandler` 签名 `(PipelineFrame* frame,const void* params,char* err,int cap)`；`register` `borrowed(params)`；`run_single/batch` 返回 `0/-成功` 错误进 `err` | DATA 契约 `docs/contracts/DATA_SEMANTICS.md` `docs/architecture/PIPELINE.md` |
| aio | `aio_upm.h` | `aio_upm_*` | `int aio_upm_write_sparse(const char* path,const char* model_json)` / `AioUpmSparse* aio_upm_open(const char* path)` / `aio_upm_{read_info,read_all,read_all_dynamic,close}` / `AioUpmDense* aio_upm_dense_begin(const char* path,const char* source_hash,int target_order,uint32_t prec,uint64_t frames,uint64_t tiles)` / `aio_upm_dense_{write_tile,end,abort,info,read_dense_block} / aio_upm_last_error` | `open/begin` `owned` 句柄配 `close/end/abort`；`read_all` 调用方分配传容量；`read_all_dynamic(new char[])` `owned(*out)` 调用方 `delete[]`；`dense` 按 `(frame_index,tile)` 单调流式写；`source_hash` 不一致返回 `2 stale` 拒绝；`checksum SHA-256`；`g_upm_error thread_local` | SCI-UPM-PERSIST `docs/science/PHASE2_UPM.md` `docs/algorithms/UPM_SOLVER.md` |
| aio | `hiss_format.h` / `aio_healpix_io.h` / `aio_ahpx_format.h` | `hiss_*` / `aio_hiss_*` / `aio::ahpx` | `struct HissGridSpec / HissWriter/Reader {add_tile_{f32,f64,ivar},read_tile_*,close}` / `HioSnrModel / HioSnrControlPoint(20B)/F64(24B)` / `aio_hiss_{write,read,write_snr_model,read_snr_model}` / `ahpx::{MAGIC,VERSION,BlockIndex,WeightInfo,WeightMode,Codec}` | Hiss 512 tile NESTED 稀疏容器 legacy 仅 validation；`nested_local_to_fits_index` 标准 HiPS 排列；ahpx `HEADER_FIXED_SIZE=18` | SCI-DRZ/HIPS `docs/architecture/IO_AND_ATOMICITY.md` |

契约要点: 所有 `*_open/begin/create` 配 `*_close/end/destroy`；输出 `buf` 调用方分配；`aio_upm_last_error / aio_hips_last_error` `thread_local`；`AstroAbiInfo capability_bits` (BASIC/CACHE_V1/ALLOCATOR/FP64)。

---

## 3. calibration (`astro_calibration.dll` + `cosmetic_corrector.dll`)

| 模块 | 头文件 | 前缀 | 函数/类型(签名节选) | 参数/返回值/错误码要点 | 追溯ID |
|---|---|---|---|---|---|
| calibration | `astro_calibration.h` | `ac_*` | `int ac_generate_master_{bias,dark,flat} (const float* stack,int n_frames,int w,int h,float* out, float sigma_low/high,int max_iter,int combine)` / `int ac_calibrate_frame(const float* light,int w,int h,const float* master_dark/flat/bias,float* out,int dark_opt,float k,float* actual_k)` / `int ac_correct_frame(const float* data,int w,int h,const float* dark/bias,float* out,float hot/cold_sigma,int method,int max_struct,int* out_hot/cold)` / `ac_*_f64` 变体 `(const double* ... double* out)` / `void ac_set_num_threads(int)` / `const char* ac_version()` / `namespace ac { float optimize_dark_k(const float* light/bias/dark/flat,int w,int h,float k_init,hiss::Stage1Diagnostics& diag); }` | `0=AC_OK -1=AC_ERR_PARAM -2=MEMORY -3=INTERNAL`；输入 `stack` `[n_frames*H*W]` 行主序；`master_*` 可 `nullptr`；`out` 调用方分配；`flat` 未提供跳过除法；`_f64` 统计路径内转 `float` 仅 `calibrate_f64` 真双精度算术；`optimize_dark_k` `hiss::Stage1Diagnostics` 回退语义 | SCI-CAL-001 `docs/science/CALIBRATION.md` `docs/algorithms/CALIBRATION_ALGORITHMS.md` |

---

## 4. star_detector (`star_detector.dll`)

| 模块 | 头文件 | 前缀 | 函数/类型(签名节选) | 参数/返回值/错误码要点 | 追溯ID |
|---|---|---|---|---|---|
| star_detector | `star_detector.h` | `sdet_*` | `struct SDetParams {structureLayers,hotPixelFilterRadius,iterativeClipSigma,iterativeMaxRounds,medianFilterDetail,maxStars,fitRadius,fwhmClipSigma,maxAxisRatio}` / `typedef StarDetectorHandle` / `StarDetectorHandle sdet_create(const SDetParams* p)` / `void sdet_destroy(handle)` / `int sdet_detect(handle,const uint16_t* img,int w,int h,double** out_x,double** out_y,int* out_count)` / `sdet_detect_debug / sdet_detect_ex / sdet_detect_ex_f64(const double* img,... double** out_x/y,float** out_flux,int** out_sat,float** out_mag,int** out_has_sat,... const char** extra_names,int extra_cnt,float*** out_extras)` / `void sdet_free_{coords,debug_maps,detect_ex}` | `sdet_create` `owned(handle)` 配 `destroy`；`*_detect*` `owned(*out_*)` 配 `sdet_free_*`；`detect_ex_f64` 不降级 `float`；`star_det v1` `[N,6]=x/y/flux/mag/saturated/has_saturated` (FP64) 被 `dynamic_psf/dpsf_fit_batch_f32` 消费 | SCI-PSF/star `docs/science/PSF.md` `docs/algorithms/STAR_PSF_ALGORITHMS.md` |

---

## 5. dynamic_psf (`dynamic_psf.dll`)

| 模块 | 头文件 | 前缀 | 函数/类型(签名节选) | 参数/返回值/错误码要点 | 追溯ID |
|---|---|---|---|---|---|
| dynamic_psf | `dynamic_psf.h` | `dpsf_*` | `struct DPSFFitResult {status,B,A,cx,cy,sx,sy,theta,fwhm_x/y,mad,flux,eccentricity}` / `DPSF_FIT_OK/NO_CONVERGE/INVALID_PARAMS/ITER_LIMIT` / `struct DPSFFitParams {fitRadius,maxIter,tolerance}` / `int dpsf_fit(const uint16_t* img,int w,int h,double cx,double cy,const DPSFFitParams* p,DPSFFitResult* r)` / `int dpsf_fit_batch(const uint16_t* img,... const double* cx/cy,int cnt,const DPSFFitParams* p,DPSFFitResult** out)` / `int dpsf_fit_batch_f(const float* img,...)` / `int dpsf_fit_batch_f32(const float* img,int w,int h,const double* detections,int n, const DPSFFitParams* p,double* out_psf[N*9],int* n_valid)` / `int dpsf_fit_batch_f64(const double* img,...)` / `int dpsf_fit_batch_d(const double* img,... const double* cx/cy,int cnt,... DPSFFitResult** out)` / `void dpsf_free_results(DPSFFitResult*)` | `fit_batch*` `owned(*out)` 配 `free_results`；`f32/f64` 直消 `float/double` 不经 `uint16 clip`；`[N*9]=B/A/cx/cy/sx/sy/theta/fwhm_x/fwhm_y` 失败置 `NaN`；Moffat4 β=4 `MOFFAT4_FWHM_FACTOR=1.230310` `flux=2πA·sxsy/3` | SCI-PSF `docs/science/PSF.md` `docs/algorithms/STAR_PSF_ALGORITHMS.md` |

---

## 6. plate_solve / ipv (`ipv_solver.dll`)

| 模块 | 头文件 | 前缀 | 函数/类型(签名节选) | 参数/返回值/错误码要点 | 追溯ID |
|---|---|---|---|---|---|
| ipv | `ipv_api.h` (+ `ipv_solver.h/ipv_wcs.h/ipv_sip.h/ipv_types.h` 等14头) | `ipv_*` | `struct IpvWcsResult {cd[4],crval[2],crpix[2](1-based),sip_order,sip_a/b[36],sip_ap_order,sip_ap/bp[36],rms_px/arcsec,n_pairs,success,n_detected/n_catalog,trans_order,best_inliers,ctype1/2[16],error_msg[256]}` / `struct IpvParams {polygon_sides,n_pivot,sigma_d_arcsec,vote_threshold,ransac_max_iter,ransac_thresh,s_min/s_max,img_n_target,gaia_density_ratio,log_dir[256]}` / `void* ipv_solve_create()` / `void ipv_solve_destroy(void* s)` / `void ipv_set_{gaia,detector}_handle(void* s,intptr_t h)` / `int ipv_solve(void* s,const char* image_path,double ra0,dec0,focal_mm,pixel_um,const IpvParams* p,IpvWcsResult* r)` / `int ipv_solve_from_memory(void* s,const float* px,int w,int h,...)` / `int ipv_solve_from_detections_v1(void* s,const double* det[N*6],int n,int w,int h,...)` / `int ipv_solve_from_memory_with_callback[_d](void* s,const float/double* px,int w,int h, IpvDetectionCallback cb,void* ud,IpvWcsResult* r)` / `void ipv_get_default_params(IpvParams*)` / `int ipv_get_last_inlier_count(void* s)` / `int ipv_get_last_inliers(void* s,double* buf,int max)` 每行 `9*double = det_x/y, gaia_ra/dec, pred_x/y, res_x/y, dist` | `create` `owned(void*)` 配 `destroy`；`*_handle` `borrowed(opaque)`；`*_solve*` 返回 `1=success 0=fail` 结果进 `IpvWcsResult`；`_with_callback_d` 双精度不降级；`get_last_inliers` 线程内权威缓存 (供 WCS Gate v2 双层闭环) | SCI-AST-001 `docs/science/ASTROMETRY.md` `docs/algorithms/PLATESOLVE.md` 05/24/25 |

---

## 7. photometric_calib (`photometric_calib.dll`)

| 模块 | 头文件 | 前缀 | 函数/类型(签名节选) | 参数/返回值/错误码要点 | 追溯ID |
|---|---|---|---|---|---|
| photometric_calib | `photometric_calib.h` | `pc_*` | `struct PhotometricDiag {spectrum_rows_total,valid_fsyn,gaia_projected_in_frame,psf_total/valid,spatial_candidates,unique_matches,rejected_{ambiguous,distance,quality},fit_used,robust_iterations,scale_factor,sigma_residual,r_median/p90/max,match_distance_median/p90/max}` / `struct PcMatchRecord {star_id,dr3sp_id,reference_flux(Fsyn),residual(r=log10(Finstr/Fsyn)),status(0..3),reject_reason(0..6)}` / `int pc_calibrate_simple(const float* px,int w,int h, const double* gaia_ra/dec/mag/fsyn,int n_gaia,const double* psf_cx/cy/flux,const int* psf_status,int n_psf, const double* qe_wl/trans,int qe_cnt, double crval/crpix/cd...,int sip_order,const double* sip_a/b/ap/bp, float* out_px,int* n_matched,double* scale,double* sigma,PhotometricDiag* diag)` / `pc_calibrate_simple_f64 / pc_calibrate_simple_with_gaia[_f64,_with_gaia_v2,_with_gaia_f64_v2](void* gaia_client_handle,double ra_center,dec_center,radius_deg,... double* out_psf_params,int* n_valid, PcMatchRecord* out_records,...)` | `0=success <0=fail`；`gaia_client_handle borrowed`；`out_*` 调用方分配；`qe` 可 `nullptr`；`sigma_residual=MAD(r_inliers)/0.6745` `scale=10^{-location}` Tukey `c=4.685 max_iter=50 tol=1e-6`；饱和 `status!=0 \|\| qf & SATURATED` 拒绝 | SCI-PHOT-001 `docs/science/PHOTOMETRY.md` `docs/algorithms/PHOTOMETRIC_FIT.md` |

---

## 8. snr_estimator (`snr_estimator.dll`, 三层噪声模型)

| 模块 | 头文件 | 前缀 | 函数/类型(签名节选) | 参数/返回值/错误码要点 | 追溯ID |
|---|---|---|---|---|---|
| snr | `snr_estimator.h` | `snr_*` | `struct PhotometricCalibrationQuality {sigma_logflux_dex,sigma_mag=2.5*dex,sigma_cal_rel=ln10*dex,n_matches,fit_status}` / `int snr_phot_cal_quality(double sigma,int n,PhotometricCalibrationQuality* out)` / `struct PsfFitQualityRow {flux,amplitude_above_bg,background,fwhm,eccentricity,residual_scale,robust_residual_sigma=residual_scale/0.7316728,q_psf=A/residual_scale,fit_status}` / `int snr_psf_fit_quality(const double* psf[N*9],int n,const int64_t* ids,const uint32_t* qf,PsfFitQualityRow* out)` / `struct SnrNoiseModelConfig / struct NoiseWeightModelV1 {h,w,n_points,control_points,coeff_a/b/c,variance_floor,degenerate}` / `int snr_noise_model_v1_default_config(SnrNoiseModelConfig* cfg)` / `int snr_noise_model_v1[_f64](const float/double* data,int h,int w,const float* mask,const double* star_x/y,int n,const SnrNoiseModelConfig* cfg,NoiseWeightModelV1* out)` / `int snr_noise_model_v1_fill(const NoiseWeightModelV1* m,int h,int w,float* var,float* ivar)` / `void snr_noise_model_v1_free(NoiseWeightModelV1*)` / `void snr_noise_scale_law(double alpha,double* var,double* ivar)` / `double snr_noise_gain_variance(double signal,double gain,double rn)` / `int snr_estimate[_f64](const float/double* data,int h,int w,const double* psf,int n,double sigma_residual,float* out_snr)` / `struct SnrWcsParams/SnrSipCoeffs` / `enum SnrDropReason 0..255` / `struct SnrControlPoint(20B)/F64(24B)/V3(36B)/F64V3(40B) #pragma pack(1)` / `struct SnrModel/V2/V3 {n_points,value_dtype,points,snr_phot,median_snr,idw_power}` / `int snr_extract_model[_v2,_v3](const double* psf,int n,double sigma,const SnrWcsParams* wcs,int dtype,const int64_t* ids,const uint32_t* qf,const uint32_t* photo,SnrModel* out)` / `void snr_free_model[_v2,_v3](SnrModel*)` / `enum SnrQualityFlagBits {PSF_OK=1,SATURATED=2,HAS_SATURATED=4,PHOTO_MATCHED=8,PHOTO_REJECTED=16}` | `0=成功 1=退化 3=nullptr`；`NoiseWeightModelV1` `owned` 内数组配 `free`；`σ_bg=1.482602218505602*MAD` 8×8 patch `5σ≤2轮` 平面场 `a+bx+cy` `floor 1e-12`；`snr_scale_law: x'=αx→var'=α²var,ivar'=ivar/α²`；`estimate` 系遗留 heuristic 已不在生产权重 | SCI-NOISE-001..015 `docs/science/NOISE_MODEL.md` `docs/algorithms/NOISE_ESTIMATION.md` 常数冻结 |

---

## 9. gaia_xpsd_client (`gaia_client.dll`)

| 模块 | 头文件 | 前缀 | 函数/类型(签名节选) | 参数/返回值/错误码要点 | 追溯ID |
|---|---|---|---|---|---|
| gaia | `gaia_client.h` (`src/gaia_client.h`) | `gaia_client_*` | `enum GaiaDbType {AUTO=0,DR3=1,DR3SP=2}` / `struct GaiaStar {ra,dec,magG/BP/RP,parallax,pmra/pmdec,source_id}` / `struct GaiaSpectrumStar {ra,dec,magG,flux_min/mul}` / `GaiaClient* gaia_client_create[_ex](const char* data_dir,GaiaDbType t)` / `void gaia_client_destroy(GaiaClient*)` / `int gaia_client_cone_search[_for_solver,_with_spectrum,_with_photometry,_query_spectrum_by_coords](GaiaClient* c,double ra,dec,radius_deg,double mag_low/high,GaiaStar** out,int* n)` / `int gaia_client_get_{db_type,file_count,total_sources,spectrum_params}(GaiaClient*)` | `create` `owned(GaiaClient*)` 配 `destroy`；`*_search*` `owned(*out)` 调用方 `free`；`data_dir` 必填；RA 环绕 `cos(dec)` 归一、极区 `C=π/2 C45=π/(2√2)` 剪枝 `false_negative=0`；`QueryCache 64 TTL60s / BlockCache 8192/4GB LRU` `cache_lock` 互斥线程安全 | SCI-AST/GAIA `docs/algorithms/GAIA_QUERY.md` |

---

## 10. healpix_drizzle (`healpix_drizzle.dll`, 球面 Drizzle)

| 模块 | 头文件 | 前缀 | 函数/类型(签名节选) | 参数/返回值/错误码要点 | 追溯ID |
|---|---|---|---|---|---|
| drizzle | `hp_drizzle_api.h` / `drizzle_engine.h` / `wcs_sip.h` / `spherical_overlap.h` 等 | `hp_drizzle_*` | `struct HpDrizzleResult {n_healpix_pixels,n_source_pixels,nside,nested,pixfrac,elapsed_sec,error_msg[512]}` / `int hp_drizzle_fits_to_ahpx(const char* fits,const char* out,int nside,int nested,double pixfrac,const char* snr/weight,HpDrizzleResult* r)` / `int hp_drizzle_run(PipelineFrame* frame,int nside,int nested,double pixfrac,const char* out,HpDrizzleResult* r,int prec)` / `int hp_drizzle_run_hips(PipelineFrame* frame,int nside,int nested,double pixfrac,const char* hips_dir,const char* legacy_hiss,HpDrizzleResult* r,int prec)` / `struct HpReverseDrizzleInput {nside,nested,target_w/h,pixfrac,output_fp64,crval/crpix/cd,sip_order/ap_order,sip_a/b/ap/bp,leaf_ipix,n_leaf,leaf_signal_f32/f64,leaf_support,no_data_as_zero}` / `int hp_drizzle_reverse_run(const HpReverseDrizzleInput* in,void* sig_out,void* cov_out,HpReverseDrizzleResult* r)` / `uint32_t hp_drizzle_reverse_capability()` / `const char* hp_drizzle_reverse_version(void)` | 返回 `0=成功 非0=失败`；`run` 从 `PipelineFrame data/header` 直通；`pixfrac∈(0,1] 默认0.8`；`nside=0 auto` 经 `compute_auto_nside` Jacobian；`precision_mode 0=FP32 1=FP64` | SCI-DRZ-001/014 `docs/science/DRIZZLE.md` `docs/algorithms/DRIZZLE_GEOMETRY.md` |
| drizzle | `drizzle_engine.h` | `drizzle::*` | `struct DrizzleConfig {nside,nested,pixfrac,apply_photometry,photscal,photometry_applied_upstream,precision_mode,threads,tile_depth}` / `struct PixelAccumulator {sumFlux,sumWeight,sumSnrSq,sumArea,sumVarNum,nContrib}` / `template<Scalar> struct TileLeafAccumulatorT {sumFlux,sumArea,sumVarNum,nContrib}` / `template<Scalar> struct TileAccumulatorT {parent_ipix,pixels[],touched[]}` / `int compute_auto_nside(const WcsParams& wcs,int w,int h)` / `class DrizzleEngine {drizzle(const FitsImage&,...,unordered_map<uint64_t,PixelAccumulator>&,DrizzleStats&,...); drizzleTiled(... vector<TileAccumulatorT<Scalar>>& ...);}` / `struct DrizzleStats {nHealpixPixels,nSourcePixels,nside,op_* counters}` | `sumVarNum=Σv·w²` `variance=sumVarNum/D²` `sumVarNum α²` 缩放；三层缓冲 `1.25×hp_res / 3.0× / 1.25×1.15`；`TileAccumulatorT` 线程本地按 `parent_ipix` 分组 `local_ipix` 连续寻址；`tile_depth=9→512` HIPS | 同上, `k_corr=1.4`, `sumVarNum/D²` |

---

## 11. healpix_browser_qt (`healpix_browser_qt.exe`, 只读 HiPS 浏览器)

| 模块 | 头文件 | 前缀 | 函数/类型(签名节选) | 参数/返回值/错误码要点 | 追溯ID |
|---|---|---|---|---|---|
| browser | `healpix_browser_core.h` → `browser_backend.h` `stf_engine.h` `healpix_math.h` `gl_renderer.h` | `BrowserBackend` `STFEngine` `HealpixMath` | `class BrowserBackend {open_file/load_hiss, read_tile_{f32,f64}, query_pixel, ud_grade, get_properties, tile_count/ipix}` / `class STFEngine {get_preset,mtf,auto_stretch,apply}` / `class HealpixMath {ang2pix/pix2ang wrappers}` / `class GLRenderer {init/draw/tile_cache}` | 仅经 `aio_hips_reader`/`browser_backend` 只读 HiPS，不解释科学数据；主线程+IO 线程；LRU 异步 Tile；GL 3.3 Core；浮点顺序固定 | `docs/architecture/PIPELINE.md` Browser 规格 BROWSER-001 |

---

## 12. phase2 (`phase2.a` + `astrocs-stage2.exe`, 统一光度模型)

| 模块 | 头文件 | 前缀 | 函数/类型(签名节选) | 参数/返回值/错误码要点 | 追溯ID |
|---|---|---|---|---|---|
| phase2 | `astro/phase2/coverage.h` | `p2_*` | `struct P2MocCell/P2HipsInputInfo/P2CoverageResult {inputs,union_cells,target_order}` / `int p2_coverage_build(const char* const* hips_paths,uint64_t n,P2CoverageResult* out)` / `int p2_coverage_free(P2CoverageResult*)` | `union = MOC union` `target_order = min leaf`；`coverage_build` `owned` 结果配 `free`；返回 `0/非0` | SCI-UPM `docs/science/PHASE2_UPM.md` `docs/algorithms/UPM_SOLVER.md` |
| phase2 | `astro/phase2/sampler.h` | `p2_*` | `struct P2SamplerConfig / P2ControlObservation` / `int p2_sample_controls(const P2CoverageResult* cov,const char* const* hips_paths,const P2SamplerConfig* cfg,P2ControlObservation** out,uint64_t* n)` / `int p2_frame_id(const char* path,uint64_t* out)` / `double p2_stats_{median,mad}(const double* v,uint64_t n,...)` | `background-clean` 仅 `≥2 clean` 帧重叠区采样；`control_ivar=k_corr·(π/2)·σ²/N_retained` `k_corr=1.4`；`N_retained` 裁剪后；`frame_id = stable SHA-256 前16hex` 与路径无关 | 同上, `docs/algorithms/PHASE2_SAMPLER.md` |
| phase2 | `astro/phase2/upm.h` | `p2_*` | `struct P2ControlObservation {frame_id,control_id,leaf_ipix,ra/dec,value,uncertainty,snr,ivar,control_variance/ivar,snr_available,support,quality_flags}` / `struct P2ModelInfo / P2UpmBuildConfig {robust_loss,snr_weight_mode,huber_delta,smoothing_lambda,zero_anchor_weight,max_iter,tolerance,target_order,sigma_floor,support_power,quality_mode,use_ivar_weight,control_reliability,input_manifest_hash}` / `int p2_upm_build[_geo](const P2ControlObservation* obs,uint64_t n,const P2ControlNode* nodes,uint64_t nn,const P2UpmBuildConfig* cfg,void** out_model)` / `int p2_upm_{save,open,info,close}` / `int p2_upm_calibrate_block(const void* m,uint64_t frame_id,const uint64_t* leaf_ipix,const double* in,double* out,uint64_t cnt)` / `double p2_upm_evaluate_c(const void* m,uint64_t frame_id,uint64_t leaf_ipix)` / `int p2_upm_{raw_weight,normalized_weights,geometry_hash,component_gauges,materialize_dense,dense_info,dense_read_block}` | `build_geo` 全量 `nodes` 含单帧区 harmonic continuation；`calibrate_block` 唯一求值 `out=in-C_f(leaf)`；`raw_w=quality*control_ivar` (缺 `ivar` 返回 `2` 不回退)；`normalized=raw/sum·geom` per-control；弱零锚+连通分量 `gauge=min frame_id`；持久化 `parameter_rows[index]↔frame_id_by_index[index]` 同长无重复；`model` `owned(void*)` 配 `close` | SCI-UPM-001..010/PERSIST/WEIGHT/ALG `docs/science/PHASE2_UPM.md` `docs/algorithms/UPM_SOLVER.md` |
| phase2 | `astro/phase2/rejection.h` | `p2_*` | `enum P2RejectionMethod {NONE=0,SIGMA=1,WINSORIZED=2,AVERAGED=3,LINEAR_FIT=4,GENERALIZED_ESD=5,RCR=6,PERCENTILE=7,MEDIAN_SIGMA=8,MINMAX=9,AUTO=10}` / `enum P2RejectReason {ACCEPTED=0,REJECTED_LOW=1,REJECTED_HIGH=2,UNDERDETERMINED=3}` / `enum P2Status {OK=0,MIN_SAMPLES=1,ALL_REJECTED=2,INVALID_INPUT=3,UNDERDETERMINED=4,INVALID_CONFIG=5,INVALID_METHOD=6,INTERNAL_ERROR=7}` / `struct P2CandidateStack/P2RejectionPlan/P2Eligibility*` / `const char* p2_rejection_semantic_id(int method)` / `int p2_eligibility_filter(const P2EligibilityInput* in,P2EligibilityOutput* out)` / `int p2_collect_candidate_stack(...)` / `int p2_reject_stack_ex(const P2CandidateStack* s,const P2RejectionPlan* plan,P2RejectionResult* out)` / `int p2_large_scale_apply / p2_reject_stack / p2_reject_plan_resolve / p2_eligibility_filter` / `struct P2RejectionTypedParams` | `eligibility` 分层 `invalid_finite/invalid_support/reason`；`UNDERDETERMINED=≤min_samples` 不猜测；`auto` 仅 planning 层 `nominal contributors` 非 per-pixel；`large_scale` 仅扩展结构；typed params V17 冻结 | SCI-REJ `docs/science/REJECTION.md` `docs/algorithms/REJECTION.md` |
| phase2 | `astro/phase2/integrate.h` | `p2_*` | `enum P2IntegrateStatus {OK=0,NO_CANDIDATES=1,ALL_REJECTED=2,ZERO_VALID_WEIGHT=3,INVALID_INPUT=4}` / `struct P2PixelStack {values[],weights[],supports[],n}` / `struct P2PixelResult {signal,support,status}` / `int p2_integrate_pixel(const P2PixelStack* in,P2PixelResult* out)` / `int p2_validate_candidate_weights(const double* w,uint32_t n)` | `signal=Σw·x/Σw (w=ivar)` `support=max(accepted)`；`w==0 continue` 合法；`NaN/Inf→INVALID_INPUT` 全0→`ZERO_VALID_WEIGHT`；确定性求和顺序 | SCI-INT `docs/science/INTEGRATION.md` `docs/algorithms/INTEGRATION.md` |
| phase2 | `astro/phase2/stage2_common.h` | `p2_*` | `struct P2Stage2Config {inputs,model,integration,output,diagnostics,acr_route,weight_mode=2(ivar)}` / `int p2_stage2_parse_config(const nlohmann::json& j,P2Stage2Config* cfg,string* err)` / `int p2_stage2_make_upm_cfg(const P2Stage2Config& cfg,int target_order,P2UpmBuildConfig* out)` | `stage2_common.cpp` 为 `model/reject/profile/normalization/large_scale/typed params` 默认值单一来源；`weight_mode 2=ivar` 冻结 | `docs/development/CONFIG_SCHEMA.md` |
| phase2 | `astro/phase2/block.h` | `p2_*` | `struct P2BlockPlannerInput/Plan` / `int p2_block_plan(const P2BlockPlannerInput* in,P2BlockPlan* out)` | 分块调度 `coverage union` | `docs/architecture/PIPELINE.md` |
| phase2 | `astro/phase2/acr_kernels.h` | `acr::*` | `void register_phase2_acr_kernels()` | 幂等注册，科学语义不变 | ACR-IVAR-001 |

---

## 13. orchestrator (`orchestrator.exe`, Phase1 编排)

| 模块 | 头文件 | 前缀 | 函数/类型(签名节选) | 参数/返回值/错误码要点 | 追溯ID |
|---|---|---|---|---|---|
| orchestrator | `orchestrator.h` `json_config.h` `dll_loader.h` `checkpoint.h` `logger.h` 等 | `Orchestrator` `AstroCsExitCode` `PipelineStageV2` `PrecisionMode` | `enum class PipelineStageV2 {CALIBRATE,STAR,PSF,PLATESOLVE,PHOTOMETRIC,NOISE,DRIZZLE,HIPS_WRITE}` (V2 权威，兼容旧 `PipelineStage`) / `enum class PrecisionMode:uint8_t {FP32,FP64}` / `namespace AstroCsExitCode {SUCCESS=0,GENERIC=1,DLL_LOAD_FAILED=2,BLOCK_MISSING=3,CALIBRATE=4,PLATESOLVE=5,DRIZZLE=6,CONFIG=7,FILE_IO=8,TIMEOUT=9,CANCELLED=10, STAR_DETECT=20..INPUT_INVALID=28,MODULE_SPECIFIC_BASE=100}` / `struct Stage1Config {input{light,master_bias/dark/flat},calibration,detection,psf,platesolve{ra0,dec0,focal,pixel_size},photometric,snr,drizzle{nside,pixfrac},hips{out_dir},gaia_data_dir,precision,nside}` / `int parse_stage1_config(const nlohmann::json& j,Stage1Config* out,string* err)` / `bool validate_stage1_schema(const nlohmann::json& j,string* err)` / `class Orchestrator {set_stage1_config(const Stage1Config&), init_dlls(), run_stage1(const Stage1Config&)->TaskResult{stage,exit_code,error_msg}, run_stage2(...), request_cancel(), get_current_stage()->PipelineStageV2, stage_name_v2(), precision}` / `enum class DllModule {CALIBRATION,STAR_DETECTOR,DPSF,IPV,PHOTOMETRIC,SNR,DRIZZLE}` / `class DllLoader {load(path),get_function<T>(name),unload}` | `stage1.schema.json v1.1` `validate_stage1_schema` nlohmann-json-schema-validator v2.4.0；`gaia_data_dir` 必填 缺失硬失败；`PipelineStageV2` 为 `stage_ids` 权威 (`P1.*`/`P2.*` 与 `ERROR_MODEL.md` 一致)；`DllLoader` 动态加载 纯 C ABI 跨界 禁异常；`exit_code 0-10` 进程码 `20-28` `numeric_code` `100` 预留；`checkpoint` 断点续跑；`spill_manager/admission/resource_monitor` 溢写/准入/资源 | 全链编排 `docs/architecture/PIPELINE.md` `docs/architecture/ERROR_MODEL.md` 机检 `error_taxonomy` 全集合 |

---

## 14. acr (异构计算抽象)

| 模块 | 头文件 | 前缀 | 函数/类型(签名节选) | 参数/返回值/错误码要点 | 追溯ID |
|---|---|---|---|---|---|
| acr | `core/api` `scheduler/*` `backends/cuda/bridge/*` `include/astro/compute/*` | `acr::*` | `struct TaskDescriptor {id,stage,kernel,inputs,outputs,priority}` / `class KernelRegistry {register_kernel/descriptor,lookup}` / `void register_phase2_acr_kernels()` / `class Scheduler {submit/schedule}` / `class DeviceExecutor/CudaBridge` | `register_phase2_acr_kernels` 幂等；`acr_route auto` 由 `stage2_common` 决定；科学语义不变仅路由；`work_pool+device_executor` 并发 | ACR-IVAR-001 `docs/modules/acr.md` `docs/architecture/THREADING_MODEL.md` |

---

## 附录 A — 错误码与 stage IDs (与 `docs/architecture/ERROR_MODEL.md` 全集合一致)

- `AstroCsExitCode` 进程码 `0..10` + `numeric_code 20..28` + `100` base: 见上表 orchestrator 行；V19R3 已修正此前虚假 `ARGS_ERROR/STAGE_*` 枚举，机检 `error_taxonomy_exit_codes` 校验 `name+value` 全集合等于 `orchestrator.h`。
- `P2IntegrateStatus 0..4` / `P2RejectReason 0..3` / `P2Status 0..7` / `P2RejectionMethod 0..10` / `AC_OK/-1/-2/-3` / `DPSF_FIT_*` / `SnrDropReason 0..255` 与头文件/算法文档全集合一致，机检 `integration_status_full_set` `rejection_status_full_set`。

## 附录 B — 所有权与线程 (与 `docs/architecture/OWNERSHIP_AND_LIFETIME.md` / `THREADING_MODEL.md` 一致)

- `owned` 句柄/模型/reader 由调用方 `*_close/destroy/free`；`aio_upm_read_all_dynamic` `new char[]` 配 `delete[]`；`sdet_detect*` 配 `sdet_free_*`；`psf/psf_*` 配 `dpsf_free_results`；`SnrModel*` 配 `snr_free_model*`；`NoiseWeightModelV1` 配 `snr_noise_model_v1_free`；`P2CoverageResult` 配 `p2_coverage_free`；`phase2 model` `void*` 配 `p2_upm_close`；`GaiaClient*` 配 `gaia_client_destroy`；`IpvWcsResult` 值类型无所有权。
- 输出 `buffer` 调用方分配传 `capacity/max_count`；`aio_upm`/`hips` 错误串 `thread_local`；编排层顺序 `stage` + 模块内 `OpenMP parallel-for`；`ACR work_pool+device_executor`；浏览器主+IO 线程；浮点累积顺序固定(确定性)。

## 附录 C — 机检锚点

- `tools/docs_machine_consistency.py 9/9` (`config_weight_mode_ivar`, `frame_id_contract_exact`, `error_taxonomy_exit_codes`, `integration_status_full_set`, `rejection_status_full_set`, `stage_ids_docs_vs_orchestrator`, `snr_constants`, `product_contracts`, `drizzle_variance_formula`) + `tools/config_consistency_check.py mismatches=[]`。

---
*Stage C 产出: `docs/ARCHITECTURE.md`(133 行 Engineering Anchor) + 本文件 + `工程控制/docs/18_CODE_CHANGE_MAP.md` + `reports/engineering_doc_review.md`；单目的 commit，不改 `lib/**/src`。*
