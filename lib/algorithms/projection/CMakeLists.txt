# lib/algorithms/projection/CMakeLists.txt — projection 算法模块 target 清单
#
# BLD-002: 唯一产品事实源 = 根 CMakeLists.txt (唯一 project / 唯一 add_executable(acsd))。
# 本文件只声明本模块的算法库 target 与共址测试; 由根 CMakeLists.txt 的
# add_subdirectory(lib/algorithms/projection) 引入产品图 (INT-001 W4-A9 批次 1)。
#
# 目标归属:
#   acsd_p3_projection_wcs — TAN 投影/WCS 内核 (p3_wcs.cpp, ALG-P3-002)。
#     原址 lib/phase3_session/p3_wcs.{h,cpp}; W4-A9 按 docs/ACSD_DESIGN §7.1
#     「projection」行迁入本模块 (WCS 属投影域)。C++17 命名空间
#     acsd::phase3 与公共符号名保持不变 (架构重构不改科学语义,
#     ENGINEERING_SPEC §3)。
#   p3_projection.cpp / p3_proj.cpp 的 target 声明归 P3-002 线 (本批次未动)。
if(NOT TARGET acsd_p3_projection_wcs)
  # 产品声明注册表 p3_projection_registry.h 为 header-only(inline),
  # p3_wcs.cpp 直接包含 —— 请求面门(p3_wcs_validate_request)与构造面门
  # (p3_wcs_make) 共用同一份注册表数据, 不产生第二权威, 也不新增链接面
  # (p3_wcs.cpp 另被 eng/tests/unit 以同 TU 直编方式复用)。
  add_library(acsd_p3_projection_wcs STATIC ${CMAKE_CURRENT_SOURCE_DIR}/p3_wcs.cpp)
  target_include_directories(acsd_p3_projection_wcs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})
  # 严格 warning 分层与根图 acsd_phase3_session 一致
  # (根 CMakeLists.txt 的 QA-001 foreach 之外, target-local 声明)。
  if(NOT MSVC)
    target_compile_options(acsd_p3_projection_wcs PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

# 共址可复用测试 (ENGINEERING_SPEC §4.6): 唯一注册点 = 根 CMakeLists.txt 的
# ACSD_BUILD_TESTS 段（enable_testing() 之后）—— add_subdirectory(
# lib/algorithms/projection/tests/p3wcs) 直接挂载本模块 tests/p3wcs/
# （p3_wcs_test / p3_projection_registry_test / unsupported CLI 负例）。
# P-083 (台账 B8): 此前此处是一段恒假死守卫 —— EXISTS 指向不存在的
# <module>/eng/tests/CMakeLists.txt，且 add_subdirectory(tests) 指向无
# CMakeLists.txt 的 tests/（真体在 tests/p3wcs/），双重死代码。不得在此
# 重复挂载: 同一源目录只允许 add_subdirectory 一次（根注释同义），且
# tests/p3wcs 依赖根图 target（acsd / acsd_common），独立 configure 下
# 本就无法成立 —— 故按台账 fix「删死代码」分支移除，防后续误激活。
# P-083 修复注记：本模块唯一测试注册点 = 根 CMakeLists.txt:1122 的
# add_subdirectory(lib/algorithms/projection/tests/p3wcs)。此处一律不挂载：
# 模块 CMakeLists 求值早于根图注册 p3_wcs_test（NOT TARGET 恒真），
# 任何条件挂载都会与根图形成同源目录双挂（binary dir 冲突）。
