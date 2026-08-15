# Uncertainty & Covariance

## 目的

给出产品方差/ivar 与相邻像素相关性的权威说明。

## 逐像素方差

- 输入方差：NoiseWeightModelV1（空背景稳健方差，SCI-NOISE-001..015）；
- Drizzle 传播：var_p = Σ v_j w_jp² / D_p²（SCI-DRZ-014）；
- 产品：HiPS variance + ivar（1/variance）。

## 协方差（重要边界）

同一源像素贡献多个输出像素 ⇒ Drizzle 后相邻输出**非严格独立**：

```text
Cov(S_p, S_q) = Σ_j c_jp c_jq v_j
```

V19 不保存完整 covariance matrix；Monte Carlo 量化（SNR-012）：
nside=512 合成帧相邻像素 mean|ρ|≈0.19、max|ρ|≈0.57。

## 对使用的约束

- pixel variance ≠ aperture variance；
- 下游科学（如光度测量）如需 aperture 误差须显式考虑 pixfrac/resampling
  协方差；ivar 权重默认只用于逐像素最优组合。

## 数值精度

FP64；MC 表征 seed 固定可复现。

## ID

SCI-NOISE-011/012；ALG-DRZ-VAR-*。
