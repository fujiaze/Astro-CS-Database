# Module: snr_estimator

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

patch OpenMP；median 局部。

## Errors

无合格 patch → NO_DATA/fallback。

## Science IDs

SCI-NOISE-001..015；ALG-NOISE-MAD-001..。

## Tests

SNR-001..015 全矩阵（pedestal/scale/star-pop/Gaussian/Poisson/场恢复/
coadd/独立性/MC 协方差）。

## Source files

lib/snr_estimator/cpp/。

---

> **P1-NOISE-DOC 增补（2026-09-07）**：本页为 P1-005 期 legacy
> ACTIVE_INFORMATIVE 摘要，部分表述已过时（"patch OpenMP" 实测现状为
> 单线程顺序；"ALG-NOISE-MAD-001" 占位 ID 已由 ALG-NOISE-001..003 取代）。
> 模块合同权威已冻结于 `lib/snr_estimator/README.md`（r1，
> CONTRACT_READY）+ `lib/snr_estimator/module.yaml`（MOD-astrocs-phase1-
> noise-snr），逐符号源码锚定与 DISP-NOISE-001..009 登记
> 见 `docs/algorithms/NOISE_ESTIMATION.md` §13；本页不据此更新正文，
> 冲突时以冻结合同为准。
