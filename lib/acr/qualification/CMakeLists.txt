# lib/acr/qualification/CMakeLists.txt — Phase E Qualification 微基准框架
# 链接 acr_api（用 parallel_for 调度 CPU benchmark）+ acr_diagnostics（hardware_report）
# Phase E3：profile_generator 包含 hardware_profile.hpp（生成 hardware-profile.json）
# Phase C：新增 acr_benchmarks 静态库（Google Benchmark 驱动的微基准）
#          + acr-benchmark-gb 可执行文件（JSON 输出）
# 不依赖 nlohmann/json（手写 ostringstream 序列化，与 diagnostics 风格一致）
add_library(acr_qualification STATIC
    benchmark_driver.cpp
    profile_generator.cpp
    focused/focused_operations.cpp
    focused/operation_profile.cpp
    focused/focused_benchmark.cpp
    ../../common/crypto/sha256.cpp   # V18R2 (CODE-002): 归一化 SHA-256
)
target_link_libraries(acr_qualification PUBLIC
    acr_options
    acr_api
    acr_diagnostics
    acr_cuda_bridge_loader
)
target_include_directories(acr_qualification PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../backends/cuda
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common
)
message(STATUS "ACR qualification: benchmark_driver + profile_generator + hardware_profile (Phase E3)")

# ============================================================================
# Phase C：Google Benchmark 驱动的 CPU 画像微基准
# ============================================================================
# 设计（05_OPEN_SOURCE_REUSE_PLAN.md §6）：
#   - Google Benchmark 驱动单设备基础微基准，参数化尺寸/精度/ISA/线程点
#   - 公共 API 不暴露第三方类型（benchmark/benchmark.h 仅在 .cpp 内用）
#   - 无网络时用系统包（find_package）；缺失则 FetchContent（ADR-008）

# Phase C 新增：Google Benchmark 微基准库
# 包含 STREAM / 算术 / 归约 / 线程曲线 / NUMA 五类画像 benchmark
if(ACR_BUILD_BENCHMARK)
    # Google Benchmark 依赖获取：优先 find_package，缺失则 FetchContent MakeAvailable
    # 与 GoogleTest 一致：MSYS2 mingw-w64-x86_64-google-benchmark 提供系统包
    # 禁用 Google Benchmark 自身的 -Werror（v1.9.1 的 sysinfo.cc 在 MinGW 上有 switch 警告）
    set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
    find_package(benchmark QUIET)
    if(NOT benchmark_FOUND)
        # FetchContent fallback（ADR-008：声明在根 CMake，此处 MakeAvailable）
        FetchContent_MakeAvailable(benchmark)
        message(STATUS "ACR qualification: Google Benchmark via FetchContent (v1.9.1)")
    else()
        message(STATUS "ACR qualification: Google Benchmark via system package: ${benchmark_VERSION}")
    endif()

    # 微基准源文件（Google Benchmark 注册的 benchmark）
    # Commit E：补全 transfer/convolution/irregular/atomic/branch/overhead benchmark
    set(ACR_BENCHMARK_SOURCES
        benchmarks/stream_benchmark.cpp
        benchmarks/arithmetic_benchmark.cpp
        benchmarks/reduction_benchmark.cpp
        benchmarks/thread_curve_benchmark.cpp
        benchmarks/numa_benchmark.cpp
        benchmarks/transfer_benchmark.cpp
        benchmarks/convolution_benchmark.cpp
        benchmarks/irregular_benchmark.cpp
        benchmarks/atomic_benchmark.cpp
        benchmarks/branch_benchmark.cpp
        benchmarks/overhead_benchmark.cpp
    )

    # OBJECT 库：所有 Google Benchmark 注册（不含 main）
    # 用 OBJECT 而非 STATIC：避免 BENCHMARK() 宏的静态注册变量被链接器 dead-stripped
    # OBJECT 库的 object 文件直接传递给最终可执行文件，不经过 .a 归档和符号筛选
    add_library(acr_benchmarks OBJECT ${ACR_BENCHMARK_SOURCES})
    target_link_libraries(acr_benchmarks PUBLIC
        acr_qualification
        acr_api
        benchmark::benchmark
    )
    target_include_directories(acr_benchmarks PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    # 禁用未使用函数警告：宏定义的 ISA 变体 kernel 可能未被引用（按硬件能力筛选）
    if(NOT MSVC)
        target_compile_options(acr_benchmarks PRIVATE -Wno-unused-function)
    endif()
    # hwloc 链接（NUMA benchmark 需要）
    if(ACR_HAVE_HWLOC)
        target_include_directories(acr_benchmarks PRIVATE ${HWLOC_INCLUDE_DIR})
        target_link_libraries(acr_benchmarks PRIVATE ${HWLOC_LIBRARY})
    endif()

    # 可执行文件：benchmark_main.cpp + acr_benchmarks
    add_executable(acr-benchmark-gb benchmarks/benchmark_main.cpp)
    target_link_libraries(acr-benchmark-gb PRIVATE
        acr_benchmarks
        benchmark::benchmark_main
    )
    # 默认输出到 ACR_OUTPUT_DIR
    set_target_properties(acr-benchmark-gb PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${ACR_OUTPUT_DIR}
    )
message(STATUS "ACR qualification: acr-benchmark-gb (Google Benchmark driver, JSON output)")
endif()

# ============================================================================
# 聚焦版（08 号计划 §4）：acr-benchmark-focused
# 只测目标像素 Operation 与基础传输，输出 OperationProfile（schema 校验通过）。
# ============================================================================
add_executable(acr-benchmark-focused
    focused/acr_benchmark_focused_main.cpp
)
target_link_libraries(acr-benchmark-focused PRIVATE
    acr_qualification
    acr_api
)
set_target_properties(acr-benchmark-focused PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${ACR_OUTPUT_DIR}
)
message(STATUS "ACR qualification: acr-benchmark-focused (OperationProfile, 08 号计划)")
