# Phase2 Sampler

关联：SCI-UPM-*；模块：lib/phase2/src/sampler.cpp。

## 输入

coverage union MOC + 帧 SNR catalogue。

## 输出

control cell 观测（value/uncertainty/snr/ivar/support/quality）。

## Preconditions

target_order 已定；cell 网格 8×8。

## Postconditions

- patch estimator：support>0 + finite 过滤，median 位置 + MAD 尺度；
- SNR 来自 catalogue 邻近星点（不重新检测）；snr_available=0 时不伪装 1.0。

## Invariants

保留负值；cell 无 obs 时标记几何节点（不进数据分量）。

## 复杂度

O(cells + catalogue log n)（dec 排序索引 + 帧 median 预计算）。

## 并行模型

cell 并行；catalogue 只读。

## 数值风险

MAD=0；patch 样本不足 → min_samples 拒绝。

## ID

ALG-P2-SAMPLE-*；TEST-P2-SAMPLE-*。
