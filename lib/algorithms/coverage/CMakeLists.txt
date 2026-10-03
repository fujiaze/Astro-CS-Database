# lib/algorithms/coverage/CMakeLists.txt — COMPATIBILITY 声明 (BLD-002, 非产品事实源)
#
# 唯一产品事实源 = 根 CMakeLists.txt (唯一 project / 唯一 add_executable(acsd))。
# 本文件仅保留 lib/algorithms/coverage 模块的 phase2 compat 静态库与按需工具
# (compatibility target):
#   - phase2 STATIC 库的正式产品源集由根 CMakeLists.txt 的 acsd_phase2 声明
#     (lib/algorithms/coverage/src/*.cpp 显式列表, 无 GLOB); 本文件不再作为生产 target 事实源;
#   - acsd-stage2 / calibrated_pair_diag / rejection_cli 为按需工具
#     (EXCLUDE_FROM_ALL, 非正式 ALL target), 不进入默认构建与任何 install;
#   - 禁止正式 install (BLD-002): 安装/发布规则只允许存在于根 CMakeLists.txt。
cmake_minimum_required(VERSION 3.24)
project(astro_phase2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT MSVC)
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()
if(NOT CMAKE_GENERATOR AND CMAKE_HOST_WIN32)
    set(CMAKE_GENERATOR "MinGW Makefiles" CACHE STRING "" FORCE)
endif()

# Hotfix: OpenMP 硬禁用（Fatduck: 即使存在 libgomp 也串行；避免 0xC0000005）
# 如需实验并行，改此 OFF→ON 后重建（默认 OFF 才能过 32→1+4/4 门禁）
option(P2_ENABLE_OPENMP "Enable OpenMP parallel sampler (default OFF, hotfix revert)" OFF)
if(P2_ENABLE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        message(STATUS "Phase2: OpenMP enabled (P2_ENABLE_OPENMP=ON)")
    else()
        message(STATUS "Phase2: P2_ENABLE_OPENMP=ON but OpenMP NOT found, fallback single thread")
    endif()
else()
    # 关键：显式禁掉上游/缓存中残留的 OpenMP_CXX_FOUND，避免误链 libgomp
    set(OpenMP_CXX_FOUND FALSE)
    message(STATUS "Phase2: OpenMP disabled (hotfix default, serial sampler) - libgomp NOT linked")
endif()

add_library(phase2 STATIC
    src/upm.cpp
    src/sky_plane.cpp
    src/identifiability.cpp
    src/stage2_common.cpp
    src/accelerator_fallback.cpp
    src/rejection.cpp
    src/coverage.cpp
    src/sampler.cpp
    src/block.cpp
    src/integrate.cpp
    src/async_io.cpp
    ../shared/healpix/healpix_core.cpp
    ../shared/crypto/sha256.cpp
)
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../infrastructure/aio/include)
# CLEAN-403: sky_plane.cpp 的文件读写经 aio 机制原语 (aio_atomic_file.h /
# aio_file_io.h, header-only) ⇒ compatibility 目标同样需要 aio/src include 面。
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../infrastructure/aio/src)
# 性能探针头面 (ACSD_PROBES 默认 OFF; 未定义时 acsd/probe.h 的宏
# 全部为空语句)。独立 configure 不走根图 acsd_probes target, 故此处显式登记。
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../infrastructure/observability/probes/include)
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../algorithms/shared)
# SCI-FIX-AIO (V11-N-01) 连带修: compatibility 目标此前缺 third_party 头路径 ⇒
# nlohmann/json 落到系统副本 (json_abi_v3_11_3), 而根图目标 (acsd_hips/aio)
# 带 third_party (v3_12_0); 两者混链时 p2_stage2_parse_config 之类的符号
# mangling 不一致 (旧 astro_image_io.dll 恰好掩盖了该不一致)。统一到仓内
# vendored json。
target_include_directories(phase2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/third_party)
if(P2_ENABLE_OPENMP AND OpenMP_CXX_FOUND)
    target_link_libraries(phase2 PUBLIC OpenMP::OpenMP_CXX)
    # CON-004: 把 P2_ENABLE_OPENMP 作为编译宏定义（原为装饰性 option，源内 #if 恒假）。
    # 仅 ON 且找到 OpenMP 时定义 => 源内 #if defined(P2_ENABLE_OPENMP) 才真正激活并行路径。
    target_compile_definitions(phase2 PRIVATE P2_ENABLE_OPENMP=1)
endif()

# SCI-FIX-AIO (V11-N-01): 源树内 **未跟踪** 的
# ../../infrastructure/aio/astro_image_io.dll 是 ARCH-001 迁移前的历史构建
# 产物 (ELF .so 误名 .dll; 旧 AstroSphereTileView 布局, 无版本化 ABI 头)。
# 它与本仓当前 aio_hips.h 不一致时字段整体错位 —— 实测 writer 读到的
# width=0 ⇒ rc=-2 "view 与产品集不匹配", 或更糟: 静默错位写数据。
# 根图构建一律走 acsd_hips/acsd_aio 静态目标; 仅模块独立 configure
# (无根目标) 才回退到该历史产物。下面的 compat 工具走同一判据。
if(TARGET acsd_hips AND TARGET acsd_aio)
    set(P2_AIO_LINK acsd_hips acsd_aio)
else()
    set(P2_AIO_LINK ${CMAKE_CURRENT_SOURCE_DIR}/../../infrastructure/aio/astro_image_io.dll)
endif()

# BLD-002: 以下工具降级为 compatibility (EXCLUDE_FROM_ALL) —— 非正式 ALL target,
# 仅按需显式构建 (cmake --build --target ...); 不进入任何 install/发布树。
add_executable(acsd-stage2 tools/stage2.cpp)
target_link_libraries(acsd-stage2 PRIVATE phase2
    ${P2_AIO_LINK})
if(P2_ENABLE_OPENMP AND OpenMP_CXX_FOUND)
    # CON-006: stage2 工具本身有逐 pixel integration 并行代码，也需要编译宏。
    target_compile_definitions(acsd-stage2 PRIVATE P2_ENABLE_OPENMP=1)
endif()
set_target_properties(acsd-stage2 PROPERTIES EXCLUDE_FROM_ALL TRUE)
# acsd-stage2 是 EXCLUDE_FROM_ALL 的 compat 工具，只被显式 `--target acsd-stage2`
# 拉建；下面的守卫保证「若将来有人把它登记成别的目标的依赖」时不会在守卫外
# 无条件 add_dependencies（落在守卫外时 CMake 直接报
# "Cannot add target-level dependencies to non-existent target"）。
if(TARGET phase2_ivar_wiring)
  add_dependencies(phase2_ivar_wiring acsd-stage2)
endif()

add_executable(calibrated_pair_diag tools/calibrated_pair_diag.cpp)
target_link_libraries(calibrated_pair_diag PRIVATE phase2
    ${P2_AIO_LINK})
set_target_properties(calibrated_pair_diag PROPERTIES EXCLUDE_FROM_ALL TRUE)

add_executable(rejection_cli tools/rejection_cli.cpp)
target_link_libraries(rejection_cli PRIVATE phase2)
set_target_properties(rejection_cli PROPERTIES EXCLUDE_FROM_ALL TRUE)
