# CMakeLists.txt - acsd_p1_drizzle 模块 DLL (P1-DRZ-IMPL 迁移面)
#
# 对齐先例: lib/infrastructure/gaia_xpsd_client/CMakeLists.txt (CAT-GAIA-IMPL, babe752d)。
# 关键事实 (迁移报告 T-1): 现有 STATIC 库 acsd_drizzle 的对象非 PIC
# (TLS local-exec 重定位 R_X86_64_TPOFF32, 链接 .so 失败实证), 因此本模块
# SHARED 目标从源文件独立重编译生产源 (POSITION_INDEPENDENT_CODE ON),
# 不链接 build/libacsd_drizzle.a。
#
# 导出面净化 (ABI-006): 生产源 hp_drizzle_api.cpp 以 `#define HP_DRIZZLE_EXPORTS`
# 硬编码, legacy 六符号在本 target 上经链接 version-script 降 local
# (Linux --version-script; Windows /DEF); 唯一导出 acsd_module_query_v1。
# 科学域零改动: 源列表/编译宏/include/优化与根 acsd_drizzle 逐项一致。
#
# 注: 本子目录由根 CMakeLists.txt :178 (gaia 之后) 注册, 早于 acsd_aio/
# acsd_drizzle 等 STATIC target 定义 → include 路径直接写死 (与根
# acsd_drizzle :368 逐项一致), 链接以 target 名引用 (生成期解析)。

# ── 生产源 (与根 CMakeLists.txt acsd_drizzle 逐文件一致) ──
set(DRZ_PROD_DIR "${CMAKE_SOURCE_DIR}/lib/algorithms/drizzle/healpix_drizzle")
set(DRZ_PROD_SOURCES
    ${DRZ_PROD_DIR}/fits_reader.cpp
    ${DRZ_PROD_DIR}/wcs_sip.cpp
    ${DRZ_PROD_DIR}/poly_clip.cpp
    ${DRZ_PROD_DIR}/spherical_overlap.cpp
    ${DRZ_PROD_DIR}/drizzle_engine.cpp
    ${DRZ_PROD_DIR}/reverse_drizzle.cpp
    ${DRZ_PROD_DIR}/hp_drizzle_api.cpp
    ${DRZ_PROD_DIR}/hp_drizzle_hips_api.cpp
    ${DRZ_PROD_DIR}/astro_sphere_sink.cpp
    ${DRZ_PROD_DIR}/snr_evaluator.cpp
)
set(DRZ_ADAPTER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/module_entry.cpp
)

# 依赖闭包源一并重编译进 SHARED (T-1 事实: acsd_aio/acsd_hips/
# acsd_common/acsd_cfitsio STATIC 对象非 PIC, TLS local-exec 重定位
# R_X86_64_TPOFF32 链 .so 失败实证 → 不可链 .a)。源列表与根 STATIC 库
# 逐文件一致, 语义同源; cfitsio 源变量在根 :66 已 include, 此处可直接引用。
set(DRZ_AIO_SOURCES
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/aio_fits.cpp
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/aio_api.cpp
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/aio_log.cpp
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/aio_compressor.cpp
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/aio_xisf.cpp
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/aio_pipeline.cpp
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/hips/aio_hips_reader.cpp
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/hips/aio_hips_writer.cpp
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/healpix/aio_healpix_io.cpp
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/aio_tile_geometry.cpp
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/hiss_codec.cpp
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/hiss_common.cpp
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/hiss_reader.cpp
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/hiss_stream_writer.cpp
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/hiss_tile_model.cpp
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/hiss_transform.cpp
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/hiss_writer.cpp
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src/aio_upm.cpp
    ${CMAKE_SOURCE_DIR}/lib/algorithms/shared/crypto/sha256.cpp
    ${CMAKE_SOURCE_DIR}/lib/algorithms/shared/healpix/healpix_core.cpp
)
add_library(acsd_p1_drizzle SHARED
    ${DRZ_ADAPTER_SOURCES}
    ${DRZ_PROD_SOURCES}
    ${DRZ_AIO_SOURCES}
)
# cfitsio 源列表是相对根路径 (BLD-001) → 子目录内需显式前缀变换
set(DRZ_CFITSIO_SOURCES "")
foreach(_f ${ACSD_CFITSIO_SOURCES})
    list(APPEND DRZ_CFITSIO_SOURCES "${CMAKE_SOURCE_DIR}/${_f}")
endforeach()
target_sources(acsd_p1_drizzle PRIVATE ${DRZ_CFITSIO_SOURCES})

# ── 第三方源的平台面: 与根 acsd_cfitsio **同一份**实现 ────────────────
# 本目标同样把 vendored cfitsio 整套源再编一遍 (含 zcompress.c/zuncompress.c,
# 二者 #include "zlib.h"), 故必须接上同一份平台垫片与 zlib 依赖包含面 ——
# 判据与清单见 eng/cmake/cfitsio_platform.cmake 抬头。
if(NOT COMMAND acsd_cfitsio_apply_platform_shim)
  include("${CMAKE_SOURCE_DIR}/eng/cmake/cfitsio_platform.cmake")
endif()
acsd_cfitsio_apply_platform_shim(acsd_p1_drizzle)
acsd_cfitsio_apply_third_party_deps(acsd_p1_drizzle)
# 第三方告警隔离 (与基准目标 acsd_cfitsio 同一份实现): 本目标编的是 vendored
# cfitsio 源, 不该把 drvrnet.c C4206 / zuncompress.c C4267 之类灌进项目告警面。
acsd_cfitsio_isolate_warnings(acsd_p1_drizzle)

