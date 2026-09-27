# ============================================================================
# P1-INTEGRATE-001 集成测试注册（write_scope: eng/tests/integration/p1_integrate/）
# C-004.4：根/公共 CMakeLists 由 RUNTIME-CI-001/W9 统一注册；本文件自包含可
# 独立构建: cmake -S eng/tests/integration/p1_integrate -B build/p1_int && ctest --test-dir build/p1_int
# ============================================================================
cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(astrocs_p1_integrate LANGUAGES CXX)
  enable_testing()
endif()

set(P1_IT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(P1_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
get_filename_component(P1_REPO "${P1_REPO}" ABSOLUTE)

add_subdirectory("${P1_REPO}/lib/algorithms/integration/phase1_product"
                 "${CMAKE_CURRENT_BINARY_DIR}/phase1_product_lib")

add_executable(p1_integrate_test "${P1_IT_DIR}/p1_integrate_test.cpp")
target_link_libraries(p1_integrate_test PRIVATE astrocs_phase1_product)
set_target_properties(p1_integrate_test PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

set(P1_WORK "${CMAKE_CURRENT_BINARY_DIR}/work")
set(P1_PYTHON "python3" CACHE STRING "Python interpreter for the reopen oracle")

add_test(NAME p1_positive COMMAND p1_integrate_test positive "${P1_WORK}")
add_test(NAME p1_group COMMAND p1_integrate_test group "${P1_WORK}")
add_test(NAME p1_negative COMMAND p1_integrate_test negative "${P1_WORK}")
add_test(NAME p1_halfproduct COMMAND p1_integrate_test halfproduct "${P1_WORK}")
set_tests_properties(p1_group PROPERTIES DEPENDS p1_positive)
set_tests_properties(p1_positive PROPERTIES TIMEOUT 900)
set_tests_properties(p1_negative PROPERTIES TIMEOUT 900)
set_tests_properties(p1_group PROPERTIES TIMEOUT 300)
set_tests_properties(p1_halfproduct PROPERTIES TIMEOUT 300)

add_test(NAME p1_schema_oracle
  COMMAND ${P1_PYTHON} "${P1_IT_DIR}/oracle/p1_reopen_oracle.py"
          --workdir "${P1_WORK}" --repo "${P1_REPO}")
set_tests_properties(p1_schema_oracle PROPERTIES
  DEPENDS "p1_positive;p1_group" TIMEOUT 300)
