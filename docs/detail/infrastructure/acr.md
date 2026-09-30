# Module: acr

> 上游：docs/ASTROCS_DESIGN.md §8.4（顶层结构：lib/infrastructure/acr/ 隔离实验位）、
> §8.5（模块与 ABI）、§1.3（非目标：CPU/GPU 混合生产路由，生产不可达）、
> §9（CPU 后端与资源：仅纯 CPU 进生产）
> 依赖面正本：docs/engineering/DEPENDENCY_RULES.md（ACR 为 DORMANT：不进生产构建/加载/路由/benchmark/发布）
> 等价性正本：docs/science/algorithms/ACR_EQUIVALENCE_ALGORITHMS.md

## 职责

异构计算运行时：kernel registry、CPU/GPU 调度、device executor、
route calibration、cuda bridge（phase2 集成 CPU reference + CUDA）。
本模块是**隔离实验**：`p2_acr_block_eligible(...)` 在生产恒返回 false，
ACR 块在生产不可达是构造性结论；ACR 状态为 DORMANT，保留源码与隔离测试，
不进生产构建、加载、路由、benchmark 与发布（`DEPENDENCY_RULES.md`）。

## 非职责

不改变业务算法语义；CPU reference 是权威科学语义。

## Public API

`lib/infrastructure/acr/include/astro/compute/*.hpp`：
- `acr.hpp` —— 执行面 `submit_range`/`submit_2d`/`submit_tiles`/`submit_batch`/`submit_chunks`/`submit_serial`/`submit_reduce` 与 `submit_*_with_desc`（按 `OperationId`+`TaskTraits` 定路由）；运行时生命周期 `runtime_init`/`runtime_initialized`/`runtime_shutdown`/`runtime_worker_count`/`runtime_status_json`；载体类型 `Event`（`wait`/`ready`/`cancel`/`cancelled`）、`Buffer<T>`/`BufferView<T>`；几何与提示 `Range1D`/`Extent2D`/`TileShape`/`ExecutionHints`/`RuntimeConfig`。
- `kernel_registry.hpp` —— `KernelRegistry` 与进程级 `global_kernel_registry()`（注册键 = (module_id, operation)）。
- `task_traits.hpp` —— `TaskTraits`/`NumericPolicy`/`TaskClass`/`AccessPattern`/`WorkUniformity`/`RouteMode`/`PartitionKind`/`ResidencyPolicy`。
- `topology.hpp`/`hardware_profile.hpp` —— 硬件拓扑探测与硬件画像。

## Data contract

kernel 描述 = `OperationId` + `TaskTraits`（`task_traits.hpp`）；路由配置 = `RouteProfileV2`/`RouteRequest`/`RouteDecision`（`routing/route_profile_v2.hpp`）与静态路由解析（`routing/`，只读）；qualification 画像 = `KernelBenchmarkResult`/`DeviceFingerprint`/`ProfileKind`（`qualification/profile_schema.hpp`，生成器 `qualification/profile_generator.hpp` 的 `ProfileGenerator::build_fingerprint`）；硬件 profile = `hardware_profile.hpp`。

## Ownership

runtime 句柄 RAII；device buffer 显式所有权。

## Thread safety

work_pool 调度；dispatcher 纯 CPU 回退；设备不可用时按 partial 契约交付（现行语义）。

## Errors

设备不可用 → CPU fallback；kernel 失败 → 显式状态。

## Science IDs

依赖 phase2（ALG-UPM-*、ALG-REJ-*、SCI-INT-* 等）；无独立科学定义，
等价性判据正本 = `docs/science/algorithms/ACR_EQUIVALENCE_ALGORITHMS.md`。

## 性能特征

route estimator 由 qualification 微基准标定（`routing/benchmark_route_estimator.hpp` 读 `RouteSamplePoint`/`RouteReplayPoint` 生成 `RoutePrediction`）；qualification 矩阵 = (operation, device, precision, size 档) 组合的路由准入表（`qualification/profile_generator.hpp` + `routing/route_profile_v2.hpp` 的 `OperationRouteProfile`），组合未登记时回退 CPU reference。等价性判据正本 = `docs/science/algorithms/ACR_EQUIVALENCE_ALGORITHMS.md`。

## Tests

unit/classic/fault/integration 套件；cuda 不可用 GTEST_SKIP。

## Known limitations

GPU kernel 仅 phase2 `mosaic_reject_legacy` launcher 注册。

## Source files

lib/infrastructure/acr/（api/backends/core/cost/diagnostics/profile/routing/scheduler/
eng/tools/tests）。
