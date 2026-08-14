# Science Freeze

```text
PHASE1_BASE_ALGORITHMS = FROZEN（V14 审核通过后）
PHASE2_BASE_ALGORITHMS = FROZEN
CROSS_STAGE_CONTRACTS  = FROZEN
BASE_API_CONTRACT      = FROZEN
PERFORMANCE_BASELINE   = FROZEN
HIPS_BROWSER_BASE      = FROZEN
```

## V15 Final Semantic Closure 冻结状态（2026-08-14，V15 控制包）

```text
PHASE1_BASE_ALGORITHMS = FROZEN
PHASE2_BASE_ALGORITHMS = FROZEN
REJECTION_SEMANTICS    = FROZEN（canonical semantic IDs + typed params +
                        eligibility/rejection 分层 + per-sample reason）
WBPP_AUTO_POLICY       = FROZEN（wbpp_current = WBPP 2.9.1 bestRejectionMethod；
                        nominal<6→percentile；6..15→winsorized；>15→linear_fit）
SATELLITE_REJECTION_GATE = PASS（20-exposure 受控注入 recall=1.0；
                        n<=2 → REJECTION_UNDERDETERMINED，不宣称可剔除）
BASE_API_CONTRACT      = FROZEN
PERFORMANCE_BASELINE   = FROZEN
FINALIZATION_SELF_REVIEW = PASS（6 轮自审 + clean-tree 终验，见 self_review/；
                        ROUND0-6 全 PASS）
```

PIXINSIGHT_EXACT_COMPATIBILITY = NOT_CLAIMED（WBPP profile 仅提供 Auto
routing 政策，不宣称与 PixInsight 内核 bit-exact）。

## 已冻结基线

- HiPS 几何/序列化/hierarchy：V11（外部 oracle）。
- background-clean sampler / standardized Huber / smooth global
  continuation：V13（用户 ACCEPTED）。
- UPM component 语义：V14（data/geometry/unobserved 分开）。

## 冻结后允许

- 业务扩展、GUI、新算法（不改基础定义）；
- 经科学等价门（C/M 逐位或数值等价 + 回归集）的性能/重构优化。
