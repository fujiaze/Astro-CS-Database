# lib/acr/tools/acr_classic_runner/CMakeLists.txt — Phase H 经典实验运行器
# CLI 工具：运行 E01-E21 全部经典实验，输出 JSON 报告
if(NOT ACR_BUILD_CLASSIC_RUNNER)
    return()
endif()
if(NOT ACR_BUILD_TESTS)
    # classic_experiments 库依赖 GTest，无测试时不构建 runner
    message(STATUS "ACR classic-runner: skipped (ACR_BUILD_TESTS=OFF)")
    return()
endif()

add_executable(acr-classic-runner main.cpp)
target_link_libraries(acr-classic-runner PRIVATE
    acr_classic_experiments
    acr_api
)
# classic_common.hpp 通过 acr_classic_experiments 的 PUBLIC include 传递
target_include_directories(acr-classic-runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/classic
)

message(STATUS "ACR classic-runner: acr-classic-runner CLI (E01-E21 JSON report)")
