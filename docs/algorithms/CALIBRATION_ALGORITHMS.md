# Calibration Algorithms

关联：SCI-CAL-001；模块：lib/calibration。

## 输入

raw 亮场 + masterBias/masterDark/masterFlat + cosmetic map。

## 输出

校准后亮场（run/calibrated/<dataset>/<filter>/）。

## Preconditions

- 母版曝光/滤镜匹配；维度一致；数值有限。

## Postconditions

- cal = (raw − bias − dark·t) / flat_norm；flat_norm≠0。

## Invariants

- 校准可逆（相同母版下多次校准一致）；不改变 WCS/header 科学键。

## 伪代码

```text
for each pixel: v = raw − bias − dark×t; v /= flat_norm; cosmetic 修复
```

## 复杂度

O(pixels)，单 pass。

## 并行模型

OpenMP parallel-for 像素块；每块独立；浮点顺序按行固定。

## 数值风险

flat_norm 除零/母版 0 方差；FP64 中间量。

## fast/reference/oracle

主实现即 reference；母版生成对比 Siril 语义（工程控制 04 spec）。

## ID

ALG-CAL-001..；TEST-SCI-CAL-*。
