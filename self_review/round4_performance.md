# Round 4 — Performance / Concurrency / Resource Review（V16）

## 数据（本机，3+ 次或全量）

```text
真实 16 帧 Phase2（wbpp_current linear_fit median_center）：
  truth 23.5 / clean 24.6 / trail 24.6 / trail_none 23.4 s
合成 20 帧（V15 复跑）：52.83 / 43.80 / 43.02 → median 43.80s
n2 overlap：42.91 / 41.60 / 41.60 → median 41.60s
sampler：RealHipsControlSampling 9.2s；G6 13.4s
rejection kernel n=200 单栈（修复后）：oracle/matrix 全 PASS（无崩溃）
```

## V16 优化（先正确性后性能）

1. ScratchVec fixed-scratch（n≤64 免堆；>64 heap_mode 一次性迁移）；
2. eligibility 单次 strided 收集（stage2/ACR 不再内联手写）；
3. wbpp_current group plan 一次解析（tile 复用）；
4. MinMax 固定 rank（O(n log n) 单次，无迭代）。

## 检查

| 项 | 结论 |
| --- | --- |
| per-pixel heap churn | kernel n≤64 消除；RCR 保留（P3，非默认） |
| O(N²) | sampler 已索引化（V15）；无新增二次方 |
| thread safety | 无新增共享可变状态 |
| cache/GC | stage2 无 GC；browser LRU 有界 |
| science equivalence | 65/65 gate + satellite clean/truth std ratio 0.9991 |
| >5% 无解释回退 | 无（同负载 23-25s 量级，优于 V14 基线） |

```text
ROUND4=PASS
```
