# Photometry Science

## 目的

将仪器流量校准到相对/绝对光度尺度。

## 科学定义

测光定标残差（相对合成星表通量）：

```text
r_i = log10(F_instr,i / F_syn,i)      # dex
location = IRLS_Tukey(r_i, c=4.685, max_iter=50, tol=1e-6)
         # Tukey biweight: w = (1-(r/cσ)^2)^2, u=(r-location)/(c·S), |u|>=1 时 w=0
         # 初值 location_0=median(r), S=MAD(r)/0.6745；收敛判据 |location_new - location_old| < 1e-6；S=0 时跳过迭代直接取 median(r)
scale = 10^{-location}                 # 仪器通量校正因子 I_cal = I · scale
sigma_residual = MAD(r_inliers)/0.6745 # dex，r_inliers={i|w_i>0} 为 IRLS 内点集（Tukey 权重>0）
sigma_mag = 2.5 × sigma_residual       # mag
outlier 率 = 1 - |r_inliers|/|r_consistent|，其中 r_consistent 为星等一致性预过滤后集合
```

PhotometricCalibrationQuality 即该残差尺度（QA/systematic metadata），
**不是**逐像素噪声方差（SCI-NOISE 边界）。
离群由两级清洗决定：星等一致性 `|delta - median(delta)| > mag_tolerance` 拒绝 + IRLS/Tukey 权重为 0 拒绝。

## 变量/单位

- F_instr：仪器通量（ADU·px 或 e⁻）；F_syn：合成通量（星表模型）；
- r：dex；mag：mag；sigma_cal_rel：相对误差（×ln10）。
- delta = -2.5·log10(F_instr) - G_Gaia（粗略零点差，mag）

## 假设

- 合成星表（Gaia）提供可信参考通量；大气/仪器零点在观测尺度内稳定。
- 饱和判据：PSF 有效星要求 `psf_status==0` 且无 `SATURATED` 标志（`qf & (SNR_QF_SATURATED|SNR_QF_HAS_SATURATED) == 0`，由 `lib/snr_estimator` 饱和掩膜阶段判定）；不满足则视为饱和/受邻域饱和污染，不进入匹配与定标。
- 星等一致性容忍 `mag_tolerance=3.0 mag`：以 `median(delta)` 为粗略零点，`|delta_i - median(delta)| > 3.0` 预拒绝（进入 IRLS 前，`lib/photometric_calib/cpp/src/star_matcher.cpp:241-248, 435-475`）。

## 有效域

- 亮星非饱和、PSF 解析、无云/无极端消光。

## 不保证

- 不保证颜色项之外的带通效应（QA 暴露残差分布）。

## 失效条件

- 参考星不足 → NO_DATA；残差 MAD=0 → 显式处理（`S=0` 时跳过 IRLS 迭代，`sigma_residual=0`，`scale` 退化为 `10^{-median(r)}` 并记 `robust_iterations=0`）。
- 饱和/质量标志异常（`psf_status != 0` 或 `qf & SATURATED`）→ 该星不参与匹配与 IRLS，计入 `rejected_quality`。

## 系统/随机误差

- 系统：零点漂移、带通不匹配；随机：星点测光噪声。

## 数值精度

FP64 对数空间；flux 比值带权（ivar 不用于此层）。
IRLS 仅在 `S=MAD(r)/0.6745 > 0` 时迭代，否则判定全体 `r` 相同直接取 `median(r)`；迭代上限 50，收敛阈值 1e-6（`lib/photometric_calib/cpp/src/star_matcher.cpp:21-27,478-525,552-559`）。

## 参考文献

Siril/IRAF 测光语义；Gaia DR3 合成光度。

## ID

SCI-PHOT-001；ALG-PHOTOMETRIC-FIT-*。
