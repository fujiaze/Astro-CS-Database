# cli/CMakeLists.txt — COMPATIBILITY 声明 (BLD-002, 非产品事实源)
#
# 唯一产品事实源 = 根 CMakeLists.txt (唯一 project / 唯一 add_executable(astrocs))。
# 本文件仅保留给遗留 tests/cli 独立构建与 CLI 调试 (compatibility target):
#   - 不声明 install 规则 (禁止正式 install, BLD-002);
#   - 不作为任何发布/安装构建入口;
#   - 产品二进制一律由根 CMakeLists.txt 产出。
# 历史: 本文件曾为第二产品事实源 (V6 前 cli 独立工程 + install), BLD-002 移除其正式性。
cmake_minimum_required(VERSION 3.16)
project(astrocs_cli LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 第三方/整体告警抑制: MSVC 用 /w, GCC/Clang 用 -w
if(MSVC)
  set(ACS_WARN_SUPPRESS "/w")
else()
  set(ACS_WARN_SUPPRESS "-w")
endif()
# V5: 禁全局 -march=/arch:AVX* (ARCH-003 §2); baseline 可加载性优先
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# 版本单源: 根 VERSION 文件为唯一权威 (tools/gen_version.py 同式读取)
# VER-001: BASE_VERSION 从 VERSION 文件读取, 不再内嵌字面量, 防止版本漂移
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/../VERSION BASE_VERSION)
string(STRIP "${BASE_VERSION}" BASE_VERSION)
# VERSION + git commit → X.Y.Z-alpha.N+g<hash12>[.dirty] (tools/gen_version.py 同式)
execute_process(
  COMMAND git rev-parse HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
  OUTPUT_VARIABLE GIT_COMMIT_FULL OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT GIT_COMMIT_FULL)
  set(GIT_COMMIT_FULL "0000000000000000000000000000000000000000")
endif()
execute_process(
  COMMAND git rev-parse --short=12 HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
  OUTPUT_VARIABLE GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT GIT_COMMIT)
  set(GIT_COMMIT "unknown")
endif()
execute_process(
  COMMAND git status --porcelain
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
  OUTPUT_VARIABLE GIT_DIRTY OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(GIT_DIRTY)
  set(GIT_DIRTY_SUFFIX ".dirty")
else()
  set(GIT_DIRTY_SUFFIX "")
endif()
set(ASTROCS_VERSION_STRING "${BASE_VERSION}+g${GIT_COMMIT}${GIT_DIRTY_SUFFIX}")

set(GENIN "version_generated.h.in")
configure_file(version_generated.h.in version_generated.h @ONLY)
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/version_generated.h
  "#define ASTROCS_COMMIT_SHA \"${GIT_COMMIT_FULL}\"\n")

