# AstroCS 根 CMake — BLD-001 显式 target 图 (V6)
cmake_minimum_required(VERSION 3.24)
project(astrocs VERSION 0.10.0 LANGUAGES C CXX)

# ── 选项 (03/BLD-001 冻结) ──
option(ASTROCS_ENABLE_ACR "Build dormant ACR tree (default OFF, production exclusion)" OFF)
option(ASTROCS_BUILD_TESTS "Build tests" ON)
option(ASTROCS_ENABLE_SANITIZERS "Enable ASan/UBSan (GCC/Clang)" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# 版本单源: 根 VERSION 文件 (VER-001)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/VERSION ASTROCS_BASE_VERSION)
string(STRIP "${ASTROCS_BASE_VERSION}" ASTROCS_BASE_VERSION)
execute_process(COMMAND git rev-parse HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE ASTROCS_GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT ASTROCS_GIT_COMMIT)
  set(ASTROCS_GIT_COMMIT "0000000000000000000000000000000000000000")
endif()
set(ASTROCS_VERSION_STRING "${ASTROCS_BASE_VERSION}+g${ASTROCS_GIT_COMMIT}")

# 版本生成头 (VER-001: 从 VERSION 唯一源)
configure_file(cli/version_generated.h.in ${CMAKE_CURRENT_BINARY_DIR}/version_generated.h @ONLY)
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/version_generated.h
  "#define ASTROCS_COMMIT_SHA \"${ASTROCS_GIT_COMMIT}\"\n")

# vendored cfitsio 显式清单 (BLD-001 禁 GLOB; 由 tools/gen_cfitsio_list.py 生成)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/cfitsio_sources.cmake)

# ── 显式 target 依赖图 (依赖方向见 ARCH-001 §3) ──

# core/contracts: 公共类型 (DATA/API/MOD)
add_library(astrocs_contracts INTERFACE)
target_include_directories(astrocs_contracts INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/include)

