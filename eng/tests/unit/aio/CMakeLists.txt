# ============================================================================
# IMPL-AIO-001 共址单元测试 (eng/tests/unit/aio) — 独立可构建的 ctest 面
# 可独立: cmake -S eng/tests/unit/aio -B build/aio && ctest --test-dir build/aio
# 也可被根构建 add_subdirectory 复用 (不重复 project/enable_testing)。
# AIO_PRODUCT_IO_LIB_DIR 可覆盖被测源码根 (负向 mutation harness 使用)。
# ============================================================================
cmake_minimum_required(VERSION 3.16)

set(AIO_PRODUCT_IO_STANDALONE FALSE)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(AIO_PRODUCT_IO_STANDALONE TRUE)
  project(astrocs_product_io_tests CXX)
endif()

if(NOT AIO_PRODUCT_IO_LIB_DIR)
  set(AIO_PRODUCT_IO_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/infrastructure/aio/product_io")
endif()
get_filename_component(AIO_PRODUCT_IO_LIB_DIR "${AIO_PRODUCT_IO_LIB_DIR}" ABSOLUTE)
get_filename_component(AIO_PRODUCT_IO_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)

add_subdirectory("${AIO_PRODUCT_IO_LIB_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/product_io_lib")

add_executable(aio_test aio_test.cpp)
target_link_libraries(aio_test PRIVATE astrocs_product_io)
if(UNIX)
  target_link_libraries(aio_test PRIVATE m)
endif()

if(AIO_PRODUCT_IO_STANDALONE)
  enable_testing()
endif()

set(AIO_PRODUCT_IO_ARTIFACT_DIR "${CMAKE_CURRENT_BINARY_DIR}/artifacts")
set(AIO_PRODUCT_IO_PYTHON "python3")

add_test(NAME aio_units COMMAND aio_test units)
add_test(NAME aio_provenance COMMAND aio_test provenance)
add_test(NAME aio_manifest COMMAND aio_test manifest)
add_test(NAME aio_fits COMMAND aio_test fits "${CMAKE_CURRENT_BINARY_DIR}/fits_work")
add_test(NAME aio_atomic COMMAND aio_test atomic "${CMAKE_CURRENT_BINARY_DIR}/atomic_work")

add_test(NAME aio_artifacts COMMAND aio_test artifacts "${AIO_PRODUCT_IO_ARTIFACT_DIR}")
set_tests_properties(aio_artifacts PROPERTIES FIXTURES_SETUP aio_artifacts)

add_test(NAME aio_oracle
  COMMAND ${AIO_PRODUCT_IO_PYTHON} "${CMAKE_CURRENT_SOURCE_DIR}/oracle/aio_oracle.py"
          "${AIO_PRODUCT_IO_ARTIFACT_DIR}" "${AIO_PRODUCT_IO_REPO_ROOT}")
set_tests_properties(aio_oracle PROPERTIES FIXTURES_REQUIRED aio_artifacts)

add_test(NAME aio_oracle_negative
  COMMAND ${AIO_PRODUCT_IO_PYTHON} "${CMAKE_CURRENT_SOURCE_DIR}/oracle/aio_oracle_negative.py"
          "${AIO_PRODUCT_IO_ARTIFACT_DIR}" "${AIO_PRODUCT_IO_REPO_ROOT}")
set_tests_properties(aio_oracle_negative PROPERTIES FIXTURES_REQUIRED aio_artifacts)

add_test(NAME aio_impl_mutations
  COMMAND ${AIO_PRODUCT_IO_PYTHON} "${CMAKE_CURRENT_SOURCE_DIR}/mutations/mutate_and_check.py"
          "${AIO_PRODUCT_IO_REPO_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/mutations_work")
set_tests_properties(aio_impl_mutations PROPERTIES TIMEOUT 2400 LABELS "aio_slow")
