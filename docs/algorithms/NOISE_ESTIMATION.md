# Noise Estimation

关联：SCI-NOISE-001..015；模块：lib/snr_estimator/cpp/noise_model.cpp。

## 输入

校准图像（source-masked blank-sky patch）。

## 输出

patch variance 空间场 + 全局兜底 + ivar。

## Preconditions

图像有限；patch 网格 8×8。

## Postconditions

σ_bg=1.4826022185·MAD；5σ 裁剪 ≤2 轮；ivar=1/var。

## Invariants

- 与星亮度解耦（掩膜半径 ∝ sqrt(A/A_median) clamp）；
- 平面场 var=a+b·x+c·y 平滑。

## 复杂度

O(pixels)（mask+median）。

## 并行模型

patch 间 OpenMP；median 局部。

## 数值风险

全星场无空 patch → NO_DATA/fallback；裁剪偏差。

## fast/reference/oracle

MC 矩阵（Gaussian/Poisson/场恢复）SNR-004..006；与 1/unc² 对比
（SNR-009 coadd）。

## ID

ALG-NOISE-MAD-001..；TEST-SCI-NOISE-*。
