# lib/acr/utilization/CMakeLists.txt — Phase G 95% 软占用控制器
#
# 真实利用率读取：
#   - CPU: GetSystemTimes（Windows API）
#   - RAM: GlobalMemoryStatusEx（Windows API）
#   - GPU/VRAM: NVML 动态加载（可选，ACR_ENABLE_NVML=ON 时尝试；OFF 时仅队列预算估算）
#
# 纠正清单 §7：不接受人工输入 actual_ratio，必须读取实际值或明确标记估算。
option(ACR_ENABLE_NVML "Enable NVML dynamic loading for GPU utilization (graceful fallback if nvml.dll missing)" ON)

add_library(acr_utilization STATIC
    system_metrics.cpp
    cpu_controller.cpp
    gpu_controller.cpp
    memory_budget.cpp
    io_budget.cpp
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
    message(STATUS "ACR utilization: NVML dynamic loading ENABLED (graceful fallback if nvml.dll missing)")
else()
    target_compile_definitions(acr_utilization PUBLIC ACR_ENABLE_NVML=0)
    message(STATUS "ACR utilization: NVML DISABLED, GPU utilization estimated from queue budget only")
endif()

target_link_libraries(acr_utilization PUBLIC
    acr_options
)

target_include_directories(acr_utilization PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

message(STATUS "ACR utilization: real CPU/RAM read (GetSystemTimes/GlobalMemoryStatusEx) + NVML dynamic + queue-budget fallback")
