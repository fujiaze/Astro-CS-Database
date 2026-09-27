# ============================================================================
# IMPL-P1-DRZ-001 共址单元测试注册（write_scope: eng/tests/unit/p1_drz/）
#
# C-004.4：W5 各任务只允许在自身 write_scope 内注册测试；根/公共 CMakeLists
# 的注册由控制器在 W5 后以独立集成提交（RUNTIME-CI-001/W9）统一处理。
#
# 本文件既可被 eng/tests/unit/CMakeLists.txt 以 add_subdirectory 引入，也可独立
# 构建（cmake -S eng/tests/unit/p1_drz -B <build>）以便任务自验。
#
# 依赖：lib/algorithms/drizzle/healpix_drizzle/{drizzle_science,spherical_overlap_science,
# spherical_overlap}.cpp + lib/algorithms/shared/healpix/healpix_core.cpp。
# CTest: p1_drz_units / _oracle / _negative / _geometry
# ============================================================================
cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(p1_drz_tests LANGUAGES CXX)
  enable_testing()
endif()

set(V6_P1_DRZ_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(V6_P1_DRZ_REPO ${V6_P1_DRZ_DIR}/../../../..)
set(V6_P1_DRZ_IMPL ${V6_P1_DRZ_REPO}/lib/algorithms/drizzle/healpix_drizzle)

add_executable(p1_drz_test
  ${V6_P1_DRZ_DIR}/p1_drz_test.cpp
  ${V6_P1_DRZ_IMPL}/drizzle_science.cpp
  ${V6_P1_DRZ_IMPL}/spherical_overlap_science.cpp
  ${V6_P1_DRZ_IMPL}/spherical_overlap.cpp
  ${V6_P1_DRZ_REPO}/lib/algorithms/shared/healpix/healpix_core.cpp)
target_include_directories(p1_drz_test PRIVATE
  ${V6_P1_DRZ_IMPL}
  ${V6_P1_DRZ_REPO}/lib/algorithms/shared
  ${V6_P1_DRZ_REPO}/lib/algorithms/shared/healpix)
target_compile_features(p1_drz_test PRIVATE cxx_std_17)

add_test(NAME p1_drz_units COMMAND p1_drz_test units)
add_test(NAME p1_drz_oracle COMMAND p1_drz_test oracle)
add_test(NAME p1_drz_negative COMMAND p1_drz_test negative)
add_test(NAME p1_drz_geometry COMMAND p1_drz_test geometry)
# DRZ-PF-CORRECT-01 (S1 第 19 条) 的注册别名: 同一可执行 + 同一 geometry 组
# （含闭合亏损负例与「注入一个面积无效像素必判红」用例）。名字与 drizzle 侧
# 的 CHK-DRZ-PF-AREA-S1 门正则 ^drizzle_pf_sb.*$ 同族, 使单条注册项即可一次
# 覆盖两条 S1 的正例+负例; 不新增判据、不复制断言。
add_test(NAME drizzle_pf_sb_area_invalid COMMAND p1_drz_test geometry)
