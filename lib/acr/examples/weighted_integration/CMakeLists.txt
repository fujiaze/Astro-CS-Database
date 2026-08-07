# lib/acr/examples/weighted_integration/CMakeLists.txt
#
# ACR 架构冻结（07 号计划 C）：加权积分合成 Mixed 样例。
#   - acr_weighted_integration_example：CPU/OpenMP 参考 + KernelRegistry launcher
#   - acr_weighted_integration_benchmark：quick/standard/full 矩阵 Benchmark
# 样例独立于 Phase1/真实积分；CUDA 实现经桥接 DLL（MinGW 侧无 .cu）。

find_package(OpenMP QUIET)

add_library(acr_weighted_integration_example STATIC
    weighted_integration_kernels.cpp
)
target_link_libraries(acr_weighted_integration_example PUBLIC
    acr_options
    acr_api
    acr_cuda_bridge_loader
)
target_include_directories(acr_weighted_integration_example PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../backends/cuda
)
if(OpenMP_CXX_FOUND)
    target_link_libraries(acr_weighted_integration_example PRIVATE
        OpenMP::OpenMP_CXX)
endif()

add_executable(acr_weighted_integration_benchmark
    weighted_integration_benchmark.cpp
    route_profile_calibration.cpp
)
target_link_libraries(acr_weighted_integration_benchmark PRIVATE
    acr_weighted_integration_example
    acr_scheduler
    acr_qualification
    acr_cost
    acr_routing
    acr_api
    acr_cuda_bridge_loader
)
target_include_directories(acr_weighted_integration_benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler
    ${CMAKE_CURRENT_SOURCE_DIR}/../../qualification
    ${CMAKE_CURRENT_SOURCE_DIR}/../../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cost
    ${CMAKE_CURRENT_SOURCE_DIR}/../../backends/cuda
)
if(OpenMP_CXX_FOUND)
    target_link_libraries(acr_weighted_integration_benchmark PRIVATE
        OpenMP::OpenMP_CXX)
endif()
set_target_properties(acr_weighted_integration_benchmark PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${ACR_OUTPUT_DIR}
)
