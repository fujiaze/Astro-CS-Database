# lib/acr/backends/cuda/CMakeLists.txt — CUDA backend（Phase D）
# ACR_BUILD_CUDA=ON 时启用 nvcc 编译 .cu，OFF 时此目录不编译任何内容（ADR-009）。
#
# 设计：
#   - target acr_cuda 静态库：cuda_backend.cu + cuda_buffer.cpp
#   - 链接 cuda + cudart（CUDA runtime）
#   - 依赖 acr_diagnostics（register_gpu_report_callback）+ acr_options（公共头）
#   - PUBLIC ACR_BUILD_CUDA 编译定义传递给消费者（test/example）
#   - nvcc 11.8：CUDA 标准 17，host 编译器用 MinGW g++（CMAKE_CUDA_HOST_COMPILER）
if(NOT ACR_BUILD_CUDA)
    return()
endif()

# CUDA 编译器校验（enable_language(CUDA) 已在顶层触发）
if(NOT CMAKE_CUDA_COMPILER)
    message(FATAL_ERROR
        "ACR_BUILD_CUDA=ON but CMAKE_CUDA_COMPILER not found. "
        "Install CUDA Toolkit or pass -DCMAKE_CUDA_COMPILER=<nvcc>.")
endif()

# CUDA 标准 17（nvcc 11.8 完整支持；C++20 部分支持，稳妥用 17）
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# 让 nvcc 使用 MinGW g++ 作为 host 编译器（Windows 默认 MSVC，需显式指定）
if(NOT CMAKE_CUDA_HOST_COMPILER AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CUDA_HOST_COMPILER "${CMAKE_CXX_COMPILER}" CACHE STRING "" FORCE)
    message(STATUS "ACR CUDA: host compiler = ${CMAKE_CUDA_HOST_COMPILER}")
endif()

add_library(acr_cuda STATIC
    cuda_backend.cu
    cuda_buffer.cpp
    cuda_executor.cpp
)
target_link_libraries(acr_cuda PUBLIC
    acr_options
    acr_diagnostics
)
target_include_directories(acr_cuda PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}
)
# F-fix 8: CUDA runtime headers + scheduler 头文件路径
# cuda_executor.cpp include "scheduler/device_executor.hpp"，需 lib/acr/ 根目录
target_include_directories(acr_cuda PRIVATE
    ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..  # lib/acr/ 根目录
)
# ACR_BUILD_CUDA 传递给消费者（test/example 源文件用 #ifdef 保护）
target_compile_definitions(acr_cuda PUBLIC ACR_BUILD_CUDA=1)

# nvcc 选项：-O2 + 让 host 编译器用 C++17 + /utf-8 + 抑制 deprecated 警告
target_compile_options(acr_cuda PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:-O2 -Xcompiler=-std=c++17 -Xcompiler=/utf-8 -Wno-deprecated-gpu-targets>
)

# CUDA runtime 库链接（cuda + cudart）
target_link_libraries(acr_cuda PUBLIC cuda cudart)

message(STATUS "ACR CUDA: nvcc = ${CMAKE_CUDA_COMPILER}")
message(STATUS "ACR CUDA: CUDA standard = ${CMAKE_CUDA_STANDARD}")
