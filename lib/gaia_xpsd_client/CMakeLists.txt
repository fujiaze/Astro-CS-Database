# lib/gaia_xpsd_client — astrocs_catalog_gaia 模块 DLL (CAT-GAIA-IMPL)
#
# 矩阵行: CAT-GAIA | SA-P1-W16 | service | astrocs.catalog.gaia |
#         astrocs_catalog_gaia.dll | "lib/gaia_xpsd_client" | ABI-005;DATA-004;IO-003
#
# 导出面合同 (12_DLL_ABI_AND_LOADER_STANDARD §1; ABI-006 全查):
#   唯一导出 astrocs_module_query_v1; legacy GAIA_EXPORT 符号 (gaia_client_* 等
#   12 个) 经 target 编译定义 GAIA_EXPORT=(空) 本地化 — 头内宏覆盖见
#   src/gaia_client.h (CAT-GAIA-IMPL 注释)。Linux: -fvisibility=hidden 兜底。
# 模块 ABI 定义: ASTROCS_ABI_SHARED=1 + ASTROCS_ABI_EXPORTS=1 (status_codes.h §6)。
# 依赖: zlib (XPSD 块解压), Threads, OpenMP (每文件并行, 保持生产实现不动)。
# 注册: 根 CMakeLists.txt add_subdirectory(lib/gaia_xpsd_client)。
cmake_minimum_required(VERSION 3.16)

add_library(astrocs_catalog_gaia SHARED
    src/gaia_client.c
    src/module_entry.c
)

target_include_directories(astrocs_catalog_gaia PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include          # astrocs/abi/* (模块 C ABI v1 冻结头)
)

target_compile_definitions(astrocs_catalog_gaia PRIVATE
    ASTROCS_ABI_SHARED=1
    ASTROCS_ABI_EXPORTS=1
    # 本地化 legacy 导出: GAIA_EXPORT=(空) → 符号不进 DLL 导出面 (ABI-006)
    GAIA_EXPORT=
)

if(WIN32)
    set_target_properties(astrocs_catalog_gaia PROPERTIES
        PREFIX ""
        OUTPUT_NAME "astrocs_catalog_gaia"     # → astrocs_catalog_gaia.dll
        RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}>)
    target_compile_options(astrocs_catalog_gaia PRIVATE
        /Zc:__cplusplus /W3)
else()
    set_target_properties(astrocs_catalog_gaia PROPERTIES
        PREFIX ""
        OUTPUT_NAME "astrocs_catalog_gaia"     # → astrocs_catalog_gaia.so
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
    target_compile_options(astrocs_catalog_gaia PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-fvisibility=hidden>)
    # 纯 C11 adapter: 无异常/RTTI 概念; legacy 实现亦为纯 C
    target_compile_features(astrocs_catalog_gaia PRIVATE c_std_11)
endif()

find_package(Threads REQUIRED)
target_link_libraries(astrocs_catalog_gaia PRIVATE Threads::Threads)

# zlib (legacy 依赖; 已在工程内使用 find_package/ZLIB 变量约定)
if(NOT TARGET ZLIB::ZLIB)
    find_package(ZLIB REQUIRED)
endif()
target_link_libraries(astrocs_catalog_gaia PRIVATE ZLIB::ZLIB)

# OpenMP: legacy 生产实现使用 (4 个文件级并行循环; 迁移不改并行结构)。
# 无 OpenMP 时退化为串行 (gaia_client.c 对 _OPENMP 有条件编译)。
find_package(OpenMP QUIET)
if(OpenMP_C_FOUND)
    target_link_libraries(astrocs_catalog_gaia PRIVATE OpenMP::OpenMP_C)
else()
    message(STATUS "astrocs_catalog_gaia: OpenMP not found -> serial fallback")
endif()
