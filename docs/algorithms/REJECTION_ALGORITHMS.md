# Rejection Algorithms (ALG-REJ)

> ID: ALG-REJ-001  范围: ALG-REJ-001..008  上游 SCI: SCI-REJ-001  状态: DERIVED (T207 冻结; V5 ALG-006 重验 2026-08-28)  模块: phase2/rejection

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

## 7 CPU-only 后端策略（V5）

- 仅 CPU: 逐像素独立, worker pool（按 affinity）按像素行带并行, **禁止硬编码线程数**；7 方法逻辑与线程划分无关（每像素独立决策树）；large_scale 结构半径仅邻域读, 无跨像素写。

## 5c SIMD 安全与取消点

- 排序/median/MAD 为固定输入序选择(value tie-break frame_id 冻结)；ESD/RCR 迭代为固定序统计(逐像素局部数组)；7 方法阈值比较逐样本独立(SIMD 安全: 栈数组连续无别名)。
- 取消点: 像素行带粒度检查; 取消时该行带 accepted 掩膜不写(整帧重做, 掩膜以帧为原子单元)。

## 8 参考实现/Oracle

- NIST ESD 120/120; 卫星线注入 recall=1.0

## 9 容差来源

- σ阈 4.0/3.0 预冻结, 归约确定性.

## 10 关联 ARC/API/TST

- API: rejection.h: p2_reject, p2_reject_plan_resolve
- TST: synthetic_gate 74/74, NIST ESD

## 11 数据布局

- 输入：每像素候选栈 `values[], weights[], support[], accepted[], frame_id[]`
  （`P2EligibilityGatherInput`，`rejection.h`）；**帧主序** `value_stride=support_stride=chunk_pixels`
  （stage2 `process_cpu_pixel_parallel`：`gidx=s·stride+pixel`），ACR/kernels 亦共享该布局。
- 规划层：`p2_reject_plan_resolve` 以 `n`（nominal contributors，一次解析）路由到 method
  （`rejection.h:153-154`），禁止 per-pixel effective 路由；同 n 方法确定性一致。
- 输出：reject plan（method+阈值）、per-sample reason（`P2_REASON_*`）、`P2_STATUS_*`；
  结构生长（trail）/紧凑（cosmic 不生长，`rejection.cpp:1501-1592`）。
- 内存：候选栈 O(n)；逐像素拒绝就地；无整帧副本。

## 12 误差预算

- 阈值冻结：`sigma/winsorized/averaged 4.0/3.0/8`，`linear_fit 5.0/3.5/8`，
  `percentile 0.2/0.1`，ESD alpha 0.05/max 10（`rejection.cpp:9,1075-1083`）。
- 数值：FP64 全链路；ESD/RCR 参照 NIST 独立实现验证；`双 sqrt 已修 RJ-004`、`NONE NaN 已修 RJ-002`；
  归一化 `astrocs_median_center_v1` 默认（`REJ-005`）。
- 归约确定性：按固定顺序归约；`UNDERDETERMINED (n≤2)` 全接受、`recall=0` 显式（不做伪剔除）。
- 阈值不变量：同 `n` 的 `plan.resolve` 输出 method 唯一（`synthetic_gate`）；非有限
  weights/support → `INVALID_INPUT` hard fail。
- 误差排序：**数值 FP64 ≪ 统计阈值(冻结) ≪ 门禁容差**；卫星线受控注入 recall=1.0。
- 各 F 映射：`p2_collect_candidate_stack`→`rejection.h`（gather, 共享）；`reject_linear_fit_impl`
  →`rejection.cpp:1407`（`NIST ESD`）；计划路由→`p2_reject_plan_resolve`（`synthetic_gate`）。
