# ============================================================================
# P1-CAL-TEST (TEST-P1-CAL-001) — lib/calibration 可复用验证面
# ----------------------------------------------------------------------------
# 控制包任务: P1-CAL-TEST (SA-P1C-T, queue 31, lock-P1-CAL; 依赖 P1-CAL-DOC 闭环)
# 合同锚: docs/algorithms/CALIBRATION_ALGORITHMS.md §9 TEST-CAL-DESIGN-001
#        (P1-CAL-DOC 冻结, 2026-09-07, wave W1); 矩阵行 P1-CAL (MOD
#        astrocs.p1.calibration, TEST-P1-CAL-001)。
#
# 结构 (模块 tests/{unit,properties,oracle,fixtures,negative,performance} 对应):
#   p1cal/p1cal_fixtures.hpp     — FIX-CAL-A..F 固定 seed fixture generator
#   p1cal/p1cal_oracle.hpp       — 独立 oracle (不调用被测函数, sort-path 统计
#                                  + double 域代数重算 + BFS/IDW 直接复算)
#   p1cal/p1cal_tests_core.cpp   — units/properties/negative/cosmetic 四组
#   p1cal/p1cal_tests_perf.cpp   — performance 基线 (CI 森严哨兵)
#   p1cal/p1cal_tests_selfcheck.cpp — 故障注入必败自检 (baseline+injection)
#
# CTest 注册 (单跑: ctest -R p1cal_):
#   p1cal_units        — ALG-CAL-003 校准算术 oracle + I1/I2/I5 + FP64 位级
#   p1cal_properties   — FIX-CAL-C clip 性质 + I3 幂等 + I4 确定性 (1/2/4 线程)
#   p1cal_negative     — FIX-F 参数域 (NULL/0 维/负中位数拒绝/NaN/sigma 禁用)
#   p1cal_cosmetic     — FIX-CAL-E 检测/结构过滤/修复 oracle + 1-N worker + I6
#   p1cal_performance  — perf 基线哨兵 (parity/trend/bitwise)
#   p1cal_selfcheck    — 故障注入必败自检 (验收: 注入可失败)
# 既有组保持不动: p1_calibration / master_flat_median / cal_photometry_apply。
# ============================================================================

set(P1CAL_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# core 组可执行 (units/properties/negative/cosmetic 共享执行器框架)
add_executable(p1cal_tests
  ${P1CAL_DIR}/p1cal_tests_main.cpp
  ${P1CAL_DIR}/p1cal_tests_core.cpp)
target_include_directories(p1cal_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${P1CAL_DIR})
target_link_libraries(p1cal_tests PRIVATE astrocs_calibration ${ASTROCS_OMP_TARGET})
# 单组单跑: ctest -R p1cal_units / p1cal_properties / p1cal_negative / p1cal_cosmetic
add_test(NAME p1cal_units COMMAND p1cal_tests units)
add_test(NAME p1cal_properties COMMAND p1cal_tests properties)
add_test(NAME p1cal_negative COMMAND p1cal_tests negative)
add_test(NAME p1cal_cosmetic COMMAND p1cal_tests cosmetic)

# performance 基线 (独立可执行, 时长敏感与 core 组隔离)
add_executable(p1cal_perf_test
  ${P1CAL_DIR}/p1cal_tests_perf.cpp)
target_include_directories(p1cal_perf_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${P1CAL_DIR})
target_link_libraries(p1cal_perf_test PRIVATE astrocs_calibration ${ASTROCS_OMP_TARGET})
add_test(NAME p1cal_performance COMMAND p1cal_perf_test)

# 故障注入必败自检 (验收: baseline 必 PASS + 注入必 FAIL 双向排除恒常)
add_executable(p1cal_selfcheck_test
  ${P1CAL_DIR}/p1cal_tests_selfcheck.cpp
  ${P1CAL_DIR}/p1cal_tests_core.cpp)
target_include_directories(p1cal_selfcheck_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${P1CAL_DIR})
target_link_libraries(p1cal_selfcheck_test PRIVATE astrocs_calibration ${ASTROCS_OMP_TARGET})
add_test(NAME p1cal_selfcheck COMMAND p1cal_selfcheck_test)