set(AIO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../lib/astro_image_io)
# QA-002: cfitsio 60 源显式清单 (禁 GLOB)
set(CFITSIO_SRCS
  ${AIO_DIR}/third_party/cfitsio/buffers.c
  ${AIO_DIR}/third_party/cfitsio/cfileio.c
  ${AIO_DIR}/third_party/cfitsio/checksum.c
  ${AIO_DIR}/third_party/cfitsio/drvrfile.c
  ${AIO_DIR}/third_party/cfitsio/drvrmem.c
  ${AIO_DIR}/third_party/cfitsio/drvrnet.c
  ${AIO_DIR}/third_party/cfitsio/editcol.c
  ${AIO_DIR}/third_party/cfitsio/edithdu.c
  ${AIO_DIR}/third_party/cfitsio/eval_f.c
  ${AIO_DIR}/third_party/cfitsio/eval_l.c
  ${AIO_DIR}/third_party/cfitsio/eval_y.c
  ${AIO_DIR}/third_party/cfitsio/fits_hcompress.c
  ${AIO_DIR}/third_party/cfitsio/fits_hdecompress.c
  ${AIO_DIR}/third_party/cfitsio/fitscore.c
  ${AIO_DIR}/third_party/cfitsio/getcol.c
  ${AIO_DIR}/third_party/cfitsio/getcolb.c
  ${AIO_DIR}/third_party/cfitsio/getcold.c
  ${AIO_DIR}/third_party/cfitsio/getcole.c
  ${AIO_DIR}/third_party/cfitsio/getcoli.c
  ${AIO_DIR}/third_party/cfitsio/getcolj.c
  ${AIO_DIR}/third_party/cfitsio/getcolk.c
  ${AIO_DIR}/third_party/cfitsio/getcoll.c
  ${AIO_DIR}/third_party/cfitsio/getcols.c
  ${AIO_DIR}/third_party/cfitsio/getcolsb.c
  ${AIO_DIR}/third_party/cfitsio/getcolui.c
  ${AIO_DIR}/third_party/cfitsio/getcoluj.c
  ${AIO_DIR}/third_party/cfitsio/getcoluk.c
  ${AIO_DIR}/third_party/cfitsio/getkey.c
  ${AIO_DIR}/third_party/cfitsio/group.c
  ${AIO_DIR}/third_party/cfitsio/grparser.c
  ${AIO_DIR}/third_party/cfitsio/histo.c
  ${AIO_DIR}/third_party/cfitsio/imcompress.c
  ${AIO_DIR}/third_party/cfitsio/iraffits.c
  ${AIO_DIR}/third_party/cfitsio/modkey.c
  ${AIO_DIR}/third_party/cfitsio/pliocomp.c
  ${AIO_DIR}/third_party/cfitsio/putcol.c
  ${AIO_DIR}/third_party/cfitsio/putcolb.c
  ${AIO_DIR}/third_party/cfitsio/putcold.c
  ${AIO_DIR}/third_party/cfitsio/putcole.c
  ${AIO_DIR}/third_party/cfitsio/putcoli.c
  ${AIO_DIR}/third_party/cfitsio/putcolj.c
  ${AIO_DIR}/third_party/cfitsio/putcolk.c
  ${AIO_DIR}/third_party/cfitsio/putcoll.c
  ${AIO_DIR}/third_party/cfitsio/putcols.c
  ${AIO_DIR}/third_party/cfitsio/putcolsb.c
  ${AIO_DIR}/third_party/cfitsio/putcolu.c
  ${AIO_DIR}/third_party/cfitsio/putcolui.c
  ${AIO_DIR}/third_party/cfitsio/putcoluj.c
  ${AIO_DIR}/third_party/cfitsio/putcoluk.c
  ${AIO_DIR}/third_party/cfitsio/putkey.c
  ${AIO_DIR}/third_party/cfitsio/quantize.c
  ${AIO_DIR}/third_party/cfitsio/region.c
  ${AIO_DIR}/third_party/cfitsio/ricecomp.c
  ${AIO_DIR}/third_party/cfitsio/scalnull.c
  ${AIO_DIR}/third_party/cfitsio/simplerng.c
  ${AIO_DIR}/third_party/cfitsio/swapproc.c
  ${AIO_DIR}/third_party/cfitsio/wcssub.c
  ${AIO_DIR}/third_party/cfitsio/wcsutil.c
  ${AIO_DIR}/third_party/cfitsio/zcompress.c
  ${AIO_DIR}/third_party/cfitsio/zuncompress.c)
