# lib/acr/qualification/CMakeLists.txt — Phase E Qualification 微基准框架
# 链接 acr_api（用 parallel_for 调度 CPU benchmark）+ acr_diagnostics（hardware_report）
# 不依赖 nlohmann/json（手写 ostringstream 序列化，与 diagnostics 风格一致）
add_library(acr_qualification STATIC
    benchmark_driver.cpp
    profile_generator.cpp
)
target_link_libraries(acr_qualification PUBLIC
    acr_options
    acr_api
    acr_diagnostics
)
target_include_directories(acr_qualification PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}
)
message(STATUS "ACR qualification: benchmark_driver + profile_generator (CPU-only, no JSON lib)")
