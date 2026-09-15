# ============================================================================
# AstroCS V6 Phase2 三模式产品链集成库 (P2-INTEGRATE-001, Wave 8)
# 写域: lib/phase2_int/v6/。本层只"接线" Wave 5/7 生产源，不复制科学公式。
# 根/公共 CMakeLists 的注册归 RUNTIME-CI-001/W9 (C-004.4)；本文件自包含可独立
# 构建（cmake -S lib/phase2_int/v6 -B <build>），也可被上层 add_subdirectory 复用。
# ============================================================================
cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(astrocs_v6_phase2_integrate LANGUAGES C CXX)
endif()

if(NOT TARGET astrocs_v6_phase2_integrate)
  set(V6P2_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
  get_filename_component(V6P2_REPO "${V6P2_REPO}" ABSOLUTE)

  # 生产 phase2 需要 legacy AIO (aio_hips_reader / aio_upm) -> vendored cfitsio。
  if(NOT DEFINED ASTROCS_CFITSIO_SOURCES)
    include("${V6P2_REPO}/cmake/cfitsio_sources.cmake")
  endif()
  set(_V6P2_CFITSIO_ABS "")
  foreach(_s ${ASTROCS_CFITSIO_SOURCES})
    list(APPEND _V6P2_CFITSIO_ABS "${V6P2_REPO}/${_s}")
  endforeach()

  add_library(astrocs_v6_phase2_integrate STATIC
    src/phase2_integrate.cpp
    "${V6P2_REPO}/lib/phase2/src/upm.cpp"
    "${V6P2_REPO}/lib/phase2/src/rejection.cpp"
    "${V6P2_REPO}/lib/phase2/src/sampler.cpp"
    "${V6P2_REPO}/lib/phase2/src/coverage.cpp"
    "${V6P2_REPO}/lib/phase1/v6/src/phase1_product.cpp"
    "${V6P2_REPO}/lib/astro_image_io/v6/src/v6_sha256.cpp"
    "${V6P2_REPO}/lib/astro_image_io/v6/src/v6_bunit.cpp"
    "${V6P2_REPO}/lib/astro_image_io/v6/src/v6_fits.cpp"
    "${V6P2_REPO}/lib/astro_image_io/v6/src/v6_atomic_publish.cpp"
    "${V6P2_REPO}/lib/astro_image_io/v6/src/v6_provenance.cpp"
    "${V6P2_REPO}/lib/astro_image_io/v6/src/v6_hips_manifest.cpp"
    "${V6P2_REPO}/lib/astro_image_io/v6/src/v6_product_io.cpp"
    "${V6P2_REPO}/lib/calibration/src/v6_calibration_covariance.cpp"
    "${V6P2_REPO}/lib/dynamic_psf/src/psf_information.cpp"
    "${V6P2_REPO}/lib/snr_estimator/cpp/src/information_weight.cpp"
    "${V6P2_REPO}/lib/photometric_calib/cpp/src/psfsw.cpp"
    "${V6P2_REPO}/lib/healpix_db/healpix_drizzle/v6_drizzle_science.cpp"
    "${V6P2_REPO}/lib/healpix_db/healpix_drizzle/v6_spherical_overlap.cpp"
    "${V6P2_REPO}/lib/healpix_db/healpix_drizzle/spherical_overlap.cpp"
    "${V6P2_REPO}/lib/common/healpix/healpix_core.cpp"
    "${V6P2_REPO}/lib/common/crypto/sha256.cpp"
    # legacy AIO 后端（sampler/coverage/upm 依赖 aio_hips_* / aio_upm_*）
    "${V6P2_REPO}/lib/astro_image_io/src/hips/aio_hips_reader.cpp"
    "${V6P2_REPO}/lib/astro_image_io/src/aio_upm.cpp"
    ${_V6P2_CFITSIO_ABS})

  target_include_directories(astrocs_v6_phase2_integrate PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${V6P2_REPO}/lib/phase1/v6/include"
    "${V6P2_REPO}/lib/phase2/include"
    "${V6P2_REPO}/lib/astro_image_io/v6/include"
    "${V6P2_REPO}/lib/astro_image_io/include"
    "${V6P2_REPO}/lib/astro_image_io/src"
    "${V6P2_REPO}/lib/astro_image_io/src/hips"
    "${V6P2_REPO}/lib/astro_image_io/third_party/cfitsio"
    "${V6P2_REPO}/third_party"
    "${V6P2_REPO}/lib/calibration/include"
    "${V6P2_REPO}/lib/dynamic_psf/include"
    "${V6P2_REPO}/lib/snr_estimator/include"
    "${V6P2_REPO}/lib/photometric_calib/include"
    "${V6P2_REPO}/lib/healpix_db/healpix_drizzle"
    "${V6P2_REPO}/lib/common"
    "${V6P2_REPO}/lib/common/healpix"
    "${V6P2_REPO}/lib/common/crypto")

  target_compile_definitions(astrocs_v6_phase2_integrate PRIVATE
    AIO_ENABLE_FITS AIO_ENABLE_XISF _REENTRANT)
  set_target_properties(astrocs_v6_phase2_integrate PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON)
  if(UNIX)
    target_link_libraries(astrocs_v6_phase2_integrate PUBLIC m z)
  endif()
endif()
