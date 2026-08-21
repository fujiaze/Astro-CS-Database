# Star/PSF Algorithms

关联：SCI-PSF-001；模块：lib/star_detector、lib/dynamic_psf。

## 输入

校准图像 + 噪声估计。

## 输出

星点表（x,y,flux,A,B,σ,q_psf,residual_scale）+ PSF 块 [N,9]。

## Preconditions

图像有限；背景可估计。

## Postconditions

每星 q_psf=A/residual_scale；residual_scale=10–90% trimmed mean |res|；robust_residual_sigma=residual_scale/0.7316728 仅 Gaussian 残差假设下有尺度意义（E[trimmed mean |r|]=0.7316728·σ, kTrimMeanToSigma, lib/snr_estimator/cpp/src/noise_model.cpp:35-37）。

## Invariants

q_psf 与图像噪声 SNR 解耦（QA 语义，不进 science weight）。

## 伪代码

```text
检测 → 质心 → Moffat4 拟合(Levenberg-Marquardt, 7参 B,A,x0,y0,sx,sy,θ; I=B+A/(1+Q)⁴ Q=p1dx²+2p2dxdy+p3dy² FWHM=1.230310·sx/sy flux=2πAsxsy/3 θ四象限{θ,π/2−θ,π/2+θ,π−θ} trimmed-mad消歧 lib/dynamic_psf/src/dpsf_psf.cpp:13-18,66-95,351-363,368) → 残差统计 → QA 列
```

## 复杂度

O(pixels + n_stars × iter)。

## 并行模型

OpenMP 按 tile/块并行；拟合局部。

## 数值风险

拟合退化（平坦星）/饱和；MAD 为 0。

## fast/reference/oracle

合成 Moffat 图像恢复测试（PSF-001..008）；残差分布 Gaussian 假设校验。

## ID

ALG-STAR-PSF-*；TEST-PSF-*。
