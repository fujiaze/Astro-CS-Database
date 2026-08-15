# Integration Algorithms

关联：SCI-INT-001/002/004/008；模块：lib/phase2/src/integrate.cpp。

## 输入

每像素候选（values/weights/support/accepted）。

## 输出

P2PixelResult：signal/status/counters/support。

## Preconditions

权重正有限；计数一致。

## Postconditions

- signal=Σw x/Σw；support=max(accepted support)；
- 状态显式：OK/INVALID_INPUT/NO_CANDIDATES/ALL_REJECTED/
  ZERO_VALID_WEIGHT。

## Invariants

无 NaN 产品；计数器（n_candidates/n_accepted/n_finite/n_positive_weight/
n_used）一致。

## 复杂度

O(k) 每像素。

## 并行模型

像素并行；无共享状态。

## 数值风险

Σw=0；大动态范围 → FP64。

## ID

ALG-INTEGRATE-*；TEST-INT-*。
