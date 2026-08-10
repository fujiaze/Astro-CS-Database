# lib/acr/backends/classic/CMakeLists.txt — 经典实验内核注册
#
# 23 号计划 §1.4：Copy/AXPY/Reduction/Convolution 的 CPU + CUDA launcher。
# CUDA launcher 经 bridge::api() 调用真实 GPU kernel（桥接缺失时如实失败）。
add_library(acr_classic_kernels STATIC classic_kernels.cpp)
target_link_libraries(acr_classic_kernels PUBLIC acr_api acr_options)
target_include_directories(acr_classic_kernels PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../cuda
)
message(STATUS "ACR classic kernels: copy/axpy/reduce/conv3x3 (cpu + cuda launchers)")