list(LENGTH CFITSIO_SRCS _cfn)
message(STATUS "ASTROCS_CFITSIO_n=${_cfn}")
set_property(SOURCE ${CFITSIO_SRCS} PROPERTY COMPILE_OPTIONS "${ACS_WARN_SUPPRESS}")
set(AIO_SRCS ${AIO_DIR}/src/aio_fits.cpp ${AIO_DIR}/src/aio_api.cpp ${AIO_DIR}/src/aio_log.cpp ${AIO_DIR}/src/aio_compressor.cpp ${AIO_DIR}/src/aio_xisf.cpp)
# aio_api 需要完整 AIOImageData(src/aio_fits.h)
set_property(SOURCE ${AIO_DIR}/src/aio_api.cpp APPEND PROPERTY INCLUDE_DIRECTORIES "${AIO_DIR}/src")
set_property(SOURCE ${AIO_SRCS} PROPERTY COMPILE_OPTIONS "${ACS_WARN_SUPPRESS};-DAIO_ENABLE_FITS")
# aio_read(自动探测 .xisf)在 aio_api.cpp; aio_read_xisf 在 aio_xisf.cpp —— 两者都需 XISF 开关
set_property(SOURCE ${AIO_DIR}/src/aio_api.cpp APPEND PROPERTY COMPILE_OPTIONS "-DAIO_ENABLE_XISF")
set_property(SOURCE ${AIO_DIR}/src/aio_xisf.cpp APPEND PROPERTY COMPILE_OPTIONS "-DAIO_ENABLE_XISF")
set_property(SOURCE ${AIO_DIR}/src/aio_xisf.cpp APPEND PROPERTY INCLUDE_DIRECTORIES "${AIO_DIR}/src;${AIO_DIR}/include")
# QA-002: P2 显式清单 (禁 GLOB); ACR 文件 (acr_kernels/kernel_registry/device_executor/cuda_bridge_stub)
# 不编入生产 (LEG-004 ACR dormant, 生产仅纯 CPU)
set(P2_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase2/src/async_io.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase2/src/block.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase2/src/coverage.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase2/src/integrate.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase2/src/rejection.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase2/src/sampler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase2/src/stage2_common.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase2/src/upm.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/astro_image_io/src/hips/aio_hips_reader.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/astro_image_io/src/aio_upm.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/astro_image_io/src/hips/aio_hips_writer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/common/healpix/healpix_core.cpp)
# Phase1 Final Closure: 生产 Drizzle→HiPS 链(cli 需链接 hp_drizzle_run_hips; 见 hp_drizzle_api.h)
# 排除模块内 heapix_core.cpp 去重 shim(cli 已链接 lib/common/healpix/healpix_core.cpp)
set(DRIZZLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../lib/healpix_db/healpix_drizzle)
set(DRIZZLE_SRCS
  ${DRIZZLE_DIR}/fits_reader.cpp ${DRIZZLE_DIR}/wcs_sip.cpp ${DRIZZLE_DIR}/poly_clip.cpp
  ${DRIZZLE_DIR}/spherical_overlap.cpp ${DRIZZLE_DIR}/drizzle_engine.cpp
  ${DRIZZLE_DIR}/reverse_drizzle.cpp ${DRIZZLE_DIR}/hp_drizzle_api.cpp
  ${DRIZZLE_DIR}/astro_sphere_sink.cpp ${DRIZZLE_DIR}/snr_evaluator.cpp)
