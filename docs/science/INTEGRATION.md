# Integration Science

## 目的

将每像素多候选加权合并为统一信号与支持度。

## 科学定义

```text
signal   = Σ w_i x_i / Σ w_i          (w = ivar, 正有限)
support  = max(accepted support)      (canonical reducer)
```

## 状态

P2_INTEGRATE_OK / INVALID_INPUT / NO_CANDIDATES / ALL_REJECTED /
ZERO_VALID_WEIGHT（显式且互斥）。

## 变量/单位

- x：信号；w：ivar（信号⁻²）；support ∈ [0,1]。

## 假设

- 权重与信号独立（由噪声模型保证，SNR-010）；
- 帧间像素经 UPM 校准到公共零底。

## 有效域

- 正有限权重；≥1 候选。

## 不保证

- 不保证 aperture 统计（相邻像素相关见 UNCERTAINTY_AND_COVARIANCE）。

## 失效条件

- NaN/Inf 权重/support → INVALID_INPUT；全 0 权重 → ZERO_VALID_WEIGHT。

## 数值精度

FP64；加权求和顺序固定（确定性）。零权重合法但不贡献 `w==0 continue`（`integrate.cpp:48`），与 `ZERO_VALID_WEIGHT` 区分见 65-66 行。

## 参考文献

Zackay & Ofek (2017) optimal coaddition（inverse-noise-aware 组合）。

## ID

SCI-INT-001, 002, 004, 008；ALG-INTEGRATION-*（S2）。
