# Integration Algorithms

关联：SCI-INT-001/002/004/008；V19R3 零权重合同
（INTEGRATION_ZERO_WEIGHT_CONTRACT / INTEGRATION_POLICY_REDUCER_SEPARATION）；
模块：lib/phase2/src/integrate.cpp。

## 输入

每像素候选（values/weights/support/accepted）。

## 输出

P2PixelResult：signal/status/counters/support。

## Preconditions

values/support/accepted 计数一致；weights 可空（等权）；
提供时须 nonnegative finite（V19R3 冻结：零权重合法）。

## V19R3 权重资格合同

```text
weight NaN/Inf/负  → INVALID_INPUT（validator 与 reducer 同判）
weight == 0        → 合法但不贡献（ZERO_VALID_WEIGHT 可达）
weight > 0         → 可用
value/support 非 finite、support<=0 → INVALID_INPUT
```

policy/reducer 分离：`P2PixelStack.weight_mode` 已删除（V19R3 前未实际
读取，reducer 只消费调用方构造的 numeric weights）。ivar/SNR 科学策略
完全在上游（stage2 构造权重处）解析，reducer 无政策知识。

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

ALG-INTEGRATE-*；TEST-INT-*；V19R3 门：V17NonFiniteWeightInvalid /
V17StatusesExplicit（含 ZERO_VALID_WEIGHT 正权重=0 路径）。
