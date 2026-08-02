# ACR 现有分支纠正审计报告

**日期**: 2026-08-02
**分支**: feature/astrocompute-runtime
**HEAD**: a0d7783 (含旧 routes.json 路由设计)
**新控制包**: AstroCS_ACR_Branch_Control_Package_2026-08-02.zip (SHA-256: 908ffd3405c4b92c94ecae6df40ae832af3da107d25bf55bd7d88fc493ec1f69)

## 1. 现有实现概况

### 1.1 已完成 Phase（旧规范）
- Phase A: 分支创建 + 依赖 ADR
- Phase B: 公共 API + oneTBB CPU runtime（`parallel_for` 等忽略 KernelId）
- Phase C: hwloc 拓扑 + cpu_features ISA 检测 + SSE/AVX/AVX2/AVX-512 kernel 分发
- Phase D: CUDA backend 代码完成（编译集成未通过，nvcc 11.8 vs MinGW g++ 16.1.0 不兼容）
- Phase E: BenchmarkDriver + profile_generator（生成 routes.json，固定 per-kernel 推荐 backend）
- Phase F: dispatcher + fallback + partitioner + mixed_runner（coverage bitmap，但可能接受固定比例）
- Phase G: cpu/gpu/memory/io controller（可能接受人工输入数值）
- Phase H: E01-E16 经典实验 + 故障注入 + sanitizer smoke（未实际启用 ASan）

### 1.2 测试状态
- 单元测试: 305/305 通过（修复 HardwareReport.FirstCallbackWins 隔离 bug 后）
- 经典实验: 142/142 通过
- 故障注入: 10/10 通过
- sanitizer smoke: 10/10 通过（未实际启用 ASan）
- persistence: 5/5 通过

## 2. 与新规范的核心差距（按 19_EXISTING_BRANCH_CORRECTION_TASKS.md）

### 2.1 固定比例概念（§1）
**现状**: 搜索 `cpu_share`/`gpu_share`/`device_weight`/`0.18`/`0.82` → 无匹配。
**现状**: `routing/route_profile.hpp` 有 `RouteEntryView::preferred_backend`（per-kernel 固定推荐 backend），不是 CPU/GPU 百分比，但仍是"固定路由"而非"能力曲线推算"。
**差距**: 需删除 per-kernel 推荐 backend，改为 DeviceProfile 能力曲线 + CostEstimator 动态推算。

### 2.2 Route Profile → Hardware Profile（§2）
**现状**: `qualification/profile_generator.cpp` 生成 `routes.json`（含 per-kernel routes 数组）。
**现状**: `routing/route_profile.hpp` 的 `RouteProfile` 只有 `routes` 数组，无能力曲线。
**差距**: 需新建 `DeviceProfile` 数据结构，包含：
- `arithmetic[precision][operation][isa/thread or gpu]`
- `memory[level/residency][size]`
- `transfer[direction][memory_type][size]`
- `reduction[operation][precision][size]`
- `convolution[method][kernel_shape][precision][size/stride]`
- `irregular[pattern][sparsity/contention][size]`
- `branch[uniformity][size]`
- `overhead[submit/launch/event/alloc/merge]`
- `library[fft/gemm/...]`

### 2.3 接通公共调用链（§3）
**现状**: `acr.hpp` 的 `parallel_for(KernelId /*id*/, Range1D, KernelFn, ExecutionHints)` 中 KernelId 被注释忽略（`/*id*/`），直接调 `detail::submit_range` → CPU runtime。
**现状**: 无 `TaskTraits`/`TaskClass`/`OperationId`/`TaskDescriptor` 类型。
**现状**: 无 `CostEstimator`。
**差距**: 需重写公共 API 签名为 `parallel_for(OperationId, Range1D, TaskTraits, KernelFn, args...)`，并接通：
```
Public API → TaskDescriptor → CostEstimator → Dispatcher → CPU/GPU backend
```

### 2.4 真实 CPU ISA（§4）
**现状**: `backends/cpu/isa/` 有 SSE/AVX/AVX2/AVX-512 kernel 分发，cpu_features 做门禁。
**差距**: 需验证 Benchmark 分别计时各 ISA 变体，不用"检测到了 AVX2"代替真正 AVX2 实现证据。

### 2.5 扩展 Benchmark（§5）
**现状**: `qualification/benchmark_driver.cpp` 只有 Copy/AXPY/Triad 等基础 kernel。
**差距**: 必须补齐：
- FP32/FP64 算术（add/mul/FMA/div/sqrt）
- CPU STREAM 式曲线（Copy/Scale/Add/Triad，覆盖 L1/L2/L3/主存）
- GPU BabelStream 式曲线（device-resident Copy/Mul/Add/Triad/Dot）
- H2D/D2H（普通 + pinned）
- reduction（sum/dot/min/max/sum of squares/mean/variance）
- direct/separable 卷积（3×3/5×5/7×7/15×15/31×31）
- irregular/atomic/branch（Gather/Scatter/Histogram/Mandelbrot）
- submit/launch/event/alloc/merge 固定开销
- 模型拟合与留出验证

