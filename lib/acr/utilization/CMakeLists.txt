# lib/acr/utilization/CMakeLists.txt — 26 号计划 §2/§9 资源边界
#
# CPU/GPU 精确利用率控制已撤销并移除（CpuController/GpuController/IoBudget 删除）。
# 只保留：
#   - system_metrics：RAM/VRAM 真实读取（GlobalMemoryStatusEx + NVML 动态加载）
#   - memory_budget：RAM/每 GPU VRAM 容量预算、峰值估算与反压动作
#   - config_hot_read：配置热读取（内存比例/保留量/backend 启用）
#   - actual_tracker：诊断 telemetry（不影响 claim/路由/PASS/FAIL）
option(ACR_ENABLE_NVML "Enable NVML dynamic loading for GPU memory read (graceful fallback if nvml.dll missing)" ON)

add_library(acr_utilization STATIC
    system_metrics.cpp
    memory_budget.cpp
    staging_ledger.cpp
    config_hot_read.cpp
    actual_tracker.cpp
)

# Windows API：GetSystemTimes / GlobalMemoryStatusEx / LoadLibrary
# MinGW 提供 windows.h，无需额外链接库
if(WIN32)
    # MinGW: pdh 可选（用于 PDH 计数器，当前用 GetSystemTimes 不需要）
    # 链接 kernel32（LoadLibrary/GetSystemTimes/GlobalMemoryStatusEx 在 kernel32）
    target_link_libraries(acr_utilization PUBLIC kernel32)
endif()

# NVML：通过 LoadLibrary 动态加载，编译期不依赖 nvml.lib/nvml.h
# ACR_ENABLE_NVML=ON 时定义 ACR_HAVE_NVML 宏（当前实现总是动态加载，宏仅用于日志标记）
if(ACR_ENABLE_NVML)
    target_compile_definitions(acr_utilization PUBLIC ACR_ENABLE_NVML=1)
    message(STATUS "ACR memory budget: NVML dynamic loading ENABLED (graceful fallback if nvml.dll missing)")
else()
    target_compile_definitions(acr_utilization PUBLIC ACR_ENABLE_NVML=0)
    message(STATUS "ACR memory budget: NVML DISABLED, GPU VRAM estimated")
endif()

target_link_libraries(acr_utilization PUBLIC
    acr_options
)

target_include_directories(acr_utilization PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

message(STATUS "ACR memory budget: real RAM/VRAM read (GlobalMemoryStatusEx + NVML dynamic)")
