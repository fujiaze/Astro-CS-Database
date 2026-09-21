# ============================================================================
# IMPL-P2-SAMP-001 — Phase2 采样/覆盖 V6 目标态共址单元测试
# ----------------------------------------------------------------------------
# 被测面 = 生产目标 astrocs_phase2（根 CMakeLists.txt 声明；本任务 V6 源码
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
project(v6_p2_samp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT MSVC)
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(v6_p2_samp_test v6_p2_samp_test.cpp)
target_include_directories(v6_p2_samp_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib//algorithms/coverage/include)

if(TARGET astrocs_phase2)
  # 根构建面（控制器 W9 集成后）
  target_link_libraries(v6_p2_samp_test PRIVATE astrocs_phase2)
else()
  # 独立复跑：预构建归档（默认仓库 build/，可用 -DASTROCS_PREBUILT_DIR=... 覆盖）
  if(NOT ASTROCS_PREBUILT_DIR)
    set(ASTROCS_PREBUILT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../build")
  endif()
  target_link_libraries(v6_p2_samp_test PRIVATE
    ${ASTROCS_PREBUILT_DIR}/libastrocs_phase2.a
    ${ASTROCS_PREBUILT_DIR}/libastrocs_aio.a
    ${ASTROCS_PREBUILT_DIR}/libastrocs_hips.a
    ${ASTROCS_PREBUILT_DIR}/libastrocs_common.a
    ${ASTROCS_PREBUILT_DIR}/libastrocs_cfitsio.a
    pthread m z)
endif()

enable_testing()
foreach(case covsupport covsupport_negative weight_gate boundary_determinism
             spatial spatial_negative summary scalar_gate determinism)
  add_test(NAME v6_p2_samp_${case} COMMAND v6_p2_samp_test ${case})
  set_tests_properties(v6_p2_samp_${case} PROPERTIES LABELS "v6_p2_samp")
endforeach()
add_test(NAME v6_p2_samp_all COMMAND v6_p2_samp_test all)
set_tests_properties(v6_p2_samp_all PROPERTIES LABELS "v6_p2_samp")
