# ============================================================================
# P1-HIPS-TEST (TEST-P1-HIPS-001) — lib/astro_image_io/src/hips 可复用验证面
# ----------------------------------------------------------------------------
# 控制包任务: P1-HIPS-TEST (SA-P1H-T, queue 39, lock-P1-HIPS; 依赖
# P1-HIPS-DOC 闭环)。合同锚: docs/algorithms/HIPS_WRITER.md §9
# TEST-HIPS-DESIGN-001 (P1-HIPS-DOC 冻结, 2026-09-07); 矩阵行 P1-HIPS
# (MOD astrocs.phase1.hips-writer, registry 页
# docs/modules/registry/astrocs.phase1.hips-writer.md)。
#
# 结构 (模块 tests/{unit,properties,oracle,fixtures,negative,performance} 对应):
#   p1hips/p1hips_fixtures.hpp     — FIX-HIPS-A..F 固定 seed fixture
#                                    (splitmix64; 解析可控, 不内嵌生产算法)
#   p1hips/p1hips_oracle.hpp       — 独立 oracle (不调用被测函数: 独立位
#                                    解交织 FITS 排列 + IVOA MOC UNIQ 式 +
#                                    CFITSIO 只读参考通道 + 独立 MJD/球面角)
#   p1hips/p1hips_test_main.hpp    — 执行器框架 (FaultRegistry + 组 runner)
#   p1hips/p1hips_tests_units.cpp     — units 组 (U1..U5)
#   p1hips/p1hips_tests_properties.cpp — properties 组 (1-N worker 确定性
#                                    + I5/I6/I7/I11 + F5 prov 两态)
#   p1hips/p1hips_tests_oracle.cpp    — oracle 组 (期望值非被测函数生成)
#   p1hips/p1hips_tests_negative.cpp  — negative 组 (§9 负面矩阵逐行)
#   p1hips/p1hips_tests_perf.cpp   — performance 基线 (CI 森严哨兵)
#   p1hips/p1hips_tests_selfcheck.cpp — 故障注入必败自检 (baseline+injection)
#
# CTest 注册 (单跑: ctest -R p1hips_):
#   p1hips_units        — F1/F6 正常路径 + I1/I2 解析期望 (f64 bitwise /
#                         f32 rtol=1e-7) + 头键/properties 精确 + I12 SNR
#                         round-trip + I8 manifest 计数
#   p1hips_properties   — 确定性 (1/2/4 worker 位级) + I5/I6 MOC + I7 层次
#                         闭合 (f64 bitwise / f32 rtol=1e-6) + I11 + F5
#   p1hips_oracle       — FITS 排列全量对拍 + MOC UNIQ 独立式 + SNR cell
#                         弱 oracle + I4 互倒 (期望非被测函数生成)
#   p1hips_negative     — §9 负面矩阵: begin 参数域/写错误码 -1..-5,-6,-7/
#                         FITS 不可写/prov 2/重复 finalize -2 (I9)/last_error
#   p1hips_performance  — perf 基线哨兵 (parity 4x/trend 0.25x)
#   p1hips_selfcheck    — 故障注入必败自检 (验收: 注入可失败, 双注入点)
# 被测面: 现状唯一生产实现 astrocs_hips (根 CMakeLists.txt:326,
# lib/astro_image_io/src/hips/aio_hips_writer.cpp C ABI 九导出,
# API-HIPS-001); 本任务仅新增测试, 不改生产源 (批次 R: 域 hardening
# 已覆盖, 负面用例从 ABI 合同层注入)。
# ============================================================================

set(P1HIPS_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(P1HIPS_INC ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

# core 组可执行 (units/properties/oracle/negative 共享执行器框架)
add_executable(p1hips_tests
  ${P1HIPS_DIR}/p1hips_tests_main.cpp
  ${P1HIPS_DIR}/p1hips_tests_units.cpp
  ${P1HIPS_DIR}/p1hips_tests_properties.cpp
  ${P1HIPS_DIR}/p1hips_tests_oracle.cpp
  ${P1HIPS_DIR}/p1hips_tests_negative.cpp)
target_include_directories(p1hips_tests PRIVATE
  ${P1HIPS_INC}
  ${P1HIPS_DIR})
target_link_libraries(p1hips_tests PRIVATE astrocs_hips ${ASTROCS_OMP_TARGET})
# 单组单跑: ctest -R p1hips_units / p1hips_properties / p1hips_oracle / p1hips_negative
add_test(NAME p1hips_units COMMAND p1hips_tests units)
add_test(NAME p1hips_properties COMMAND p1hips_tests properties)
add_test(NAME p1hips_oracle COMMAND p1hips_tests oracle)
add_test(NAME p1hips_negative COMMAND p1hips_tests negative)

# performance 基线 (独立可执行, 时长敏感与 core 组隔离)
add_executable(p1hips_perf_test
  ${P1HIPS_DIR}/p1hips_tests_perf.cpp)
target_include_directories(p1hips_perf_test PRIVATE
  ${P1HIPS_INC}
  ${P1HIPS_DIR})
target_link_libraries(p1hips_perf_test PRIVATE astrocs_hips ${ASTROCS_OMP_TARGET})
add_test(NAME p1hips_performance COMMAND p1hips_perf_test)

# 故障注入必败自检 (验收: baseline 必 PASS + 注入必 FAIL 双向排除恒常)
add_executable(p1hips_selfcheck_test
  ${P1HIPS_DIR}/p1hips_tests_selfcheck.cpp
  ${P1HIPS_DIR}/p1hips_tests_units.cpp
  ${P1HIPS_DIR}/p1hips_tests_properties.cpp
  ${P1HIPS_DIR}/p1hips_tests_oracle.cpp
  ${P1HIPS_DIR}/p1hips_tests_negative.cpp)
target_include_directories(p1hips_selfcheck_test PRIVATE
  ${P1HIPS_INC}
  ${P1HIPS_DIR})
target_link_libraries(p1hips_selfcheck_test PRIVATE astrocs_hips ${ASTROCS_OMP_TARGET})
add_test(NAME p1hips_selfcheck COMMAND p1hips_selfcheck_test)

# P1-HIPS-DIGEST-001 归一化专项验收 harness (EXCLUDE_FROM_ALL + 不入 CTest:
# 含 sleep 2 强制跨秒与字节翻转双向验证, 秒级慢测不适合 CI 常驻; 手动验收
# 见 p1hips_digest_verify.cpp 文件头)
add_executable(p1hips_digest_verify EXCLUDE_FROM_ALL
  ${P1HIPS_DIR}/p1hips_digest_verify.cpp)
target_include_directories(p1hips_digest_verify PRIVATE
  ${P1HIPS_INC}
  ${P1HIPS_DIR})
target_link_libraries(p1hips_digest_verify PRIVATE astrocs_hips ${ASTROCS_OMP_TARGET})
