# lib/acr/backends/cpu/CMakeLists.txt — CPU backend（ISA 专用 kernel）
# Phase C：baseline/SSE/AVX/AVX2/AVX-512 AXPY kernel + dispatch
# 用 target attribute 启用 ISA（函数级），TU 默认 baseline，无需 -mavx* 全局开关
add_library(acr_cpu_isa STATIC
    isa/scalar.cpp
    isa/sse.cpp
    isa/avx.cpp
    isa/avx2.cpp
    isa/avx512.cpp
    isa/dispatch.cpp
)
target_link_libraries(acr_cpu_isa PUBLIC acr_options acr_topology)
target_include_directories(acr_cpu_isa PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)
target_include_directories(acr_cpu_isa PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/isa
)
