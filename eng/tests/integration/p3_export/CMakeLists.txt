# eng/tests/integration/p3_export/CMakeLists.txt — P3-INTEGRATE-001 端点测试注册
#
# 只注册本任务 write_scope 内目标；根/公共 CMakeLists 注册由控制器统一处理
# （控制包 CONTROLLER_LOG C-004.4 / C-007 AR-033；W9 RUNTIME-CI-001 统一注册）。
# 本文件既可被根 add_subdirectory(eng/tests/integration/p3_export)，也可
# cmake -S eng/tests/integration/p3_export -B <build> 独立 configure。
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.16)
  project(p3_integration_tests CXX)
  enable_testing()
endif()

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(P3SESS_DIR  "${REPO_ROOT}/lib/phase3_session")
set(P3PROJ_DIR  "${REPO_ROOT}/lib/algorithms/projection")
set(P3RSMP_DIR  "${REPO_ROOT}/lib/algorithms/resample")
set(AIO_PRODUCT_IO_DIR  "${REPO_ROOT}/lib/infrastructure/aio/product_io")
# WIN-PORT 缺陷⑤: 第三方头唯一落点 = lib/third_party（根目录整合后无根 third_party/）。
set(JSON_DIR    "${REPO_ROOT}/lib/third_party")

# AIO v6 生产库（IMPL-AIO-001 提供；不修改其源码，仅链接）。
if(NOT TARGET astrocs_product_io)
  add_subdirectory("${AIO_PRODUCT_IO_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/product_io_lib")
endif()

set(P3_RSMP_SOURCES
  ${P3RSMP_DIR}/p3_rsmp_units.cpp
  ${P3RSMP_DIR}/p3_rsmp_kernel_registry.cpp
  ${P3RSMP_DIR}/p3_rsmp_operator.cpp
  ${P3RSMP_DIR}/p3_rsmp_covariance.cpp
  ${P3RSMP_DIR}/p3_rsmp_propagation.cpp
  ${P3RSMP_DIR}/p3_rsmp_failclosed.cpp
)

add_executable(p3_export_test
  p3_export_e2e_test.cpp
  ${P3SESS_DIR}/p3_export.cpp
  ${P3PROJ_DIR}/p3_proj.cpp
  ${P3_RSMP_SOURCES}
)
target_include_directories(p3_export_test PRIVATE
  ${P3SESS_DIR}
  ${P3PROJ_DIR}
  ${P3RSMP_DIR}
  ${AIO_PRODUCT_IO_DIR}/include
  ${JSON_DIR}
)
target_compile_features(p3_export_test PRIVATE cxx_std_17)
target_link_libraries(p3_export_test PRIVATE astrocs_product_io)
# 平台系统库面: 引用根 CMakeLists 唯一判定点声明的共用件 astrocs_platform_math。
# 本文件**可被独立 configure** (行 7 的「当前目录即工程根」守卫 + 独立 project()):
# 那种情况下没有根入口 ⇒ 共用件未定义, 直接写 astrocs_platform_math 会被 CMake 当
# 成**普通库名**展开成 -lastrocs_platform_math ⇒ 链接期 ld: cannot find。实测
# (FINAL-07): 独立 configure 后未守卫时 build rc=1, 链接行含 -lastrocs_platform_math。
# 故按「目标是否存在」守卫, 不按平台守卫 (平台判断仍只在共用件内部一处)。
# 注: 上一行的 astrocs_product_io 走 add_subdirectory 拉入, 其内部已自带同形守卫。
if(TARGET astrocs_platform_math)
  target_link_libraries(p3_export_test PRIVATE astrocs_platform_math)
endif()

set(P3_ARTIFACT_DIR "${CMAKE_CURRENT_BINARY_DIR}/artifacts")
set(P3_NEG_DIR      "${CMAKE_CURRENT_BINARY_DIR}/neg_artifacts")

add_test(NAME p3_export_positive
  COMMAND p3_export_test positive "${P3_ARTIFACT_DIR}")
set_tests_properties(p3_export_positive PROPERTIES FIXTURES_SETUP p3_artifacts)

add_test(NAME p3_export_negative
  COMMAND p3_export_test negative "${P3_NEG_DIR}")

find_program(PYTHON3_EXECUTABLE python3)
if(PYTHON3_EXECUTABLE)
  # 独立 Python Oracle：纯 stdlib FITS 解析 + CHECKSUM 重算 + provenance 生产 schema 门
  # + 冻结单位表反查。对 positive 产物做磁盘重开对照（非同实现自证）。
  add_test(NAME p3_export_oracle
    COMMAND ${PYTHON3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/p3_export_oracle.py
            "${P3_ARTIFACT_DIR}" "${REPO_ROOT}")
  set_tests_properties(p3_export_oracle PROPERTIES
    FIXTURES_REQUIRED p3_artifacts)
endif()
