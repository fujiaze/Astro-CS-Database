> **ARCHIVED_NON_NORMATIVE** — GOV-002 归档历史技术文档，不再作为当前权威。
> 替代文档：docs/performance/

# AstroCS 性能文档 (V19)

## 基线 (冻结历史, 不重复刷 batch)

```text
V18R2: Phase1 ~67.35 s/frame ; Drizzle ~64 s/frame (16-frame batch)
```

## V19 度量方法

- representative single frame
- operation counters (`operation_counts.json`)
- CPU/resource profile
- microbench (bench_drizzle_head / bench_write)
- science oracle 不变

## 重点评价指标

```text
每 source pixel 实际 work      = candidates/source_pixels
candidate efficiency           = true_overlaps/candidates
geometry recomputation         = geometry_builds/source_pixels
S-H fraction                   = sh_calls/(sh_calls+quick_rejects)
memory/cache behavior          = hot_loop_heap_allocations (目标 ~0)
```

不以"CPU 利用率高"推断 GPU 无收益; ACR 是否进入由真实
arithmetic/data-transfer profile 决定 (V20)。
