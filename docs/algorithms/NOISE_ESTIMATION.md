# Noise Estimation

关联：SCI-NOISE-001..015；模块：lib/snr_estimator/cpp/noise_model.cpp。

## 输入

校准图像（source-masked blank-sky patch）。

## 输出

patch variance 空间场 + 全局兜底 + ivar。

## Preconditions

图像有限；patch 网格 8×8。

## Postconditions

σ_bg=1.4826022185·MAD（`noise_model.cpp:robust_sigma=1.482602218505602·median(|x−median|)`）；5σ 裁剪 ≤2 轮；ivar=1/var。

## Invariants

- 与星亮度解耦（fixed conservative 统一半径 `rmax=max(1,r0)×max(1,scale)`，不按振幅/星亮度缩放，已冻结；见 `docs/science/NOISE_MODEL.md` 假设）；
- 平面场 var=a+b·x+c·y 平滑。

## 复杂度

O(pixels)（mask+median）。

## 并行模型

patch 间 OpenMP；median 局部。

## 数值风险

全星场无空 patch → NO_DATA/fallback；裁剪偏差。

## Gain/Readnoise（仅诊断/仅 SNR-005，不入生产）

仅诊断：`snr_noise_gain_variance`（`noise_model.cpp:457-464`，`var_ADU=max(signal,0)/gain+(rn/gain)^2`）仅用于 SNR-005 诊断交叉验证；生产 `NoiseWeightModelV1 source==0 empirical`（blank-sky 稳健估计）不融合 gain/readnoise，`gain/readnoise` 字段仅诊断/追溯（锚点：`docs/science/NOISE_MODEL.md` Gain/Readnoise 诊断节、`noise_model_science_test.cpp:238-272`）。

## fast/reference/oracle

MC 矩阵（Gaussian/Poisson/场恢复）SNR-004..006；与 1/unc² 对比
（SNR-009 coadd）。

## ID

ALG-NOISE-MAD-001..；TEST-SCI-NOISE-*。
