# Calibration Algorithms

关联：SCI-CAL-001；模块：lib/calibration。

## 输入

raw 亮场 + masterBias/masterDark/masterFlat + cosmetic map。

## 输出

校准后亮场（run/calibrated/<dataset>/<filter>/）。

## Preconditions

- 母版曝光/滤镜匹配；维度一致；数值有限。

## Postconditions

- cal = (raw − dark)/flat_norm (dark_opt=0, Dark已含Bias) 或 (raw − bias − K·(dark−bias))/flat_norm (dark_opt=1, K=t_light/t_dark)；flat_norm = max(median=1.0, 0.1)（`lib/calibration/src/calibrator.cpp:78-93,104-136` normalize_flat median→1.0, <0.1→0.1；`lib/calibration/src/master_generator.cpp:222-234` 最终 median=1.0 clamp 0.1）。

## Invariants

- 校准可逆（相同母版下多次校准一致）；不改变 WCS/header 科学键。

## 伪代码

```text
normalize_flat: flat /= median(flat); flat = max(flat, 0.1)  // median→1.0 clamp 0.1
for each pixel:
  if dark_opt==1 && bias&&dark: v = raw − bias − K·(dark−bias)  // K=k_init=t_light/t_dark
  else:                         v = raw − dark                   // k→1.0 fallback
  v /= max(flat_norm, 0.1); cosmetic 修复
```

## 复杂度

O(pixels)，单 pass。

## 并行模型

OpenMP parallel-for 像素块；每块独立；浮点顺序按行固定。

## 数值风险

flat_norm 除零/母版 0 方差；FP64 中间量；flat 归一后 clamp 0.1 避免过小除数（`calibrator.cpp:90,120,130,164,173`）。

## fast/reference/oracle

主实现即 reference；母版生成对比 Siril 语义（工程控制 04 spec）。

## ID

ALG-CAL-001..；TEST-SCI-CAL-*。
