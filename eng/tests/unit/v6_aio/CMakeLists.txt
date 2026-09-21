# ============================================================================
# IMPL-AIO-001 共址单元测试 (eng/tests/unit/v6_aio) — 独立可构建的 ctest 面
# 可独立: cmake -S eng/tests/unit/v6_aio -B build/v6_aio && ctest --test-dir build/v6_aio
# 也可被根构建 add_subdirectory 复用 (不重复 project/enable_testing)。
# AIO_V6_LIB_DIR 可覆盖被测源码根 (负向 mutation harness 使用)。
# ============================================================================
cmake_minimum_required(VERSION 3.16)

set(AIO_V6_STANDALONE FALSE)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(AIO_V6_STANDALONE TRUE)
  project(astrocs_v6_aio_tests CXX)
endif()

if(NOT AIO_V6_LIB_DIR)
  set(AIO_V6_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib//infrastructure/aio/v6")
endif()
get_filename_component(AIO_V6_LIB_DIR "${AIO_V6_LIB_DIR}" ABSOLUTE)
get_filename_component(AIO_V6_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)

add_subdirectory("${AIO_V6_LIB_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/v6_aio_lib")

add_executable(v6_aio_test v6_aio_test.cpp)
target_link_libraries(v6_aio_test PRIVATE astrocs_v6_aio)
if(UNIX)
  target_link_libraries(v6_aio_test PRIVATE m)
endif()

if(AIO_V6_STANDALONE)
  enable_testing()
endif()

set(AIO_V6_ARTIFACT_DIR "${CMAKE_CURRENT_BINARY_DIR}/artifacts")
set(AIO_V6_PYTHON "python3")

add_test(NAME v6_aio_units COMMAND v6_aio_test units)
add_test(NAME v6_aio_provenance COMMAND v6_aio_test provenance)
add_test(NAME v6_aio_manifest COMMAND v6_aio_test manifest)
add_test(NAME v6_aio_fits COMMAND v6_aio_test fits "${CMAKE_CURRENT_BINARY_DIR}/fits_work")
add_test(NAME v6_aio_atomic COMMAND v6_aio_test atomic "${CMAKE_CURRENT_BINARY_DIR}/atomic_work")

add_test(NAME v6_aio_artifacts COMMAND v6_aio_test artifacts "${AIO_V6_ARTIFACT_DIR}")
set_tests_properties(v6_aio_artifacts PROPERTIES FIXTURES_SETUP v6_aio_artifacts)

add_test(NAME v6_aio_oracle
  COMMAND ${AIO_V6_PYTHON} "${CMAKE_CURRENT_SOURCE_DIR}/oracle/v6_aio_oracle.py"
          "${AIO_V6_ARTIFACT_DIR}" "${AIO_V6_REPO_ROOT}")
set_tests_properties(v6_aio_oracle PROPERTIES FIXTURES_REQUIRED v6_aio_artifacts)

add_test(NAME v6_aio_oracle_negative
  COMMAND ${AIO_V6_PYTHON} "${CMAKE_CURRENT_SOURCE_DIR}/oracle/v6_aio_oracle_negative.py"
          "${AIO_V6_ARTIFACT_DIR}" "${AIO_V6_REPO_ROOT}")
set_tests_properties(v6_aio_oracle_negative PROPERTIES FIXTURES_REQUIRED v6_aio_artifacts)

add_test(NAME v6_aio_impl_mutations
  COMMAND ${AIO_V6_PYTHON} "${CMAKE_CURRENT_SOURCE_DIR}/mutations/mutate_and_check.py"
          "${AIO_V6_REPO_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/mutations_work")
set_tests_properties(v6_aio_impl_mutations PROPERTIES TIMEOUT 2400 LABELS "v6_aio_slow")
