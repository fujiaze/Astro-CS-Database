# lib/algorithms/fits_output/CMakeLists.txt — Phase3 FITS 写出模块 target 清单
#
# BLD-002: 唯一产品事实源 = 根 CMakeLists.txt (唯一 project / 唯一 add_executable(acsd))。
# 本文件由根 CMakeLists.txt 的 add_subdirectory(lib/algorithms/fits_output) 引入产品图。
#
# 目标:
#   astrocs_p3_fits_output — FITS 原子写 + 重开验证 (ALG-P3-FITS-IMPL-001)。
#   W4-A9 批次 3 (2026-09-17): p3_output.{h,cpp} 由 lib/phase3_session/ 迁入本目录
#   (ASTROCS_DESIGN §7.1「fits_output」行); 源逐字节等价, 命名空间 astrocs::phase3
#   与全部公共符号名不变。
#   依赖闭包 (均为本模块外部的既有 target, 无环):
#     astrocs_common            — SHA-256 (lib/algorithms/shared/crypto) + shared include 面
#     astrocs_aio               — astro_image_io.h / aio_cfitsio_mutex.h include 面 + AIO 符号
#     astrocs_cfitsio           — CFITSIO (fits_* 调用)
#     astrocs_p3_projection_wcs — p3_wcs_validate_request (批次 1 迁入 projection)
if(NOT TARGET astrocs_p3_fits_output)
  add_library(astrocs_p3_fits_output STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/p3_output.cpp
  )
  target_include_directories(astrocs_p3_fits_output PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(astrocs_p3_fits_output PUBLIC
    astrocs_common astrocs_aio astrocs_cfitsio astrocs_p3_projection_wcs)
  set_target_properties(astrocs_p3_fits_output PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
  )
endif()
