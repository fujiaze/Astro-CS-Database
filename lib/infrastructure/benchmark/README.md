# benchmark

本目录是 kernel benchmark 与 cpu_profile 的实现处：测量数值误差、吞吐与线程扩展，生成绑定机器特征的性能档案。

## 职责边界

- 放：后端加载与选路、基准测试框架与报告、性能档案生成与存储、CPU 能力探测与各 ISA 提供者。
- 不放：生产调度逻辑（lib/infrastructure/scheduler/）、科学算法（lib/algorithms/）、门禁阈值登记（eng/ci/）。
- 发货档由本目录产出的 benchmark 实测档案决定，代码不硬编码线程、ISA 或 block。

## 内容

- backend_host/ —— 后端加载与 CPU 选路（backend_loader、cpu_routing、baseline/avx/avx2/avx512 后端）、基准框架（bench_harness、bench_report）、性能档案（profile_gen、profile_store）、worker 建议与硬件探测。
- cpu/ —— baseline、avx2、avx512 三个提供者（include 与 src）与 common/（能力探测、cpu_capability.schema.json）。
- PENDING.md —— 本目录内容与目标位置的迁移登记文件。

## 上游

上游：docs/ASTROCS_DESIGN.md §9（CPU 后端与资源：benchmark 生成 cpu_profile）、§8.4（infrastructure/benchmark 条目）。