# eng/tests/integration/v6_p3/CMakeLists.txt — P3-INTEGRATE-001 端点测试注册
#
# 只注册本任务 write_scope 内目标；根/公共 CMakeLists 注册由控制器统一处理
# （控制包 CONTROLLER_LOG C-004.4 / C-007 AR-033；W9 RUNTIME-CI-001 统一注册）。
# 本文件既可被根 add_subdirectory(eng/tests/integration/v6_p3)，也可
# cmake -S eng/tests/integration/v6_p3 -B <build> 独立 configure。
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.16)
  project(v6_p3_integration_tests CXX)
  enable_testing()
endif()

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(P3SESS_DIR  "${REPO_ROOT}/lib/phase3_session")
set(P3PROJ_DIR  "${REPO_ROOT}/lib/algorithms/projection")
set(P3RSMP_DIR  "${REPO_ROOT}/lib/algorithms/resample")
set(AIO_V6_DIR  "${REPO_ROOT}/lib/infrastructure/aio/v6")
set(JSON_DIR    "${REPO_ROOT}/third_party")

# AIO v6 生产库（IMPL-AIO-001 提供；不修改其源码，仅链接）。
if(NOT TARGET astrocs_v6_aio)
  add_subdirectory("${AIO_V6_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/v6_aio_lib")
endif()

set(V6_P3_RSMP_SOURCES
  ${P3RSMP_DIR}/p3_rsmp_units.cpp
  ${P3RSMP_DIR}/p3_rsmp_kernel_registry.cpp
  ${P3RSMP_DIR}/p3_rsmp_operator.cpp
  ${P3RSMP_DIR}/p3_rsmp_covariance.cpp
  ${P3RSMP_DIR}/p3_rsmp_propagation.cpp
  ${P3RSMP_DIR}/p3_rsmp_failclosed.cpp
)

add_executable(v6_p3_export_test
  p3_v6_export_e2e_test.cpp
  ${P3SESS_DIR}/p3_v6_export.cpp
  ${P3PROJ_DIR}/p3_proj_v6.cpp
  ${V6_P3_RSMP_SOURCES}
)
target_include_directories(v6_p3_export_test PRIVATE
  ${P3SESS_DIR}
  ${P3PROJ_DIR}
  ${P3RSMP_DIR}
  ${AIO_V6_DIR}/include
  ${JSON_DIR}
)
target_compile_features(v6_p3_export_test PRIVATE cxx_std_17)
target_link_libraries(v6_p3_export_test PRIVATE astrocs_v6_aio)
if(UNIX)
  target_link_libraries(v6_p3_export_test PRIVATE m)
endif()

set(V6_P3_ARTIFACT_DIR "${CMAKE_CURRENT_BINARY_DIR}/artifacts")
set(V6_P3_NEG_DIR      "${CMAKE_CURRENT_BINARY_DIR}/neg_artifacts")

add_test(NAME v6_p3_export_positive
  COMMAND v6_p3_export_test positive "${V6_P3_ARTIFACT_DIR}")
set_tests_properties(v6_p3_export_positive PROPERTIES FIXTURES_SETUP v6_p3_artifacts)

add_test(NAME v6_p3_export_negative
  COMMAND v6_p3_export_test negative "${V6_P3_NEG_DIR}")

find_program(PYTHON3_EXECUTABLE python3)
if(PYTHON3_EXECUTABLE)
  # 独立 Python Oracle：纯 stdlib FITS 解析 + CHECKSUM 重算 + provenance 生产 schema 门
  # + 冻结单位表反查。对 positive 产物做磁盘重开对照（非同实现自证）。
  add_test(NAME v6_p3_export_oracle
    COMMAND ${PYTHON3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/p3_v6_export_oracle.py
            "${V6_P3_ARTIFACT_DIR}" "${REPO_ROOT}")
  set_tests_properties(v6_p3_export_oracle PROPERTIES
    FIXTURES_REQUIRED v6_p3_artifacts)
endif()
