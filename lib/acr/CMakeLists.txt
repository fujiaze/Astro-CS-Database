cmake_minimum_required(VERSION 3.24)

# ACR_BUILD_* 选项（ADR-009 CPU-only build gate）
option(ACR_BUILD_TESTS        "Build ACR unit tests"               ON)
option(ACR_BUILD_BENCHMARK    "Build acr-benchmark tool"           ON)
option(ACR_BUILD_TOOLS        "Build acr-status/report" ON)
option(ACR_BUILD_EXAMPLES     "Build standalone examples"          ON)
option(ACR_BUILD_CUDA         "Enable CUDA backend (requires CUDA SDK)" OFF)
option(ACR_BUILD_HIP          "Enable HIP backend (requires ROCm)"  OFF)
option(ACR_BUILD_SYCL         "Enable SYCL backend (requires oneAPI/DPC++)" OFF)
option(ACR_ENABLE_STARPU      "Enable StarPU PoC (not for production)" OFF)
option(ACR_FETCHCONTENT_FULL  "Fetch all deps via FetchContent"    ON)
option(ACR_BUILD_CLASSIC_RUNNER "Build acr-classic-runner CLI"     ON)
option(ACR_ENABLE_SANITIZER   "Enable ASan/UBSan (GCC/Clang)"      OFF)

project(astro_compute_runtime
    VERSION 0.1.0
    LANGUAGES CXX
)

# C++20（alpaka 1.2.0 要求）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# F-fix 8: CUDA 11.8 不支持 C++20，CUDA 标准设为 17
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 启用 ctest（顶层调用，确保 build 根生成 CTestTestfile.cmake）
enable_testing()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(NOT CMAKE_GENERATOR AND CMAKE_HOST_WIN32)
    set(CMAKE_GENERATOR "MinGW Makefiles" CACHE STRING "" FORCE)
endif()

# MinGW：加大可执行文件主线程栈保留（与 orchestrator -Wl,--stack,33554432 同思路）。
# 原因：oneTBB 在 MinGW 下的 teardown 偶发 0xC00000FD（栈溢出，负载相关）。
# 保留 16MB 仅增加虚拟地址保留，不提交物理内存。
if(MINGW AND NOT MSVC)
    add_link_options(-Wl,--stack,16777216)
endif()

set(ACR_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin)
set(ACR_LIB_DIR    ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ACR_OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ACR_LIB_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${ACR_LIB_DIR})

if(MSVC)
    # F-fix 8: MSVC 特有 flags 只设到 CMAKE_CXX_FLAGS，不对 CUDA 生效
    # （nvcc 不识别 slash 格式，会把 /W4 等当成输入文件路径）
    # /utf-8 让 cl 用 UTF-8 解析源文件（含中文注释）
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /permissive- /Zc:__cplusplus /EHsc /utf-8")
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# === Sanitizer 选项（Phase H）===
# ACR_ENABLE_SANITIZER=ON 时启用 ASan + UBSan（GCC/Clang），用于 sanitizer smoke 测试
if(ACR_ENABLE_SANITIZER AND NOT MSVC)
    message(STATUS "ACR: Sanitizer ENABLED (ASan + UBSan)")
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -g)
    add_link_options(-fsanitize=address,undefined)
    # TSan 与 ASan 互斥，单独开关（默认不启用）
endif()

# === FetchContent 依赖声明（ADR-008）===
# Phase A 只声明，不 MakeAvailable。Phase B 各模块按需 MakeAvailable。
# 这样 Phase A configure 不联网，秒过；CPU-only 构建不拉 GPU 依赖（ADR-009）。
include(FetchContent)

set(FETCHCONTENT_BASE_DIR ${CMAKE_BINARY_DIR}/_deps CACHE STRING "FetchContent cache dir")
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Use cache after first fetch")

