# ============================================================================
# ACSD V6 Phase1 单帧产品装配库 (P1-INTEGRATE-001, Wave 7)
# 写域: lib/algorithms/integration/phase1_product/。本层只"接线" Wave 5 生产源，不复制科学公式。
# 根/公共 CMakeLists 的注册归 RUNTIME-CI-001 (C-004.4)；本文件自包含可独立
# 构建（cmake -S lib/algorithms/integration/phase1_product -B <build>），也可被上层 add_subdirectory 复用。
# ============================================================================
cmake_minimum_required(VERSION 3.16)

if(NOT TARGET astrocs_phase1_product)
  set(P1_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../../../")
  get_filename_component(P1_REPO "${P1_REPO}" ABSOLUTE)

  add_library(astrocs_phase1_product STATIC
    # 本层装配
    src/phase1_product.cpp
    # IMPL-AIO-001
    "${P1_REPO}/lib/infrastructure/aio/product_io/src/sha256.cpp"
    "${P1_REPO}/lib/infrastructure/aio/product_io/src/bunit.cpp"
    "${P1_REPO}/lib/infrastructure/aio/product_io/src/fits.cpp"
    "${P1_REPO}/lib/infrastructure/aio/product_io/src/atomic_publish.cpp"
    "${P1_REPO}/lib/infrastructure/aio/product_io/src/provenance.cpp"
    "${P1_REPO}/lib/infrastructure/aio/product_io/src/hips_manifest.cpp"
    "${P1_REPO}/lib/infrastructure/aio/product_io/src/product_io.cpp"
    # IMPL-P1-CAL-001
    "${P1_REPO}/lib/algorithms/calibration/src/calibration_covariance.cpp"
    # IMPL-P1-PSFW-001
    "${P1_REPO}/lib/algorithms/psf/src/psf_information.cpp"
    "${P1_REPO}/lib/algorithms/noise_snr/cpp/src/information_weight.cpp"
    "${P1_REPO}/lib/algorithms/photometry/cpp/src/psfsw.cpp"
    # IMPL-P1-DRZ-001
    "${P1_REPO}/lib/algorithms/drizzle/healpix_drizzle/drizzle_science.cpp"
    "${P1_REPO}/lib/algorithms/drizzle/healpix_drizzle/spherical_overlap_science.cpp"
    "${P1_REPO}/lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp"
    "${P1_REPO}/lib/algorithms/shared/healpix/healpix_core.cpp")

  target_include_directories(astrocs_phase1_product PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${P1_REPO}/lib/infrastructure/aio/product_io/include"
    # CLEAN-403: phase1_product.cpp 的文本读写经 aio 机制原语
    # (aio_atomic_file.h / aio_file_io.h, header-only)。
    "${P1_REPO}/lib/infrastructure/aio/src"
    # WIN-PORT 缺陷⑤: 同上 —— nlohmann/json.hpp 在 lib/third_party/ 下;
    # 原 ${P1_REPO}/third_party 是根目录整合后遗留的死路径 ⇒ MSVC C1083。
    "${P1_REPO}/lib/third_party"
    "${P1_REPO}/lib/algorithms/calibration/include"
    "${P1_REPO}/lib/algorithms/psf/include"
    "${P1_REPO}/lib/algorithms/noise_snr/include"
    "${P1_REPO}/lib/algorithms/photometry/include"
    "${P1_REPO}/lib/algorithms/drizzle/healpix_drizzle"
    "${P1_REPO}/lib/algorithms/shared"
    "${P1_REPO}/lib/algorithms/shared/healpix")

  set_target_properties(astrocs_phase1_product PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON)
  if(UNIX)
    target_link_libraries(astrocs_phase1_product PUBLIC m)
  endif()
endif()
