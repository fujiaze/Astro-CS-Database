# ============================================================================
# P1-WCS-TEST (TEST-P1-WCS-001) — lib/plate_solve + lib/phase1/wcs 可复用验证面
# ----------------------------------------------------------------------------
# 控制包任务: P1-WCS-TEST (lock-P1-WCS; 依赖 P1-WCS-DOC 闭环, 2026-09-07)。
# 合同锚: docs/algorithms/PLATESOLVE.md §11.4 TEST-WCS-DESIGN-001 (F1-F6,
# 冻结容差不放宽); docs/science/ASTROMETRY.md SCI-WCS-001; 矩阵行 P1-WCS
# (MOD astrocs.phase1.wcs-platesolve, registry 页
# docs/modules/registry/astrocs.phase1.wcs-platesolve.md)。
#
# 结构 (模块 tests/{unit,properties,oracle,fixtures,negative,performance} 对应):
#   p1wcs/p1wcs_fixtures.hpp      — FIX-WCS-A..E 固定 seed fixture
#                                   (splitmix64, 不提交大二进制)
#   p1wcs/p1wcs_oracle.hpp        — 独立 oracle (球面三角闭式解/Cramer/
#                                   inv(M)·q SIP 期望/固定点逆解; 不调用
#                                   被测函数, 只依赖标准库与被测 API 头)
#   p1wcs/p1wcs_test_main.hpp     — 执行器框架 (FaultRegistry 双注入点:
#                                   env ASTROCS_P1WCS_FAULT + argv --fault=)
#   p1wcs/p1wcs_tests_units.cpp      — units 组 (F1/F3/F6 + n=3 边界)
#   p1wcs/p1wcs_tests_properties.cpp — properties 组 (F5 确定性 bitwise +
#                                   线程 1/2/4 + 结构性质, 1-N worker)
#   p1wcs/p1wcs_tests_oracle.cpp     — oracle 组 (期望值非被测函数生成;
#                                   F2 SIP 场对拍 |Δ|≤1e-4 px 冻结)
#   p1wcs/p1wcs_tests_negative.cpp   — negative 组 (F4 负例 fork 隔离 +
#                                   DISP-WCS-001 缺陷行为锚)
#   p1wcs/p1wcs_tests_perf.cpp    — performance 基线 (CI 森严哨兵 parity 4x/
#                                   trend 0.25x, 独立可执行)
#   p1wcs/p1wcs_tests_selfcheck.cpp — 故障注入必败自检 (baseline+env/argv
#                                   双注入点+正控, argv 子进程模式)
#
# CTest 注册 (单跑: ctest -R p1wcs_):
#   p1wcs_units / p1wcs_properties / p1wcs_oracle / p1wcs_negative
#   p1wcs_performance / p1wcs_selfcheck
#
# 被测面 (生产源零改动, 数学内核 TU 直接编译进测试):
#   - lib/plate_solve/cpp/ipv/src/ipv_itertrans.cpp (iter_trans_solve,
#     迭代重投影多项式拟合, ALG-WCS-001 §5)
#   - lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp (extract_wcs_sip, WCS 提取,
#     CD/CRVAL/CRPIX/SIP/Y-down, ALG-WCS-001 §6)
#   - lib/plate_solve/cpp/ipv/src/ipv_sip.cpp (fit_sip, DISP-WCS-003 legacy
#     路径, ipv_wcs.cpp 链接依赖)
#   - lib/plate_solve/cpp/ipv/src/ipv_triangle.cpp (triangle_match,
#     Valdes 1995 投票, OpenMP 并行区 — F5 线程确定性被测面)
#   - lib/phase1/wcs/wcs_tan.cpp (WcsTan legacy 桥, F6 冻结回归锚)
#   平台注记: ipv_select/ipv_solver/ipv_entry (C ABI 完整链) 依赖
#   windows.h (ipv_select.cpp:38), 本 Linux 验证面不可编译 — C ABI 层负面
#   (ret=0/error_msg/DLL 缺失) 归 P1-WCS-IMPL 迁移落地后验收; 本面覆盖
#   数学内核全部可达路径。ipv 日志走 stderr (模块自带 logger), 不影响
#   PASS/FAIL 判定 (rc 权威)。
# ============================================================================