set_property(SOURCE ${DRIZZLE_SRCS} PROPERTY COMPILE_OPTIONS "${ACS_WARN_SUPPRESS};-DAIO_ENABLE_HEALPIX")
set_property(SOURCE ${DRIZZLE_SRCS} PROPERTY INCLUDE_DIRECTORIES
  "${DRIZZLE_DIR};${CMAKE_CURRENT_SOURCE_DIR}/../lib/astro_image_io/include;${CMAKE_CURRENT_SOURCE_DIR}/../lib/astro_image_io/src;${CMAKE_CURRENT_SOURCE_DIR}/../lib/common;${CMAKE_CURRENT_SOURCE_DIR}/../lib/common/healpix;${CMAKE_CURRENT_SOURCE_DIR}/../include;${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
# hiss 格式写出器 + HEALPix I/O(drizzle_engine 依赖 hiss::HissWriter/compute_tile_*)
set(HISS_SRCS
  ${AIO_DIR}/src/hiss_tile_model.cpp ${AIO_DIR}/src/hiss_writer.cpp ${AIO_DIR}/src/hiss_common.cpp
  ${AIO_DIR}/src/hiss_transform.cpp ${AIO_DIR}/src/hiss_codec.cpp ${AIO_DIR}/src/hiss_reader.cpp
  ${AIO_DIR}/src/hiss_stream_writer.cpp ${AIO_DIR}/src/healpix/aio_healpix_io.cpp
  ${AIO_DIR}/src/aio_pipeline.cpp ${AIO_DIR}/src/aio_pipeline_engine.cpp)
set_property(SOURCE ${HISS_SRCS} PROPERTY COMPILE_OPTIONS "${ACS_WARN_SUPPRESS};-DAIO_ENABLE_HEALPIX")
set_property(SOURCE ${HISS_SRCS} PROPERTY INCLUDE_DIRECTORIES
  "${AIO_DIR}/include;${AIO_DIR}/src;${CMAKE_CURRENT_SOURCE_DIR}/../lib/common;${CMAKE_CURRENT_SOURCE_DIR}/../lib/common/healpix;${CMAKE_CURRENT_SOURCE_DIR}/../include;${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
# QA-001: P2 生产源已去 -w 抑制 (警告清零)
# sampler.cpp 用 C++ try/catch(...)/EHa 捕获结构化异常(原 __try/__except, MSVC 禁与对象展开混用)
if(MSVC)
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase2/src/sampler.cpp PROPERTIES COMPILE_OPTIONS "/EHa")
endif()
set_property(SOURCE ${P2_SRCS} PROPERTY INCLUDE_DIRECTORIES
  "${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase2/include;${CMAKE_CURRENT_SOURCE_DIR}/../lib/acr/include;${CMAKE_CURRENT_SOURCE_DIR}/../lib/acr;${CMAKE_CURRENT_SOURCE_DIR}/../lib/common;${CMAKE_CURRENT_SOURCE_DIR}/../lib/astro_image_io/include;${CMAKE_CURRENT_SOURCE_DIR}/../lib/astro_image_io/src;${CMAKE_CURRENT_SOURCE_DIR}/../lib/astro_image_io/third_party/cfitsio;${CMAKE_CURRENT_SOURCE_DIR}/../lib/acr/backends/cuda/bridge;${CMAKE_CURRENT_SOURCE_DIR}/../include;${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
set(CAL_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../lib/calibration/src/calibrator.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/calibration/src/master_generator.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/calibration/src/cosmetic_corrector.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/calibration/src/dark_optimizer.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/calibration/src/ac_api.cpp)
if(MSVC)
  set_source_files_properties(${CAL_SRCS} PROPERTIES COMPILE_OPTIONS "/openmp")
else()
  set_source_files_properties(${CAL_SRCS} PROPERTIES COMPILE_OPTIONS "-fopenmp")
endif()
# QA-002: P3 显式清单 (禁 GLOB)
set(P3_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase3_session/hips_properties.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase3_session/p3_output.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase3_session/p3_resample.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase3_session/p3_wcs.cpp)
# QA-001: P3 生产源已去 -w 抑制 (警告清零)
set_property(SOURCE ${P3_SRCS} APPEND PROPERTY INCLUDE_DIRECTORIES
  "${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase3_session;${CMAKE_CURRENT_SOURCE_DIR}/../lib/common;${CMAKE_CURRENT_SOURCE_DIR}/../lib/common/healpix;${CMAKE_CURRENT_SOURCE_DIR}/../lib/common/crypto;${CMAKE_CURRENT_SOURCE_DIR}/../lib/astro_image_io/include;${CMAKE_CURRENT_SOURCE_DIR}/../lib/astro_image_io/src;${CMAKE_CURRENT_SOURCE_DIR}/../lib/astro_image_io/third_party/cfitsio;${CMAKE_CURRENT_SOURCE_DIR}/../include;${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
add_executable(astrocs main.cpp parser.cpp commands.cpp runtime_client.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/common/crypto/sha256.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/cpu_features.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/hardware_inspect.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/backend_loader.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/bench_harness.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/profile_gen.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/profile_gen_v2.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/cpu_routing.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/baseline_backend.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host/host_services.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase1_session/p1_session.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase2_session/p2_session.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase3_session/p3_session.cpp ${P3_SRCS} ${P2_SRCS} ${AIO_SRCS} ${CFITSIO_SRCS} ${CAL_SRCS} ${DRIZZLE_SRCS} ${HISS_SRCS} ${CMAKE_CURRENT_SOURCE_DIR}/../lib/core/src/artifact.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/core/src/artifact_store.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/core/src/module.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/core/src/pipeline.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/core/src/context.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/core/src/runtime.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/core/src/scheduler.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/core/src/checkpoint.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/core/src/logging.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/core/src/module_adapters.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../lib/common/healpix/healpix_core.cpp ${CMAKE_CURRENT_BINARY_DIR}/version_generated.h)
if(MSVC)
  # gomp/m 为 Unix 库, MSVC 不用; zstd/lz4 在CLI走 fallback(HAS_* 未定义), 仅 cfitsio 需 zlib(z)。
  # zlib: 由 MSVC 从源码构建(zlib.h+zlib.lib), 用 -DACS_ZLIB_ROOT=... 指定。
  set(ACS_ZLIB_ROOT "C:/Users/fujia/zlib-msvc" CACHE PATH "MSVC 编译的 zlib 根目录")
  target_include_directories(astrocs PRIVATE "${ACS_ZLIB_ROOT}/include")
  # 优先静态 zlib(libzs.lib), 避免 libz.dll 运行时缺失; 其次 zlib.lib 导入库
  if(EXISTS "${ACS_ZLIB_ROOT}/lib/libzs.lib")
    target_link_libraries(astrocs PRIVATE "${ACS_ZLIB_ROOT}/lib/libzs.lib")
  elseif(EXISTS "${ACS_ZLIB_ROOT}/lib/zlibstatic.lib")
    target_link_libraries(astrocs PRIVATE "${ACS_ZLIB_ROOT}/lib/zlibstatic.lib")
  else()
    target_link_libraries(astrocs PRIVATE "${ACS_ZLIB_ROOT}/lib/libz.lib")
  endif()
else()
  target_link_libraries(astrocs PRIVATE gomp z zstd lz4 m)
endif()
# vendored/per-lib includes(aio/calibration/cfitsio) — 只影响对应源
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/common/healpix/healpix_core.cpp PROPERTY INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/../lib/common")
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase2_session/p2_session.cpp APPEND PROPERTY INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase2/include")
set_property(SOURCE ${AIO_SRCS} ${CFITSIO_SRCS} ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase1_session/p1_session.cpp PROPERTY INCLUDE_DIRECTORIES "${AIO_DIR}/include;${AIO_DIR}/third_party/cfitsio;${CMAKE_CURRENT_SOURCE_DIR}/../lib/calibration/include;${CMAKE_CURRENT_SOURCE_DIR}/../include;${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
# MSVC: 无 <unistd.h>; 为 cfitsio 纯 C 源提供 win_compat/unistd.h shim(用户 /I 先于系统搜索)
if(MSVC)
  set_property(SOURCE ${CFITSIO_SRCS} APPEND PROPERTY INCLUDE_DIRECTORIES "${AIO_DIR}/third_party/cfitsio/win_compat")
endif()
set_property(SOURCE ${CAL_SRCS} PROPERTY INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/../lib/calibration/include;${AIO_DIR}/include;${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(astrocs PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../third_party ${CMAKE_CURRENT_SOURCE_DIR}/../lib/common/crypto ${CMAKE_CURRENT_SOURCE_DIR}/../lib/backend_host ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase1_session ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase2_session ${CMAKE_CURRENT_SOURCE_DIR}/../lib/phase3_session ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CURRENT_SOURCE_DIR}/../lib/astro_image_io/include ${CMAKE_CURRENT_SOURCE_DIR}/../lib/astro_image_io/src ${CMAKE_CURRENT_SOURCE_DIR}/../lib/astro_image_io/third_party/cfitsio ${CMAKE_CURRENT_SOURCE_DIR}/../lib/healpix_db/healpix_drizzle)
target_compile_options(astrocs PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra>
  $<$<CXX_COMPILER_ID:MSVC>:/W4;/EHsc;/utf-8>)
# MSVC: 目标 Windows 10 → 启用 LoadLibraryEx LOAD_LIBRARY_SEARCH_* 与 GetActiveProcessorCount 等 API
#       _USE_MATH_DEFINES 暴露 M_PI(POSIX)
target_compile_definitions(astrocs PRIVATE $<$<CXX_COMPILER_ID:MSVC>:_WIN32_WINNT=0x0A00;NOMINMAX;_USE_MATH_DEFINES>)

# BLD-002: 禁止正式 install —— 本文件为 compatibility 声明 (非产品事实源),
# 安装/发布规则只允许存在于根 CMakeLists.txt (唯一产品入口)。
