# ACR 模块开发日志

**模块**: AstroCompute Runtime (ACR)
**分支**: `feature/astrocompute-runtime` (base `8f50519`)
**worktree**: `run/worktrees/acr/`

## 进度

### 2026-08-02 Phase A 完成
- worktree 从 origin/main (8f50519) 创建 feature/astrocompute-runtime
- astro_toolkit.py 自检通过（ok:true）
- 仓库审计完成：MSYS2 MinGW64 g++ 16.1.0、C++17 现有模块用 OpenMP、无第三方库、CUDA 11.8 + RTX 3060 Ti、无 vcpkg/Conan
- 9 个 ADR 创建：alpaka/oneTBB/hwloc/cpu_features/Google Benchmark/GoogleTest/StarPU 评估/FetchContent/CPU-only 门禁
- dependency-lock.json 创建（12 个依赖，版本锁定）
- forbidden-paths.md 创建（11 个算法目录 + 顶层文件只读）
- ci/path_guard.ps1 创建并验证（中文路径用 git pathspec 排除，pwsh 执行）
- CMakeLists.txt 骨架（Phase A 只声明 FetchContent 不 MakeAvailable，CPU-only 默认不拉 GPU 依赖）
- spec.md + checklist.md + tasks.md 三件套就位

## 重大决策

1. worktree 隔离在 run/worktrees/acr/（沙箱限制，主仓库 experiment/fast-drizzle-r06 不动）
2. ACR 在 lib/acr/ 内独立 CMake，不引入顶层 CMakeLists.txt
3. 依赖全部 CMake FetchContent 拉取固定 tag，不入仓
4. 测试框架 GoogleTest 1.15.2
5. CUDA backend 启用真实验证（RTX 3060 Ti，本机 CUDA 11.8）
6. 本次范围 Phase A-H，Phase I 合并 main 等用户二次授权
7. 公共 API 不暴露第三方类型（PIMPL/type-erased）
8. ACR lazy initialization，合并后普通 AstroCS 运行无副作用
9. path guard 用 git pathspec exclusion 处理中文路径（pwsh 执行）

## 下一阶段

Phase C 完成。下一阶段：Phase D（GPU backend）并行 subcoding agent，Phase E（Qualification/路由）依赖 B/C/D。

### 2026-08-02 Phase C 完成
- **公共头** `include/astro/compute/topology.hpp`：IsaLevel 位掩码枚举（SSE/SSE2/SSE3/SSSE3/SSE41/SSE42/AVX/AVX2/FMA/AVX512F/CD/BW/DQ/VL）+ CpuIsaCaps（has/has_isa 安全门禁）+ HwlocTopology（PIMPL）+ detect_topology/detect_isa_caps/generate_hardware_report + GpuReportCallback 回调注册（Phase D 用）
- **topology/hwloc_topo.cpp**：hwloc 拓扑枚举（package/core/PU/L1L2L3 cache/NUMA/PCI 设备）+ JSON 序列化。无 hwloc 时 `__has_include` 降级返回 `{"status":"unavailable"}`，不抛异常
- **topology/cpu_features.cpp**：CpuIsaCaps 检测。无 cpu_features 库时用 `__builtin_cpu_supports`（GCC/Clang）降级。AVX-512 子集独立 bit（ADR-004 关键约束）
- **backends/cpu/isa/{scalar,sse,avx,avx2,avx512,dispatch}.cpp**：AXPY kernel 实现。用 `__attribute__((target("...")))` 函数级启用 ISA intrinsics（TU 默认 baseline，无需全局 -mavx*）。`kernel_<name>_axpy_safe` 运行时门禁（caps 不支持返回 false 不调用，无 SIGILL）。`dispatch_axpy` 自动选最优 kernel（AVX-512 > AVX2 > AVX > SSE > scalar）
- **diagnostics/hardware_report.cpp**：合并 topology+isa+GPU 回调为 hardware.json（schema=acr.hardware.v1）。GPU 回调用 atomic CAS 首次生效
- **tests/unit/test_topology.cpp**：18 tests（HwlocTopology JSON/降级、CpuIsaCaps SSE2 必有/AVX-512 门禁/子集独立、AXPY dispatch 正确性、hardware_report schema/GPU 回调）
- **依赖获取**：
  - hwloc：MSYS2 mingw-w64-x86_64-hwloc 已装（C:/msys64/mingw64/include/hwloc.h + libhwloc.dll.a），无 cmake config，用 find_path/find_library
  - cpu_features：MSYS2 无包，用 `__builtin_cpu_supports` 降级（g++ 16.1.0 支持）。FetchContent 未启用（ADR-008 工具链问题已知）
