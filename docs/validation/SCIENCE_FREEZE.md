# Science Freeze

```text
PHASE1_BASE_ALGORITHMS = FROZEN（V14 审核通过后）
PHASE2_BASE_ALGORITHMS = FROZEN
CROSS_STAGE_CONTRACTS  = FROZEN
BASE_API_CONTRACT      = FROZEN
PERFORMANCE_BASELINE   = FROZEN
HIPS_BROWSER_BASE      = FROZEN
```

## 已冻结基线

- HiPS 几何/序列化/hierarchy：V11（外部 oracle）。
- background-clean sampler / standardized Huber / smooth global
  continuation：V13（用户 ACCEPTED）。
- UPM component 语义：V14（data/geometry/unobserved 分开）。

## 冻结后允许

- 业务扩展、GUI、新算法（不改基础定义）；
- 经科学等价门（C/M 逐位或数值等价 + 回归集）的性能/重构优化。
