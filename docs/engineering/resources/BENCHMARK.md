# Benchmark 标准

上游：最高设计的 CPU 后端与资源一章；机器画像的落点口径见 `CONFIG` 合同。

benchmark 的运行条件、读数口径与机器画像产物的工程正本。

- benchmark 只跑 Release；记录 toolchain/CPU/线程/数据规模。
- 性能断言需 variance 报告（多次运行）；结论面 = 多次运行统计。
- 输出到 run/；testdata 保持只读。
- fast path 性能与 reference path 等价性必须成对出现。

## 参考文献

[1] 内部文档 `docs/ACSD_DESIGN.md，最高设计`，上位来源。
[2] 内部文档 `docs/engineering/resources/PERFORMANCE_MODEL.md`，同层相关正本。
