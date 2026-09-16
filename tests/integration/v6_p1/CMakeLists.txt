# ============================================================================
# P1-INTEGRATE-001 集成测试注册（write_scope: tests/integration/v6_p1/）
# C-004.4：根/公共 CMakeLists 由 RUNTIME-CI-001/W9 统一注册；本文件自包含可
# 独立构建: cmake -S tests/integration/v6_p1 -B build/v6_p1_int && ctest --test-dir build/v6_p1_int
# ============================================================================
cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(astrocs_v6_p1_integrate LANGUAGES CXX)
  enable_testing()
endif()

set(V6P1_IT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(V6P1_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
get_filename_component(V6P1_REPO "${V6P1_REPO}" ABSOLUTE)

add_subdirectory("${V6P1_REPO}/lib/algorithms/integration/v6_phase1"
                 "${CMAKE_CURRENT_BINARY_DIR}/v6_phase1_product_lib")

add_executable(v6_p1_integrate_test "${V6P1_IT_DIR}/v6_p1_integrate_test.cpp")
target_link_libraries(v6_p1_integrate_test PRIVATE astrocs_v6_phase1_product)
set_target_properties(v6_p1_integrate_test PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

set(V6P1_WORK "${CMAKE_CURRENT_BINARY_DIR}/work")
set(V6P1_PYTHON "python3" CACHE STRING "Python interpreter for the reopen oracle")

add_test(NAME v6_p1_positive COMMAND v6_p1_integrate_test positive "${V6P1_WORK}")
add_test(NAME v6_p1_group COMMAND v6_p1_integrate_test group "${V6P1_WORK}")
add_test(NAME v6_p1_negative COMMAND v6_p1_integrate_test negative "${V6P1_WORK}")
add_test(NAME v6_p1_halfproduct COMMAND v6_p1_integrate_test halfproduct "${V6P1_WORK}")
set_tests_properties(v6_p1_group PROPERTIES DEPENDS v6_p1_positive)
set_tests_properties(v6_p1_positive PROPERTIES TIMEOUT 900)
set_tests_properties(v6_p1_negative PROPERTIES TIMEOUT 900)
set_tests_properties(v6_p1_group PROPERTIES TIMEOUT 300)
set_tests_properties(v6_p1_halfproduct PROPERTIES TIMEOUT 300)

add_test(NAME v6_p1_schema_oracle
  COMMAND ${V6P1_PYTHON} "${V6P1_IT_DIR}/oracle/v6_p1_reopen_oracle.py"
          --workdir "${V6P1_WORK}" --repo "${V6P1_REPO}")
set_tests_properties(v6_p1_schema_oracle PROPERTIES
  DEPENDS "v6_p1_positive;v6_p1_group" TIMEOUT 300)
