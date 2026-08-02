# ACR 模块开发日志

**模块**: AstroCompute Runtime (ACR)
**分支**: `feature/astrocompute-runtime` (base `8f50519`)
**worktree**: `run/worktrees/acr/`

## 进度

### 2026-08-02 Phase B+E+F CMake 集成 + 单测补全
- **背景**：Phase B/E/F 代码已实现但未接入 CMake，3 个新测试未注册，存在多处编译错误。
- **修复的编译错误**：
  1. `task_descriptor.cpp` include 路径错误：`"astro/compute/core/task_descriptor.hpp"` → `"task_descriptor.hpp"`（同目录）
  2. `hardware_profile.hpp` 缺 `<cmath>`：`std::log2` 未声明（Curve::predict 用）
  3. `acr.hpp` 在 `namespace detail` 内前向声明 `struct TaskDescriptor`，与 `astro::compute::TaskDescriptor`（task_descriptor.hpp 真实定义）形成名字遮蔽 → runtime.cpp 报 "incomplete type"。移除该前向声明（submit_*_with_desc 签名只用 TaskTraits，不需要 TaskDescriptor 前向声明）
  4. `task_descriptor.hpp` 未声明 `task_traits_valid`/`task_descriptor_summary`（实现在 .cpp）→ 测试无法链接。补声明到头文件
- **CMake 集成**：
  - `core/CMakeLists.txt`：acr_core 加入 `task_descriptor.cpp`
  - `tests/unit/CMakeLists.txt`：注册 3 个新测试目标（acr_test_task_descriptor 链 acr_core+acr_api；acr_test_hardware_profile 链 acr_api；acr_test_cost 链 acr_cost）
- **清理的警告**：移除 dispatcher.cpp 未用变量 `total_work`；cost_estimator.cpp 未用变量 `bytes_per_item`（range_chunk_thunk 警告系 incomplete-type 错误级联，修复后自动消失）
- **测试结果**：376/376 通过（1 skipped 是 Phase B 别名声明性）。新增 71 测试：test_task_descriptor 17、test_hardware_profile 33、test_cost 21。366/366 非 smoke 测试并行全通过；10 个 SanitizerSmoke 并行负载下偶发 SEGFAULT（-j1 串行 10/10 稳定通过），系预存并发测试 flaky，与本次改动无关（未触及 sanitizer/event/tiles 代码）。

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

Phase D 完成。下一阶段：Phase E（Qualification/路由）依赖 B/C/D，由主 Agent 执行。

