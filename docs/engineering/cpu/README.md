# cpu

本目录存放 CPU 后端子域的架构合同：能力探测与向量提供器。

## 职责边界

- 放：CPU 能力探测矩阵与 AVX2/FMA 后端提供器的合同。
- 不放：性能基准与优化记录（在 docs/performance/）；基准命令说明（在 docs/detail/infrastructure/20_benchmark.md）；ISA 变体与逐 kernel 选路结论（在上级目录 ISA_VARIANTS.md）。

## 内容

- `CPU_001_CAPABILITY_PROBE.md` —— CPU 能力探测与安全矩阵合同。
- `CPU_003_AVX2_PROVIDER.md` —— 热点 kernel 的 AVX2/FMA 后端提供器合同。

## 上游

上游：docs/ASTROCS_DESIGN.md §9（CPU 后端与资源）。
