cmake_minimum_required(VERSION 3.24)

# ACR_BUILD_* 选项（ADR-009 CPU-only build gate）
option(ACR_BUILD_TESTS        "Build ACR unit tests"               ON)
option(ACR_BUILD_BENCHMARK    "Build acr-benchmark tool"           ON)
option(ACR_BUILD_TOOLS        "Build acr-status/report/invalidate" ON)
option(ACR_BUILD_EXAMPLES     "Build standalone examples"          ON)
option(ACR_BUILD_CUDA         "Enable CUDA backend (requires CUDA SDK)" OFF)
option(ACR_BUILD_HIP          "Enable HIP backend (requires ROCm)"  OFF)
option(ACR_BUILD_SYCL         "Enable SYCL backend (requires oneAPI/DPC++)" OFF)
option(ACR_ENABLE_STARPU      "Enable StarPU PoC (not for production)" OFF)
option(ACR_FETCHCONTENT_FULL  "Fetch all deps via FetchContent"    ON)

project(astro_compute_runtime
    VERSION 0.1.0
    LANGUAGES CXX
)

# C++20（alpaka 1.2.0 要求）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(NOT CMAKE_GENERATOR AND CMAKE_HOST_WIN32)
    set(CMAKE_GENERATOR "MinGW Makefiles" CACHE STRING "" FORCE)
endif()

set(ACR_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin)
set(ACR_LIB_DIR    ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ACR_OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ACR_LIB_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${ACR_LIB_DIR})

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /EHsc /constexprSteps-1)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
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

# === ACR interface library（占位，Phase B 填充源码）===
add_library(acr_options INTERFACE)
target_include_directories(acr_options INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(acr_options INTERFACE cxx_std_20)

# === 子目录（Phase B 起逐步填充，存在则添加）===
foreach(subdir
    api core buffers topology diagnostics
    backends/cpu backends/alpaka backends/cuda
    qualification routing scheduler utilization
    tests tools/acr_benchmark tools/acr_status tools/acr_report tools/acr_invalidate
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
message(STATUS "  FetchContent base: ${FETCHCONTENT_BASE_DIR}")
message(STATUS "  (Phase A: deps declared but not fetched. Phase B modules MakeAvailable on demand.)")