# WIN-LNK4217: 本 target 是 **SHARED**, 但 module_entry.cpp 与 hp_drizzle_api.cpp /
# hp_drizzle_hips_api.cpp 被编进**同一个 DLL**。两个 hp_drizzle TU 在文件头自带
# `#define HP_DRIZZLE_EXPORTS` ⇒ 拿 dllexport; module_entry.cpp 不定义 ⇒ 落 dllimport
# ⇒ 引用 __imp_hp_drizzle_* 而被「导入」的正是它自己所在的这个 DLL。link.exe 靠本地
# 定义回退放行, 但报 2 条 LNK4217 —— Windows 装载期禁止真自导入, 现在能过纯粹靠这个回退。
# 处置: 定义 HP_DRIZZLE_STATIC (三态中间态, 与根 CMakeLists 对 acsd_drizzle 的同款处置
# 同构)。⚠ hp_drizzle_api.h:17-27 把 EXPORTS 放**最前**先判, 而两个定义侧 TU 在
# include 之前已自行定义 HP_DRIZZLE_EXPORTS ⇒ 它们的 dllexport 导出面**原样保留**,
# 只有 module_entry.cpp 脱离 dllimport ⇒ DLL 对外导出面零变化, 内部调用由
# call *__imp_X 变为 call X (同一地址、同一语义) ⇒ 零行为变化。
target_compile_definitions(acsd_p1_drizzle PRIVATE HP_DRIZZLE_STATIC)

set_target_properties(acsd_p1_drizzle PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME "acsd_p1_drizzle"
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    CXX_STANDARD 17
)

# ── include 路径 (与根 acsd_drizzle :368 一致; 第三方 SYSTEM 隔离) ──
target_include_directories(acsd_p1_drizzle PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${DRZ_PROD_DIR}
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/include
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src
    ${CMAKE_SOURCE_DIR}/lib/algorithms/shared
    ${CMAKE_SOURCE_DIR}/lib/algorithms/shared/healpix
)
target_include_directories(acsd_p1_drizzle SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/third_party/cfitsio
    ${CMAKE_SOURCE_DIR}/lib/third_party          # nlohmann/json.hpp (aio_upm)
)
# QA-001: nanoflann.hpp (snr_evaluator KD-tree) -isystem 隔离 (若 vendored)
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/nanoflann/include")
    target_include_directories(acsd_p1_drizzle SYSTEM PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/nanoflann/include)
endif()

# ── 编译宏: 与根 acsd_drizzle 一致 + ABI 导出开关 ──
target_compile_definitions(acsd_p1_drizzle PRIVATE
    AIO_ENABLE_FITS
    AIO_ENABLE_HEALPIX
    ACSD_ABI_SHARED=1
    ACSD_ABI_EXPORTS=1
    # HP_DRIZZLE_EXPORTS 不得在此定义: hp_drizzle_api.cpp 文件头自带
    # `#define HP_DRIZZLE_EXPORTS`; legacy 符号的导出净化由链接
    # version-script / def 兜底 (见下), 生产源零改动。
)

find_package(Threads REQUIRED)
find_package(OpenMP)
find_package(ZLIB QUIET)

# ── 链接依赖 (闭包源已重编译; 只剩系统库) ──
set(DRZ_LINK_LIBS Threads::Threads)
# 事实(改前): 本处只判 OpenMP_CXX_FOUND 就无条件传 GCC 旗标 `-fopenmp`, 没有平台
#   门控 —— 与根 5 个静态库段的 `if(UNIX AND OpenMP_CXX_FOUND)` 以及本目录同族
#   (lib/algorithms/calibration、lib/algorithms/cosmetic 都嵌套了 if(UNIX)) **两套
#   口径**。Windows/MSVC 全量构建实测: 该旗标被交给 cl.exe (D9002 unknown option),
#   pragma omp 仍被消除。
# 现口径: 门控与旗标收在 eng/cmake/cfitsio_platform.cmake 的
#   acsd_openmp_link_if_unix() 一处 (非 UNIX / 无 OpenMP 时空操作),
#   调用点不再各写一份判断。同族另两处同步收敛到同一函数。
acsd_openmp_link_if_unix(acsd_p1_drizzle)
if(ZLIB_FOUND)
    list(APPEND DRZ_LINK_LIBS ZLIB::ZLIB)
endif()
# cfitsio 需要的数学库 (与根 STATIC 链路面一致)
  list(APPEND DRZ_LINK_LIBS acsd_platform_math)
target_link_libraries(acsd_p1_drizzle PRIVATE ${DRZ_LINK_LIBS})

# ── 导出面净化 (唯一导出 acsd_module_query_v1; ABI-006) ──
if(WIN32)
    # MSVC: /DEF 白名单 (legacy HP_DRIZZLE_API=dllexport 符号被 def 过滤)
    set(DRZ_DEF "${CMAKE_CURRENT_SOURCE_DIR}/src/acsd_p1_drizzle.def")
    target_link_options(acsd_p1_drizzle PRIVATE "/DEF:${DRZ_DEF}")
else()
    # Linux/macOS: version-script 白名单 + hidden 可见性预设
    set(DRZ_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/src/module_exports.map")
    target_link_options(acsd_p1_drizzle PRIVATE
        "-Wl,--version-script=${DRZ_VERSION_SCRIPT}")
    set_target_properties(acsd_p1_drizzle PROPERTIES
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
endif()