# io adapter (IO-001: 依赖方向 core <- io; 不 include runtime/模块)
add_library(astrocs_io STATIC lib/io/src/io_adapter.cpp)
target_include_directories(astrocs_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(astrocs_io PUBLIC astrocs_core)

add_library(astrocs_core STATIC
  lib/core/src/artifact.cpp
  lib/core/src/artifact_store.cpp
  lib/core/src/module.cpp
  lib/core/src/pipeline.cpp
  lib/core/src/context.cpp
  lib/core/src/runtime.cpp
  lib/core/src/scheduler.cpp
  lib/core/src/checkpoint.cpp
  lib/core/src/logging.cpp)
target_include_directories(astrocs_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(astrocs_core PUBLIC astrocs_contracts Threads::Threads)
find_package(Threads REQUIRED)

# core/common: sha256/healpix (纯算法)
add_library(astrocs_common STATIC
  lib/common/crypto/sha256.cpp
  lib/common/healpix/healpix_core.cpp)
target_include_directories(astrocs_common PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/crypto
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/healpix)

# vendored cfitsio (第三方, SYSTEM 隔离; 不参与模块图)
add_library(astrocs_cfitsio STATIC ${ASTROCS_CFITSIO_SOURCES})
target_include_directories(astrocs_cfitsio PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/third_party/cfitsio)
target_compile_options(astrocs_cfitsio PRIVATE -w)
# IO-003: CFITSIO 显式 reentrant 构建 (fits_is_reentrant() 必须返回 1)
target_compile_definitions(astrocs_cfitsio PRIVATE _REENTRANT)

# io/astro_image_io: 图像/HiPS I/O adapter (aio_pipeline* 保留待 LEG-003 退出)
add_library(astrocs_aio STATIC
  lib/astro_image_io/src/aio_fits.cpp
  lib/astro_image_io/src/aio_api.cpp
  lib/astro_image_io/src/aio_log.cpp
  lib/astro_image_io/src/aio_compressor.cpp
  lib/astro_image_io/src/aio_xisf.cpp
  lib/astro_image_io/src/aio_pipeline.cpp)
target_include_directories(astrocs_aio PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/third_party/cfitsio
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common)
target_compile_definitions(astrocs_aio PRIVATE AIO_ENABLE_FITS AIO_ENABLE_XISF)
target_link_libraries(astrocs_aio PUBLIC astrocs_common astrocs_cfitsio z)

# io/hips: HiPS reader/writer + hiss (无科学积分)
add_library(astrocs_hips STATIC
  lib/astro_image_io/src/hips/aio_hips_reader.cpp
  lib/astro_image_io/src/hips/aio_hips_writer.cpp
  lib/astro_image_io/src/healpix/aio_healpix_io.cpp
  lib/astro_image_io/src/hiss_codec.cpp
  lib/astro_image_io/src/hiss_common.cpp
  lib/astro_image_io/src/hiss_reader.cpp
  lib/astro_image_io/src/hiss_stream_writer.cpp
  lib/astro_image_io/src/hiss_tile_model.cpp
  lib/astro_image_io/src/hiss_transform.cpp
  lib/astro_image_io/src/hiss_writer.cpp
  lib/astro_image_io/src/aio_upm.cpp)
target_include_directories(astrocs_hips PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/third_party/cfitsio
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/healpix)
target_compile_definitions(astrocs_hips PRIVATE AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(astrocs_hips PUBLIC astrocs_aio astrocs_common)

# modules/calibration
add_library(astrocs_calibration STATIC
  lib/calibration/src/calibrator.cpp
  lib/calibration/src/master_generator.cpp
  lib/calibration/src/cosmetic_corrector.cpp
  lib/calibration/src/ac_api.cpp)
target_include_directories(astrocs_calibration PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/calibration/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/include)
target_link_libraries(astrocs_calibration PUBLIC astrocs_aio astrocs_common)

# modules/phase2 (纯 CPU 生产模块; ACR 源不在此列 — LEG-004)
add_library(astrocs_phase2 STATIC
  lib/phase2/src/upm.cpp
  lib/phase2/src/stage2_common.cpp
  lib/phase2/src/rejection.cpp
  lib/phase2/src/coverage.cpp
  lib/phase2/src/sampler.cpp
  lib/phase2/src/block.cpp
  lib/phase2/src/integrate.cpp
  lib/phase2/src/cuda_bridge_stub.cpp)
target_include_directories(astrocs_phase2 PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase2/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/acr/backends/cuda/bridge
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/third_party/cfitsio
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common)
target_link_libraries(astrocs_phase2 PUBLIC astrocs_contracts astrocs_common astrocs_aio astrocs_hips)

# modules/healpix_drizzle
add_library(astrocs_drizzle STATIC
  lib/healpix_db/healpix_drizzle/fits_reader.cpp
  lib/healpix_db/healpix_drizzle/wcs_sip.cpp
  lib/healpix_db/healpix_drizzle/poly_clip.cpp
  lib/healpix_db/healpix_drizzle/spherical_overlap.cpp
  lib/healpix_db/healpix_drizzle/drizzle_engine.cpp
  lib/healpix_db/healpix_drizzle/reverse_drizzle.cpp
  lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp
  lib/healpix_db/healpix_drizzle/astro_sphere_sink.cpp
  lib/healpix_db/healpix_drizzle/snr_evaluator.cpp)
target_include_directories(astrocs_drizzle PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/healpix_db/healpix_drizzle
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/third_party/cfitsio
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/healpix)
target_compile_definitions(astrocs_drizzle PRIVATE AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(astrocs_drizzle PUBLIC astrocs_contracts astrocs_common astrocs_aio astrocs_hips)

# backends/cpu: provider ABI + baseline (AVX2/512 见 CPU-003/004)
add_library(astrocs_cpu STATIC
  lib/backend_host/cpu_features.cpp
  lib/backend_host/hardware_inspect.cpp
  lib/backend_host/backend_loader.cpp
  lib/backend_host/baseline_backend.cpp
  lib/backend_host/profile_gen.cpp
  lib/backend_host/host_services.cpp
  lib/backend_host/bench_harness.cpp)
target_include_directories(astrocs_cpu PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/backend_host
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/crypto)
target_link_libraries(astrocs_cpu PUBLIC astrocs_contracts astrocs_common)

# phase1 wcs + photometry (P1-004)
add_library(astrocs_phase1_wcs STATIC lib/phase1/wcs/wcs_tan.cpp)
target_include_directories(astrocs_phase1_wcs PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase1/wcs ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(astrocs_phase1_phot STATIC lib/phase1/photometry/photometer.cpp)
target_include_directories(astrocs_phase1_phot PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase1/photometry ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(astrocs_phase1_phot PUBLIC astrocs_contracts)

# phase1 noise (P1-005 SCI-NOISE-001)
add_library(astrocs_phase1_noise STATIC lib/phase1/noise/noise_model.cpp)
target_include_directories(astrocs_phase1_noise PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase1/noise ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(astrocs_phase1_noise PUBLIC astrocs_contracts)

# phase1 stars (P1-003 StarDetector)
add_library(astrocs_phase1_stars STATIC lib/phase1/stars/star_detector.cpp)
target_include_directories(astrocs_phase1_stars PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase1/stars
  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(astrocs_phase1_stars PUBLIC astrocs_contracts)

# phases sessions (facade, 后续迁移为 preset)
add_library(astrocs_phase1_session STATIC lib/phase1_session/p1_session.cpp)
target_include_directories(astrocs_phase1_session PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase1_session
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/calibration/include)
target_link_libraries(astrocs_phase1_session PUBLIC astrocs_contracts astrocs_calibration astrocs_aio)

add_library(astrocs_phase2_session STATIC lib/phase2_session/p2_session.cpp)
target_include_directories(astrocs_phase2_session PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase2_session
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase2/include)
target_link_libraries(astrocs_phase2_session PUBLIC astrocs_contracts astrocs_phase2)

add_library(astrocs_phase3_session STATIC
  lib/phase3_session/p3_session.cpp
  lib/phase3_session/p3_wcs.cpp
  lib/phase3_session/hips_properties.cpp
  lib/phase3_session/p3_output.cpp
  lib/phase3_session/p3_resample.cpp)
target_include_directories(astrocs_phase3_session PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase3_session
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/crypto
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/healpix
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/include
  ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(astrocs_phase3_session PUBLIC astrocs_contracts astrocs_aio astrocs_common)

# app/astrocs_cli: 薄 CLI (显式链接各库; 无 GLOB; ACR 不链)
add_executable(astrocs
  cli/main.cpp)
target_include_directories(astrocs PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/cli
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/src
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/include
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/astro_image_io/third_party/cfitsio
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/healpix_db/healpix_drizzle
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/crypto
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/healpix
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/phase2/include)
target_link_libraries(astrocs PRIVATE
  astrocs_contracts astrocs_core astrocs_common astrocs_cfitsio astrocs_aio astrocs_hips
  astrocs_calibration astrocs_phase2 astrocs_drizzle astrocs_cpu
  astrocs_phase1_session astrocs_phase2_session astrocs_phase3_session
  ${CMAKE_DL_LIBS} m z)
if(UNIX)
  find_package(OpenMP QUIET)
  if(OpenMP_CXX_FOUND)
    target_link_libraries(astrocs PRIVATE OpenMP::OpenMP_CXX)
  endif()
endif()

# RT-005 可执行模块适配器: Phase1/2/3 session → IModule (core 不依赖会话库)
add_library(astrocs_module_adapters STATIC lib/core/src/module_adapters.cpp)
target_include_directories(astrocs_module_adapters PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(astrocs_module_adapters PUBLIC
  astrocs_core astrocs_common astrocs_contracts
  astrocs_phase1_session astrocs_phase2_session astrocs_phase3_session
  astrocs_cpu)

if(ASTROCS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests/unit)
endif()

message(STATUS "AstroCS ${ASTROCS_VERSION_STRING} root target graph ready (ACR=${ASTROCS_ENABLE_ACR})")
