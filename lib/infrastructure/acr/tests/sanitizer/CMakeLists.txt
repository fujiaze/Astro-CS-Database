# lib/infrastructure/acr/tests/sanitizer/CMakeLists.txt
# ----------------------------------------------------------------------------
# WIRING-W34-01 (CHK-PROD-WIRING W4): msvc_asan_main.cpp 此前**全仓无构建路径** ——
# 唯一的构建配方是源文件头注释里手敲的一行 `cl /fsanitize=address ...`（Windows
# 开发机），外加 run_acr_sanitizers.ps1 只运行**预编译** exe（-MsvcAsanExe 默认指向
# F:\...\run\temp\sanitizer_msvc\acr_sanitizer_msvc.exe）。机器可校验的构建路径 = 本文件。
#
# 归属说明（不改 ACR 纪律）: lib/infrastructure/acr/CMakeLists.txt:15/22/33 明令本树
# **禁止被生产构建 add_subdirectory() 引入**（ACR 生产不可达，根 CMakeLists 的
# ASTROCS_ENABLE_ACR 默认 OFF）。故本 target 只在 ACR 独立配置 + MSVC 下存在，
# 不进产品构建图。ASan 运行库在 MinGW 上缺失（源文件头 :3-5），因此守卫是 MSVC。
# ----------------------------------------------------------------------------
if(NOT ACR_BUILD_TESTS)
    return()
endif()

if(NOT MSVC)
    return()   # /fsanitize=address 由 MSVC 提供; MinGW 无 libasan (源文件头 :3-5)
endif()

set(_acr_asan_srcs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler/shared_work_pool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../api/kernel_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler/device_executor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../utilization/system_metrics.cpp
    msvc_asan_main.cpp)

add_executable(acr_sanitizer_msvc ${_acr_asan_srcs})
target_include_directories(acr_sanitizer_msvc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../scheduler
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)
target_compile_options(acr_sanitizer_msvc PRIVATE /fsanitize=address)
target_link_options(acr_sanitizer_msvc PRIVATE /fsanitize=address)

add_test(NAME acr_sanitizer_msvc COMMAND acr_sanitizer_msvc)
add_test(NAME acr_sanitizer_msvc_uaf COMMAND acr_sanitizer_msvc --uaf)
set_tests_properties(acr_sanitizer_msvc_uaf PROPERTIES
    WILL_FAIL TRUE   # --uaf 故意触发 use-after-free; ASan 应终止进程 (非零退出)
)
