# Integration Algorithms (ALG-INT)

> 上游：ASTROCS_DESIGN.md §5.3（SNR 重建与逆方差叠加）

## 1 上游 SCI 与输入输出

- 上游: `SCI-INT-001,002,004,008` (signal=Σw·x/Σw, support=max, 零权重合同)
- 输入: `P2PixelStack` (values/weights/support/accepted, count)
- 输出: `P2PixelResult` (signal, support, n_used, n_candidates/accepted/finite/positive_weight, status)

### 1a 状态枚举取值（唯一事实源）

| 名称 | 值 |
|---|---|
| P2_INTEGRATE_OK | 0 |
| P2_INTEGRATE_NO_CANDIDATES | 1 |
| P2_INTEGRATE_ALL_REJECTED | 2 |
| P2_INTEGRATE_ZERO_VALID_WEIGHT | 3 |
| P2_INTEGRATE_INVALID_INPUT | 4 |

- 唯一事实源: `lib/algorithms/coverage/include/astro/phase2/integrate.h` (`enum P2IntegrateStatus`)；
- 本表为**取值（代码事实）抄录**，不引入新判据：F5 的分支语义与上表逐值对应；
- 机器门: `eng/tools/docs_machine_consistency.py :: integration_status_full_set`（全集合比对，禁 subset）。


## 2 离散公式

```text
F1: w_i = weights[i] if 提供 else 1.0; 零权重 continue (合法不贡献)
F2: valid: finite(values) ∧ (support空∨finite∧>0) ∧ finite(w) ∧ w≥0 ∧ accepted
F3: invalid_input ⇔ accepted且 non-finite value/support≤0/non-finite/负w
F4: n_accepted, n_finite, n_positive_weight计数
F5: if invalid → INVALID_INPUT; else if n_positive==0 → (n_accepted==0? ALL_REJECTED: ZERO_VALID_WEIGHT)
F6: signal = Σ w·x / Σw, support = max(accepted support) if提供 else 1.0, n_used=n_positive
F6a: support reducer 作用域 = accepted ∧ finite(value/support)（**不含** w>0 要求）；
     零权 accepted 样本合法不贡献 signal, 但必须进入 sup_max
```

来源: `lib/algorithms/coverage/src/integrate.cpp` `lib/algorithms/coverage/include/astro/phase2/integrate.h`（符号 `P2PixelStack` / `Result`）

## 3 伪代码

```text
function validate_weights(w):
  if w==null return 0; for each: if !finite or <0 return 1; if ==0 continue; return 0

function integrate_pixel(in, out):
  memset out; out.n_candidates=in.count
  if count==0 or values==null → NO_CANDIDATES
  for i: acc=accepted?.accepted[i]:true; count n_accepted; if !acc continue
         if !finite(values) → invalid; if support non-finite/≤0 → invalid
         ++n_finite; sup_max=max(sup_max, support)        # 资格通过即进 reducer
         w=weights?.w[i]:1; if !finite(w)或w<0→invalid; if w==0 continue
         ++n_positive; vs+=w·x; wsum+=w
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
| w==0 | 合法不贡献 signal，但计入 support reducer |
| support空 | 1.0 |

## 5 确定性与归约

- 像素独立, for i=0..count-1 固定顺序求和 vs/wsum, 确定性 FP64。

## 6 复杂度

- O(k) 每像素, k=candidate数

## 7 CPU-only 后端策略（V5）

- 仅 CPU: 像素独立无共享, worker pool（按 affinity）按像素行带并行, **线程数取自 benchmark profile**；signal 与线程划分无关（像素内固定候选序归约）。

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

## 参考文献与参考代码库（含许可证）


- 加权均值/逆方差聚合：教科书级（Bevington & Robinson 2003, Data Reduction and Error Analysis for the Physical Sciences 3rd ed., McGraw-Hill；Aitken 1935, Proc. Roy. Soc. Edinburgh 55, 42（DOI 10.1017/S0370164600014346 【出版年双源登记】本仓取 1935 = 论文出版年：一手依据 = 纸本合卷 Proc. R. Soc. Edin. Vol. LV 逐字「Read March 4, 1935」与「Issued separately March 6, 1935」（Internet Archive dli.ernet.7410）；另一源 CrossRef 与 Cambridge Core 卷期页记 1936 = **合卷印年**（同卷扉页逐字「VOL. LV. / 1934-1935 / MCMXXXVI」）。**结论：不改数字，保留 1935**；双源差异在此登记，不按错处理。）的 GLS）。**差异**：本层 reducer 不编码 ivar 语义，权重策略在调用方（SCI-NOISE/SCI-UPM）。
- 最优叠加：Zackay & Ofek 2017, ApJ 836, 187/188；Naylor 1998, MNRAS 296, 339。
- support=max canonical reducer：Project-defined（覆盖并集保守下界，本文件 F6）。

参考代码库（含许可证）正本 = docs/engineering/SCIENTIFIC_REFERENCES.md §M。

