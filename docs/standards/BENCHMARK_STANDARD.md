# AstroCS Benchmark Standard

- benchmark 只跑 Release；记录 toolchain/CPU/线程/数据规模。
- 性能断言需 variance 报告（多次运行）；禁止单次计时作结论。
- 输出到 run/ 或 reports/，禁止写入 testdata。
- fast path 性能与 reference path 等价性必须成对出现。
