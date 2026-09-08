# CMakeLists.txt - astrocs_p1_drizzle 模块 DLL (P1-DRZ-IMPL 迁移面)
#
# 对齐先例: lib/gaia_xpsd_client/CMakeLists.txt (CAT-GAIA-IMPL, babe752d)。
# 关键事实 (迁移报告 T-1): 现有 STATIC 库 astrocs_drizzle 的对象非 PIC
# (TLS local-exec 重定位 R_X86_64_TPOFF32, 链接 .so 失败实证), 因此本模块
# SHARED 目标从源文件独立重编译生产源 (POSITION_INDEPENDENT_CODE ON),
# 不链接 build/libastrocs_drizzle.a。
#
# 导出面净化 (ABI-006): 生产源 hp_drizzle_api.cpp 以 `#define HP_DRIZZLE_EXPORTS`
# 硬编码, legacy 六符号在本 target 上经链接 version-script 降 local
# (Linux --version-script; Windows /DEF); 唯一导出 astrocs_module_query_v1。
# 科学域零改动: 源列表/编译宏/include/优化与根 astrocs_drizzle 逐项一致。
#
# 注: 本子目录由根 CMakeLists.txt :178 (gaia 之后) 注册, 早于 astrocs_aio/
# astrocs_drizzle 等 STATIC target 定义 → include 路径直接写死 (与根
# astrocs_drizzle :368 逐项一致), 链接以 target 名引用 (生成期解析)。

# ── 生产源 (与根 CMakeLists.txt astrocs_drizzle 逐文件一致) ──
set(DRZ_PROD_DIR "${CMAKE_SOURCE_DIR}/lib/healpix_db/healpix_drizzle")
set(DRZ_PROD_SOURCES
    ${DRZ_PROD_DIR}/fits_reader.cpp
    ${DRZ_PROD_DIR}/wcs_sip.cpp
    ${DRZ_PROD_DIR}/poly_clip.cpp
    ${DRZ_PROD_DIR}/spherical_overlap.cpp
    ${DRZ_PROD_DIR}/drizzle_engine.cpp
    ${DRZ_PROD_DIR}/reverse_drizzle.cpp
    ${DRZ_PROD_DIR}/hp_drizzle_api.cpp
    ${DRZ_PROD_DIR}/astro_sphere_sink.cpp
    ${DRZ_PROD_DIR}/snr_evaluator.cpp
)
set(DRZ_ADAPTER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/module_entry.cpp
)

# 依赖闭包源一并重编译进 SHARED (T-1 事实: astrocs_aio/astrocs_hips/
# astrocs_common/astrocs_cfitsio STATIC 对象非 PIC, TLS local-exec 重定位
# R_X86_64_TPOFF32 链 .so 失败实证 → 不可链 .a)。源列表与根 STATIC 库
# 逐文件一致, 语义同源; cfitsio 源变量在根 :66 已 include, 此处可直接引用。
set(DRZ_AIO_SOURCES
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/src/aio_fits.cpp
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/src/aio_api.cpp
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/src/aio_log.cpp
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/src/aio_compressor.cpp
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/src/aio_xisf.cpp
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/src/aio_pipeline.cpp
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/src/hips/aio_hips_reader.cpp
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/src/hips/aio_hips_writer.cpp
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/src/healpix/aio_healpix_io.cpp
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/src/hiss_codec.cpp
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/src/hiss_common.cpp
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/src/hiss_reader.cpp
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/src/hiss_stream_writer.cpp
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/src/hiss_tile_model.cpp
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/src/hiss_transform.cpp
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/src/hiss_writer.cpp
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/src/aio_upm.cpp
    ${CMAKE_SOURCE_DIR}/lib/common/crypto/sha256.cpp
    ${CMAKE_SOURCE_DIR}/lib/common/healpix/healpix_core.cpp
)
add_library(astrocs_p1_drizzle SHARED
    ${DRZ_ADAPTER_SOURCES}
    ${DRZ_PROD_SOURCES}
    ${DRZ_AIO_SOURCES}
)
# cfitsio 源列表是相对根路径 (BLD-001) → 子目录内需显式前缀变换
set(DRZ_CFITSIO_SOURCES "")
foreach(_f ${ASTROCS_CFITSIO_SOURCES})
    list(APPEND DRZ_CFITSIO_SOURCES "${CMAKE_SOURCE_DIR}/${_f}")
