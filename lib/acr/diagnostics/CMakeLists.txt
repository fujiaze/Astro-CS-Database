# lib/acr/diagnostics/CMakeLists.txt — 诊断与设备报告
# Phase C：hardware_report.cpp 合并 hwloc + isa + GPU callback
add_library(acr_diagnostics STATIC
    hardware_report.cpp
)
target_link_libraries(acr_diagnostics PUBLIC acr_options acr_topology)
target_include_directories(acr_diagnostics PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
