# Rejection Algorithms (ALG-REJ)

> 上游 SCI: SCI-REJ-001..008  状态: DERIVED (T207 冻结, 2026-08-23)  模块: phase2/rejection

## 1 上游 SCI 与输入输出

- 上游: `SCI-REJ-001..008` (7方法+wbpp路由+INVALID/UNDERDETERMINED分层)
- 输入: candidate stack (values/support/weights/frame_ids) + P2RejectionPlan (method/profile/thresholds)
- 输出: per-sample reason (P2_REASON_*) + stack status (P2_STATUS_*) + low/high计数

## 2 离散公式

```text
F1: plan resolve: n<6→percentile 0.2/0.1, 6≤n≤15→winsorized 4/3/8, n>15→linear_fit 5/3.5/8 (nominal n)
F2: sigma: median ws, MAD→σ=1.4826·MAD, thresholds 4.0 low /3.0 high 8iter
F3: winsorized: winsor at σ阈, 再sigma
F4: linear_fit: 线性拟合残差MAD尺度 5/3.5 8iter
F5: ESD: Rosner α=0.05 max10 双sqrt已修
F6: RCR: Maples Chauvenet, large_scale trail仅扩展结构 compact不生长
F7: 状态机: n≤underdetermined(2) → UNDERDETERMINED; non-finite → INVALID_INPUT hard fail
```

来源: `rejection.cpp:1,1051-1592` `rejection.h:76-154`

## 3 伪代码

```text
function p2_reject(stack, plan):
  if plan.method==AUTO → INVALID_METHOD
  n_nominal = plan nominal contributors
  method = resolve_profile(n) # wbpp_2_9_1
  if n ≤2 or n<minimum_n → status=UNDERDETERMINED, reason=UNDERDETERMINED
  switch method:
    None: all ACCEPTED
    Sigma/Winsorized/Averaged: iterative σ clipping low/high 8iter
    LinearFit: robust linear fit残差
    ESD: generalized ESD α0.05
    RCR: robust Chauvenet
    Percentile: low 0.2/high0.1
    Minmax: 1/1/4
  large_scale: trail生长 if enabled && extended structure
  return reasons + status OK/ALL_REJECTED/INVALID
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| n≤2 | UNDERDETERMINED 全接受 |
| non-finite weights/support | INVALID_INPUT hard fail |
| method AUTO | INVALID_METHOD |
| n<minimum_n | UNDERDETERMINED |
| 空栈 | NO_CANDIDATES |

## 5 确定性与归约

- 排序确定性按value tie-break frame_id；ESD/RCR迭代固定顺序；无跨像素归约。

## 6 复杂度

- O(n log n) 排序/ESD/RCR

## 7 CPU/GPU

- CPU per-pixel; GPU按pixel切分, 7方法逻辑等价, large_scale仅CPU.

## 8 参考实现/Oracle

- NIST ESD 120/120; 卫星线注入 recall=1.0

## 9 容差来源

- σ阈 4.0/3.0 预冻结, 归约确定性.

## 10 关联 ARC/API/TST

- API: rejection.h: p2_reject, p2_reject_plan_resolve
- TST: synthetic_gate 74/74, NIST ESD
