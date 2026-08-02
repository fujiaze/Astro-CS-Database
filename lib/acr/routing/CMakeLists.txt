# lib/acr/routing/CMakeLists.txt — Phase E 静态路由解析器（只读 profile）
# 链接 acr_diagnostics（query_current_fingerprint 用 generate_hardware_report）
# 不依赖 qualification（避免循环依赖：qualification 生成 profile，routing 只读 profile）
add_library(acr_routing STATIC
    static_router.cpp
)
target_link_libraries(acr_routing PUBLIC
    acr_options
    acr_diagnostics
)
target_include_directories(acr_routing PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}
)
message(STATUS "ACR routing: static_router (read-only profile, no JSON lib)")
