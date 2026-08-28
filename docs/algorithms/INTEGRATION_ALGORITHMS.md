# Integration Algorithms (ALG-INT)

> ID: ALG-INT-001  范围: ALG-INT-001..003  上游 SCI: SCI-INT-001  状态: DERIVED (T208 冻结; V5 ALG-006 重验 2026-08-28)  模块: phase2/integrate

## 1 上游 SCI 与输入输出

- 上游: `SCI-INT-001,002,004,008` (signal=Σw·x/Σw, support=max, 零权重合同)
- 输入: `P2PixelStack` (values/weights/support/accepted, count)
- 输出: `P2PixelResult` (signal, support, n_used, n_candidates/accepted/finite/positive_weight, status)

## 2 离散公式

```text
F1: w_i = weights[i] if 提供 else 1.0; 零权重 continue (合法不贡献)
F2: valid: finite(values) ∧ (support空∨finite∧>0) ∧ finite(w) ∧ w≥0 ∧ accepted
F3: invalid_input ⇔ accepted且 non-finite value/support≤0/non-finite/负w
F4: n_accepted, n_finite, n_positive_weight计数
F5: if invalid → INVALID_INPUT; else if n_positive==0 → (n_accepted==0? ALL_REJECTED: ZERO_VALID_WEIGHT)
F6: signal = Σ w·x / Σw, support = max(accepted support) if提供 else 1.0, n_used=n_positive
```

来源: `integrate.cpp:10-79` `integrate.h: P2PixelStack/Result`

## 3 伪代码

```text
function validate_weights(w):
  if w==null return 0; for each: if !finite or <0 return 1; if ==0 continue; return 0

function integrate_pixel(in, out):
  memset out; out.n_candidates=in.count
  if count==0 or values==null → NO_CANDIDATES
  for i: acc=accepted?.accepted[i]:true; count n_accepted; if !acc continue
         if !finite(values) → invalid; if support non-finite/≤0 → invalid
         ++n_finite; w=weights?.w[i]:1; if !finite(w)或w<0→invalid; if w==0 continue
         ++n_positive; vs+=w·x; wsum+=w; sup_max=max(sup_max, support)
  out.n_finite/positive/accepted=n_*
  if invalid → INVALID_INPUT
  else if n_positive==0 → ALL_REJECTED or ZERO_VALID_WEIGHT
  else signal=vs/wsum, support=sup_max, status=OK
```

## 4 边界/NaN/Inf

| 条件 | 行为 |
|---|---|
| count==0 | NO_CANDIDATES |
| non-finite value/support | INVALID_INPUT |
| 负w | INVALID_INPUT |
| w==0 | 合法不贡献 |
| support空 | 1.0 |

## 5 确定性与归约

- 像素独立, for i=0..count-1 固定顺序求和 vs/wsum, 确定性 FP64。

## 6 复杂度

- O(k) 每像素, k=candidate数

## 7 CPU-only 后端策略（V5）

- 仅 CPU: 像素独立无共享, worker pool（按 affinity）按像素行带并行, **禁止硬编码线程数**；signal 与线程划分无关（像素内固定候选序归约）。

## 5c SIMD 安全与取消点

- eligibility 判定与 `vs/wsum` 累加在单像素栈内(≤n_frames 元素, 连续无别名)；**候选索引固定序归约**(FP64, 禁重结合)——即逐像素串行语义, 并行仅在像素间；support=max 为选择非归约。
- 取消点: 像素行带粒度; 取消时该行带 P2PixelResult 不写(以行带为原子单元)。

## 8 参考实现/Oracle

- 常量场 C max_abs 0; 零权重门; 五态互斥; NumPy复算 rtol1e-12

## 9 容差来源

- FP64 1e-12, fixpoint signal, 预冻结。

## 10 关联 ARC/API/TST

- API: integrate.h: p2_integrate_pixel, p2_validate_candidate_weights
- TST: TST-INT-001 常量场, TST-INT-ZERO, FAIL四态
