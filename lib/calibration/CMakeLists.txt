# CMakeLists.txt - astrocs_p1_calibration 模块 DLL (P1-CAL-IMPL 迁移面)
#
# 对齐先例: lib/drizzle/CMakeLists.txt (P1-DRZ-IMPL) / lib/gaia_xpsd_client
# (CAT-GAIA-IMPL, babe752d)。关键事实 (T-1, drizzle 实证): STATIC 库对象非
# PIC (TLS local-exec R_X86_64_TPOFF32), 链接 .so 失败 → 本 SHARED 目标从源
# 独立重编译生产源 (POSITION_INDEPENDENT_CODE ON), 不链 libastrocs_calibration.a。
#
# 导出面净化 (ABI-006): legacy 12 个 AC_API 符号在本 target 上经
# `-DAC_API=` 预定义空 (astro_calibration.h :8 守卫, 生产源零改动) 本地化;
# 再加链接 version-script (Linux) / /DEF (Windows) 双保险。
# 唯一导出 astrocs_module_query_v1。科学域零改动: 源列表/编译宏/include/
# 优化与根 astrocs_calibration (:333) 逐项一致。
#
# 链接闭包: calibration 生产 4 TU 自包含 (系统头 + OpenMP; hiss_format.h 对
# 未用 C++ 声明是纯头结构) → 无需重编译 aio/common 闭包 (与 drizzle 差异点)。
#
# 注: 本子目录由根 CMakeLists.txt (drizzle 之后) 注册。

# ── 生产源 (与根 CMakeLists.txt astrocs_calibration :333 逐文件一致) ──
set(CAL_PROD_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/calibrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/master_generator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cosmetic_corrector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ac_api.cpp
)
set(CAL_ADAPTER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/module_entry.cpp
)

add_library(astrocs_p1_calibration SHARED
    ${CAL_ADAPTER_SOURCES}
    ${CAL_PROD_SOURCES}
)

set_target_properties(astrocs_p1_calibration PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME "astrocs_p1_calibration"
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    CXX_STANDARD 17
)

# Windows (MSVC): DLL 无 import lib 前缀需求; 输出到 build 根 (host loader
# 与测试按 build 根定位, 对齐 drizzle $<1:...> 惯例)
if(WIN32)
    set_target_properties(astrocs_p1_calibration PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}>)
endif()

# ── include 路径 (与根 astrocs_calibration :338 一致) ──
target_include_directories(astrocs_p1_calibration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/include
)

# ── 编译宏: ABI 导出开关 + legacy 符号本地化 (AC_API= 守卫, 头 :8) ──
target_compile_definitions(astrocs_p1_calibration PRIVATE
    ASTROCS_ABI_SHARED=1
    ASTROCS_ABI_EXPORTS=1
    AC_API=
)

find_package(Threads REQUIRED)

# ── 链接依赖 (闭包自包含; 只剩系统库 + OpenMP) ──
set(CAL_LINK_LIBS Threads::Threads)
if(UNIX)
    list(APPEND CAL_LINK_LIBS m)
endif()
target_link_libraries(astrocs_p1_calibration PRIVATE ${CAL_LINK_LIBS})

# QA-001 同口径: calibrator/cosmetic_corrector/master_generator 含 omp pragma;
# 根 STATIC 库 (UNIX AND OpenMP_CXX_FOUND) → -fopenmp 编译 + 链接
if(OpenMP_CXX_FOUND)
    target_link_libraries(astrocs_p1_calibration PRIVATE OpenMP::OpenMP_CXX)
    if(UNIX)
        target_compile_options(astrocs_p1_calibration PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-fopenmp>)
    endif()
endif()

# ── 导出面净化 (唯一导出 astrocs_module_query_v1; ABI-006) ──
if(WIN32)
    # MSVC: /DEF 白名单 (AC_API= 已本地化 legacy 符号, def 双保险)
    set(CAL_DEF "${CMAKE_CURRENT_SOURCE_DIR}/src/astrocs_p1_calibration.def")
    target_link_options(astrocs_p1_calibration PRIVATE "/DEF:${CAL_DEF}")
else()
    # Linux/macOS: version-script 白名单 + hidden 可见性预设
    set(CAL_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/src/module_exports.map")
    target_link_options(astrocs_p1_calibration PRIVATE
        "-Wl,--version-script=${CAL_VERSION_SCRIPT}")
    set_target_properties(astrocs_p1_calibration PROPERTIES
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
endif()
