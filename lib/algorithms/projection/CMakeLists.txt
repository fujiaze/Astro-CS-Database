# lib/algorithms/projection/CMakeLists.txt — projection 算法模块 target 清单
#
# BLD-002: 唯一产品事实源 = 根 CMakeLists.txt (唯一 project / 唯一 add_executable(astrocs))。
# 本文件只声明本模块的算法库 target 与共址测试; 由根 CMakeLists.txt 的
# add_subdirectory(lib/algorithms/projection) 引入产品图 (INT-001 W4-A9 批次 1)。
#
# 目标归属:
#   astrocs_p3_projection_wcs — TAN 投影/WCS 内核 (p3_wcs.cpp, ALG-P3-002)。
#     原址 lib/phase3_session/p3_wcs.{h,cpp}; W4-A9 按 ASTROCS_DESIGN §7.1
#     「projection」行迁入本模块 (WCS 属投影域)。C++17 命名空间
#     astrocs::phase3 与公共符号名保持不变 (架构重构不改科学语义,
#     ENGINEERING_SPEC §3)。
#   p3_projection.cpp / p3_proj_v6.cpp 的 target 声明归 P3-002 线 (本批次未动)。
if(NOT TARGET astrocs_p3_projection_wcs)
  add_library(astrocs_p3_projection_wcs STATIC ${CMAKE_CURRENT_SOURCE_DIR}/p3_wcs.cpp)
  target_include_directories(astrocs_p3_projection_wcs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})
  # 严格 warning 分层与根图 astrocs_phase3_session 一致
  # (根 CMakeLists.txt 的 QA-001 foreach 之外, target-local 声明)。
  if(NOT MSVC)
    target_compile_options(astrocs_p3_projection_wcs PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

# 共址可复用测试 (ENGINEERING_SPEC §4.6): 仅根图 (ASTROCS_BUILD_TESTS=ON) 注册,
# 模块独立 configure 时不新增测试面。
if(ASTROCS_BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt)
  add_subdirectory(tests)
endif()
