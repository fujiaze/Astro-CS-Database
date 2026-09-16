# ============================================================================
# IMPL-P2-REJ-001 — V6 Phase2 分类排异共址测试（本任务 write_scope 内自注册）
# ----------------------------------------------------------------------------
# 依据 C-004.4：根 tests/unit/CMakeLists.txt 不属本任务写域；控制器在 W5 后
# 以独立集成提交 add_subdirectory(tests/unit/v6_p2_rej) 即可。
#
# 自包含：本 target 直接编译生产源 lib/algorithms/coverage/src/rejection.cpp，不链接
# astrocs_phase2（避免与既有成员产生重复符号），因此本目录可独立配置/运行
# （shadow build 见 run/v6/IMPL-P2-REJ-001/）。
# ============================================================================
set(P2_REJ_V6_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/algorithms/coverage/src/rejection.cpp)
set(P2_REJ_V6_INC ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/algorithms/coverage/include)

add_executable(p2_rej_v6_test p2_rej_v6_test.cpp ${P2_REJ_V6_SRC})
target_include_directories(p2_rej_v6_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${P2_REJ_V6_INC})
if(UNIX)
  target_link_libraries(p2_rej_v6_test PRIVATE m)
endif()

add_test(NAME p2_rej_v6_units       COMMAND p2_rej_v6_test units)
add_test(NAME p2_rej_v6_oracle      COMMAND p2_rej_v6_test oracle)
add_test(NAME p2_rej_v6_reference   COMMAND p2_rej_v6_test reference)
add_test(NAME p2_rej_v6_negative    COMMAND p2_rej_v6_test negative)
add_test(NAME p2_rej_v6_calibration COMMAND p2_rej_v6_test calibration)
add_test(NAME p2_rej_v6_determinism COMMAND p2_rej_v6_test determinism)

set_tests_properties(
  p2_rej_v6_units p2_rej_v6_oracle p2_rej_v6_reference
  p2_rej_v6_negative p2_rej_v6_calibration p2_rej_v6_determinism
  PROPERTIES LABELS "v6;p2_rej;IMPL-P2-REJ-001")