- **工具链问题与解决**：
  1. `__builtin_cpu_supports` 参数必须是字符串字面量，不能用变量传入（g++ 报 "parameter to builtin must be a string constant or literal"）。改为直接展开 14 个 if 语句
  2. gtest_discover_tests 默认 POST_BUILD 模式在 build 时运行 exe 发现测试，但 hwloc.dll 不在 PATH 导致 discovery 失败。改用 `DISCOVERY_MODE PRE_TEST`（ctest 运行时才发现）
  3. ctest "No tests found" 因顶层缺 `enable_testing()`。在 lib/acr/CMakeLists.txt 顶层加 enable_testing()（tests/CMakeLists.txt 也有，但顶层才生成根 CTestTestfile.cmake）
  4. ctest 每个测试独立进程，全局变量在测试间重置。FirstCallbackWins 测试改为单测试内注册 cb1+cb2 验证 CAS 首次生效
- **构建验证**：cmake configure (0.8s) + build 成功 + ctest 52/52 通过（1 skipped 是 Phase B 别名声明性）。path_guard 通过（所有改动在 lib/acr/ 内）
- **本机 ISA 检测结果**：SSE/SSE2/SSE3/SSSE3/SSE4.1/SSE4.2/AVX/AVX2/FMA = true，AVX-512 全子集 = false（RTX 3060 Ti 的 CPU 不支持 AVX-512）

### 2026-08-02 Phase B 完成
- acr.hpp 完整公共 API：parallel_for/for_2d/tiles/reduce/batch/scan/chunks/run_for + Buffer/BufferView/Event/RuntimeConfig + StatusCode/AcrError + detail::submit_* type-erased 接口
- runtime_internal.h：EventImpl/EventState（mutex/atomic/condition_variable）
- core/runtime.cpp：oneTBB lazy singleton + submit_range/2d/tiles/batch/chunks/serial/reduce（tbb::parallel_for/parallel_reduce + task_arena + global_control）
- api/event.cpp：Event 类实现（wait/ready/cancel/status）
- 公共头不暴露 tbb 类型（tbb 完全封装在 runtime.cpp，模板内联调用 detail::submit_*）
- examples：minimal_parallel_for（N=1M reduce 验证）、legacy_chunk_adapter（chunk 适配）
- GoogleTest 单测：test_api.cpp（23 tests）+ test_buffer.cpp（11 tests），33/33 通过（1 skipped 是别名声明性）
- **依赖获取变更**：ADR-008 原计划 FetchContent oneTBB v2022.0.0，但 CMake 版本检测脚本与 MinGW g++ 16.1.0 不兼容（/dev/null 重定向 + 版本号解析失败）。fallback 到 MSYS2 系统包 mingw-w64-x86_64-tbb 2023.0.0 + gtest 1.17.0。dependency-lock.json 已记录 acquisition_note。
- oneTBB 2023 API 变更：parallel_reduce functional 形式 identity 是值不是 lambda，已修复
- 构建验证：cmake configure (0.8s) + build 成功 + examples 运行正确（FP32 末位差异 1e-4 符合允许范围）
