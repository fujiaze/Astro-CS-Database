# lib/algorithms/resample/CMakeLists.txt — Phase3 重采样模块 target 清单
#
# BLD-002: 唯一产品事实源 = 根 CMakeLists.txt (唯一 project / 唯一 add_executable(astrocs))。
# 本文件由根 CMakeLists.txt 的 add_subdirectory(lib/algorithms/resample) 引入产品图。
#
# 目标:
#   astrocs_p3_rsmp — 本模块全部生产源:
#     · V6 三模式重采样传播 (IMPL-P3-RSMP-001): p3_rsmp_*.cpp
#     · 叶级重采样内核 (ALG-P3-003): p3_resample.cpp
#
# 迁移沿革 (W4-A9 批次 2 → W8/批次 4):
#   批次 2 (2026-09-17): p3_resample.{h,cpp} 文件由 lib/phase3_session/ 迁入本目录
#     (ASTROCS_DESIGN §7.1「resample」行), 但编译单元暂留 astrocs_phase3_session ——
#     该源依赖 hips_properties_parse/hips_product_validate (时在会话库内) 与
#     aio_hips_* (astrocs_hips), 直接并入本库会成 astrocs_p3_rsmp ⇄ astrocs_phase3_session 环。
#   批次 4 (W8): 解析器已独立成库 astrocs_hips_properties (根 CMakeLists.txt 声明, 源在
#     lib/algorithms/coverage/; 该库不依赖任何 phase3 目标 ⇒ 环解除), 本库随之收编
#     p3_resample.cpp —— 模块自持其全部生产源, 会话库不再编译本模块源。
if(NOT TARGET astrocs_p3_rsmp)
  add_library(astrocs_p3_rsmp STATIC
    # V6 三模式传播 (IMPL-P3-RSMP-001)
    ${CMAKE_CURRENT_SOURCE_DIR}/p3_rsmp_units.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/p3_rsmp_kernel_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/p3_rsmp_operator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/p3_rsmp_covariance.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/p3_rsmp_propagation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/p3_rsmp_failclosed.cpp
    # 叶级重采样内核 (ALG-P3-003; 批次 4 归位)
    ${CMAKE_CURRENT_SOURCE_DIR}/p3_resample.cpp
  )
  target_include_directories(astrocs_p3_rsmp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  # hips_properties.h: 经 astrocs_hips_properties 的 PUBLIC include 面 (coverage 目录)
  # aio_hips_reader.h / astro_image_io.h / aio_cfitsio_mutex.h: 经 astrocs_aio (PUBLIC 面)
  # aio_hips_* 符号: astrocs_hips; healpix_core.h: astrocs_common; fitsio 与 zlib:
  # 由 astrocs_aio 的 PUBLIC 面传递 (aio PUBLIC astrocs_cfitsio z)。
  # 不在此重复直列 cfitsio —— GCC ld 单向解析, 直列会把 cfitsio 排到 -lz 之后,
  # zlib 符号 (inflate/deflate) 无法回填 (实测 4 个 Phase3 单测链接失败)。
  target_link_libraries(astrocs_p3_rsmp PUBLIC
    astrocs_common astrocs_aio astrocs_hips astrocs_hips_properties)
  set_target_properties(astrocs_p3_rsmp PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
  )
endif()
