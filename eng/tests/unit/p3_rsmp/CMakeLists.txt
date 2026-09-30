# eng/tests/unit/p3_rsmp/CMakeLists.txt — IMPL-P3-RSMP-001 共址测试注册
#
# 只注册本任务 write_scope 内目标；根/公共 CMakeLists 注册由控制器统一处理
# （控制包 CONTROLLER_LOG C-004.4；CTRL-AR033）。本文件可被
# add_subdirectory(eng/tests/unit/p3_rsmp) 或独立 cmake -S 直接使用
# （与 IMPL-P3-PROJ-001 的 eng/tests/unit/p3_proj/CMakeLists.txt 同构）。
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.16)
  project(p3_rsmp_tests CXX)
  enable_testing()
endif()

set(V6_P3RSMP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib//algorithms/resample)
set(V6_P3RSMP_SOURCES
  ${V6_P3RSMP_DIR}/p3_rsmp_units.cpp
  ${V6_P3RSMP_DIR}/p3_rsmp_kernel_registry.cpp
  ${V6_P3RSMP_DIR}/p3_rsmp_operator.cpp
  ${V6_P3RSMP_DIR}/p3_rsmp_covariance.cpp
  ${V6_P3RSMP_DIR}/p3_rsmp_propagation.cpp
  ${V6_P3RSMP_DIR}/p3_rsmp_failclosed.cpp
)

# 生产源直接编入各测试可执行（自足；不依赖 acsd_p3_rsmp target，
# 使本目录既可被根 add_subdirectory 也可独立 configure）。
# 可执行名字保留 p3_rsmp_*_test；**ctest 用例名与可执行名同族（p3_rsmp_*）**
# （控制器 AR-034 处置）：与 eng/ci/checks.json 的 V6-CTEST-UNIT 期望 glob
# p3_rsmp_* 一致，且使 CTEST-REGISTRATION 静态检查能看到具体用例名。
function(p3rsmp_test name src)
  add_executable(${name} ${src} ${V6_P3RSMP_SOURCES})
  target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${V6_P3RSMP_DIR})
  target_compile_features(${name} PRIVATE cxx_std_17)
endfunction()

p3rsmp_test(p3_rsmp_core_test p3_rsmp_core_test.cpp)
add_test(NAME p3_rsmp_core COMMAND p3_rsmp_core_test)
p3rsmp_test(p3_rsmp_oracle_test p3_rsmp_oracle_test.cpp)
add_test(NAME p3_rsmp_oracle COMMAND p3_rsmp_oracle_test)
p3rsmp_test(p3_rsmp_gate_test p3_rsmp_gate_test.cpp)
add_test(NAME p3_rsmp_gate COMMAND p3_rsmp_gate_test)

# 负向 mutation 驱动（20 条注入 → 测试必红）自 FINAL-07 起进 ctest **默认面**。
# 前台裁决（FINAL-07；依据三条：①下方实测数据；②同族既有先例 aio_impl_mutations 默认 ON；
# ③负责人本轮授权「工程层由前台自决」）：默认关闭 → 默认开启。此前默认 OFF 不是「跑过没红」，而是驱动根本没有
# 任何执行单元：eng/ci/ctest_face.py 的面 B（ctest -N 实际配置面）里查无此名，而同族
# p3_rsmp_core/oracle/gate 都在跑 —— 即「有测试、无变异面」的发布面风险
# （check_mutation_gates.py 的 A6d 当时直接点名本条）。
#
# 实测代价（FINAL-07；mem_guard 进程树 RSS 采样，16 核开发节点，gcc 14.2 -O2）：
#   墙钟 527.7 s；进程树 RSS 峰值 0.146 GiB（单进程 133 MiB）；退出码 0。
#   21 变体 × 3 用例 = 63 次 g++ 复编译，驱动内部串行，墙钟由单核速度而非核数决定。
#   内存面无风险；耗时面在 ctest 默认 1500 s 之下（2.8x 余量）。
#
# 档位与超时照抄本仓既有的重型 mutation 惯例 eng/tests/unit/aio/CMakeLists.txt:65
# （aio_impl_mutations：默认 ON + LABELS <族名>_slow + TIMEOUT 2400），不自创分档。
option(P3RSMP_ENABLE_MUTATION_DRIVER "register mutation driver as a ctest case" ON)
if(P3RSMP_ENABLE_MUTATION_DRIVER)
  find_program(PYTHON3_EXECUTABLE python3)
  if(PYTHON3_EXECUTABLE)
    add_test(NAME p3_rsmp_mutation_driver
      COMMAND ${PYTHON3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_mutations.py)
    set_tests_properties(p3_rsmp_mutation_driver PROPERTIES TIMEOUT 2400 LABELS "p3_rsmp_slow")
  endif()
endif()
