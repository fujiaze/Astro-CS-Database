# CMakeLists.txt - astrocs_p1_cosmetic 模块 DLL (P1-COS-IMPL 迁移面)
#
# 对齐先例: lib/calibration/CMakeLists.txt (P1-CAL-IMPL, adf820ac) /
# lib/drizzle/CMakeLists.txt (P1-DRZ-IMPL) / lib/gaia_xpsd_client
# (CAT-GAIA-IMPL, babe752d)。关键事实 (T-1, drizzle/CAL 实证): STATIC 库
# 对象非 PIC (TLS local-exec R_X86_64_TPOFF32), 链接 .so 失败 → 本 SHARED
# 目标从源独立重编译生产源 (POSITION_INDEPENDENT_CODE ON), 不链
# libastrocs_calibration.a。
#
# 导出面净化 (ABI-006): legacy AC_API 符号 (ac_api.cpp 定义 12 符号) 在本
# target 上经 `-DAC_API=` 预定义空 (astro_calibration.h :8 守卫, P1-CAL-IMPL
# 引入, 生产头零改动) 本地化; 再加链接 version-script (Linux) / /DEF
# (Windows) 双保险。唯一导出 astrocs_module_query_v1。
# 科学域零改动: 生产闭包 = calibration 4 TU 全集 (与 CAL-IMPL 同款)。
# 实证: ac_api.cpp 是 calibration 全域 C API 转发层 (内聚不可拆), 除
# cosmetic 的 ac::correct_frame 外还转发 ac::generate_master /
# generate_master_flat / calibrate / calibrate_d (master_generator.cpp +
# calibrator.cpp 定义) —— 2 TU 裁剪闭包不成立, dlopen undefined symbol
# _ZN2ac15generate_master... 实证; 剔除转发函数则需改生产源, 违反
# scientific_change=false。被本地化的 12 符号仍全 local (cosmetic adapter
# 仅消费其中 ac_correct_frame/_f64), 导出面唯一 query 不变。
# hiss_format.h (astro_calibration.h :172) → lib/astro_image_io/include。
# 并行轴: cosmetic op 科学实现居 cosmetic_corrector.cpp; master_generator/
# calibrator 为转发层闭包伴随, 其 OMP 代码路径不被 cosmetic op 触达。
#
# 链接闭包: 4 TU 自包含 (系统头 + OpenMP + hiss_format.h; CAL-IMPL 已实证
# 4 TU 无 aio/common 闭包需求) → 无需重编译 aio/common。
#
# OMP 教训 (P1-CAL-INT 58d20223): 本 DLL 自链 OpenMP (libgomp); 消费方
# 测试 TU 无 OMP 符号引用时 --as-needed 会把测试可执行的 libgomp 从
# DT_NEEDED 丢弃, DLL 内 GOMP 并行区退化为 dlopen 晚装载 → SEGV。
# 处置在测试注册段: tests/unit/cosmetic_adapter_test.cpp TU 链
# astrocs_calibration (天然 OMP 符号引用) 且显式
# target_link_options(LINKER:--no-as-needed) 双保险。
#
# 注: 本子目录由根 CMakeLists.txt (calibration 之后) 注册。

# ── 生产源 (calibration 4 TU 全集; 与根 CMakeLists astrocs_calibration
# :333 逐文件一致; 闭包实证据见上方导出面净化注释) ──
set(COS_PROD_SOURCES
    ${CMAKE_SOURCE_DIR}/lib/calibration/src/calibrator.cpp
    ${CMAKE_SOURCE_DIR}/lib/calibration/src/master_generator.cpp
    ${CMAKE_SOURCE_DIR}/lib/calibration/src/cosmetic_corrector.cpp
    ${CMAKE_SOURCE_DIR}/lib/calibration/src/ac_api.cpp
)
set(COS_ADAPTER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/module_entry.cpp
)

add_library(astrocs_p1_cosmetic SHARED
    ${COS_ADAPTER_SOURCES}
    ${COS_PROD_SOURCES}
)

set_target_properties(astrocs_p1_cosmetic PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME "astrocs_p1_cosmetic"
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    CXX_STANDARD 17
)

# Windows (MSVC): DLL 无 import lib 前缀需求; 输出到 build 根 (host loader
# 与测试按 build 根定位, 对齐 CAL/drizzle $<1:...> 惯例)
if(WIN32)
    set_target_properties(astrocs_p1_cosmetic PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}>)
endif()

# ── include 路径 (生产源口径 = 根 astrocs_calibration :338 同集) ──
target_include_directories(astrocs_p1_cosmetic PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/calibration/include
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ── 编译宏: ABI 导出开关 + legacy 符号本地化 (AC_API= 守卫, 头 :8) ──
target_compile_definitions(astrocs_p1_cosmetic PRIVATE
    ASTROCS_ABI_SHARED=1
    ASTROCS_ABI_EXPORTS=1
    AC_API=
)

find_package(Threads REQUIRED)

# ── 链接依赖 (闭包自包含; 只剩系统库 + OpenMP) ──
set(COS_LINK_LIBS Threads::Threads)
if(UNIX)
    list(APPEND COS_LINK_LIBS m)
endif()
target_link_libraries(astrocs_p1_cosmetic PRIVATE ${COS_LINK_LIBS})

# QA-001 同口径: cosmetic_corrector 含 omp pragma; 根 STATIC 库
# (UNIX AND OpenMP_CXX_FOUND) → -fopenmp 编译 + 链接 (DLL 自链 libgomp)
if(OpenMP_CXX_FOUND)
    target_link_libraries(astrocs_p1_cosmetic PRIVATE OpenMP::OpenMP_CXX)
    if(UNIX)
        target_compile_options(astrocs_p1_cosmetic PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-fopenmp>)
    endif()
endif()

# ── 导出面净化 (唯一导出 astrocs_module_query_v1; ABI-006) ──
if(WIN32)
    # MSVC: /DEF 白名单 (AC_API= 已本地化 legacy 符号, def 双保险)
    set(COS_DEF "${CMAKE_CURRENT_SOURCE_DIR}/src/astrocs_p1_cosmetic.def")
    target_link_options(astrocs_p1_cosmetic PRIVATE "/DEF:${COS_DEF}")
else()
    # Linux/macOS: version-script 白名单 + hidden 可见性预设
    set(COS_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/src/module_exports.map")
    target_link_options(astrocs_p1_cosmetic PRIVATE
        "-Wl,--version-script=${COS_VERSION_SCRIPT}")
    set_target_properties(astrocs_p1_cosmetic PROPERTIES
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
endif()
