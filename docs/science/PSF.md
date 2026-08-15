# PSF Science

## 目的

描述点源响应并给出拟合质量代理。

## 科学定义

Moffat4 模型：

```text
I(r) = B + A / (1 + Q)⁴,  Q = 0.5·r²/σ²
```

A=振幅、B=背景、σ=尺度、Q 无量纲。

## PsfFitQuality

```text
residual_scale        = 10–90% trimmed mean |residual|   (PSF 块第 8 列)
robust_residual_sigma = residual_scale / 0.7316728       (Gaussian 假设)
q_psf                 = A / residual_scale               (fit-quality proxy)
```

`q_psf` 是拟合质量代理（剔星/QA），不是图像噪声 SNR；默认不进入 Phase2
逐像素 science weight（SNR-008 退休旧 (A−B)/mad 科学路径）。

## 变量/单位

- I：ADU；r：px；σ：px；A：ADU；B：ADU；q_psf：无量纲。

## 假设

- 视场内 PSF 形状缓变（块状拟合）；星点不饱和。

## 有效域

- 采样充足（≥FWHM/px 合理范围）；无密集混淆。

## 不保证

- 不保证 PSF 各向异性/色差模型（超出 Moffat4）。

## 失效条件

- 拟合不收敛/无星点 → PSF 阶段显式状态。

## 数值精度

FP64 拟合；Levenberg-Marquardt 类求解（已审计，V19 移除未用高斯路径）。

## 参考文献

Moffat (1969)；Trujillo et al. (2001)。

## ID

SCI-PSF-001；ALG-STAR-PSF-*。
