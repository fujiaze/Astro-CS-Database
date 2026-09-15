# ============================================================================
# IMPL-P1-DRZ-001 共址单元测试注册（write_scope: tests/unit/v6_p1_drz/）
#
# C-004.4：W5 各任务只允许在自身 write_scope 内注册测试；根/公共 CMakeLists
# 的注册由控制器在 W5 后以独立集成提交（RUNTIME-CI-001/W9）统一处理。
#
# 本文件既可被 tests/unit/CMakeLists.txt 以 add_subdirectory 引入，也可独立
# 构建（cmake -S tests/unit/v6_p1_drz -B <build>）以便任务自验。
#
# 依赖：lib/healpix_db/healpix_drizzle/{v6_drizzle_science,v6_spherical_overlap,
# spherical_overlap}.cpp + lib/common/healpix/healpix_core.cpp。
# CTest: v6_p1_drz_units / _oracle / _negative / _geometry
# ============================================================================
cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(v6_p1_drz_tests LANGUAGES CXX)
  enable_testing()
endif()

set(V6_P1_DRZ_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(V6_P1_DRZ_REPO ${V6_P1_DRZ_DIR}/../../..)
set(V6_P1_DRZ_IMPL ${V6_P1_DRZ_REPO}/lib/healpix_db/healpix_drizzle)

add_executable(v6_p1_drz_test
  ${V6_P1_DRZ_DIR}/v6_p1_drz_test.cpp
  ${V6_P1_DRZ_IMPL}/v6_drizzle_science.cpp
  ${V6_P1_DRZ_IMPL}/v6_spherical_overlap.cpp
  ${V6_P1_DRZ_IMPL}/spherical_overlap.cpp
  ${V6_P1_DRZ_REPO}/lib/common/healpix/healpix_core.cpp)
target_include_directories(v6_p1_drz_test PRIVATE
  ${V6_P1_DRZ_IMPL}
  ${V6_P1_DRZ_REPO}/lib/common
  ${V6_P1_DRZ_REPO}/lib/common/healpix)
target_compile_features(v6_p1_drz_test PRIVATE cxx_std_17)

add_test(NAME v6_p1_drz_units COMMAND v6_p1_drz_test units)
add_test(NAME v6_p1_drz_oracle COMMAND v6_p1_drz_test oracle)
add_test(NAME v6_p1_drz_negative COMMAND v6_p1_drz_test negative)
add_test(NAME v6_p1_drz_geometry COMMAND v6_p1_drz_test geometry)
