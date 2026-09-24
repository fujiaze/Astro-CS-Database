# Astro Celestial Sphere Database（ACSD） Benchmark Standard

> 上游：ASTROCS_DESIGN.md §8.4（模块与 ABI）

- benchmark 只跑 Release；记录 toolchain/CPU/线程/数据规模。
- 性能断言需 variance 报告（多次运行）；结论面 = 多次运行统计。
- 输出到 run/ 或 reports/；testdata 保持只读。
- fast path 性能与 reference path 等价性必须成对出现。
