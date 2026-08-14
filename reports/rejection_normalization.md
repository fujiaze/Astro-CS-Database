# Rejection Normalization（V16）

## 分层

```text
CalibratedScienceStack
  → RejectionNormalizationPolicy（plan.normalization）
  → RejectionWorkingStack（判定工作域）
  → rejection decision（reasons[]）
  → mask 应用回 CalibratedScienceStack
  → weighted integration（原始科学值）
```

## Modes

| mode | formula | 说明 |
| --- | --- | --- |
| none | working = value | identity 工作域 |
| median_center | working = value − median(stack) | per-pixel robust 位置；平移不变方法决策不变；percentile 负值安全 |
| median_scale | working = value / max(|median|, floor) | per-pixel 尺度；WBPP rejectionNormalization=Scale 的 mapped 形式 |

## Contract

- formula / units：工作域无单位（判定用）；积分仍用原始科学值（物理单位）；
- reference：per-pixel stack median（robust 位置；不需要跨帧参考帧，因为
  UPM 已把帧校准到共同模型）；
- when computed：kernel 进入方法前一次（每像素）；
- cached scope：每 pixel stack 一次，无跨像素缓存；
- numerical guard：MEDIAN_SCALE 用 normalization_floor（默认 1e-12）防除零；
- inverse：不需要（decision-only；mask 回原始值）。

## 方法×normalization 合法性（INVALID_CONFIGURATION）

- percentile 必须 median_center（负值科学域安全）；
- rcr 必须 none（官方 oracle 原始值域冻结）；
- 其余方法平移不变 → 任何 mode 决策一致（V16NormalizationTransparent 测试）。

## 必测项（V16NormalizationTransparentAndNegativeSafe / V16InvalidConfigurationCombos）

```text
positive median（与 Siril 原公式等价）
negative physical median（阈值方向正确：-2.0→HIGH、-18.0→LOW）
global additive differences（平移不变方法决策一致）
global scale（median_scale 覆盖）
star field（真实 16 帧门：星点 rel bias=0）
faint structure（背景 std ratio=0.9991，无人工洞）
```
