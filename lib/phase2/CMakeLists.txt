cmake_minimum_required(VERSION 3.24)
project(astro_phase2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT MSVC)
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
if(NOT CMAKE_GENERATOR AND CMAKE_HOST_WIN32)
    set(CMAKE_GENERATOR "MinGW Makefiles" CACHE STRING "" FORCE)
endif()

add_library(phase2 STATIC
    src/upm.cpp
    src/stage2_common.cpp
    src/rejection.cpp
    src/coverage.cpp
    src/sampler.cpp
    src/block.cpp
    src/integrate.cpp
    src/acr_kernels.cpp
    ../acr/api/kernel_registry.cpp
    ../acr/backends/cuda/cuda_bridge_loader.cpp
    ../acr/scheduler/device_executor.cpp
    ../common/healpix/healpix_core.cpp
    ../common/crypto/sha256.cpp
)
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../acr/include)
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../acr)
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../acr/backends/cuda/bridge)
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../acr/scheduler)
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../astro_image_io/include)
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../common)

enable_testing()
find_package(GTest QUIET)
if(GTest_FOUND)
    add_executable(phase2_synthetic_gate tests/synthetic_gate.cpp)
    target_link_libraries(phase2_synthetic_gate PRIVATE phase2 GTest::gtest_main
        ${CMAKE_CURRENT_SOURCE_DIR}/../astro_image_io/astro_image_io.dll)
    include(GoogleTest)
    gtest_discover_tests(phase2_synthetic_gate DISCOVERY_MODE PRE_TEST)
endif()

add_executable(astrocs-stage2 tools/stage2.cpp)
target_link_libraries(astrocs-stage2 PRIVATE phase2
    ${CMAKE_CURRENT_SOURCE_DIR}/../astro_image_io/astro_image_io.dll)

add_executable(calibrated_pair_diag tools/calibrated_pair_diag.cpp)
target_link_libraries(calibrated_pair_diag PRIVATE phase2
    ${CMAKE_CURRENT_SOURCE_DIR}/../astro_image_io/astro_image_io.dll)

add_executable(rejection_cli tools/rejection_cli.cpp)
target_link_libraries(rejection_cli PRIVATE phase2)

# V17：CUDA bridge DLL（nvcc 构建产物）复制到各 exe 目录（bridge loader
# 搜索路径 1 = exe 目录），保证 clean build 与增量 build 环境一致
# （GPU 不可用时 CudaEquivalent 之外其余 CUDA 测试已 GTEST_SKIP）。
set(_cuda_bridge "${CMAKE_CURRENT_SOURCE_DIR}/../acr/backends/cuda/bridge/acr_cuda_bridge.dll")
if(EXISTS "${_cuda_bridge}")
  foreach(_t phase2_synthetic_gate astrocs-stage2 calibrated_pair_diag rejection_cli)
    add_custom_command(TARGET ${_t} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${_cuda_bridge}" "$<TARGET_FILE_DIR:${_t}>")
  endforeach()
endif()
