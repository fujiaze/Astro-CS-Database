# 当前唯一执行计划

本轮完成后冻结ACR底层，仍不改Phase1和真实业务算法。

## A. 固化架构

- 继续`feature/astrocompute-runtime`，不建新仓库或版本分支；
- 将`01_ARCHITECTURE_FREEZE.md`落实到现有类型和注释；
- 清除与冻结方向冲突的旧通用化入口、固定份额和利用率控制残留；
- 保留已通过的KernelRegistry、WorkToken、makespan路由、内存预算和resident基础；
- 补齐Buffer访问角色、generation、output ownership和真实传输计数所需最小字段；
- 每GPU保持一个executor，多stream只在executor内部。

## B. 关闭V3剩余门禁

- 确认成本全部统一ns；
- recommended chunk、profitability threshold和tail chunk分离；
- qualified与GPU eligible分离；
- Dispatcher生产launcher真实使用resident device view；
- frames/weights预取发生在worker启动前；
- output ownership和GPU范围物化正确；
- 当前同步bridge若限制in-flight，扩展最小异步submit/event接口；
- 不为此扩展通用异构平台。

## C. 实现加权积分核心

新增独立Operation：

```text
synthetic.weighted_integration.fp64acc
```

新增建议文件（可按仓库现有组织调整）：

```text
lib/acr/examples/weighted_integration/
  weighted_integration_common.hpp
  weighted_integration_cpu.cpp
  weighted_integration_cuda.cu
  weighted_integration_benchmark.cpp
  CMakeLists.txt

lib/acr/tests/integration/
  test_weighted_integration.cpp
```

要求：

- frame-major连续输入；
- FP32输入/权重、FP64累加、FP32输出；
- Serial和OpenMP参考；
- CPU launcher无嵌套OpenMP；
- CUDA launcher使用resident frames/weights；
- `IndependentOutputTiles`；
- CPU写host范围，GPU写device范围并物化；
- 注册到KernelRegistry；
- 生成OperationProfile。

## D. 实现GPU内部通道

- 每GPU一个CudaExecutor；
- 实现1/2 stream候选，full可3；
- 每slot持有stream、event和所需输出/staging视图；
- executor支持多个in-flight token并统一计入同一GPU队列和VRAM预算；
- 通过实测选择stream count；无收益时保持1。

## E. Benchmark与报告

- 实现quick/standard/full矩阵；
- 实现single-shot和resident-reuse；
- 模式：Serial、OpenMP、AcrCpuOnly、GpuOnly host/resident、ForcedMixed、AutoMixed；
- 2 warmup、7 repeats，输出median/min/p90；
- 报告speedup、CPU/GPU items、chunks、stream、H2D/D2H、RAM/VRAM、误差；
- 符合`weighted_integration_report.schema.json`；
- 每case和整体有明确超时；容量不足准确跳过。

## F. 测试与Evidence

- 新增四类CTest；
- CPU-only与CUDA构建；
- compute-sanitizer覆盖；
- standard正式Evidence；
- 如无性能提升，写`PERFORMANCE_NOT_QUALIFIED`，不得伪造通过；
- 最终单一干净HEAD生成源码、日志、JSON、git、path guard和SHA；
- 交付小型审核包，不携带build缓存和无关大文件。

## G. 完成定义

满足`06_TEST_AND_ACCEPTANCE.md`与`CHECKLIST.md`后：

- ACR架构冻结；
- 加权积分样例成为未来业务接入模板；
- 可dormant合并main备用；
- 后续真实积分/Drizzle改造另发控制包。