### 2.6 真实 Mixed（§6）
**现状**: `tests/classic/e13_mixed.cpp` 可能用 `enable_gpu=false` 模拟 mixed。
**差距**: 必须在真实 GPU 上同时观察 CPU 和 GPU 完成不同唯一 chunk。无 GPU 则 SKIPPED，不能 PASS。

### 2.7 真实 95% 控制（§7）
**现状**: `utilization/` 可能只接受人工输入 0.92/0.99 并验证数学函数。
**差距**: 必须在持续负载下读取实际 CPU/GPU 利用率或明确受限平台的可审计估算，控制提交并报告结果。

### 2.8 Sanitizer（§8）
**现状**: `tests/fault/sanitizer_smoke.cpp` 作为常规 smoke 测试运行，未实际启用 ASan/UBSan。
**差距**: 构建选项和日志必须证明 ASan/UBSan 实际开启。普通生命周期测试不能命名为 sanitizer 验证。

### 2.9 开源复用落地（§9）
**现状**: oneTBB/hwloc 已使用。Google Benchmark 未使用（自写 BenchmarkDriver）。
**差距**: Google Benchmark 或等价成熟框架用于微基准。GEMM/FFT/scan 使用成熟库 adapter，不以自写 naive 实现宣称生产适配完成。

### 2.10 Evidence 统一（§10）
**现状**: 现有 Evidence 从不同 HEAD 生成（修复前后混装）。
**差距**: 从同一干净 HEAD 一次生成。

## 3. 纠正任务清单

### 优先级 P0（架构性变更，必须先做）
1. **删除 per-kernel 固定路由**：`routing/route_profile.hpp` 删除 `RouteEntryView::preferred_backend`，`qualification/profile_generator.cpp` 停止生成 `routes.json`
2. **新增 TaskTraits/TaskClass/OperationId/TaskDescriptor**：按 03_PUBLIC_API_SPEC.md
3. **重写公共 API 签名**：`parallel_for(OperationId, Range1D, TaskTraits, KernelFn, args...)`，强制 TaskTraits
4. **接通调用链**：Public API → TaskDescriptor → CostEstimator → Dispatcher → backend
5. **新增 DeviceProfile 数据结构**：多维能力曲线

### 优先级 P1（Benchmark 扩展）
6. **CPU 画像**：STREAM 式内存 + 算术 + 归约 + 线程曲线 + NUMA
7. **GPU 画像**：BabelStream 式显存 + H2D/D2H/pinned + launch/event
8. **Google Benchmark 集成**：替换自写 BenchmarkDriver

### 优先级 P2（运行时完善）
9. **CostEstimator**：按 TaskTraits + profile 推算成本
10. **动态工作保持 Dispatcher**：共享工作池 + guided 尾部收缩 + coverage
11. **真实 95% 控制**：实际利用率读取 + 软目标 + RAM/VRAM 预算

### 优先级 P3（测试与证据）
12. **经典实验扩展到 E01-E21**：STREAM/BabelStream/PolyBench 风格
13. **真实 Mixed**：真实 GPU 或 SKIPPED
14. **Sanitizer 实际开启**：ASan/UBSan 构建选项
15. **统一 Evidence**：从同一 HEAD 生成

## 4. 保留的有效代码

以下代码经审计可保留，增量修正而非推倒重来：
- `lib/acr/include/astro/compute/acr.hpp`：Buffer/Event/RuntimeConfig（API 签名需扩展）
- `lib/acr/core/runtime.cpp`：oneTBB runtime（submit_* 需改为走 dispatcher）
- `lib/acr/topology/`：hwloc + cpu_features（保留）
- `lib/acr/backends/cpu/isa/`：SSE/AVX/AVX2/AVX-512 kernel（保留，扩展画像）
- `lib/acr/backends/cuda/`：CUDA backend（保留，解决编译集成）
- `lib/acr/diagnostics/`：硬件报告（保留）
- `lib/acr/tests/unit/`：单元测试框架（保留，扩展）

## 5. 建议的提交序列（按 11_GIT_BRANCH_AND_AGENT_RULES.md §4）

1. `docs(acr): replace fixed-share routing with hardware profiling`
2. `refactor(acr): add task traits and profile-based cost model`
3. `bench(acr): add arithmetic memory reduction and convolution profiles`
4. `feat(acr): connect public API to dispatcher and backends`
5. `feat(acr): add dynamic heterogeneous work queues`
6. `feat(acr): enforce utilization and memory budgets`
7. `test(acr): validate real CPU GPU mixed execution and fallbacks`
8. `docs(acr): regenerate consistent evidence from one head`

## 6. 风险

- **规模大**：涉及 API 签名变更、多个模块重写，需分批 subagent 推进
- **编译集成**：Phase D CUDA 仍未解决
- **无 GPU 环境**：真实 Mixed 实验可能只能 SKIPPED
- **Sanitizer**：MinGW g++ 对 ASan 支持有限，可能需要 MSVC
