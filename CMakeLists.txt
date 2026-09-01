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

# OpenMP 探测提前: 遗留模块 (calibration/aio/drizzle) 含 omp pragma, 编译需 -fopenmp
# (QA-001: 自有遗留 target target-local 编译, 不污染主程序; 第三方 cfitsio 仍 -w 隔离)
if(UNIX)
  find_package(OpenMP QUIET)
endif()

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
# QA-001: aio_fits.cpp 含 omp pragma → -fopenmp 编译消除 unknown-pragmas
if(UNIX AND OpenMP_CXX_FOUND)
  target_compile_options(astrocs_aio PRIVATE -fopenmp)
  target_link_libraries(astrocs_aio PRIVATE OpenMP::OpenMP_CXX)
endif()

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
# QA-001: calibrator/cosmetic_corrector/master_generator 含 omp pragma → -fopenmp
if(UNIX AND OpenMP_CXX_FOUND)
  target_compile_options(astrocs_calibration PRIVATE -fopenmp)
  target_link_libraries(astrocs_calibration PRIVATE OpenMP::OpenMP_CXX)
endif()

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
# QA-001: drizzle_engine/snr_evaluator 含 omp pragma → -fopenmp 编译消除 unknown-pragmas
# (omp 并行仅为遗留 drizzle 内部; 生产调度走 Runtime lease, 见 MON/CHK 系列)
if(UNIX AND OpenMP_CXX_FOUND)
  target_compile_options(astrocs_drizzle PRIVATE -fopenmp)
  target_link_libraries(astrocs_drizzle PRIVATE OpenMP::OpenMP_CXX)
endif()

# backends/cpu: provider ABI + baseline (AVX2/512 见 CPU-003/004)
add_library(astrocs_cpu STATIC
  lib/backend_host/cpu_features.cpp
  lib/backend_host/hardware_inspect.cpp
  lib/backend_host/backend_loader.cpp
  lib/backend_host/baseline_backend.cpp
  lib/backend_host/profile_gen.cpp
  lib/backend_host/profile_gen_v2.cpp
  lib/backend_host/cpu_routing.cpp
  lib/backend_host/host_services.cpp
  lib/backend_host/bench_harness.cpp)
target_include_directories(astrocs_cpu PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/backend_host
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/crypto)
target_link_libraries(astrocs_cpu PUBLIC astrocs_contracts astrocs_common)

# CPU-001 (G3): 独立 provider 变体 target — 真实独立编译单元 (05 §2)
# 编译隔离合同: -mavx2/-mfma 等高级旗标仅作用于对应 provider target(PRIVATE),
# 不污染 astrocs_cpu/baseline 与主 CLI; 主 CLI 保持最低 amd64/SSE2 (ARCH-003 §2)。
# 变体与 baseline 共享 baseline_kernels_impl.inc/backend_table.inc 同一源(零复制漂移),
# 仅编译旗标与 ASTROCS_BACKEND_ID 不同 → 每条路径独立编译成不同目标并各自 self-test。
# 注意: 两个变体 target 均不得被 astrocs(主 CLI) 链接 — 由 check_isa_leak 的 link map 验证把关。
add_library(astrocs_cpu_avx2 STATIC lib/backend_host/avx2_backend.cpp)
target_include_directories(astrocs_cpu_avx2 PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/backend_host
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/crypto)
target_compile_options(astrocs_cpu_avx2 PRIVATE -mavx2 -mfma)
target_link_libraries(astrocs_cpu_avx2 PUBLIC astrocs_contracts astrocs_common)

add_library(astrocs_cpu_avx512 STATIC lib/backend_host/avx512_backend.cpp)
target_include_directories(astrocs_cpu_avx512 PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/backend_host
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/common/crypto)
# AVX512 实际使用子集: F + DQ + BW + VL (ISA-004, 与 cpu_features.h 检测面一致)
target_compile_options(astrocs_cpu_avx512 PRIVATE
  -mavx512f -mavx512bw -mavx512vl -mavx512dq)
target_link_libraries(astrocs_cpu_avx512 PUBLIC astrocs_contracts astrocs_common)

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
# RT-008: main.cpp 仅入口壳; parser/commands/runtime_client 为独立编译单元
add_executable(astrocs
  cli/main.cpp
  cli/parser.cpp
  cli/commands.cpp
  cli/runtime_client.cpp)
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
  astrocs_contracts astrocs_core astrocs_cli_runtime astrocs_common astrocs_cfitsio astrocs_aio astrocs_hips
  astrocs_calibration astrocs_phase2 astrocs_drizzle astrocs_cpu
  astrocs_phase1_session astrocs_phase2_session astrocs_phase3_session
  ${CMAKE_DL_LIBS} m z)
if(UNIX)
  find_package(OpenMP QUIET)
  if(OpenMP_CXX_FOUND)
    target_link_libraries(astrocs PRIVATE OpenMP::OpenMP_CXX)
  endif()
endif()

# ── QA-001: 自有生产 targets 分层严格 warning (GCC/Clang; MSVC 用 /W4) ──
# 第三方 (cfitsio) 与遗留 (aio/hips/drizzle/calibration) 已 target-local 隔离:
#  - cfitsio: -w (第三方)
#  - aio/hips/drizzle/calibration: -fopenmp 编译 omp pragma (遗留, 无 -Wconversion 惩罚)
# 自有生产: core/common/phase2/phase3/cpu/CLI 全量 -Wall -Wextra -Wpedantic -Wconversion
if(NOT MSVC)
  foreach(tgt astrocs_core astrocs_common astrocs_phase2 astrocs_phase3_session
              astrocs_cpu astrocs_phase1_wcs astrocs_phase1_phot astrocs_phase1_noise
              astrocs_phase1_stars astrocs_phase1_session astrocs_phase2_session
              astrocs_module_adapters astrocs_cli_runtime astrocs_cpu_avx2 astrocs_cpu_avx512)
    if(TARGET ${tgt})
      target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
  endforeach()
  target_compile_options(astrocs PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
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

# RT-008 CLI Runtime client: CLI 经公开 Runtime API 驱动 pipeline（不 include session/IO 内部头）
add_library(astrocs_cli_runtime STATIC cli/runtime_client.cpp)
target_include_directories(astrocs_cli_runtime PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/cli
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(astrocs_cli_runtime PUBLIC
  astrocs_core astrocs_module_adapters)

if(ASTROCS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests/unit)
endif()

message(STATUS "AstroCS ${ASTROCS_VERSION_STRING} root target graph ready (ACR=${ASTROCS_ENABLE_ACR})")
