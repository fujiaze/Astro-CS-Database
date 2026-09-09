# ============================================================================
# P1-NOISE-TEST (TEST-P1-NOISE-001) — lib/snr_estimator (astrocs.p1.noise-snr)
# 可复用验证面
# ----------------------------------------------------------------------------
# 控制包任务: P1-NOISE-TEST (SA-P1N-T, queue 41, lock-P1-NOISE; 依赖
# P1-NOISE-DOC 闭环)。合同锚: docs/algorithms/NOISE_ESTIMATION.md §13.4
# TEST-NOISE-DESIGN-001 (P1-NOISE-DOC 冻结, 2026-09-07, wave W1); 矩阵行
# P1-NOISE (MOD astrocs.p1.noise-snr); 上游 SCI-NOISE-001..015 (NOISE_MODEL.md
# FROZEN T104 2026-08-23, 不改)。
#
# 结构 (模块 tests/{unit,properties,oracle,fixtures,negative,performance} 对应):
#   p1noise/p1noise_fixtures.hpp     — FIX-NOISE-A..G(+PERF) 固定 seed fixture
#                                      generator (splitmix64/Box-Muller, 全离线)
#   p1noise/p1noise_oracle.hpp       — 独立 oracle (不调用被测函数, sort-path
#                                      统计 + 合同公式独立编码 + 逐像素距判圆盘)
#   p1noise/p1noise_tests_core.cpp   — units/properties/oracle/negative/
#                                      scale_law/fill 六组
#   p1noise/p1noise_tests_perf.cpp   — performance 基线哨兵 (parity/trend 双侧,
#                                      无绝对速度门)
#   p1noise/p1noise_tests_selfcheck.cpp — 故障注入必败自检 (baseline+injection)
#
# CTest 注册 (单跑: ctest -R p1noise_):
#   p1noise_units        — FIX-A σ5% (4 seed) + oracle bitwise + FIX-B 平面 10%
#                        + FIX-E gain 解析 bitwise
#   p1noise_properties   — I1-I6 不变量 + I4 确定性 + free 幂等
#   p1noise_oracle       — o1 全模型 bitwise + FIX-D 掩膜解耦 (振幅无关/双通道
#                        bitwise) + DISP-NOISE-003 惰性 + FIX-E 经验交叉 5%
#                        + v1/f64 parity
#   p1noise_negative     — 参数域 rc=3 / cfg=NULL / 全掩膜退化 / NaN-Inf-饱和
#                        过滤 / NaN 星跳过 / fill 参数域 / FIX-B 负梯度 clamp
#                        / 静默钳位现状
#   p1noise_scale_law    — FIX-F 逐位/回乘恒等/NaN-α 直传现状
#   p1noise_fill         — FIX-G 常量/平面 bitwise/可空输出/无空间场
#   p1noise_performance  — perf 基线哨兵 (parity 4x/trend 0.25x/线性 O(h·w)/
#                          bitwise/64 控制点界)
#   p1noise_selfcheck    — 故障注入必败自检 (验收: 注入可失败)
# 被测面: 现状唯一生产实现 lib/snr_estimator/cpp/src/noise_model.cpp
# (snr_noise_model_v1(+_f64)/fill/free/scale_law/gain_variance; 根构建无
# snr_estimator CMake 目标 — README §1, 故本测试面从生产源独立编译
# EXCLUDE_FROM_ALL 静态目标 astrocs_p1_noise_prod, 生产源零改动);
# lib/snr_estimator/ 为 P1-NOISE 迁移目标目录 (P1-NOISE-IMPL 落码后本测试面
# 直接复用, 冻结容差不变)。
# ============================================================================

set(P1NOISE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(P1NOISE_PROD_SRC ${CMAKE_SOURCE_DIR}/lib/snr_estimator/cpp/src/noise_model.cpp)
set(P1NOISE_PROD_INC ${CMAKE_SOURCE_DIR}/lib/snr_estimator/cpp/include)

# 被测面 (生产源只读编译; EXCLUDE_FROM_ALL 不入主构建/安装面)
add_library(p1noise_under_test STATIC EXCLUDE_FROM_ALL ${P1NOISE_PROD_SRC})
target_include_directories(p1noise_under_test PUBLIC ${P1NOISE_PROD_INC})
set_target_properties(p1noise_under_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)

# core 组可执行 (units/properties/oracle/negative/scale_law/fill 共享执行器)
add_executable(p1noise_tests
  ${P1NOISE_DIR}/p1noise_tests_main.cpp
  ${P1NOISE_DIR}/p1noise_tests_core.cpp)
target_link_libraries(p1noise_tests PRIVATE p1noise_under_test)
if(UNIX)
  target_link_libraries(p1noise_tests PRIVATE m)
endif()
# 单组单跑: ctest -R p1noise_units / p1noise_properties / p1noise_oracle /
#            p1noise_negative / p1noise_scale_law / p1noise_fill
add_test(NAME p1noise_units COMMAND p1noise_tests units)
add_test(NAME p1noise_properties COMMAND p1noise_tests properties)
add_test(NAME p1noise_oracle COMMAND p1noise_tests oracle)
add_test(NAME p1noise_negative COMMAND p1noise_tests negative)
add_test(NAME p1noise_scale_law COMMAND p1noise_tests scale_law)
add_test(NAME p1noise_fill COMMAND p1noise_tests fill)

# performance 基线 (独立可执行, 时长敏感与 core 组隔离)
add_executable(p1noise_perf_test
  ${P1NOISE_DIR}/p1noise_tests_perf.cpp)
target_link_libraries(p1noise_perf_test PRIVATE p1noise_under_test)
if(UNIX)
  target_link_libraries(p1noise_perf_test PRIVATE m)
endif()
add_test(NAME p1noise_performance COMMAND p1noise_perf_test)

# 故障注入必败自检 (验收: baseline 必 PASS + 注入必 FAIL 双向排除恒常)
add_executable(p1noise_selfcheck_test
  ${P1NOISE_DIR}/p1noise_tests_selfcheck.cpp
  ${P1NOISE_DIR}/p1noise_tests_core.cpp)
target_link_libraries(p1noise_selfcheck_test PRIVATE p1noise_under_test)
if(UNIX)
  target_link_libraries(p1noise_selfcheck_test PRIVATE m)
endif()
add_test(NAME p1noise_selfcheck COMMAND p1noise_selfcheck_test)
