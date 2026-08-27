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

# Hotfix: OpenMP 硬禁用（Fatduck: 即使存在 libgomp 也串行；避免 0xC0000005）
# 如需实验并行，改此 OFF→ON 后重建（默认 OFF 才能过 32→1+4/4 门禁）
option(P2_ENABLE_OPENMP "Enable OpenMP parallel sampler (default OFF, hotfix revert)" OFF)
if(P2_ENABLE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        message(STATUS "Phase2: OpenMP enabled (P2_ENABLE_OPENMP=ON)")
    else()
        message(STATUS "Phase2: P2_ENABLE_OPENMP=ON but OpenMP NOT found, fallback single thread")
    endif()
else()
    # 关键：显式禁掉上游/缓存中残留的 OpenMP_CXX_FOUND，避免误链 libgomp
    set(OpenMP_CXX_FOUND FALSE)
    message(STATUS "Phase2: OpenMP disabled (hotfix default, serial sampler) - libgomp NOT linked")
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
    # Windows: real DLL loader; Linux: stub (bridge always unavailable, CPU path)
    $<$<PLATFORM_ID:Windows>:../acr/backends/cuda/cuda_bridge_loader.cpp>
    $<$<NOT:$<PLATFORM_ID:Windows>>:src/cuda_bridge_stub.cpp>
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
if(P2_ENABLE_OPENMP AND OpenMP_CXX_FOUND)
    target_link_libraries(phase2 PUBLIC OpenMP::OpenMP_CXX)
endif()

enable_testing()
find_package(GTest QUIET)
if(GTest_FOUND)
    add_executable(phase2_synthetic_gate tests/synthetic_gate.cpp)
    target_link_libraries(phase2_synthetic_gate PRIVATE phase2 GTest::gtest_main
        ${CMAKE_CURRENT_SOURCE_DIR}/../astro_image_io/astro_image_io.dll)
    add_executable(phase2_ivar_wiring tests/ivar_wiring_test.cpp)
    target_link_libraries(phase2_ivar_wiring PRIVATE phase2 GTest::gtest_main
        ${CMAKE_CURRENT_SOURCE_DIR}/../astro_image_io/astro_image_io.dll)
    add_executable(phase2_execution_options tests/execution_options_test.cpp)
    target_link_libraries(phase2_execution_options PRIVATE phase2 GTest::gtest_main)
    add_executable(phase2_routing tests/routing_test.cpp)
    target_link_libraries(phase2_routing PRIVATE phase2 GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(phase2_synthetic_gate DISCOVERY_MODE PRE_TEST)
    gtest_discover_tests(phase2_ivar_wiring DISCOVERY_MODE PRE_TEST)
    gtest_discover_tests(phase2_execution_options DISCOVERY_MODE PRE_TEST)
    gtest_discover_tests(phase2_routing DISCOVERY_MODE PRE_TEST)
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
