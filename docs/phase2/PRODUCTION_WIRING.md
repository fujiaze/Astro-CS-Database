# Phase2/ACR Production Wiring Audit (T305)

> 审计结论: 以 `a7e063e` 为基线, T305 记录生产执行接线事实, 机器核验见 `docs/architecture/threading.md` + `BUILD_GRAPH.md` + `PRODUCTION_WIRING.md` 本文档

## 1 P2_ENABLE_OPENMP: CMake option → compile definition + linker

| 项 | 值 |
|---|---|
| CMake option | `option(P2_ENABLE_OPENMP "... OFF)` lib/phase2/CMakeLists.txt:18, default OFF hard-disable |
| 逻辑 | `if(P2_ENABLE_OPENMP) find_package(OpenMP)` `else set(OpenMP_CXX_FOUND FALSE)` — 避免缓存误链 libgomp |
| Compile def | 无 P2_ENABLE_OPENMP define 到源 (条件编译 `#if defined(P2_ENABLE_OPENMP) && defined(_OPENMP)` 仅 sampler.cpp:30) |
| Link | `if(P2_ENABLE_OPENMP AND OpenMP_CXX_FOUND) target_link_libraries(phase2 PUBLIC OpenMP::OpenMP_CXX)` |
| 线程数 | 不固定 16; 由 `OMP_NUM_THREADS` / `omp_get_max_threads()` 运行时取得, 配置未硬编码 |
| 生产状态 | OFF (serial sampler), 需 `-DP2_ENABLE_OPENMP=ON` 显式开启才 `parallel for` cells |

## 2 Sampler 串/并行与 AIO 锁

| 阶段 | 串/并 | 证据 |
|---|---|---|
| control cells loop | 条件并行 `parallel for` cells when ON else serial | `sampler.cpp:604` + CMakeLists.txt:18 |
| tile read (aio_hios) | 若 OpenMP ON 则 `critical(aio_read)` 串行化 | `sampler.cpp:610 critical` + THREADING_MODEL ARC-004 |
| patch median/MAD | 并行可用 (per-cell 独立) | same parallel region |
| catalogue query | 并行 | same |
| rejected_* counters | `atomic` | `sampler.cpp` |
| cells vector | 预分配 `c*64+off` 确定性索引 | 同上 |

结论: AIO reader 非线程安全, 已用 critical 细粒度串行化 tile 读, 批策略为 cells 预分配 + tile 读临界区最小化。

## 3 Tile/pixel/work item 切分与归约确定性

| 项 | 单位 | 归约 |
|---|---|---|
| Phase2 block | per-tile (512×512) | block planner 按内存分块, 校准后归约 |
| Rejection | per-pixel candidate stack | `p2_reject` per-pixel parallel, 无跨 pixel 归约 |
| Integration | per-pixel `Σw·x/Σw` | `p2_integrate_pixel` 按 i=0..count-1 固定顺序求和 vs/wsum, support `max(accepted)` |
| Drizzle | per-source-pixel candidate | `drizzle_engine.cpp:1662 reduction(+:nSourcePixels)` + tile merge `TileAccumulator` 串行 `t=1..num_threads` |
| ACR chunk | per-tile chunk (px) | `Dispatcher::dispatch_range` `Σ px_i = total`, `p0_i` 连续, concat 等价 |

确定性: 所有路径按输入索引固定顺序, 无调度相关归约, float 累积文档化 (THREADING_MODEL ARC-004)。

## 4 weight_mode=ivar + auto → linear_fit 时 ACR 是否允许 / 禁止

| 配置 | Plan resolve | ACR |
|---|---|---|
| `weight_mode=auto → 2 (ivar)` 默认 | `n<6 percentile, 6-15 winsorized, >15 linear_fit` wbpp_2_9_1 | 禁  |
| `weight_mode=ivar` | 同上 | 禁  |
| `weight_mode=equal/support_x_snr2` | 同上 | 允 (当 acr_route=auto 且 large_scale等允许) |

**科学原因 (ACR-IVAR-001)**: `ivar` 为逐像素 `variance` 逆方差 `1/variance` (SCI-NOISE), ACR 粒度为 per-tile chunk `cell-ivar×support` 与 CPU 逐像素 ivar 不等价 (cell 级平滑 vs 像素级), 混用会改变加权语义, 故 `weight_mode==2` 强制 CPU canonical (TRACEABILITY ACR-IVAR-001: `stage2_common.cpp:391, acr_kernels.cpp` 禁)。

**性能后果**: ivar 生产路径失去 ACR GPU 加速, 为保科学等价的必要代价; equal/legacy 模式可启用 Mixed, 但生产默认 ivar 不混。

## 5 AutoMixed 资格 / 路由 / 实际数据

| 项 | 结论 |
|---|---|
| 资格 | `Operation.qualified = scenario_qualified 3场景全 qualified` (acr memory BDR 三集合隔离), 未 qualified 直接 OpenMP fallback |
| 路由 | `Dispatcher::decide` 按 Profile `route_replay` chosen≤best×1.10, `dispatch_range` Mixed时 Estimator仅 |
| 实际 CPU/GPU item | `MixedRunResult` 含 per-device stats, `dispatch_chunks` 按 `RangeChunk{begin,end}` 计量 |
| 驻留 | `ResidencyManager` input residency, `absolute_peak_vram_bytes` 真实 delta |
| H2D/D2H | CUDA stream async, cold Mixed `timed H2D>0` fresh Dispatcher (BDR D gate) |
| fallback | GPU OOM/无画像信任 → OpenMP per-pixel, 错误经 `FallbackDecision` → CPU equiv |
| 错误传播 | `handle_failure` → fallback, 诊断 via `route_replay` + `qualification_reason` |

## 6 CUDA/CPU 精度与离散 exact 要求

| 项 | 要求 | 证据 |
|---|---|---|
| signal (float32 mosaic_reject) | `max_abs ≤1e-6` | `SCI-ACR-EQUIV-001` 容差 1e-6 |
| signal (float64 vs) | `max_abs ≤1e-12` | 同上 |
| support / rejection_count / mask | exact | 离散计数, 非浮点, 不可放宽 |
| status (INVALID/UNDERDETERMINED) | exact per-pixel | `integrate.cpp` / `rejection.cpp` |
| tile N ordering | exact (NESTED) | `DATA_SEMANTICS` |

结论: 连续量用 dtype-specific 容差, 离散量 exact, 失败后禁止放宽。

## 7 可执行核验

```sh
# P2_ENABLE_OPENMP
cmake -S lib/phase2 -B build/off -DP2_ENABLE_OPENMP=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && grep -c OpenMP build/off/CMakeCache.txt
cmake -S lib/phase2 -B build/on  -DP2_ENABLE_OPENMP=ON  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && grep -c OpenMP build/on/CMakeCache.txt
# ACR 资格三场景
./build/acr/qualification/acr_qualification --three-clean-ctest-runs
# Mixed cold
./build/acr/tests/acr_focused_mixed --gtest_filter=FocusedMixed.AutoMixedWithinTenPercentOfBest
```

证据目录: `02_contracts/T305_WIRING/` (本文件 + api_inventory 交叉)。
