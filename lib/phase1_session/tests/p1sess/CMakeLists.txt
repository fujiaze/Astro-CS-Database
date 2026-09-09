# ============================================================================
# P1-SESSION-TEST (TEST-P1-SESSION-001) — lib/phase1_session 可复用验证面
# ----------------------------------------------------------------------------
# 控制包任务: P1-SESSION-TEST (SA-P1SS-T, queue 47, lock-P1-SESSION; 依赖
# P1-SESSION-DOC 闭环 2026-09-07)。合同锚: lib/phase1_session/README.md
# (API-P1-SESSION / DATA-P1-SESSION 冻结) §2 DAG 节点表 + §3 装配顺序 +
# §5 错误取消传播; 工程控制 tasks/05_PHASE1_TASKS.md §3 验收:
#   "每声明节点恰好调用一次; 缺任何模块/错误 ABI/坏 artifact/取消不会写
#    完成 manifest; 仅 Phase1 run ID"。
#
# 结构 (模块 tests/{unit,properties,oracle,fixtures,negative,performance} 对应):
#   p1sess/p1sess_fixtures.hpp      — FIX-SESS-A..F 固定 seed fixture
#                                     (splitmix64 + 手写最小 FITS; 不内嵌
#                                     生产算法)
#   p1sess/p1sess_oracle.hpp        — 独立 oracle (不调用被测函数: 手写
#                                     FITS 大端解码 + ALG-CAL-001 常数场
#                                     代数式; 期望非被测函数生成)
#   p1sess/p1sess_tests_units.cpp   — units 组: 每声明节点恰好调用一次
#                                     (manifest stages 计数+顺序) / 失败
#                                     传播 (缺文件/坏 FITS/尺寸不匹配 →
#                                     不写完成 manifest) / ALG-CAL-001
#                                     oracle 断言 / NaN 传播 / 状态机
#                                     (oracle 组并入 units, U2 断言经
#                                     p1sess_oracle.hpp 独立通道)
#   p1sess/p1sess_tests_properties.cpp — properties 组: validate 幂等纯读 /
#                                     1-N worker bitwise / 取消传播 /
#                                     异常屏障 / async_io_depth 边界 /
#                                     ABI 负面 / 空参数域
#   p1sess/p1sess_tests_negative.cpp — negative 组: validate 逐键拒收矩阵
#                                     (13 case) + last_error 脱敏 + 合法对照
#   p1sess/p1sess_tests_perf.cpp    — performance 哨兵 (装配上界 + worker
#                                     完成性)
#   p1sess/p1sess_tests_selfcheck.cpp — 故障注入必败自检 (baseline + 2 注入
#                                     点, 子进程 fork/execve; 注入可失败)
#
# CTest 注册 (单跑: ctest -R p1sess_; 5 组: units 内含 oracle 断言):
#   p1sess_units        — 节点调用计数 each-node-once + 失败传播 + oracle +
#                         NaN + created 状态机
#   p1sess_properties   — 幂等 + 1-N worker bitwise + 取消 + 屏障 + 边界
#   p1sess_negative     — validate 逐键拒收 + 参数域
#   p1sess_performance  — 装配哨兵 (8 帧 32x32 <20s + worker 完成性)
#   p1sess_selfcheck    — 故障注入必败自检 (验收: 注入可失败, 双注入点)
# 被测面: 现状唯一生产实现 p1_session 五导出 C API (静态库
# astrocs_phase1_session, 根 CMakeLists.txt:448; API-P1-SESSION)。
# 生产源只读 — 本任务仅新增测试 (批次: 会话 direct 面, 不做 DLL dlopen;
# CAT-GAIA-INT 调用计数模式的 direct 面形态: manifest stages 即节点
# 调用账本)。
# ============================================================================

set(P1SESS_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(P1SESS_INC_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

# core 组可执行 (units/properties/negative/performance 共享执行器框架)
add_executable(p1sess_tests
  ${P1SESS_DIR}/p1sess_tests_main.cpp
  ${P1SESS_DIR}/p1sess_tests_units.cpp
  ${P1SESS_DIR}/p1sess_tests_properties.cpp
  ${P1SESS_DIR}/p1sess_tests_negative.cpp
  ${P1SESS_DIR}/p1sess_tests_perf.cpp)
target_include_directories(p1sess_tests PRIVATE
  ${P1SESS_INC_ROOT}/lib/phase1_session
  ${P1SESS_INC_ROOT}/lib/calibration/include
  ${P1SESS_INC_ROOT}/lib/astro_image_io/include
  ${P1SESS_INC_ROOT}/third_party
  ${P1SESS_DIR})
target_link_libraries(p1sess_tests PRIVATE astrocs_phase1_session ${ASTROCS_OMP_TARGET})
# 单组单跑: ctest -R p1sess_units / p1sess_properties / p1sess_negative / p1sess_performance
add_test(NAME p1sess_units COMMAND p1sess_tests units)
add_test(NAME p1sess_properties COMMAND p1sess_tests properties)
add_test(NAME p1sess_negative COMMAND p1sess_tests negative)
add_test(NAME p1sess_performance COMMAND p1sess_tests performance)

# 故障注入必败自检 (验收: baseline 必 PASS + 注入必 FAIL 双向排除恒常;
# POSIX fork/execve — 与 p1hips_selfcheck 同机制)
add_executable(p1sess_selfcheck_test
  ${P1SESS_DIR}/p1sess_tests_selfcheck.cpp
  ${P1SESS_DIR}/p1sess_tests_units.cpp
  ${P1SESS_DIR}/p1sess_tests_negative.cpp)
target_include_directories(p1sess_selfcheck_test PRIVATE
  ${P1SESS_INC_ROOT}/lib/phase1_session
  ${P1SESS_INC_ROOT}/lib/calibration/include
  ${P1SESS_INC_ROOT}/lib/astro_image_io/include
  ${P1SESS_INC_ROOT}/third_party
  ${P1SESS_DIR})
target_link_libraries(p1sess_selfcheck_test PRIVATE astrocs_phase1_session ${ASTROCS_OMP_TARGET})
add_test(NAME p1sess_selfcheck COMMAND p1sess_selfcheck_test)
