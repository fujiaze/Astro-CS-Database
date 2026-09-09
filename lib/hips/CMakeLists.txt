# CMakeLists.txt - astrocs_p1_hips_writer 模块 DLL (P1-HIPS-IMPL 迁移面)
#
# 对齐先例: lib/drizzle/CMakeLists.txt (P1-DRZ-IMPL, 2c065ace)、
#           lib/gaia_xpsd_client/CMakeLists.txt (CAT-GAIA-IMPL, babe752d)。
#
# 关键事实 (迁移报告 T-1, DRZ 同款): 既有 STATIC 库对象非 PIC (TLS
# local-exec 重定位 R_X86_64_TPOFF32, 链接 .so 失败实证), 因此本 SHARED
# 目标从源文件独立重编译生产源 (POSITION_INDEPENDENT_CODE ON)。
#
# 依赖闭包 (P1-COS-IMPL 教训: 先实证再定 TU 集; 2026-09-07 只读实证):
#   aio_hips_writer.cpp 编译闭包 = {aio_hips.h, healpix/healpix_core.h,
#   fitsio.h}; 项目内符号依赖仅 astrocs::healpix::nested_local_to_fits_index
#   (writer:487,638,864,901) 与 ang2pix_nest (writer:949)。零依赖剔除:
#   aio_fits/aio_log/aio_api/aio_healpix_io/hiss_*/aio_hips_reader/aio_upm
#   全部不进本 DLL (grep 0 命中; g++ -fsyntax-only 实证通过)。
#   最小闭包 = {aio_hips_writer.cpp, healpix_core.cpp} + cfitsio 源
#   (ASTROCS_CFITSIO_SOURCES, 根 :66 include; vendored CFITSIO 4.6.4)。
#
# 导出面净化 (ABI-006): legacy aio_hips_* 九符号 (aio_hips.h:23-27 宏
# dllexport/visibility default 携带) 经链接 version-script (Linux/macOS)
# / /DEF 白名单 (MSVC) 降 local; 唯一导出 astrocs_module_query_v1。
# 科学域零改动 (scientific_change=false): 生产 TU 原样编译, 无本地副本。

# ── 生产源 (依赖闭包最小集, 全部指向唯一权威源, 无本地副本) ──
set(HIPS_PROD_DIR "${CMAKE_SOURCE_DIR}/lib/astro_image_io")
set(HIPS_ADAPTER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/module_entry.cpp
    # AIO-002: 原子发布原语唯一实现 (publish.h v1 合同; staging→fsync→
    # 原子 promote→discard RAII; DISP-HIPS-001/004 模块事务面收口)
    ${CMAKE_CURRENT_SOURCE_DIR}/src/aio_publish.cpp
)
set(HIPS_PROD_SOURCES
    ${HIPS_PROD_DIR}/src/hips/aio_hips_writer.cpp
    ${CMAKE_SOURCE_DIR}/lib/common/healpix/healpix_core.cpp
)
add_library(astrocs_p1_hips_writer SHARED
    ${HIPS_ADAPTER_SOURCES}
    ${HIPS_PROD_SOURCES}
)
# cfitsio 源列表是相对根路径 (BLD-001) → 子目录内需显式前缀变换
set(HIPS_CFITSIO_SOURCES "")
foreach(_f ${ASTROCS_CFITSIO_SOURCES})
    list(APPEND HIPS_CFITSIO_SOURCES "${CMAKE_SOURCE_DIR}/${_f}")
endforeach()
target_sources(astrocs_p1_hips_writer PRIVATE ${HIPS_CFITSIO_SOURCES})

set_target_properties(astrocs_p1_hips_writer PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME "astrocs_p1_hips_writer"
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    CXX_STANDARD 17
)

# ── include 路径 (writer TU 编译闭包实证最小集) ──
target_include_directories(astrocs_p1_hips_writer PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${HIPS_PROD_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/common
    ${CMAKE_SOURCE_DIR}/lib/common/healpix
)
target_include_directories(astrocs_p1_hips_writer SYSTEM PRIVATE
    ${HIPS_PROD_DIR}/third_party/cfitsio
)

# ── 编译宏: ABI 导出开关 ──
# 注: writer TU 不需要 AIO_ENABLE_* (实证 0 引用); AIO_HIPS_EXPORT 由
# aio_hips.h:23-27 依 _WIN32 自选, legacy 九符号导出净化由链接面兜底。
target_compile_definitions(astrocs_p1_hips_writer PRIVATE
    ASTROCS_ABI_SHARED=1
    ASTROCS_ABI_EXPORTS=1
)

find_package(Threads REQUIRED)
find_package(ZLIB QUIET)

# ── 链接依赖 (闭包源已重编译; 只剩系统库) ──
set(HIPS_LINK_LIBS Threads::Threads)
if(ZLIB_FOUND)
    # zlib 仅被 cfitsio 使用 (与根 astrocs_cfitsio 链路面一致)
    list(APPEND HIPS_LINK_LIBS ZLIB::ZLIB)
endif()
if(UNIX)
    list(APPEND HIPS_LINK_LIBS m)
endif()
target_link_libraries(astrocs_p1_hips_writer PRIVATE ${HIPS_LINK_LIBS})

# ── 导出面净化 (唯一导出 astrocs_module_query_v1; ABI-006) ──
if(WIN32)
    # MSVC: /DEF 白名单 (AIO_HIPS_EXPORT=dllexport 九符号被 def 过滤)
    set(HIPS_DEF "${CMAKE_CURRENT_SOURCE_DIR}/src/astrocs_p1_hips_writer.def")
    target_link_options(astrocs_p1_hips_writer PRIVATE "/DEF:${HIPS_DEF}")
else()
    # Linux/macOS: version-script 白名单 + hidden 可见性预设
    set(HIPS_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/src/module_exports.map")
    target_link_options(astrocs_p1_hips_writer PRIVATE
        "-Wl,--version-script=${HIPS_VERSION_SCRIPT}")
    set_target_properties(astrocs_p1_hips_writer PROPERTIES
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
endif()
