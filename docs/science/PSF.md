# PSF Science

## 目的

描述点源响应并给出拟合质量代理。

## 科学定义

Moffat4 模型（β=4，`lib/dynamic_psf/src/dpsf_psf.cpp:13-18,66-95`）：

```text
I(r) = B + A / (1 + Q)⁴,  Q = p1·dx² + 2·p2·dx·dy + p3·dy²
p1 = cos²θ/(2sx²)+sin²θ/(2sy²), p2 = sin2θ/(4sx²)−sin2θ/(4sy²), p3 = sin²θ/(2sx²)+cos²θ/(2sy²)
dx = x−(cx+x0), dy = y−(cy+y0)
```

各向同性退化 `sx=sy=σ` 时 `Q=0.5·r²/σ²`。各向异性参数 `sx,sy>0, θ`（椭率方向角），离心率 `e=√(1−(s_min/s_max)²)`。标准 Moffat `M=A/(1+r²/α²)^β`，由 `Q=r²/(2σ²)=r²/α²` 得 `α=√2·σ`，故单轴 `FWHM =2α√(2^{1/β}−1)=2√2·σ·√(2^{1/4}−1)≈1.230310·σ`（`MOFFAT4_FWHM_FACTOR`），两轴 `fwhm_x/y=1.230310·sx/sy`；`θ` 存在四象限简并，拟合后以 4 候选 `{θ, π/2−θ, π/2+θ, π−θ}` 中 10–90% trimmed-mad 最小者消歧（`dpsf_psf.cpp:351-363, compute_trimmed_mad`）。

解析积分通量 `flux = 2πA·sxsy/(β−1) = 2πA·sxsy/3`（β=4，`dpsf_psf.cpp:368`）仅在整平面延伸 Moffat 假设下有效，切割/饱和域为近似。

## PsfFitQuality

```text
residual_scale        = 10–90% trimmed mean |residual|   (PSF 块第 8 列)
robust_residual_sigma = residual_scale / 0.7316728       (Gaussian 假设)
q_psf                 = A / residual_scale               (fit-quality proxy)
```

`q_psf` 是拟合质量代理（剔星/QA），不是图像噪声 SNR；默认不进入 Phase2 逐像素 science weight（SNR-008 退休旧 (A−B)/mad 科学路径）。系数 `E[trimmed mean |r|]=0.7316728·σ` 为 `r∼N(0,σ²)` 时 `10–90%` 分位截尾后 `|r|` 均值的解析常数（`kTrimMeanToSigma=0.7316727929211932`，`lib/snr_estimator/cpp/src/noise_model.cpp:35-37`），仅 Gaussian 残差假设下 `robust_residual_sigma` 有尺度意义。

## 变量/单位

- I：ADU；r,dx,dy：px；σ,sx,sy：px；A：ADU；B：ADU；θ：rad；Q：无量纲；fwhm：px；q_psf：无量纲；residual_scale：ADU。

## 假设

- 视场内 PSF 形状缓变（块状拟合）；星点不饱和。

## 有效域

- 采样充足（≥FWHM/px 合理范围）；无密集混淆。

## 不保证

- 不保证超出椭圆 Moffat4 的 PSF 色差/空间高阶各向异性（Moffat4 仅刻画一阶椭率 `e,θ`）。

## 失效条件

- 拟合不收敛/无星点 → PSF 阶段显式状态。

## 数值精度

FP64 拟合；Levenberg-Marquardt 类求解。求解器仅保留 7 参数 Moffat4（`B,A,x0,y0,sx,sy,θ`）LM 路径（`lm_solve`，`dpsf_psf.cpp:98-182`），V19 自 `lib/dynamic_psf` 创建起未引入高斯模型文件，锚点 `docs/algorithms/STAR_PSF_ALGORITHMS.md` 伪代码已仅列 Moffat4；历史“高斯路径”指早期设计讨论中未落地的 `exp(−r²/2σ²)` 备选，未进入代码/追溯。

## 参考文献

Moffat (1969)；Trujillo et al. (2001)。

## ID

SCI-PSF-001；ALG-STAR-PSF-*。
