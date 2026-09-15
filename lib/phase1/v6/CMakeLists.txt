# ============================================================================
# AstroCS V6 Phase1 单帧产品装配库 (P1-INTEGRATE-001, Wave 7)
# 写域: lib/phase1/v6/。本层只"接线" Wave 5 生产源，不复制科学公式。
# 根/公共 CMakeLists 的注册归 RUNTIME-CI-001/W9 (C-004.4)；本文件自包含可独立
# 构建（cmake -S lib/phase1/v6 -B <build>），也可被上层 add_subdirectory 复用。
# ============================================================================
cmake_minimum_required(VERSION 3.16)

if(NOT TARGET astrocs_v6_phase1_product)
  set(V6P1_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
  get_filename_component(V6P1_REPO "${V6P1_REPO}" ABSOLUTE)

  add_library(astrocs_v6_phase1_product STATIC
    # 本层装配
    src/phase1_product.cpp
    # IMPL-AIO-001
    "${V6P1_REPO}/lib/astro_image_io/v6/src/v6_sha256.cpp"
    "${V6P1_REPO}/lib/astro_image_io/v6/src/v6_bunit.cpp"
    "${V6P1_REPO}/lib/astro_image_io/v6/src/v6_fits.cpp"
    "${V6P1_REPO}/lib/astro_image_io/v6/src/v6_atomic_publish.cpp"
    "${V6P1_REPO}/lib/astro_image_io/v6/src/v6_provenance.cpp"
    "${V6P1_REPO}/lib/astro_image_io/v6/src/v6_hips_manifest.cpp"
    "${V6P1_REPO}/lib/astro_image_io/v6/src/v6_product_io.cpp"
    # IMPL-P1-CAL-001
    "${V6P1_REPO}/lib/calibration/src/v6_calibration_covariance.cpp"
    # IMPL-P1-PSFW-001
    "${V6P1_REPO}/lib/dynamic_psf/src/psf_information.cpp"
    "${V6P1_REPO}/lib/snr_estimator/cpp/src/information_weight.cpp"
    "${V6P1_REPO}/lib/photometric_calib/cpp/src/psfsw.cpp"
    # IMPL-P1-DRZ-001
    "${V6P1_REPO}/lib/healpix_db/healpix_drizzle/v6_drizzle_science.cpp"
    "${V6P1_REPO}/lib/healpix_db/healpix_drizzle/v6_spherical_overlap.cpp"
    "${V6P1_REPO}/lib/healpix_db/healpix_drizzle/spherical_overlap.cpp"
    "${V6P1_REPO}/lib/common/healpix/healpix_core.cpp")

  target_include_directories(astrocs_v6_phase1_product PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${V6P1_REPO}/lib/astro_image_io/v6/include"
    "${V6P1_REPO}/third_party"
    "${V6P1_REPO}/lib/calibration/include"
    "${V6P1_REPO}/lib/dynamic_psf/include"
    "${V6P1_REPO}/lib/snr_estimator/include"
    "${V6P1_REPO}/lib/photometric_calib/include"
    "${V6P1_REPO}/lib/healpix_db/healpix_drizzle"
    "${V6P1_REPO}/lib/common"
    "${V6P1_REPO}/lib/common/healpix")

  set_target_properties(astrocs_v6_phase1_product PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON)
  if(UNIX)
    target_link_libraries(astrocs_v6_phase1_product PUBLIC m)
  endif()
endif()
