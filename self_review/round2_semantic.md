# Round 2 — Semantic / Single-Path Review（V16）

独立重读源码验证：

1. **rejection 生产调用链**：stage2.cpp 只有 `p2_reject_stack_ex`；
   ACR legacy launcher 同函数；compat 仅测试/工具。`p2_reject_stack(`
   在 stage2 零命中 → 单路径 ✓。
2. **eligibility 单路径**：stage2 CPU 与 ACR 均调用
   `p2_collect_candidate_stack`（strided，同一 policy core）；compat 走
   `p2_eligibility_filter`（同一 eligibility_core）→ 无第二套手写资格判定 ✓。
3. **profile 语义**：wbpp_current group-level 一次解析（stage2 group_plan，
   诊断 `rejection_resolved_methods={'16': linear_fit}`）；astrocs_adaptive
   独立 per-tile（头文件与报告明确标注非 WBPP exact）→ 无冒名 ✓。
4. **normalization**：kernel 内 plan.normalization 唯一实现；
   percentile/rcr 组合校验（INVALID_CONFIGURATION）✓。
5. **MinMax**：一次性固定 rank（无迭代）；无第二实现 ✓。
6. **默认值单源**：config_consistency_check.py PASS（含 normalization、
   astrocs_adaptive、minmax 无 max_iterations）✓。
7. **HEALPix/FITS 映射**：无 common 外手写映射（V15 已删）✓。
8. **归档/冻结**：stf.js/web 仅 archive；healpix_stack 冻结 ✓。

```text
two production implementations = 0
two defaults                   = 0
legacy fallback changes science= 0
ROUND2=PASS
```
