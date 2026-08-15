# Noise Model Science（NoiseWeightModelV1）

## 目的

估计**校准后空背景随机分量**的逐像素方差（→ ivar），用于 Phase2 科学权重。

## 科学定义

目标量：`variance of calibrated blank-sky random component`。
≠ PSF 星亮度、≠ photometric calibration scatter、≠ q_psf（SNR-003/008/010）。

## 算法

```text
patch grid 8×8；星点掩膜 r = r0 × clamp(sqrt(A/A_median), 1, scale)
σ_bg = 1.4826022185 × median(|x − median(x)|)     # MAD→σ
cosmic/hot 5σ 稳健裁剪 ≤2 轮
控制点 = 合格 patch（≥ min_samples）
空间场 = 最小二乘平面 var(x,y) = a + b·x + c·y
全局兜底 = 合格 patch variance 稳健中位数
ivar = 1/variance
```

## 变量/单位

- x：像素 ADU/e⁻；variance/ivar：信号² / 信号⁻²。

## 假设

- 空背景局部平稳；源星点可掩膜；掩膜半径与星亮度解耦。

## 有效域

- 有合格 patch（≥min_samples）；无大面积云/卫星轨迹时优先。

## 不保证

- 不保证 Drizzle 后相邻像素独立（协方差见 UNCERTAINTY_AND_COVARIANCE）。

## 失效条件

- 无合格 patch → NO_DATA/fallback；NaN 权重 → INVALID_INPUT。

## 系统/随机误差

- 系统：平面场残差低阶项（空间场吸收一部分）；随机：patch 采样误差。

## 数值精度

FP64；MAD 常数 1.4826022185（Gaussian）；裁剪 5σ。

## 参考文献

SNR_REDESIGN_CONTRACT（工程控制）；Fruchter & Hook (2002) 权重语义。

## ID

SCI-NOISE-001..015（SNR-001..015）；ALG-NOISE-ESTIMATION-*（S2）。
