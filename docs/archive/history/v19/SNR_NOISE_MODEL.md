> **ARCHIVED_NON_NORMATIVE** — GOV-002 归档历史技术文档，不再作为当前权威。
> 替代文档：docs/science/NOISE_MODEL.md、docs/science/CONTROL_WEIGHT_SNR.md

# SNR / Noise Model 科学文档 (V19)

## 状态

```text
LEGACY_SNR_SCIENCE_CONSUMER = 0
PHOTOMETRIC_SCATTER_UNITS   = PASS
PSF_QUALITY_SEMANTICS       = PASS
NOISE_MODEL_V1              = PASS
SCIENCE_NOISE_MODEL_V1      = FROZEN (V19 验收通过后)
```

## 三层模型 (SNR_REDESIGN_CONTRACT.md)

### 1. PhotometricCalibrationQuality

测光定标残差:

```text
r_i = log10(F_instr,i / F_syn,i)
sigma_residual = MAD(r_inlier) / 0.6745        # 单位 dex
sigma_mag      = 2.5 × sigma_logflux_dex
sigma_cal_rel  = ln(10) × sigma_logflux_dex
```

用途: QA / frame flag / systematic metadata。**不是**像素随机噪声 σ,
不得当作逐像素 inverse-variance。

### 2. PsfFitQuality

PSF 块 [N,9] 第 8 列历史称 "mad", 实际是 10–90% trimmed mean absolute
residual (Gaussian N(0,σ²) 期望 ≈ 0.7317σ)。V19 准确重命名:

```text
residual_scale        = trimmed-mean-abs residual (原列)
robust_residual_sigma = residual_scale / 0.7316728   (Gaussian 假设)
q_psf                 = A / residual_scale           (fit-quality proxy)
```

`q_psf` 是 PSF 拟合质量代理 (剔星/QA), 不是图像噪声 SNR, 默认不进入
Phase2 逐像素 science weight。旧 `(A-B)/mad` 因违反 pedestal invariance
已从科学路径退休 (SNR-008)。

### 3. NoiseWeightModelV1 (production 基线)

source-masked blank-sky 稳健方差:

```text
patch grid (默认 8×8)
星点掩膜: 半径 = r0 × clamp(sqrt(A/A_median), 1, scale)   # 与星亮度解耦
patch 内: σ_bg = 1.4826022185 × median(|x − median(x)|)
          (cosmic/hot 5σ 稳健裁剪, ≤2 轮)
控制点   = 合格 patch (≥ min_samples)
空间场   = 最小二乘平面拟合 var(x,y) = a + b·x + c·y   # 平滑梯度
全局兜底 = 合格 patch variance 的稳健中位数
ivar     = 1/variance
```

控制点来自空背景噪声, 与星亮度/星族解耦 (SNR-003/SNR-010)。

## 方差传播 (Drizzle, DRZ-014)

```text
源像素 j → 目标像素 p:
  w_jp = a_jp / A_drop,j
  F_p  = Σ_j x_j w_jp
  D_p  = Σ_j a_jp
  S_p  = F_p / D_p

方差传播 (线性算子):
  sumVarNum += v_j × w_jp²
  variance_p = sumVarNum / D_p²
  ivar_p     = 1 / variance_p
```

缩放律: `x' = αx → var' = α²var, ivar' = ivar/α²` (SNR-002)。

## 相关噪声

同一源像素贡献多个输出像素 → Drizzle 后相邻输出非严格独立:

```text
Cov(S_p, S_q) = Σ_j c_jp c_jq v_j
```

V19 不保存完整 covariance matrix, 但通过 Monte Carlo 量化:
SNR-012 实测 (nside=512, 20×20 合成帧, 4000 实现): 相邻像素
mean|ρ| ≈ 0.19, max|ρ| ≈ 0.57。pixel variance ≠ aperture variance;
pixfrac/resampling 引入协方差, 文档化。

## 验收矩阵 (SNR-001..015)

| ID | 门 | 结果 |
|---|---|---|
| SNR-001 | pedestal invariance | PASS (rel < 1%) |
| SNR-002 | multiplicative scale | PASS (rel < 2%) |
| SNR-003 | star-population invariance | PASS (rel < 2%) |
| SNR-004 | Gaussian sky recovery | PASS (bias ≤2%, p95 ≥95%) |
| SNR-005 | Poisson+read cross-check | PASS (rel ≤5%) |
| SNR-006 | spatial noise-field recovery | PASS (corr ≥0.98, RMSE ≤5%) |
| SNR-007 | dex/mag units | PASS |
| SNR-008 | legacy A-B retired | PASS |
| SNR-009 | inverse-variance coadd | PASS (3%) |
| SNR-010 | signal independence | PASS (|ρ| < 0.02) |
| SNR-011 | Drizzle variance MC | PASS (p50 0.98–1.02, p95 0.95–1.05) |
| SNR-012 | Drizzle covariance | PASS (characterized) |
| SNR-013 | PSF quality semantics | PASS |
| SNR-014 | missing metadata fallback | PASS |
| SNR-015 | UPM ablation | PASS (phase2 gate) |

## 参考理论

- Fruchter & Hook (2002): Drizzle / variable-pixel linear reconstruction —
  采用其线性通量守恒与 drop 语义。
- Zackay & Ofek (2017): optimal coaddition — 采用 inverse-noise-aware
  combination, 不采用其完整 likelihood 域实现。
- Mandelbaum et al.: coadd PSF 与 signal-independent weights — 采用
  "权重不得由目标亮度决定" 原则。
- Samsing & Kim: PSF/pixelization/dither 对 point-source 不确定度 —
  作为协方差表征的参考, 未直接数值采用。
