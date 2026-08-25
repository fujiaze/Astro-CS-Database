# Noise Estimation Algorithms (ALG-NOISE)

> 上游 SCI: SCI-NOISE-001..015  状态: DERIVED (T204 冻结, 2026-08-23)  模块: snr_estimator/noise_model

## 1 上游 SCI 与输入输出

- 上游: `SCI-NOISE-001..015` (8×8 patch, MAD 1.4826, 平面场 a+b·x+c·y, floor 1e-12)
- 输入: 校准图像 `float32[H×W]` + `star_x/y` (掩膜) + `SnrNoiseModelConfig`
- 输出: `NoiseWeightModelV1` (patch variance空间场 + 全局兜底 + ivar + degenerate标志)

## 2 离散公式

```text
F1: σ_bg = 1.482602218505602 · median(|x−median(x)|)  (MAD→σ Gaussian)
F2: 5σ裁剪 ≤2轮: 剔除 |x−median|>5σ, 剩余求 σ_bg
F3: var(x,y)=a+b·x+c·y (LS平面, enable_spatial_field==1 && n_ctrl≥4 else 全局中位数)
F4: variance = max(var, 1e-12), ivar=1/variance
F5: g_model_floor[model*]=floor 指针隔离
F6: 诊断: var_ADU=max(signal,0)/gain + (rn/gain)² (仅 SNR-005, 不入生产)
```

来源: `noise_model.cpp:32-464` `default_config variance_floor=1e-12`

## 3 伪代码

```text
function snr_noise_model_v1(image, star_x/y, cfg):
  grid 8×8 patches
  for each patch:
    mask rmax=max(1,r0)·max(1,scale) 固定不按振幅
    vals = unmasked pixels; med=median(vals); mad=median(|vals−med|)
    sigma=1.4826·mad; 2轮内剔除 |v−med|>5σ → patch_variance
  if n_valid <4 or !spatial_field → global_median fallback
  else LS平面 a,b,c 最小二乘 patch_centers→variance
  g_model_floor[model*]=1e-12; for fill: var=max(a+b·x+c·y, floor)

function snr_noise_model_v1_free(model): g_model_floor.erase(model*)
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| 全星场无空 patch | NO_DATA fallback global |
| `MAD==0` | sigma==0 degenerate |
| `variance≤0` | max→1e-12 |
| 输入 NaN | skip |

## 5 确定性与归约

- patch独立 OpenMP 并行, median局部无跨patch归约；平面LS按patch索引固定顺序。

## 6 复杂度

- O(pixels) mask+median; LS O(8×8)

## 7 CPU/GPU

- CPU patch并行；GPU 按patch切分等价门 1e-9。

## 8 参考实现/Oracle

- MC Gaussian/Poisson/场恢复 SNR-004..006; 1/unc²对比

## 9 容差来源

- sigma 5% (MAD鲁棒性), floor 1e-12 预冻结。

## 10 关联 ARC/API/TST

- API: `snr_estimator.h: snr_noise_model_v1/_f64/_fill/_free`
- TST: `TEST-SCI-NOISE-*` MC矩阵
