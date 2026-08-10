# lib/acr/tools/acr_benchmark/CMakeLists.txt — acr-benchmark CLI 工具
# 由 ACR_BUILD_BENCHMARK 选项控制（默认 ON）
if(NOT ACR_BUILD_BENCHMARK)
    return()
endif()

add_executable(acr-benchmark main.cpp)
target_link_libraries(acr-benchmark PRIVATE
    acr_qualification
    acr_api
)
target_include_directories(acr-benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../qualification
)
message(STATUS "ACR tool: acr-benchmark (CPU-only, hand-rolled CLI parser)")
