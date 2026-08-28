# PSF Algorithms (ALG-PSF)

> ID: ALG-STARPSF-001  上游 SCI: SCI-PSF-001  状态: DERIVED (T201 冻结; V5 ALG-002 重验 2026-08-28)  模块: star_detector + dynamic_psf

## 1 上游 SCI 与输入输出

- 上游: `SCI-PSF-001` (Moffat4 β=4, FWHM=1.230310·σ, q_psf=A/residual_scale)
- 输入: 校准图像 `float32[H×W]` + 背景噪声 `σ_bg` (NoiseWeightModelV1)
- 输出: 星点表 `(x,y,flux,A,B,σ,q_psf,residual_scale)` + PSF 块 `[N,9]` (B,A,x0,y0,sx,sy,θ,residual_scale,q_psf)

## 2 离散公式

```text
F1: I(r)=B+A/(1+Q)^4, Q=p1dx²+2p2dxdy+p3dy², dx=x−(cx+x0), dy=y−(cy+y0)
    p1=cos²θ/(2sx²)+sin²θ/(2sy²), p2=sin2θ/(4sx²)−sin2θ/(4sy²), p3=sin²θ/(2sx²)+cos²θ/(2sy²)
F2: 各向同性 sx=sy=σ→ Q=0.5·r²/σ², α=√2σ, FWHM=2√2σ·√(2^{1/4}−1)=1.230310σ
F3: flux=2πA·sxsy/3 (β=4 整平面)
F4: residual_scale=10–90% trimmed mean |residual|, robust_residual_sigma=residual_scale/0.7316728
    (kTrimMeanToSigma=0.7316727929211932, E[trimmed mean |r|]=0.731673σ, Gaussian)
F5: q_psf=A/residual_scale, q_psf为QA代理不进science weight
```

来源: `dpsf_psf.cpp:13-18,66-95,351-368` `noise_model.cpp:35-37`

## 3 伪代码

```text
function detect_centroid(image, sigma_bg):
  for each pixel: if image > bkg+1.5·σbg → candidate (sdet_detector.cpp:215 THRESH_FACTOR=1.5)
  centroid weighted mean of 3×3 neighborhood
  if saturated median+0.7·dynrange reject

function fit_psf_moffat4(image, cx,cy, fitRadius):
  init B=median(patch), A=max−B, x0=y0=0, sx=sy=1.2, θ=0
  LM 7参 Levenberg-Marquardt (dpsf_psf.cpp:lm_solve) iter≤50 tol=1e-6
    J via finite diff, Δ=(JᵀJ+λI)⁻¹ Jᵀr, λ adaptive
  post: FWHM=1.230310·sx/sy, flux=2πA·sxsy/3
  θ消歧 4候选 {θ,π/2−θ,π/2+θ,π−θ} 取 trimmed-mad 最小 (dpsf_psf.cpp:351-363)
  residual_scale = trimmed mean |image−model| (10–90%)
  q_psf = A / residual_scale
  guards: sx/sy>0 else reject (333-344), MAD==0 skip scale, 平坦星 reject

function psf_block_batch(image, n_stars):
  parallel for each star: fit_psf_moffat4 independent

Batch deterministic: input order fixed, per-star independent, reduction none cross-star.
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| 图像含 NaN/Inf | 该 patch 跳过拟合，status=BAD |
| `max−B ≤0` | reject `A≤0` (dpsf_psf.cpp:45) |
| `sx/sy ≤0` | reject invalid params (333-344) |
| `MAD==0` | `robust_residual_sigma` 不换算，q_psf仍计算 |
| 平坦星 `‖∇‖→0` | LM 奇异 → iter limit, cost 阈校验 fail (169-170) |
| 饱和 `median+0.7·dynrange` | 掩膜 reject (star_detector.h) |

## 5 确定性与归约

- 每星独立 LM，无跨星归约；OpenMP tile/块并行按输入索引固定顺序；浮点归约仅 per-star `JᵀJ` 7×7 矩阵，顺序固定。

## 6 时间/空间复杂度

- O(pixels) 检测 + O(n_stars × iter × patch) 拟合；空间 O(patch) per thread

## 7 CPU-only 后端策略（V5）

- 仅 CPU：逐星独立 LM 拟合，worker pool（按 affinity）按星批并行，**禁止硬编码线程数**；per-star 结果与线程调度无关（无跨星归约）。

## 5c SIMD 安全与取消点

- 同星窗口内逐像素 residual 为逐元素算术(SIMD 安全: 无别名/行连续)；LM 内的 normal-equation 累加为**该星内固定顺序归约**(窗口行序)，禁重结合；FP32/FP64 IEEE-754, 禁 fast-math。
- 取消点: 按星批粒度检查; 取消时未完成星不写结果(调用方以 status 判别)。

## 8 参考实现/Oracle

- 合成 Moffat 图像 (已知 B,A,σ,θ) 恢复测试 PSF-001..008 位置 ≤0.05px FWHM ≤1%；残差 Gaussian 假设校验 trimmed-mean 0.73167 复算。

## 9 容差来源

- 位置: FP64 LM 0.05px (sub-pixel 插值误差)；FWHM: 1% (离散化 + α² 缩放)；预冻结。

## 10 关联 ARC/API/TST

- ARC: `THREADING_MODEL.md` OpenMP per-tile
- API: `dynamic_psf.h: dpsf_fit, dpsf_fit_batch`, `star_detector.h: sdet_detect`
- TST: `TST-PSF-001` 合成恢复, `TST-PSF-INV` q_psf解耦, `TST-PSF-FAIL` 饱和/平坦拒
