# ============================================================================
# IMPL-P2-SAMP-001 — Phase2 采样/覆盖 V6 目标态共址单元测试
# ----------------------------------------------------------------------------
# 被测面 = 生产目标 acsd_phase2（根 CMakeLists.txt 声明；本任务 V6 源码
#   位于 lib/algorithms/coverage/src/sampler.cpp + lib/algorithms/coverage/src/coverage.cpp）：
#     - coverage/support 区分 + 缺失不零填 (FZ-GATE-SUPPORT-COVERAGE)
#     - 权重角色门 (FZ-GATE-MEDIAN-SNR / FZ-FIELD-WEIGHTMODE / FZ-MODE-*)
#     - 确定性边界 (DESIGN-P2 §8)
#     - 空间模型求值 + 帧级标量降级门 (DESIGN-P2 §7 / FZ-DEGRADE-SCALAR)
# 每个用例既有正例也有负例（负例=违反冻结即失败）。独立 Python Oracle 与
# 结构校验见 run/v6/p2-samp/oracle/check_spec.py（不调用被测实现作真值）。
#
# 注册按 C-004.4：本任务只在自身 write_scope 内注册测试；根
# eng/tests/unit/CMakeLists.txt 的注册由控制器 W9 独立集成提交处理。
# ============================================================================
cmake_minimum_required(VERSION 3.24)
project(p2_samp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT MSVC)
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(p2_samp_test p2_samp_test.cpp)
target_include_directories(p2_samp_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib//algorithms/coverage/include)

if(TARGET acsd_phase2)
  # 根构建面（控制器 W9 集成后）
  target_link_libraries(p2_samp_test PRIVATE acsd_phase2)
else()
  # 独立复跑：预构建归档（默认仓库 build/，可用 -DACSD_PREBUILT_DIR=... 覆盖）
  if(NOT ACSD_PREBUILT_DIR)
    set(ACSD_PREBUILT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../build")
  endif()
  # 平台系统库面: 取**同一份**共用件（根 CMakeLists 唯一判定点）。本分支链接的是
  # 根构建产出的 .a 归档, 其系统库面与根构建逐字一致 ⇒ 必须用同一份共用件, 不能在
  # 这里写裸 pthread/m/z (那正是 Windows 侧 LNK1104 "cannot open file 'm.lib'" 的根因面;
  # 事实上这三个裸名正落在**续行**上, 逐行匹配的旧判据对它 findings=0 = 恒绿面,
  # 判据已改为按括号配平覆盖续行, 见 eng/ci/check_platform_syslib_links.py)。
  # 裸 configure 本文件（无根入口 ⇒ 共用件不存在）时 fail-fast, **不静默退回裸库名**。
  if(NOT TARGET acsd_platform_math OR NOT TARGET acsd_platform_pthread
     OR NOT TARGET acsd_platform_zlib)
    message(FATAL_ERROR
      "p2_samp 独立复跑需要平台系统库共用件 acsd_platform_{math,pthread,zlib}，"
      "它们只在根 CMakeLists 声明。裸 configure 本文件时共用件不存在 ⇒ 请经根入口配置"
      "（或把本文件复制进带根入口的临时源树再 configure）；本线不提供第二处平台判断。")
  endif()
  target_link_libraries(p2_samp_test PRIVATE
    ${ACSD_PREBUILT_DIR}/libacsd_phase2.a
    ${ACSD_PREBUILT_DIR}/libacsd_aio.a
    ${ACSD_PREBUILT_DIR}/libacsd_hips.a
    ${ACSD_PREBUILT_DIR}/libacsd_common.a
    ${ACSD_PREBUILT_DIR}/libacsd_cfitsio.a
    acsd_platform_pthread acsd_platform_math acsd_platform_zlib)
endif()

enable_testing()
foreach(case covsupport covsupport_negative weight_gate boundary_determinism
             spatial spatial_negative summary scalar_gate determinism)
  add_test(NAME p2_samp_${case} COMMAND p2_samp_test ${case})
  set_tests_properties(p2_samp_${case} PROPERTIES LABELS "p2_samp")
endforeach()
add_test(NAME p2_samp_all COMMAND p2_samp_test all)
set_tests_properties(p2_samp_all PROPERTIES LABELS "p2_samp")
