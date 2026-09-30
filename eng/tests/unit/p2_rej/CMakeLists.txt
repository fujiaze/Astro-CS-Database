# ============================================================================
# IMPL-P2-REJ-001 — V6 Phase2 分类排异共址测试（本任务 write_scope 内自注册）
# ----------------------------------------------------------------------------
# 依据 C-004.4：根 eng/tests/unit/CMakeLists.txt 不属本任务写域；控制器在 W5 后
# 以独立集成提交 add_subdirectory(eng/tests/unit/p2_rej) 即可。
#
# 自包含：本 target 直接编译生产源 lib/algorithms/coverage/src/rejection.cpp，不链接
# acsd_phase2（避免与既有成员产生重复符号），因此本目录可独立配置/运行
# （shadow build 见 run/v6/IMPL-P2-REJ-001/）。
# ============================================================================
set(P2_REJ_V6_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib//algorithms/coverage/src/rejection.cpp)
set(P2_REJ_V6_INC ${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib//algorithms/coverage/include)

add_executable(p2_rej_test p2_rej_test.cpp ${P2_REJ_V6_SRC})
target_include_directories(p2_rej_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${P2_REJ_V6_INC})
target_link_libraries(p2_rej_test PRIVATE acsd_platform_math)

add_test(NAME p2_rej_units       COMMAND p2_rej_test units)
add_test(NAME p2_rej_oracle      COMMAND p2_rej_test oracle)
add_test(NAME p2_rej_reference   COMMAND p2_rej_test reference)
add_test(NAME p2_rej_negative    COMMAND p2_rej_test negative)
add_test(NAME p2_rej_calibration COMMAND p2_rej_test calibration)
add_test(NAME p2_rej_determinism COMMAND p2_rej_test determinism)

set_tests_properties(
  p2_rej_units p2_rej_oracle p2_rej_reference
  p2_rej_negative p2_rej_calibration p2_rej_determinism
  PROPERTIES LABELS "p2_rej;IMPL-P2-REJ-001")
