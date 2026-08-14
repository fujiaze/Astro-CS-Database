# Rejection 语义（V15 冻结）

## 分层（V15 强制）

```text
Raw Contributors
  -> EligibilityPolicy（p2_eligibility_filter：finite/valid/support/quality）
  -> CandidateStack
  -> RejectionPolicy / Auto resolver（p2_reject_plan_resolve，planning 层）
  -> Explicit RejectionPlan（typed params + minimum N）
  -> RejectionKernel（p2_reject_stack_ex）
  -> RejectionDecision[]（per-sample reason + stack status）
  -> WeightedIntegration
```

## Canonical semantic IDs

| semantic_id | display（config alias） | typed params | min N |
| --- | --- | --- | --- |
| astrocs.none.v1 | none | — | 0 |
| astrocs.robust_mad_clip.v1 | sigma | lower_sigma/upper_sigma/max_iterations | 3 |
| astrocs.winsorized_sigma_siril_1_4_3.v1 | winsorized_sigma | lower_sigma/upper_sigma/max_iterations | 3 |
| astrocs.averaged_sigma.v1 | averaged_sigma（IRAF exact=NOT_CLAIMED） | lower_sigma/upper_sigma/max_iterations | 3 |
| astrocs.linear_fit_siril_1_4_3.v1 | linear_fit | lower/upper/max_iterations | 4 |
| astrocs.generalized_esd_nist.v1 | generalized_esd | alpha/max_outliers | 3 |
| astrocs.rcr_2_4_7_ss_median_dl.v1 | rcr | technique=ss_median_dl | 3 |
| astrocs.percentile_siril.v1 | percentile | low_fraction/high_fraction | 2 |
| astrocs.median_std_clip.v1 | median_sigma | lower_sigma/upper_sigma/max_iterations | 3 |
| astrocs.minmax.v1 | minmax | reject_low_count/reject_high_count/max_iterations/min_kept | 5 |

## V16 变更

- MinMax：一次性固定 rank 删除（reject_low_count 个最低 + reject_high_count
  个最高，一次；n−low−high ≥ min_kept）；`max_iterations` 已从 typed config
  删除；
- RejectionNormalizationPolicy：plan.normalization（none/median_center/
  median_scale）；percentile 必须 median_center；rcr 必须 none；
  decision 作用于 working stack，mask 回原始科学值积分；
- averaged_sigma 改名 `astrocs.averaged_sigma.v1`（IRAF exact
  compatibility = NOT_CLAIMED，不冻结为 IRAF-compatible）；
- profile：wbpp_current（group-level 一次解析）与 astrocs_adaptive
  （tile nominal-depth）分离。

## RJ-001..008 修复

| ID | 问题 | 修复 | 回归测试 |
| --- | --- | --- | --- |
| RJ-001 | NONE 重新接受非 finite | eligibility 层过滤；kernel INVALID_INPUT | V15NoneDoesNotReacceptNaN |
| RJ-002 | valid=false 保持 accepted=1 | 掩码统一先清零，仅 ACCEPT 写 1 | V15ValidFalseStaysRejected |
| RJ-003 | rejected_low/high 用数值正负号 | 阈值语义（低于 lower / 高于 upper） | V15LowHighThresholdSemantics |
| RJ-004 | status 与 sample reason 混杂 | 分离 stack status / per-sample reason 枚举 | reasons 断言（V15 系列） |
| RJ-005 | ESD 标准差双重 sqrt | 只开方一次；NIST 54 精确 rejected set | V15EsdSingleSqrtExactRosnerSet |
| RJ-006 | 参数同名异义 | method-specific typed params | V15TypedPercentileParams / V15TypedMinmaxParams |
| RJ-007 | support/quality 声称输入未消费 | eligibility 分层；kernel 只收 CandidateStack | V15FilterAllPolicies |
| RJ-008 | sigma=median+MAD 名不副实 | canonical ID robust_mad_clip；Astropy mad_std oracle | sigma_vs_astropy（oracle） |

## Auto（WBPP 2.9.1，planning 层）

```text
nominal < 6   -> percentile
6..15         -> winsorized_sigma
>15           -> linear_fit
```

effective 候选数 <= underdetermined_n（默认 2）或 < 方法 minimum N →
`P2_STATUS_UNDERDETERMINED`（reason=UNDERDETERMINED，全接受但记录）。

## 统计语义

- `rejected_low` = 低于 lower threshold 的样本数；`rejected_high` = 高于
  upper threshold 的样本数（minmax/ESD/RCR 用整体 median 边界映射 side）。
- `iterations` = 方法实际迭代轮数；`status` 与 `reasons[]` 分离。

## API 清单（V15 新增/修改）

```text
p2_reject_plan_resolve(request, plan, err, err_cap)
p2_rejection_semantic_id(method)
p2_eligibility_filter(in, out)
p2_rejection_workspace_create/free(capacity)
p2_reject_stack_ex(stack, plan, decision, ws)
p2_reject_stack(in, out)  // COMPAT adapter（生产不再调用）
```
