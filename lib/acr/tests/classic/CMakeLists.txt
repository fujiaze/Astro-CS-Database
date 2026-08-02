# lib/acr/tests/classic/CMakeLists.txt — Phase H 经典实验 E01-E16
# 编译策略：
#   1. acr_classic_experiments 静态库：所有 eXX_*.cpp（不含 main），暴露 run_eXX() 函数
#   2. acr_test_classic 可执行文件：classic_main.cpp + 链接静态库，用于 ctest
#   3. acr_classic_obj OBJECT 库：供 tools/acr_classic_runner 复用 run_eXX()
if(NOT ACR_BUILD_TESTS)
    return()
endif()

# 实验源文件（E01-E16）
set(ACR_CLASSIC_SOURCES
    e01_memory.cpp
    e02_axpy.cpp
    e03_dot.cpp
    e04_transpose.cpp
    e05_convolution.cpp
    e06_resample.cpp
    e07_histogram.cpp
    e08_scan.cpp
    e09_gather_scatter.cpp
    e10_mandelbrot.cpp
    e11_gemm.cpp
    e12_fft.cpp
    e13_mixed.cpp
    e14_utilization.cpp
    e15_failure.cpp
    e16_concurrency.cpp
)

# 静态库：供 acr_test_classic 与 acr-classic-runner 复用实验逻辑
add_library(acr_classic_experiments STATIC ${ACR_CLASSIC_SOURCES})
target_include_directories(acr_classic_experiments PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(acr_classic_experiments
    PUBLIC
        acr_api
        GTest::gtest
    PRIVATE
        acr_scheduler
        acr_utilization
        acr_routing
)
target_include_directories(acr_classic_experiments PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utilization
    ${CMAKE_CURRENT_SOURCE_DIR}/../../routing
)

# ctest 可执行文件（classic_main.cpp 提供 main，不链接 gtest_main）
add_executable(acr_test_classic classic_main.cpp)
target_link_libraries(acr_test_classic PRIVATE
    acr_classic_experiments
    GTest::gtest
)
gtest_discover_tests(acr_test_classic DISCOVERY_MODE PRE_TEST)

message(STATUS "ACR classic: E01-E16 experiments (16 files, acr_test_classic for ctest)")