FetchContent_Declare(fmt           GIT_REPOSITORY https://github.com/fmtlib/fmt.git           GIT_TAG 11.0.2      GIT_SHALLOW TRUE)
FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git        GIT_TAG v3.11.3     GIT_SHALLOW TRUE)
FetchContent_Declare(spdlog        GIT_REPOSITORY https://github.com/gabime/spdlog.git       GIT_TAG v1.15.0     GIT_SHALLOW TRUE)
FetchContent_Declare(cli11         GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git      GIT_TAG v2.5.0      GIT_SHALLOW TRUE)
FetchContent_Declare(onetbb        GIT_REPOSITORY https://github.com/uxlfoundation/oneTBB.git GIT_TAG v2022.0.0 GIT_SHALLOW TRUE)
FetchContent_Declare(hwloc         GIT_REPOSITORY https://github.com/open-mpi/hwloc.git     GIT_TAG hwloc-2.11.2 GIT_SHALLOW TRUE)
FetchContent_Declare(cpu_features  GIT_REPOSITORY https://github.com/google/cpu_features.git GIT_TAG v0.9.0     GIT_SHALLOW TRUE)
FetchContent_Declare(googletest    GIT_REPOSITORY https://github.com/google/googletest.git    GIT_TAG v1.15.2     GIT_SHALLOW TRUE)
FetchContent_Declare(benchmark     GIT_REPOSITORY https://github.com/google/benchmark.git    GIT_TAG v1.9.1      GIT_SHALLOW TRUE)
FetchContent_Declare(alpaka        GIT_REPOSITORY https://github.com/alpaka-group/alpaka.git  GIT_TAG 1.2.0       GIT_SHALLOW TRUE)

# spdlog 使用外部 fmt
set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "Use external fmt" FORCE)
# oneTBB
set(TBB_TEST OFF CACHE BOOL "" FORCE)
set(TBB_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TBB_STRICT OFF CACHE BOOL "" FORCE)
# Google Benchmark
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
# alpaka GPU 后端开关（ADR-009：默认 OFF）
set(alpaka_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(alpaka_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(alpaka_ACC_GPU_CUDA_ENABLE OFF CACHE BOOL "" FORCE)
set(alpaka_ACC_GPU_HIP_ENABLE  OFF CACHE BOOL "" FORCE)
set(alpaka_ACC_GPU_SYCL_ENABLE OFF CACHE BOOL "" FORCE)

if(ACR_BUILD_CUDA)
    enable_language(CUDA)
    set(alpaka_ACC_GPU_CUDA_ENABLE ON CACHE BOOL "" FORCE)
    message(STATUS "ACR: CUDA backend ENABLED (requires CUDA SDK)")
endif()
if(ACR_BUILD_HIP)
    set(alpaka_ACC_GPU_HIP_ENABLE ON CACHE BOOL "" FORCE)
endif()
if(ACR_BUILD_SYCL)
    set(alpaka_ACC_GPU_SYCL_ENABLE ON CACHE BOOL "" FORCE)
endif()

# === ACR interface library ===
add_library(acr_options INTERFACE)
target_include_directories(acr_options INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(acr_options INTERFACE cxx_std_20)

# === Phase B: 依赖获取 ===
# ADR-008 原计划 FetchContent，但 oneTBB v2022.0.0 的 CMake 版本检测脚本
# 与 MinGW g++ 16.1.0 不兼容（/dev/null 重定向 + 版本号解析失败）。
# 务实 fallback：优先 MSYS2 系统包（find_package），FetchContent_Declare 仅作 ADR 记录。
# dependency-lock.json 已记录版本差异（tbb 2023.0.0、gtest 1.17.0，来源 MSYS2）。
find_package(TBB QUIET)
if(TBB_FOUND)
    message(STATUS "ACR: oneTBB found via system package: ${TBB_VERSION}")
else()
    message(STATUS "ACR: TBB system package not found, using FetchContent (oneTBB v2022.0.0)")
    set(TBB_TEST OFF CACHE BOOL "" FORCE)
    set(TBB_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(TBB_STRICT OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(onetbb)
endif()
if(ACR_BUILD_TESTS)
    find_package(GTest QUIET)
    if(GTest_FOUND)
        message(STATUS "ACR: GoogleTest found via system package: ${GTest_VERSION}")
    else()
        message(STATUS "ACR: GTest system package not found, using FetchContent (googletest v1.15.2)")
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()
endif()

# === Phase C: hwloc / cpu_features 检测 ===
# hwloc：MSYS2 mingw-w64-x86_64-hwloc 提供头+库，但无 cmake config，用 find_path/find_library
find_path(HWLOC_INCLUDE_DIR NAMES hwloc.h)
find_library(HWLOC_LIBRARY NAMES hwloc libhwloc)
if(HWLOC_INCLUDE_DIR AND HWLOC_LIBRARY)
    set(ACR_HAVE_HWLOC TRUE CACHE BOOL "hwloc available" FORCE)
    message(STATUS "ACR: hwloc found: include=${HWLOC_INCLUDE_DIR} lib=${HWLOC_LIBRARY}")
else()
    set(ACR_HAVE_HWLOC FALSE CACHE BOOL "hwloc available" FORCE)
    message(STATUS "ACR: hwloc NOT found, topology degraded to {\"status\":\"unavailable\"}")
endif()

# cpu_features：MSYS2 无包，优先 find_path；缺失则用 __builtin_cpu_supports 降级
find_path(CPU_FEATURES_INCLUDE_DIR NAMES cpu_features/cpu_features_cache.h)
find_library(CPU_FEATURES_LIBRARY NAMES cpu_features)
if(CPU_FEATURES_INCLUDE_DIR AND CPU_FEATURES_LIBRARY)
    set(ACR_HAVE_CPU_FEATURES_LIB TRUE CACHE BOOL "cpu_features lib available" FORCE)
    message(STATUS "ACR: cpu_features lib found: ${CPU_FEATURES_LIBRARY}")
else()
    set(ACR_HAVE_CPU_FEATURES_LIB FALSE CACHE BOOL "cpu_features lib available" FORCE)
    message(STATUS "ACR: cpu_features lib NOT found, fallback to __builtin_cpu_supports (GCC/Clang)")
endif()

# === 子目录（Phase B 起逐步填充，存在则添加）===
foreach(subdir
    api core buffers topology diagnostics
    backends/cpu backends/alpaka backends/cuda backends/classic
    qualification scheduler utilization
    profile cost
    tests tools/acr_benchmark tools/acr_status tools/acr_report
    tools/acr_classic_runner
    examples
)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${subdir}/CMakeLists.txt)
        add_subdirectory(${subdir})
    endif()
endforeach()

message(STATUS "=== ACR configured ===")
message(STATUS "  Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "  CUDA backend:   ${ACR_BUILD_CUDA} (default OFF, ADR-009)")
message(STATUS "  HIP backend:    ${ACR_BUILD_HIP}")
message(STATUS "  SYCL backend:   ${ACR_BUILD_SYCL}")
message(STATUS "  Tests:          ${ACR_BUILD_TESTS}")
message(STATUS "  Benchmark:      ${ACR_BUILD_BENCHMARK}")
message(STATUS "  Tools:          ${ACR_BUILD_TOOLS}")
message(STATUS "  Examples:       ${ACR_BUILD_EXAMPLES}")
message(STATUS "  Classic runner: ${ACR_BUILD_CLASSIC_RUNNER}")
message(STATUS "  Sanitizer:      ${ACR_ENABLE_SANITIZER}")
message(STATUS "  FetchContent base: ${FETCHCONTENT_BASE_DIR}")
message(STATUS "  (Phase A: deps declared but not fetched. Phase B modules MakeAvailable on demand.)")