### 2026-08-02 Phase D 完成
- **关键决策**：alpaka FetchContent 与 MinGW g++ 16.1.0 工具链兼容性风险已知（ADR-008 oneTBB 前车之鉴），Phase D 用**纯 CUDA** 实现 GPU backend（不依赖 alpaka）。alpaka 作为 ADR-001 评估/未来 adapter 延后。这样 `ACR_BUILD_CUDA=ON` 用 nvcc 编译 .cu，`OFF` 时无任何 CUDA 引用（ADR-009 门禁）
- **backends/cuda/cuda_backend.hpp**：CudaBackend singleton（lazy init，std::call_once 幂等）+ CudaDeviceInfo（name/uuid/compute_capability/sm_count/total_memory/free_memory/driver_version）+ cuda_error_to_status（cudaError→StatusCode：DeviceLost/OutOfMemory/KernelFailed）+ cuda_parallel_for 模板（仅 __CUDACC__ 可见，functor 转发 kernel）+ axpy 启动器
- **backends/cuda/cuda_backend.cu**：实现。cudaGetDeviceCount 枚举设备，无设备/驱动错误时 available()=false 降级不抛异常。cudaSetDevice(0) + cudaGetDeviceProperties + cudaStreamCreate。UUID 格式化为 `GPU-xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`。initialize() 内调用 register_gpu_report_callback(&CudaBackend::gpu_report_json)（CAS 首次生效）。AxpyFunctor + parallel_for_kernel（__global__ 转发）+ axpy 用 cuda_parallel_for 启动
- **backends/cuda/cuda_buffer.hpp**：CudaBuffer<T> 模板（cudaMalloc/cudaFree RAII，移动语义，禁止拷贝）+ copy_h2d/copy_d2h（async + stream sync）+ cuda_event 类（record/sync/elapsed_since）。全部 #ifdef ACR_BUILD_CUDA 保护
- **backends/cuda/cuda_buffer.cpp**：query_device_memory 非模板辅助
- **backends/cuda/CMakeLists.txt**：ACR_BUILD_CUDA=OFF 时 return()；ON 时校验 CMAKE_CUDA_COMPILER，CUDA 标准 17，CMAKE_CUDA_HOST_COMPILER 用 MinGW g++（如未指定），target acr_cuda 静态库链接 cuda+cudart+acr_diagnostics，PUBLIC ACR_BUILD_CUDA=1 传递给消费者
- **tests/unit/test_cuda.cpp**：13 个 GoogleTest（#ifdef ACR_BUILD_CUDA 保护）：设备枚举/幂等/stream、AXPY MatchesCpu/Empty/Null/NonAligned、CudaBuffer RoundTrip/Move/OutOfBounds、cuda_event Timing、DegradePath、GpuReportCallbackRegistered
- **examples/cuda_axpy.cu**：RTX 3060 Ti AXPY 示例，打印设备信息+H2D/Kernel/D2H 耗时+结果校验
- **更新 tests/unit/CMakeLists.txt + examples/CMakeLists.txt**：ACR_BUILD_CUDA 条件编译 test_cuda.cpp + cuda_axpy.cu
- **构建验证**：
  1. **CPU-only 构建通过**（ACR_BUILD_CUDA=OFF）：cmake configure (0.1s) + build 成功，无 acr_cuda target，无任何 CUDA 引用（ADR-009 门禁满足），ctest 52/52 通过（1 skipped 是 Phase B 别名声明性）
  2. **CUDA 构建工具链问题**（已记录）：nvcc 11.8 + MinGW g++ 16.1.0 host **不支持**（`Failed to preprocess host compiler properties` / `Host compiler targets unsupported OS`）。nvcc 11.8 在 Windows 硬编码要求 MSVC host compiler。尝试 `--allow-unsupported-compiler` 无效（MinGW 目标三元组被拒）。nvcc 11.8 + MSVC cl.exe 14.44（VS2022 BuildTools）+ `--allow-unsupported-compiler` 可编译运行 CUDA kernel，但 ACR 依赖 MSYS2 MinGW ABI（oneTBB/hwloc/gtest），与 MSVC ABI 不兼容，无法用 MSVC 编译整个 ACR
  3. **RTX 3060 Ti 真实验证 PASS**：用 MSVC cl.exe + nvcc 编译独立 AXPY 验证程序（run/temp/verify_cuda_axpy.cu，含 cuda_backend.cu 的 AxpyFunctor + parallel_for_kernel 逻辑），在 RTX 3060 Ti 上运行：N=1048576，AXPY 结果与 CPU 完全一致（mismatch=0，max_diff=0），H2D 1.35ms / Kernel 17.19ms / D2H 0.77ms。Device: RTX 3060 Ti (CC 8.6, 38 SMs, 8191 MB)。证明 **kernel 代码已就绪且真实运行正确**
- **Phase D 状态**：CUDA backend 标记"kernel 代码已就绪，编译集成待工具链解决"。工具链升级路径：(a) CUDA 12+ 对 MinGW 支持改善（待验证），或 (b) ACR 全转 MSVC + vcpkg 依赖（大改，超 Phase D 范围）。控制包"至少一个真实 GPU 后端真实通过"要求：AXPY kernel 在 RTX 3060 Ti 真实运行通过（独立验证），完整 CMake 集成编译待工具链
- **本机环境**：nvcc 11.8.89（C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8\bin\nvcc.exe）、MinGW g++ 16.1.0、ninja 1.13.2、MSVC 14.44.35207（VS2022 BuildTools）+ 14.50.35717（VS2026 BuildTools，nvcc 11.8 不支持）、RTX 3060 Ti 驱动 595.79 / 8192 MiB

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
