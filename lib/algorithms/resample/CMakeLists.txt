# lib/algorithms/resample/CMakeLists.txt — Phase3 重采样模块 target 清单
#
# BLD-002: 唯一产品事实源 = 根 CMakeLists.txt (唯一 project / 唯一 add_executable(astrocs))。
# 本文件由根 CMakeLists.txt 的 add_subdirectory(lib/algorithms/resample) 引入产品图。
#
# 目标:
#   astrocs_p3_rsmp — V6 三模式重采样传播 (IMPL-P3-RSMP-001)。
#
# W4-A9 批次 2 迁移注记 (2026-09-17):
#   p3_resample.{h,cpp} (ALG-P3-003 叶级重采样: order 选择 / tile LRU 缓存 /
#   nearest+bilinear / coverage-NaN 语义) 的**文件**已由 lib/phase3_session/ 迁入
#   本目录 (ASTROCS_DESIGN §7.1「resample」行)。但其**编译单元成员关系**暂留
#   astrocs_phase3_session (根 CMakeLists.txt 源列表已按新路径登记):
#     p3_resample.cpp 调用 hips_properties_parse/hips_product_validate
#     (lib/phase3_session/hips_properties.cpp) 与 aio_hips_* (astrocs_hips);
#     若在本模块单独成库, 本库 ⇄ astrocs_phase3_session 将成链接环。前台裁决
#     hips_properties* 于批次 4 迁 lib/algorithms/coverage/, 届时解析器独立成库,
#     p3_resample.cpp 的源列表随本 target 收口 (本文件届时增列该源)。
if(NOT TARGET astrocs_p3_rsmp)
  add_library(astrocs_p3_rsmp STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/p3_rsmp_units.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/p3_rsmp_kernel_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/p3_rsmp_operator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/p3_rsmp_covariance.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/p3_rsmp_propagation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/p3_rsmp_failclosed.cpp
  )
  target_include_directories(astrocs_p3_rsmp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  set_target_properties(astrocs_p3_rsmp PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
  )
endif()
