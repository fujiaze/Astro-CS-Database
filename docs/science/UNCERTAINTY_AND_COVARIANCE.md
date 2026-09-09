# Uncertainty & Covariance

## 目的

给出产品方差/ivar 与相邻像素相关性的权威说明。

## 逐像素方差

- 输入方差：NoiseWeightModelV1（空背景稳健方差，SCI-NOISE-001..015）；
- Drizzle 传播：var_p = Σ v_j w_jp² / D_p²（SCI-DRZ-014）；与 `lib/snr_estimator` 的 `snr_noise_scale_law`（`x′=α·x → Var′=α²·Var, ivar′=ivar/α²`，SCI-NOISE-002）同源互引——Drizzle 归一化权重求和即该缩放律的加权形式；
- 产品：HiPS variance + ivar（1/variance）。

## 协方差（重要边界）

同一源像素贡献多个输出像素 ⇒ Drizzle 后相邻输出**非严格独立**：

```text
Cov(S_p, S_q) = Σ_j c_jp c_jq v_j
```

V19 不保存完整 covariance matrix；Monte Carlo 量化（SNR-012）：
nside=512 合成帧相邻像素 mean|ρ|≈0.19、max|ρ|≈0.57。

## 对使用的约束

- pixel variance ≠ aperture variance（**aperture variance 未建模**：V19 仅提供逐像素方差/ivar，未对 aperture 求和建模协方差；aperture 误差须显式加入 `Cov` 项，量化见 SNR-012——nside=512 mean|ρ|≈0.19、max|ρ|≈0.57）；
- 下游科学（如光度测量）如需 aperture 误差须显式考虑 pixfrac/resampling
  协方差；ivar 权重默认只用于逐像素最优组合。

## V19R3 control estimator 方差（ALG-UPM-CONTROL-IVAR-001）

(本节公式隶属Phase2 UPM/ALG-UPM-CONTROL-IVAR-001，不属于lib/snr_estimator的NoiseWeightModelV1；后者仅提供σ_bg，经sampler阶段乘k_corr=N_eff缩放)

UPM 的 control estimator 是 background-clean patch **median**，其方差
不是单 leaf 像素方差：

```text
control_variance = k_corr × (π/2) × sigma_bg² / N_retained
control_ivar     = 1 / control_variance
```

- 独立 Gaussian 基线 Var(median) ≈ πσ²/(2N)（UPMW-004 实证 ratio 0.997）；
- k_corr 表征 Drizzle 输出协方差导致的 N_eff<N_retained：UPMW-005 MC
  （pixfrac=0.8，2000 实现）k_corr=1.3883，N_eff≈181/251；冻结 1.4；
- N_retained 用 clipping 后保留样本（UPMW-007 patch vs truth 验证）；
- 生产 UPM 权重 = quality × geometric_reliability × control_ivar
  （SCI-UPM-WEIGHT-001），禁止再用单像素 ivar/support/SNR 乘因子。

## Phase2 马赛克合成方差（DATA-P2-VAR-001，DATA-UNC-001 冻结 2026-09-09）

马赛克加权积分（SCI-INT §5，w_i=逐样本 ivar，weight_mode=2，无 fallback）的
方差传播是 SCI-DRZ-014 一般式 `var_p = Σ_j v_j w_jp²/D_p²` 在积分权重下的
直接特例（w_i=1/v_i）：

```text
ivar_mosaic(p) = W(p) = Σ_i ivar_i(p)     # = wsum（SCI-INT §5 冻结量）
variance_mosaic(p) = 1 / W(p)
一般式（权重非纯逆方差时适用）: variance = Σ_i w_i²·v_i / W²
```

- 上游协方差（§上）不进入逐像素 variance：马赛克 variance 仍是逐像素随机
  方差，aperture 使用边界同上。
- UPM control_variance（ALG-UPM-CONTROL-IVAR-001）只进 w_UPM，与马赛克
  variance 产品严格分离。
- invalid（输出面）：无有效样本（n_used=0）→ variance/ivar=NaN（signal=NaN
  同态，writer 通道 DATA_SEMANTICS §12.4）；输入 ivar 非有限 → hard fail。
- unavailable（fail-closed）：weight_mode≠2 或 ivar 帧缺失 fallback 发生 →
  不写 variance/ivar 产品 + manifest uncertainty_available=false（禁伪值）。
- 产品/manifest 表达与验证门：DATA_SEMANTICS §30.1/§30.5（DATA-P2-VAR-001）。

## Phase3 重采样方差传播（DATA-P3-UNC-001，DATA-UNC-001 冻结 2026-09-09）

反向映射重采样（SCI-P3 §5 冻结采样核）把输入逐像素方差 u 传播到输出平面；
这是方差二次型在重采样权重下的直接应用（同 SCI-DRZ-014 二次形式、SCI-NOISE-002
缩放律同源）：

```text
nearest :  var_out = u_in
bilinear:  var_out = Σ_k c_k² · u_k     # c_k = ALG-P3-003 G4 冻结权重, Σc_k=1
ivar_out = 1 / var_out   (var_out 有限且 >0)；var_out=0→0、NaN→NaN 同态
```

- **Σc_k² ≠ 1 是正确物理**：bilinear 平均降低独立像素方差但引入相邻相关
  （§上协方差机制），禁止误用 Σc_k=1 归一 variance（常数信号场不变量
  SCI-P3 §7 只对 signal 成立，对 variance 不成立）。
- 输入选择：输入 HiPS 含 variance/ 子产品则 u=variance；否则含 ivar/ 则
  u=1/ivar；两者皆无 → uncertainty unavailable（输出无 VARIANCE/IVAR HDU +
  manifest uncertainty_available=false，宪章 §18.3 模式）；负/Inf = 产品
  损坏显式错误；NaN 传播（C=1）。
- invalid（输出面）：无覆盖（C=0）→ variance/ivar=NaN（signal=NaN 同态）；
  覆盖不一致（leaf signal 有限而 u 缺失）→ 输出 NaN + provenance 计数。
- 产品/FITS 表达（EXTNAME=VARIANCE/IVAR、BUNIT 派生）与验证门：
  DATA_SEMANTICS §30.4/§30.5（DATA-P3-UNC-001）；SCI-P3 §9a-10 的
  variance/ivar 拒绝语义由此 supersession（宪章 §7.1/§7.3 上位）。

## 数值精度

FP64；MC 表征 seed 固定可复现。

## ID

SCI-NOISE-011/012；ALG-DRZ-VAR-*；ALG-UPM-CONTROL-IVAR-001；
DATA-UPM-CONTROL-UNC-001；DATA-P2-VAR-001；DATA-P3-UNC-001
（DATA-UNC-001，DATA_SEMANTICS §30）。