set(P1WCS_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(P1WCS_IPV ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/plate_solve/cpp/ipv)
set(P1WCS_TAN ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/phase1/wcs)

# 被测生产源 TU (零改动直编; 不含 ipv_select/ipv_solver/ipv_entry —
# windows.h 平台依赖, 见头注)
set(P1WCS_PROD_TU
  ${P1WCS_IPV}/src/ipv_itertrans.cpp
  ${P1WCS_IPV}/src/ipv_wcs.cpp
  ${P1WCS_IPV}/src/ipv_sip.cpp
  ${P1WCS_IPV}/src/ipv_triangle.cpp
  ${P1WCS_TAN}/wcs_tan.cpp
)

find_package(OpenMP REQUIRED)

# core 组可执行 (units/properties/oracle/negative 共享执行器框架)
add_executable(p1wcs_tests
  ${P1WCS_DIR}/p1wcs_tests_main.cpp
  ${P1WCS_DIR}/p1wcs_tests_units.cpp
  ${P1WCS_DIR}/p1wcs_tests_properties.cpp
  ${P1WCS_DIR}/p1wcs_tests_oracle.cpp
  ${P1WCS_DIR}/p1wcs_tests_negative.cpp
  ${P1WCS_PROD_TU})
target_include_directories(p1wcs_tests PRIVATE
  ${P1WCS_IPV}/include
  ${P1WCS_TAN}
  ${P1WCS_DIR})
target_link_libraries(p1wcs_tests PRIVATE OpenMP::OpenMP_CXX)

# performance 基线 (独立可执行, 时长敏感与 core 组隔离)
add_executable(p1wcs_perf_test
  ${P1WCS_DIR}/p1wcs_tests_perf.cpp
  ${P1WCS_PROD_TU})
target_include_directories(p1wcs_perf_test PRIVATE
  ${P1WCS_IPV}/include
  ${P1WCS_TAN}
  ${P1WCS_DIR})
target_link_libraries(p1wcs_perf_test PRIVATE OpenMP::OpenMP_CXX)
add_test(NAME p1wcs_performance COMMAND p1wcs_perf_test)

# 故障注入必败自检 (验收: baseline 必 PASS + env/argv 注入必 FAIL 双向排除恒常)
add_executable(p1wcs_selfcheck_test
  ${P1WCS_DIR}/p1wcs_tests_selfcheck.cpp
  ${P1WCS_DIR}/p1wcs_tests_units.cpp
  ${P1WCS_DIR}/p1wcs_tests_properties.cpp
  ${P1WCS_DIR}/p1wcs_tests_oracle.cpp
  ${P1WCS_DIR}/p1wcs_tests_negative.cpp
  ${P1WCS_PROD_TU})
target_include_directories(p1wcs_selfcheck_test PRIVATE
  ${P1WCS_IPV}/include
  ${P1WCS_TAN}
  ${P1WCS_DIR})
target_link_libraries(p1wcs_selfcheck_test PRIVATE OpenMP::OpenMP_CXX)
add_test(NAME p1wcs_selfcheck COMMAND p1wcs_selfcheck_test)

# 单组单跑: ctest -R p1wcs_units / p1wcs_properties / p1wcs_oracle /
#           p1wcs_negative
add_test(NAME p1wcs_units COMMAND p1wcs_tests units)
add_test(NAME p1wcs_properties COMMAND p1wcs_tests properties)
add_test(NAME p1wcs_oracle COMMAND p1wcs_tests oracle)
add_test(NAME p1wcs_negative COMMAND p1wcs_tests negative)

set_tests_properties(p1wcs_units p1wcs_properties p1wcs_oracle
                     p1wcs_negative p1wcs_performance p1wcs_selfcheck
                     PROPERTIES LABELS "p1wcs")