endforeach()
target_sources(astrocs_p1_drizzle PRIVATE ${DRZ_CFITSIO_SOURCES})

set_target_properties(astrocs_p1_drizzle PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME "astrocs_p1_drizzle"
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    CXX_STANDARD 17
)

# ── include 路径 (与根 astrocs_drizzle :368 一致; 第三方 SYSTEM 隔离) ──
target_include_directories(astrocs_p1_drizzle PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${DRZ_PROD_DIR}
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/include
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/src
    ${CMAKE_SOURCE_DIR}/lib/common
    ${CMAKE_SOURCE_DIR}/lib/common/healpix
)
target_include_directories(astrocs_p1_drizzle SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/astro_image_io/third_party/cfitsio
    ${CMAKE_SOURCE_DIR}/third_party          # nlohmann/json.hpp (aio_upm)
)
# QA-001: nanoflann.hpp (snr_evaluator KD-tree) -isystem 隔离 (若 vendored)
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/nanoflann/include")
    target_include_directories(astrocs_p1_drizzle SYSTEM PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/nanoflann/include)
endif()

# ── 编译宏: 与根 astrocs_drizzle 一致 + ABI 导出开关 ──
target_compile_definitions(astrocs_p1_drizzle PRIVATE
    AIO_ENABLE_FITS
    AIO_ENABLE_HEALPIX
    ASTROCS_ABI_SHARED=1
    ASTROCS_ABI_EXPORTS=1
    # HP_DRIZZLE_EXPORTS 不得在此定义: hp_drizzle_api.cpp 文件头自带
    # `#define HP_DRIZZLE_EXPORTS`; legacy 符号的导出净化由链接
    # version-script / def 兜底 (见下), 生产源零改动。
)

find_package(Threads REQUIRED)
find_package(OpenMP)
find_package(ZLIB QUIET)

# ── 链接依赖 (闭包源已重编译; 只剩系统库) ──
set(DRZ_LINK_LIBS Threads::Threads)
if(OpenMP_CXX_FOUND)
    # QA-001 同口径: omp pragma 编译消除 unknown-pragmas; 并行归并按线程序固定
    list(APPEND DRZ_LINK_LIBS OpenMP::OpenMP_CXX)
    target_compile_options(astrocs_p1_drizzle PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fopenmp>)
endif()
if(ZLIB_FOUND)
    list(APPEND DRZ_LINK_LIBS ZLIB::ZLIB)
endif()
# cfitsio 需要的数学库 (与根 STATIC 链路面一致)
if(UNIX)
    list(APPEND DRZ_LINK_LIBS m)
endif()
target_link_libraries(astrocs_p1_drizzle PRIVATE ${DRZ_LINK_LIBS})

# ── 导出面净化 (唯一导出 astrocs_module_query_v1; ABI-006) ──
if(WIN32)
    # MSVC: /DEF 白名单 (legacy HP_DRIZZLE_API=dllexport 符号被 def 过滤)
    set(DRZ_DEF "${CMAKE_CURRENT_SOURCE_DIR}/src/astrocs_p1_drizzle.def")
    target_link_options(astrocs_p1_drizzle PRIVATE "/DEF:${DRZ_DEF}")
else()
    # Linux/macOS: version-script 白名单 + hidden 可见性预设
    set(DRZ_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/src/module_exports.map")
    target_link_options(astrocs_p1_drizzle PRIVATE
        "-Wl,--version-script=${DRZ_VERSION_SCRIPT}")
    set_target_properties(astrocs_p1_drizzle PROPERTIES
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
endif()
