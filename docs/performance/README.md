# performance

本目录存放性能基线与优化记录：测量口径、基准数字与真实热点清单。

## 职责边界

- 放：基准测量口径与基线数字、按 profile 排序的优化候选。
- 不放：性能门合同（在 docs/contracts/PERF_GATE_CONTRACT.md）；性能模型设计（在 docs/architecture/PERFORMANCE_MODEL.md）；benchmark 命令说明（在 docs/plugins/infrastructure/20_benchmark.md）。

## 内容

- `BASELINE.md` —— 性能基线数字与测量口径（同机同数据同配置、多次取统计量）。
- `OPTIMIZATION.md` —— 性能优化记录与真实热点分析（以 profile 为先）。

## 上游

上游：docs/ASTROCS_DESIGN.md §9（CPU 后端与资源）。
