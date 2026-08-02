# lib/acr/topology/CMakeLists.txt — 硬件拓扑 + ISA 检测
# hwloc：MSYS2 系统包（无 cmake config），用 find_path/find_library
# cpu_features：MSYS2 无包，用 __builtin_cpu_supports 降级
add_library(acr_topology STATIC
    hwloc_topo.cpp
    cpu_features.cpp
)
target_link_libraries(acr_topology PUBLIC acr_options)
target_include_directories(acr_topology PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# hwloc 链接（顶层 CMakeLists.txt 已 find_path/find_library）
if(ACR_HAVE_HWLOC)
    target_include_directories(acr_topology PRIVATE ${HWLOC_INCLUDE_DIR})
    target_link_libraries(acr_topology PRIVATE ${HWLOC_LIBRARY})
    target_compile_definitions(acr_topology PRIVATE ACR_HAVE_HWLOC=1)
    message(STATUS "ACR topology: hwloc enabled (${HWLOC_LIBRARY})")
else()
    message(STATUS "ACR topology: hwloc NOT available, degraded to unavailable JSON")
endif()

# cpu_features 库（如 MSYS2 未来提供包，则用；否则 __builtin_cpu_supports 降级）
if(ACR_HAVE_CPU_FEATURES_LIB)
    target_include_directories(acr_topology PRIVATE ${CPU_FEATURES_INCLUDE_DIR})
    target_link_libraries(acr_topology PRIVATE ${CPU_FEATURES_LIBRARY})
    target_compile_definitions(acr_topology PRIVATE ACR_HAVE_CPU_FEATURES_LIB=1)
    message(STATUS "ACR topology: cpu_features lib enabled")
else()
    message(STATUS "ACR topology: cpu_features lib NOT available, fallback to __builtin_cpu_supports")
endif()
