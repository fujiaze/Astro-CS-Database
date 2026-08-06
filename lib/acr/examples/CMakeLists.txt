# lib/acr/examples/CMakeLists.txt — 独立示例
add_executable(acr_example_minimal minimal_parallel_for.cpp)
target_link_libraries(acr_example_minimal PRIVATE acr_api)

add_executable(acr_example_chunk legacy_chunk_adapter.cpp)
target_link_libraries(acr_example_chunk PRIVATE acr_api)

# Phase D：CUDA AXPY 示例（ACR_BUILD_CUDA=ON 时编译 .cu）
if(ACR_BUILD_CUDA)
    add_executable(acr_example_cuda_axpy cuda_axpy.cu)
    target_link_libraries(acr_example_cuda_axpy PRIVATE acr_cuda)
    target_compile_definitions(acr_example_cuda_axpy PRIVATE ACR_BUILD_CUDA=1)
endif()

# ACR 架构冻结（07 号计划 C）：加权积分合成 Mixed 样例
add_subdirectory(weighted_integration)
