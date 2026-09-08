# ============================================================================
# P1-DRZ-TEST (TEST-P1-DRZ-001) — lib/healpix_db/healpix_drizzle 可复用验证面
# ----------------------------------------------------------------------------
# 控制包任务: P1-DRZ-TEST (SA-P1D-T, queue 37, lock-P1-DRZ; 依赖 P1-DRZ-DOC
# 闭环)。合同锚: docs/algorithms/DRIZZLE_GEOMETRY.md §9 TEST-DRZ-DESIGN-001
# (P1-DRZ-DOC 冻结, 2026-09-07, wave W1); 矩阵行 P1-DRZ (MOD
# astrocs.p1.drizzle); SCI-DRZ-001 (docs/science/DRIZZLE.md §5/§7/§8)。
#
# 结构 (模块 tests/{unit,properties,oracle,fixtures,negative,performance} 对应):
#   p1drz/p1drz_fixtures.hpp     — FIX-DRZ-A..F 固定 seed fixture generator
#                                  (splitmix64 谱系, 面亮度语义 per SCI-003)
#   p1drz/p1drz_geom.hpp         — 测试侧独立几何原语 (向量旋转 TAN + SIP,
#                                  Van Oosterom 立体角, 切平面 S-H)
#   p1drz/p1drz_oracle.hpp       — 独立 oracle (不调用被测函数; 期望值由
#                                  解析恒等式 + 独立几何重算 + 权威
#                                  astrocs::healpix 地址生成)
#   p1drz/p1drz_test_main.hpp    — 执行器框架 (FaultRegistry + 组 runner)
#   p1drz/p1drz_tests_core.cpp   — units/properties/oracle/negative 四组
#   p1drz/p1drz_tests_perf.cpp   — performance 基线 (CI 森严哨兵)
#   p1drz/p1drz_tests_selfcheck.cpp — 故障注入必败自检 (baseline+injection)
#
# CTest 注册 (单跑: ctest -R p1drz_):
#   p1drz_units        — FIX-DRZ-A 双语义/B 闭合/D 脉冲 + FP32/FP64 逐 leaf
#   p1drz_properties   — 1/2/4 线程一致 + 同线程 bitwise + α² 缩放律 (1e-4)
#   p1drz_oracle       — 独立方差闭合 + FIX-DRZ-E NaN/Inf 污染集合 + F SIP
#   p1drz_negative     — DRIZZLE.md §8 表逐行 (pixfrac/RING/多通道/缺 WCS/
#                        NaN 面/零尺寸/非法 nside)
#   p1drz_performance  — perf 基线哨兵 (parity 4x/trend 0.4x)
#   p1drz_selfcheck    — 故障注入必败自检 (验收: 注入可失败)
# 域基线: HEAD 09ee5363 (P1-DRZ-NONFINITE 删除值侧 isfinite 静默掩膜);
# 与 drizzle_nonfinite_test (负面值合同) 互补不重复。
# ============================================================================

set(P1DRZ_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# core 组可执行 (units/properties/oracle/negative 共享执行器框架)
add_executable(p1drz_tests
  ${P1DRZ_DIR}/p1drz_tests_main.cpp
  ${P1DRZ_DIR}/p1drz_tests_core.cpp)
target_include_directories(p1drz_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${P1DRZ_DIR})
# 与 astrocs_drizzle (根 CMakeLists:382) 同宏: drizzle_engine.h 引用
# aio_healpix_io.h 的 HioSnrModel(_F64) 声明
target_compile_definitions(p1drz_tests PRIVATE AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(p1drz_tests PRIVATE astrocs_drizzle ${ASTROCS_OMP_TARGET})
# 单组单跑: ctest -R p1drz_units / p1drz_properties / p1drz_oracle / p1drz_negative
add_test(NAME p1drz_units COMMAND p1drz_tests units)
add_test(NAME p1drz_properties COMMAND p1drz_tests properties)
add_test(NAME p1drz_oracle COMMAND p1drz_tests oracle)
add_test(NAME p1drz_negative COMMAND p1drz_tests negative)

# performance 基线 (独立可执行, 时长敏感与 core 组隔离)
add_executable(p1drz_perf_test
  ${P1DRZ_DIR}/p1drz_tests_perf.cpp)
target_include_directories(p1drz_perf_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${P1DRZ_DIR})
target_compile_definitions(p1drz_perf_test PRIVATE AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(p1drz_perf_test PRIVATE astrocs_drizzle ${ASTROCS_OMP_TARGET})
add_test(NAME p1drz_performance COMMAND p1drz_perf_test)

# 故障注入必败自检 (验收: baseline 必 PASS + 注入必 FAIL 双向排除恒常)
add_executable(p1drz_selfcheck_test
  ${P1DRZ_DIR}/p1drz_tests_selfcheck.cpp
  ${P1DRZ_DIR}/p1drz_tests_core.cpp)
target_include_directories(p1drz_selfcheck_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${P1DRZ_DIR})
target_compile_definitions(p1drz_selfcheck_test PRIVATE AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(p1drz_selfcheck_test PRIVATE astrocs_drizzle ${ASTROCS_OMP_TARGET})
add_test(NAME p1drz_selfcheck COMMAND p1drz_selfcheck_test)
