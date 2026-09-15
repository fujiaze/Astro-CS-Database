# tests/unit/v6_p3_rsmp/CMakeLists.txt — IMPL-P3-RSMP-001 共址测试注册
#
# 只注册本任务 write_scope 内目标；根/公共 CMakeLists 注册由控制器统一处理
# （控制包 CONTROLLER_LOG C-004.4；CTRL-AR033）。本文件可被
# add_subdirectory(tests/unit/v6_p3_rsmp) 或独立 cmake -S 直接使用
# （与 IMPL-P3-PROJ-001 的 tests/unit/v6_p3_proj/CMakeLists.txt 同构）。
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.16)
  project(v6_p3_rsmp_tests CXX)
  enable_testing()
endif()

set(V6_P3RSMP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/phase3_rsmp)
set(V6_P3RSMP_SOURCES
  ${V6_P3RSMP_DIR}/p3_rsmp_units.cpp
  ${V6_P3RSMP_DIR}/p3_rsmp_kernel_registry.cpp
  ${V6_P3RSMP_DIR}/p3_rsmp_operator.cpp
  ${V6_P3RSMP_DIR}/p3_rsmp_covariance.cpp
  ${V6_P3RSMP_DIR}/p3_rsmp_propagation.cpp
  ${V6_P3RSMP_DIR}/p3_rsmp_failclosed.cpp
)

# 生产源直接编入各测试可执行（自足；不依赖 astrocs_p3_rsmp target，
# 使本目录既可被根 add_subdirectory 也可独立 configure）。
function(v6_p3rsmp_test name src)
  add_executable(${name} ${src} ${V6_P3RSMP_SOURCES})
  target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${V6_P3RSMP_DIR})
  target_compile_features(${name} PRIVATE cxx_std_17)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

v6_p3rsmp_test(p3_rsmp_core_test p3_rsmp_core_test.cpp)
v6_p3rsmp_test(p3_rsmp_oracle_test p3_rsmp_oracle_test.cpp)
v6_p3rsmp_test(p3_rsmp_gate_test p3_rsmp_gate_test.cpp)

# 负向 mutation 驱动（20 条注入 → 测试必红）不进 ctest 默认面（耗时）；
# 由 tests/unit/v6_p3_rsmp/run_verification.sh 调用，证据落 run/v6/p3-rsmp/。
option(V6_P3RSMP_ENABLE_MUTATION_DRIVER "register mutation driver as a ctest case" OFF)
if(V6_P3RSMP_ENABLE_MUTATION_DRIVER)
  find_program(PYTHON3_EXECUTABLE python3)
  if(PYTHON3_EXECUTABLE)
    add_test(NAME v6_p3_rsmp_mutation_driver
      COMMAND ${PYTHON3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_mutations.py)
  endif()
endif()
