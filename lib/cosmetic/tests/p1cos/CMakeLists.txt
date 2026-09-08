# ============================================================================
# P1-COS-TEST (TEST-P1-COS-001) — lib/cosmetic 可复用验证面
# ----------------------------------------------------------------------------
# 控制包任务: P1-COS-TEST (SA-P1O-T, queue 34, lock-P1-COS; 依赖 P1-COS-DOC
# 闭环)。合同锚: docs/algorithms/COSMETIC_ALGORITHMS.md §9
# TEST-COS-DESIGN-001 (P1-COS-DOC 冻结, 2026-09-07, wave W1); 矩阵行
# P1-COS (MOD astrocs.p1.cosmetic)。
#
# 结构 (模块 tests/{unit,properties,oracle,fixtures,negative,performance} 对应):
#   p1cos/p1cos_fixtures.hpp     — FIX-COS-A..F 固定 seed fixture generator
#   p1cos/p1cos_oracle.hpp       — 独立 oracle (不调用被测函数, sort-path
#                                  统计 + double 域代数复算 + deque BFS/IDW)
#   p1cos/p1cos_tests_core.cpp   — units/properties/negative/cosmetic 四组
#   p1cos/p1cos_tests_perf.cpp   — performance 基线 (CI 森严哨兵)
#   p1cos/p1cos_tests_selfcheck.cpp — 故障注入必败自检 (baseline+injection)
#
# CTest 注册 (单跑: ctest -R p1cos_):
#   p1cos_units        — FIX-COS-A/B/F: 恒等/检测修复 oracle/双中位均值
#   p1cos_properties   — I1-I6 不变量 + I4 确定性 (1/2/4 线程) + I6 回退
#   p1cos_negative     — C ABI 参数域/method=2 现状/max_size<=0/NaN/Inf/f64 降级
#   p1cos_cosmetic     — FIX-COS-C 结构过滤 + FIX-COS-E IDW 方向性 + 小帧镜像
#                        + 1-N worker bitwise
#   p1cos_performance  — perf 基线哨兵 (parity 4x/trend 0.25x/bitwise)
#   p1cos_selfcheck    — 故障注入必败自检 (验收: 注入可失败)
# 被测面: 现状唯一生产实现 ac_correct_frame(+_f64) (CMake astrocs_calibration,
# lib/calibration/src/cosmetic_corrector.cpp + ac_api.cpp); lib/cosmetic/ 为
# P1-COS 迁移目标目录 (P1-COS-IMPL 落码后本测试面直接复用, 冻结容差不变)。
# ============================================================================

set(P1COS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# core 组可执行 (units/properties/negative/cosmetic 共享执行器框架)
add_executable(p1cos_tests
  ${P1COS_DIR}/p1cos_tests_main.cpp
  ${P1COS_DIR}/p1cos_tests_core.cpp)
target_include_directories(p1cos_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/calibration/include
  ${P1COS_DIR})
target_link_libraries(p1cos_tests PRIVATE astrocs_calibration ${ASTROCS_OMP_TARGET})
# 单组单跑: ctest -R p1cos_units / p1cos_properties / p1cos_negative / p1cos_cosmetic
add_test(NAME p1cos_units COMMAND p1cos_tests units)
add_test(NAME p1cos_properties COMMAND p1cos_tests properties)
add_test(NAME p1cos_negative COMMAND p1cos_tests negative)
add_test(NAME p1cos_cosmetic COMMAND p1cos_tests cosmetic)

# performance 基线 (独立可执行, 时长敏感与 core 组隔离)
add_executable(p1cos_perf_test
  ${P1COS_DIR}/p1cos_tests_perf.cpp)
target_include_directories(p1cos_perf_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/calibration/include
  ${P1COS_DIR})
target_link_libraries(p1cos_perf_test PRIVATE astrocs_calibration ${ASTROCS_OMP_TARGET})
add_test(NAME p1cos_performance COMMAND p1cos_perf_test)

# 故障注入必败自检 (验收: baseline 必 PASS + 注入必 FAIL 双向排除恒常)
add_executable(p1cos_selfcheck_test
  ${P1COS_DIR}/p1cos_tests_selfcheck.cpp
  ${P1COS_DIR}/p1cos_tests_core.cpp)
target_include_directories(p1cos_selfcheck_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/calibration/include
  ${P1COS_DIR})
target_link_libraries(p1cos_selfcheck_test PRIVATE astrocs_calibration ${ASTROCS_OMP_TARGET})
add_test(NAME p1cos_selfcheck COMMAND p1cos_selfcheck_test)
