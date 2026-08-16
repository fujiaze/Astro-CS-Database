# Noise Model Science（NoiseWeightModelV1）

## 目的

估计**校准后空背景随机分量**的逐像素方差（→ ivar），用于 Phase2 科学权重。

## 科学定义

目标量：`variance of calibrated blank-sky random component`。
≠ PSF 星亮度、≠ photometric calibration scatter、≠ q_psf（SNR-003/008/010）。

## 算法

```text
patch grid 8×8；星点掩膜（fixed conservative，统一半径
rmax = max(1, r0) × max(1, scale)，不按星亮度缩放；V19R4 冻结）
σ_bg = 1.4826022185 × median(|x − median(x)|)     # MAD→σ
cosmic/hot 5σ 稳健裁剪 ≤2 轮
控制点 = 合格 patch（≥ min_samples）
空间场 = 最小二乘平面 var(x,y) = a + b·x + c·y（负预测 clamp 1e-12）
全局兜底 = 合格 patch variance 稳健中位数
ivar = 1/variance
```

## 变量/单位

- x：像素 ADU/e⁻；variance/ivar：信号² / 信号⁻²。

## 假设

- 空背景局部平稳；源星点可掩膜；掩膜半径与星亮度解耦（fixed
  conservative 语义；若未来需要 PSF-aware adaptive mask，须先扩展
  API 输入振幅并重新冻结/测试，禁止文档与实现分叉）。

## 生产配置（NOISE-WIRE-001，V19R4）

生产调用必须先 `snr_noise_model_v1_default_config()` 再覆盖显式
元数据（gain/readnoise 等）；`cfg==nullptr`、`default_config()` 与
生产默认（default + gain/readnoise=0）三者逐字段 exact（含 spatial
field 默认开启、variance_floor=1e-12）。

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
