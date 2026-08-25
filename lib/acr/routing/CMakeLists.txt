# lib/acr/routing/CMakeLists.txt — Benchmark 驱动路由（Profile v2 + Estimator）
add_library(acr_routing STATIC
    route_profile_v2.cpp
    benchmark_route_estimator.cpp
)
target_link_libraries(acr_routing PUBLIC
    acr_options
    acr_api
    nlohmann_json::nlohmann_json
)
target_include_directories(acr_routing PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
message(STATUS "ACR routing: benchmark-driven route profile v2 + estimator")
