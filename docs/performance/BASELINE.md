# Performance Baseline

同一机器、同一数据、同一 config，每 benchmark ≥3 次，记录 median/p95。
基准数据：

```text
Phase1 小真实帧 / 代表性完整帧
Phase2 t4 overlap / GC 3-panel
Browser GC wide / pan / zoom / STF
```

完整数值见 `evidence/performance/*.json`（V14 交付）。

规则：

- 优化前后 science 输出 hash/数值等价；
- 无 >5% 无解释总体回退；
- 未安全优化项标注 `NO_SAFE_OPTIMIZATION_FOUND`。
