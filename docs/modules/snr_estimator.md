# Module: snr_estimator

> 上游：ASTROCS_DESIGN.md §8.4（模块与 ABI）

## 职责

三层噪声模型：PhotometricCalibrationQuality / PsfFitQuality /
NoiseWeightModelV1（空背景稳健方差 → ivar）。

## 非职责

不做 PSF/测光本身；q_psf 与 sigma_cal 不进 science weight。

## Public API

snr_estimator DLL（noise_model 主实现）。

## Data contract

图像 + 星表 → variance 空间场/全局兜底/ivar（HiPS ivar 产品）。

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

lib/algorithms/noise_snr/cpp/。

---

> 本页为 ACTIVE_INFORMATIVE 摘要。模块合同权威 = `lib/algorithms/noise_snr/README.md`（r1，
> CONTRACT_READY）+ `lib/algorithms/noise_snr/module.yaml`（MOD-astrocs-phase1-
> noise-snr），逐符号源码锚定与 DISP-NOISE-001..009 登记
> 见 `docs/algorithms/NOISE_ESTIMATION.md` §13；冲突时以冻结合同为准。
> **噪声模型 A 为唯一生产模型**；噪声 σ 来源 = 局部 patch + 星点掩膜 + 饱和过滤。
