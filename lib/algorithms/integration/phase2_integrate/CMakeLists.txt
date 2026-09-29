# ============================================================================
# ACSD V6 Phase2 三模式产品链集成库 (P2-INTEGRATE-001, Wave 8)
# 写域: lib/algorithms/integration/phase2_integrate/。本层只"接线" Wave 5/7 生产源，不复制科学公式。
# 根/公共 CMakeLists 的注册归 RUNTIME-CI-001 (C-004.4)；本文件自包含可独立
# 构建（cmake -S lib/algorithms/integration/phase2_integrate -B <build>），也可被上层 add_subdirectory 复用。
# ============================================================================
cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(astrocs_phase2_integrate LANGUAGES C CXX)
endif()

if(NOT TARGET astrocs_phase2_integrate)
  set(P2_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
  get_filename_component(P2_REPO "${P2_REPO}" ABSOLUTE)

  # 生产 phase2 需要 legacy AIO (aio_hips_reader / aio_upm) -> vendored cfitsio。
  if(NOT DEFINED ASTROCS_CFITSIO_SOURCES)
    include("${P2_REPO}/eng/cmake/cfitsio_sources.cmake")
  endif()
  set(_P2_CFITSIO_ABS "")
  foreach(_s ${ASTROCS_CFITSIO_SOURCES})
    list(APPEND _P2_CFITSIO_ABS "${P2_REPO}/${_s}")
  endforeach()

  add_library(astrocs_phase2_integrate STATIC
    src/phase2_integrate.cpp
    src/weight_chain.cpp
    # 残差制造者 PΣPᵀ 方差传播（纯 FP64 + std，无第三方依赖）。
    src/variance_propagation.cpp
    "${P2_REPO}/lib/algorithms/coverage/src/upm.cpp"
    # UPM-KAPPA-UNIFY-01：唯一可辨识性判据（upm.cpp 依赖它，必须同 target 编译）
    "${P2_REPO}/lib/algorithms/coverage/src/identifiability.cpp"
    "${P2_REPO}/lib/algorithms/coverage/src/rejection.cpp"
    "${P2_REPO}/lib/algorithms/coverage/src/sampler.cpp"
    "${P2_REPO}/lib/algorithms/coverage/src/coverage.cpp"
    "${P2_REPO}/lib/algorithms/integration/phase1_product/src/phase1_product.cpp"
    "${P2_REPO}/lib/infrastructure/aio/product_io/src/sha256.cpp"
    "${P2_REPO}/lib/infrastructure/aio/product_io/src/bunit.cpp"
    "${P2_REPO}/lib/infrastructure/aio/product_io/src/fits.cpp"
    "${P2_REPO}/lib/infrastructure/aio/product_io/src/atomic_publish.cpp"
    "${P2_REPO}/lib/infrastructure/aio/product_io/src/provenance.cpp"
    "${P2_REPO}/lib/infrastructure/aio/product_io/src/hips_manifest.cpp"
    "${P2_REPO}/lib/infrastructure/aio/product_io/src/product_io.cpp"
    "${P2_REPO}/lib/algorithms/calibration/src/calibration_covariance.cpp"
    "${P2_REPO}/lib/algorithms/psf/src/psf_information.cpp"
    "${P2_REPO}/lib/algorithms/noise_snr/cpp/src/information_weight.cpp"
    "${P2_REPO}/lib/algorithms/photometry/cpp/src/psfsw.cpp"
    "${P2_REPO}/lib/algorithms/drizzle/healpix_drizzle/drizzle_science.cpp"
    "${P2_REPO}/lib/algorithms/drizzle/healpix_drizzle/spherical_overlap_science.cpp"
    "${P2_REPO}/lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp"
    "${P2_REPO}/lib/algorithms/shared/healpix/healpix_core.cpp"
    "${P2_REPO}/lib/algorithms/shared/crypto/sha256.cpp"
    # legacy AIO 后端（sampler/coverage/upm 依赖 aio_hips_* / aio_upm_*）
    "${P2_REPO}/lib/infrastructure/aio/src/hips/aio_hips_reader.cpp"
    "${P2_REPO}/lib/infrastructure/aio/src/aio_upm.cpp"
    ${_P2_CFITSIO_ABS})

  target_include_directories(astrocs_phase2_integrate PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${P2_REPO}/lib/algorithms/integration/phase1_product/include"
    "${P2_REPO}/lib/algorithms/coverage/include"
    "${P2_REPO}/lib/infrastructure/aio/product_io/include"
    "${P2_REPO}/lib/infrastructure/aio/include"
    "${P2_REPO}/lib/infrastructure/aio/src"
    "${P2_REPO}/lib/infrastructure/aio/src/hips"
    "${P2_REPO}/lib/infrastructure/aio/third_party/cfitsio"
    # WIN-PORT 缺陷⑤: nlohmann/json.hpp 的唯一落点是 lib/third_party/nlohmann/
    # (根目录整合 commit ee99ca26 把第三方头自根 third_party/ 迁入 lib/third_party/,
    #  本处 ${P2_REPO}/third_party 是迁移漏网的死路径 ⇒ 本 target 编入的
    #  coverage/src/upm.cpp、aio/src/aio_upm.cpp 与 product_io 的 hips_manifest.h /
    #  provenance.h 在 MSVC 下报 C1083 找不到 nlohmann/json.hpp)。
    "${P2_REPO}/lib/third_party"
    "${P2_REPO}/lib/algorithms/calibration/include"
    "${P2_REPO}/lib/algorithms/psf/include"
    "${P2_REPO}/lib/algorithms/noise_snr/include"
    "${P2_REPO}/lib/algorithms/photometry/include"
    "${P2_REPO}/lib/algorithms/drizzle/healpix_drizzle"
    "${P2_REPO}/lib/algorithms/shared"
    "${P2_REPO}/lib/algorithms/shared/healpix"
    "${P2_REPO}/lib/algorithms/shared/crypto")

  target_compile_definitions(astrocs_phase2_integrate PRIVATE
    AIO_ENABLE_FITS AIO_ENABLE_XISF _REENTRANT)

  # ── 第三方源的平台面: 与根 astrocs_cfitsio **同一份**实现 ────────────────
  # 事实(改前): 本目标把 vendored cfitsio 的**整套源**再编一遍 (60 TU, 见上方
  #   ${_P2_CFITSIO_ABS}) 并定义 _REENTRANT, 却没有根目标那份平台垫片包含面
  #   ⇒ Windows/MSVC 全量构建报 55 条 C1083 "pthread.h" (fitsio2.h 的
  #   `#ifdef _REENTRANT` / `#include <pthread.h>` 分支被 _REENTRANT 打开);
  #   同目标还自编 zcompress.c/zuncompress.c 却未接 zlib 包含面 ⇒ C1083 "zlib.h"。
  #   根站点 (CMakeLists.txt 的 astrocs_cfitsio) 本就做了平台隔离 ⇒ 同族两处两套口径。
  # 现口径: 两处都调用 eng/cmake/cfitsio_platform.cmake 的同名函数 (唯一实现),
  #   不再各自内联路径字面量; 该模块由根 CMake 入口 include, 独立构建本目录时自补。
  if(NOT COMMAND astrocs_cfitsio_apply_platform_shim)
    include("${P2_REPO}/eng/cmake/cfitsio_platform.cmake")
  endif()
  astrocs_cfitsio_apply_platform_shim(astrocs_phase2_integrate)
  astrocs_cfitsio_apply_third_party_deps(astrocs_phase2_integrate)
# 第三方告警隔离 (与基准目标 astrocs_cfitsio 同一份实现): 本目标编的是 vendored
# cfitsio 源, 不该把 drvrnet.c C4206 / zuncompress.c C4267 之类灌进项目告警面。
astrocs_cfitsio_isolate_warnings(astrocs_phase2_integrate)
  set_target_properties(astrocs_phase2_integrate PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON)
  target_link_libraries(astrocs_phase2_integrate PUBLIC astrocs_platform_math astrocs_platform_zlib)
endif()
