# Module: snr_estimator

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

> 模块合同权威 = `lib/algorithms/noise_snr/README.md`（CONTRACT_READY）+ `lib/algorithms/noise_snr/module.yaml`；
> 逐符号源码锚定与缺陷登记见 `docs/science/algorithms/NOISE_ESTIMATION.md` §13。
> 噪声模型 A 为唯一生产模型；噪声 σ 来源 = 局部 patch + 星点掩膜 + 饱和过滤。

## 职责

三层噪声模型：PhotometricCalibrationQuality / PsfFitQuality /
NoiseWeightModelV1（空背景稳健方差 → ivar）。

## 非职责

不做 PSF/测光本身；q_psf 与 sigma_cal 不进 science weight。

## Public API

`lib/algorithms/noise_snr/`：`include/astrocs/information_weight.h`（`CovarianceView`/`PointEstimate`/`w_info_diagonal`/`w_info_dense`/`w_info_low_rank`/`w_info_solve`/`white_noise_gate`/`w_info_white_noise`/`diag_approx_report`/`combine_point_estimates`）、`include/astrocs/noise/types.h`（C 面类型）、`include/astrocs/noise/variance_plane_policy.h`（`VariancePlaneVerdict`/`classify_variance_plane`）、`include/astrocs/noise/saturation_policy.h`（`resolve_saturation_level`/`resolve_effective_saturation`/`saturation_filter_state`）；模块入口 `src/module_entry.cpp`（导出面 `src/astrocs_p1_noise.def`）。

## Data contract

输入图像 + 星表；输出 variance 空间场/全局兜底/ivar（HiPS ivar 产品）。平面判定与审计字段 = `VariancePlaneVerdict` + `variance_audit_required_fields()`（10 字段）。

## Ownership

结果 buffer 调用方。

## Thread safety

patch 串行（单线程顺序）；median 局部。

## Errors

无合格 patch → NO_DATA/fallback。

## Science IDs

SCI-NOISE-001..015；ALG-NOISE-001..003。

## Tests

SNR-001..015 全矩阵（pedestal/scale/star-pop/Gaussian/Poisson/场恢复/
coadd/独立性/MC 协方差）。

## Source files

`lib/algorithms/noise_snr/{include/astrocs,src,wrapper_phase1}/`（`src/module_entry.cpp` 为模块入口；`wrapper_phase1/snr_frame_science.{h,cpp}` 为 phase1 包装面）。
