# ============================================================================
# P2-INTEGRATE-001 集成测试注册（write_scope: eng/tests/integration/v6_p2/）
# C-004.4：根/公共 CMakeLists 由 RUNTIME-CI-001/W9 统一注册；本文件自包含可
# 独立构建: cmake -S eng/tests/integration/v6_p2 -B build/v6_p2_int && ctest --test-dir build/v6_p2_int
# ============================================================================
cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(astrocs_v6_p2_integrate LANGUAGES C CXX)
  enable_testing()
endif()

set(V6P2_IT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(V6P2_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
get_filename_component(V6P2_REPO "${V6P2_REPO}" ABSOLUTE)

add_subdirectory("${V6P2_REPO}/lib/algorithms/integration/v6"
                 "${CMAKE_CURRENT_BINARY_DIR}/v6_phase2_int_lib")

add_executable(v6_p2_integrate_test "${V6P2_IT_DIR}/v6_p2_integrate_test.cpp")
target_link_libraries(v6_p2_integrate_test PRIVATE astrocs_v6_phase2_integrate)
set_target_properties(v6_p2_integrate_test PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

set(V6P2_WORK "${CMAKE_CURRENT_BINARY_DIR}/work")
set(V6P2_PYTHON "python3" CACHE STRING "Python interpreter for the independent oracle")

add_test(NAME v6_p2_routing COMMAND v6_p2_integrate_test routing "${V6P2_WORK}")
add_test(NAME v6_p2_write COMMAND v6_p2_integrate_test write "${V6P2_WORK}")
add_test(NAME v6_p2_negative COMMAND v6_p2_integrate_test negative "${V6P2_WORK}")
add_test(NAME v6_p2_halfproduct COMMAND v6_p2_integrate_test halfproduct "${V6P2_WORK}")
set_tests_properties(v6_p2_routing PROPERTIES TIMEOUT 300)
set_tests_properties(v6_p2_write PROPERTIES TIMEOUT 900)
set_tests_properties(v6_p2_negative PROPERTIES TIMEOUT 900)
set_tests_properties(v6_p2_halfproduct PROPERTIES TIMEOUT 300)

add_test(NAME v6_p2_oracle
  COMMAND ${V6P2_PYTHON} "${V6P2_IT_DIR}/oracle/v6_p2_oracle.py"
          --workdir "${V6P2_WORK}" --repo "${V6P2_REPO}")
set_tests_properties(v6_p2_oracle PROPERTIES DEPENDS "v6_p2_write" TIMEOUT 600)
