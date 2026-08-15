# Star/PSF Algorithms

关联：SCI-PSF-001；模块：lib/star_detector、lib/dynamic_psf。

## 输入

校准图像 + 噪声估计。

## 输出

星点表（x,y,flux,A,B,σ,q_psf,residual_scale）+ PSF 块 [N,9]。

## Preconditions

图像有限；背景可估计。

## Postconditions

每星 q_psf=A/residual_scale；residual_scale=10–90% trimmed mean |res|。

## Invariants

q_psf 与图像噪声 SNR 解耦（QA 语义，不进 science weight）。

## 伪代码

```text
检测 → 质心 → Moffat4 拟合(Levenberg-Marquardt) → 残差统计 → QA 列
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
