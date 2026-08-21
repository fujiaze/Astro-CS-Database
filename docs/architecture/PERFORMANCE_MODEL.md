# Performance Model

- 热路径禁止 per-pixel alloc/log/fs/clock；per-pixel 数学用连续 buffer。
- 科学精度优先：FP64 reference；FP32 仅显式精度等价路径。
- 关键 fast path 及其 reference：
  - Drizzle candidate conservative test（false negative=0 oracle）；
  - UPM dense cache（sparse evaluate 等价，1e-12）；
  - NoiseWeightModelV1（Monte Carlo/oracle 矩阵）；
  - Gaia 极区 prune（provably-conservative，cache 键精确）。
- 已知性能基线见 docs/performance/BASELINE.md；benchmark 指标挂
  METRIC-* ID（S2 注册）；G-QA 阈值 <5% 回归。
