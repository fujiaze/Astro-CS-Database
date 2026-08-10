# lib/acr/profile/CMakeLists.txt — Phase E2 HardwareProfileReader
# 读取 hardware-profile.json，三态处理 + lazy load + CPU fallback。
# 链接 acr_api + acr_diagnostics（topology 报告）。
add_library(acr_profile STATIC
    profile_reader.cpp
)
target_link_libraries(acr_profile PUBLIC
    acr_options
    acr_api
    acr_diagnostics
)
target_include_directories(acr_profile PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../diagnostics
)
message(STATUS "ACR profile: profile_reader (Phase E2)")
