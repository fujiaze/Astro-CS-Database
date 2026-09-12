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
# F-CI-002-05 (MOD-001 域内, owner 授权修复): Windows hosted runner 的 vcpkg
# zlib 不在 find_package(ZLIB) 默认搜索路径, WIN-BUILD-RELEASE configure 必败
# (e9b82ea4 起 win-build-summary 实证, 此前被监控门 exit10 掩盖)。
# CI-WIN-001 / FD-R1-011 (裁决 R-12): e6254d4d 的修复不完整 —— 本节当时只消费
# ACS_ZLIB_ROOT (include 命中, 故报 "found version 1.3.2"), 未消费 runner
# "Provide zlib (vcpkg)" 步注入的 ACS_ZLIB_LIB; vcpkg x64-windows-static-md 的
# 静态产物名为 zs.lib, 不在 FindZLIB 的 NAMES(z zlib zdll zlib1 zlibstatic
# zlibwapi zlibvc zlibstat) 内 ⇒ ZLIB_LIBRARY 缺失 ⇒ configure 必败。根
# astrocs_cfitsio 段两个 env 都消费, 本节只消费一个 ⇒ 两段口径分叉。
#
# 现口径 (与根段 HOSTFIX-23④ 一致; CMP0074 <PackageName>_ROOT 语义,
# cmake_minimum_required 3.16 ⇒ NEW):
#   ① ACS_ZLIB_ROOT (有效目录) ⇒ ZLIB_ROOT, FindZLIB 以 <root>/include 与
#      <root>/lib 为首选搜索前缀; root 未设/无效 ⇒ 不注入, 退回系统搜索
#      (Linux env 为空 ⇒ 行为零变化, 基线不变);
#   ② ACS_ZLIB_LIB (runner 注入的确切库文件名, 如 vcpkg zs.lib / fatduck
#      libz.lib) ⇒ 直接落 ZLIB_LIBRARY (HOSTFIX-23④ "env 层确定性优先"),
#      不依赖 FindZLIB 的 NAMES 猜测, 亦避免 include 命中而库落到别的 zlib。
# 不改为可选依赖、不摘除 REQUIRED (裁决 R-12/R-13): 找不到 zlib 一律
# configure 期 fail-fast。本节不按 MSVC 门控 (与根段的差异仅在此: gaia 走
# imported target, 不拼 MSVC 专有 .lib 路径), 使该消费口径可在 Linux 侧同构
# 复验 —— 见 lib/gaia_xpsd_client/tests/test_gaia_zlib_configure_contract.py。
#
# ⚠ 条件写法硬约束: 二② 的环境变量判空**必须**用字符串比较
# (NOT "$ENV{ACS_ZLIB_LIB}" STREQUAL ""), 不得写成 if(ENV{ACS_ZLIB_LIB}) ——
# CMake 的 if(ENV{<name>}) 会把环境变量的**值再当变量名解引用**, 值 "zs.lib"
# 未定义为 CMake 变量 ⇒ 判 FALSE, 该分支恒为死分支。CI-WIN-001 本地实证:
# 根 astrocs_cfitsio 段正是 if(ENV{ACS_ZLIB_LIB}) 写法 (该段能过仅因其后
# find_library 的 NAMES 里含 zs, 属侥幸命中, 已登记 finding 交前台裁决)。
if(NOT TARGET ZLIB::ZLIB)
    set(ACS_ZLIB_ROOT "$ENV{ACS_ZLIB_ROOT}" CACHE PATH
        "zlib install root (include/zlib.h + lib); MSVC hosted runner 经 vcpkg 注入 env")
    if(ACS_ZLIB_ROOT AND IS_DIRECTORY "${ACS_ZLIB_ROOT}")
        set(ZLIB_ROOT "${ACS_ZLIB_ROOT}")
        if(NOT "$ENV{ACS_ZLIB_LIB}" STREQUAL ""
           AND EXISTS "${ACS_ZLIB_ROOT}/lib/$ENV{ACS_ZLIB_LIB}")
            set(ZLIB_LIBRARY "${ACS_ZLIB_ROOT}/lib/$ENV{ACS_ZLIB_LIB}" CACHE FILEPATH
                "ACS_ZLIB_LIB 注入的确切 zlib 链接库 (env 层确定性优先; 与根段同口径)" FORCE)
        endif()
    endif()
    find_package(ZLIB REQUIRED)
endif()
target_link_libraries(astrocs_catalog_gaia PRIVATE ZLIB::ZLIB)

# MOD-001 依赖闭包收口: gaia_client.c 使用 GNU libm 扩展 sincos (RTLD_NOW 下
# 未定义符号), 本 SHARED 的 DT_NEEDED 曾缺 libm → 独立 loader 消费方进程
# (registry host / abi003 探针, 不链 libm) dlopen 必败 OS_LOAD, 仅 astrocs 主
# exe (链 m) 侥幸可载 — 违反 §8.4 "独立加载" 依赖闭包完备要求。显式链 m 使
# 模块 DLL 自包含 (libm.so.6 入 DT_NEEDED), 由 tests/abi/mod001_install_load_check.py
# 安装树逐 unit 加载验证闭环。
if(UNIX AND NOT APPLE)
    target_link_libraries(astrocs_catalog_gaia PRIVATE m)
endif()

# OpenMP: legacy 生产实现使用 (4 个文件级并行循环; 迁移不改并行结构)。
# 无 OpenMP 时退化为串行 (gaia_client.c 对 _OPENMP 有条件编译)。
find_package(OpenMP QUIET)
if(OpenMP_C_FOUND)
    target_link_libraries(astrocs_catalog_gaia PRIVATE OpenMP::OpenMP_C)
else()
    message(STATUS "astrocs_catalog_gaia: OpenMP not found -> serial fallback")
endif()
