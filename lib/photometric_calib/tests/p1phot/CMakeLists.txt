# ============================================================================
# P1-PHOT-TEST (TEST-P1-PHOT-001) — lib/photometric_calib 可复用验证面
# ----------------------------------------------------------------------------
# 控制包任务: P1-PHOT-TEST (lock-P1-PHOT; 依赖 P1-PHOT-DOC 闭环)。合同锚:
# docs/algorithms/PHOTOMETRIC_FIT.md §13.4 TEST-PHOT-DESIGN-001 (P1-PHOT-DOC
# 冻结, 2026-09-07); docs/science/PHOTOMETRY.md §11 (SCI-PHOT-001 FROZEN);
# 矩阵行 P1-PHOT (MOD-astrocs-phase1-photometry, API-PHOT-001 六导出)。
#
# 结构 (模块 tests/{unit,properties,oracle,fixtures,negative,performance} 对应):
#   p1phot/p1phot_fixtures.hpp      — FIX-PHOT-A..F 固定 seed fixture
#                                     (splitmix64; 解析可控, 不内嵌生产算法)
#   p1phot/p1phot_oracle.hpp        — 独立 oracle (不调用被测函数: 独立
#                                     IRLS-Tukey/TAN+SIP/Akima/Simpson/XPSD
#                                     解码/暴力双向配对/aperture 求和)
#   p1phot/p1phot_test_main.hpp     — 执行器框架 (FaultRegistry + 组 runner)
#   p1phot/p1phot_gaia_stub.{hpp,cpp} — 外部域 CAT-GAIA gaia_client 替身
#                                     (被测域生产源零修改; nm 实测 undefined
#                                     面=cone_search_with_spectrum +
#                                     get_spectrum_params)
#   p1phot/p1phot_tests_units.cpp     — units 组 (U1..U10)
#   p1phot/p1phot_tests_properties.cpp — properties 组 (I5 线程扫描 1/4/max
#                                     bitwise + F2 鲁棒门 + I2/I6 + 双通道)
#   p1phot/p1phot_tests_oracle.cpp    — oracle 组 (期望值非被测函数生成)
#   p1phot/p1phot_tests_negative.cpp  — negative 组 (§13.4 负面矩阵逐行)
#   p1phot/p1phot_tests_perf.cpp      — performance 基线哨兵 (比值语义)
#   p1phot/p1phot_tests_selfcheck.cpp — 故障注入必败自检 (fork/execve argv
#                                     子进程双注入点)
#
# 被测面: 现状唯一生产实现 lib/photometric_calib/cpp/src 五 TU (pc_api/
# star_matcher/wcs_transform/image_corrector/spectrum_integrator, C ABI 六导出
# API-PHOT-001) + lib/phase1/photometry/photometer.cpp (F6 legacy 旧符号,
# README §9 合同, 只读引用)。本任务仅新增测试, 不改生产源。
# 取消语义覆盖说明: DISP-PHOT-004 (无取消检查点, P1-PHOT-IMPL 落码) —
# 现状无 cancel API 可调, 不写恒 PASS 占位; 由 P1-PHOT-IMPL 落码后补负面。
#
# CTest 注册 (单跑: ctest -R p1phot_):
#   p1phot_units        — F1/F3/F4/F5 正常路径 + 解析期望 (f64 bitwise /
#                         f32 rtol=1e-7) + 分阶段 diag + 诊断闭合 + F6 重锚
#   p1phot_properties   — I5 线程扫描 (1/4/max bitwise) + F2 鲁棒门
#                         (Δlocation<0.1 dex 冻结) + I2/I6 + v2/_f64_v2 双通道
#   p1phot_oracle       — IRLS/Akima/Simpson/aperture 独立式对拍 (期望非被测
#                         函数生成, 冻结 rtol 1e-9)
#   p1phot_negative     — §13.4 负面矩阵: -1/-2/-3/-4 错误码 + 退化显式登记
#                         + NaN/Inf + 出参向后兼容
#   p1phot_performance  — perf 哨兵 (parity 4x / trend 0.25x, 比值语义)
#   p1phot_selfcheck    — 故障注入必败自检 (验收: 注入可失败, 双注入点)
# ============================================================================

set(P1PHOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(P1PHOT_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp/src)
set(P1PHOT_INC ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp/include)
set(P1PHOT_LIB ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
set(P1PHOT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)

# 被测域生产源 (原样编译, 零修改)
set(P1PHOT_PROD_SOURCES
  ${P1PHOT_SRC}/pc_api.cpp
  ${P1PHOT_SRC}/star_matcher.cpp
  ${P1PHOT_SRC}/wcs_transform.cpp
  ${P1PHOT_SRC}/image_corrector.cpp
  ${P1PHOT_SRC}/spectrum_integrator.cpp
  # F6 legacy 旧符号 (README §9; 只读引用, 不改生产面)
  ${P1PHOT_LIB}/phase1/photometry/photometer.cpp
)

# OpenMP (被测域 pc_api/image_corrector 使用; G9 同款可选降级)
find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
  set(P1PHOT_OMP_TARGET OpenMP::OpenMP_CXX)
else()
  set(P1PHOT_OMP_TARGET "")
endif()

# core 组可执行 (units/properties/oracle/negative 共享执行器框架)
add_executable(p1phot_tests
  ${P1PHOT_DIR}/p1phot_tests_main.cpp
  ${P1PHOT_DIR}/p1phot_tests_units.cpp
  ${P1PHOT_DIR}/p1phot_tests_properties.cpp
  ${P1PHOT_DIR}/p1phot_tests_oracle.cpp
  ${P1PHOT_DIR}/p1phot_tests_negative.cpp
  ${P1PHOT_DIR}/p1phot_gaia_stub.cpp
  ${P1PHOT_PROD_SOURCES})
target_include_directories(p1phot_tests PRIVATE
  ${P1PHOT_DIR}
  ${P1PHOT_INC}
  ${P1PHOT_SRC}
  ${P1PHOT_LIB}/gaia_xpsd_client/src
  ${P1PHOT_LIB}/phase1/photometry
  ${P1PHOT_ROOT}/include)
target_link_libraries(p1phot_tests PRIVATE ${P1PHOT_OMP_TARGET})
# 单组单跑: ctest -R p1phot_units / p1phot_properties / p1phot_oracle / p1phot_negative
add_test(NAME p1phot_units COMMAND p1phot_tests units)
add_test(NAME p1phot_properties COMMAND p1phot_tests properties)
add_test(NAME p1phot_oracle COMMAND p1phot_tests oracle)
add_test(NAME p1phot_negative COMMAND p1phot_tests negative)

# performance 基线 (独立可执行, 时长敏感与 core 组隔离)
add_executable(p1phot_perf_test
  ${P1PHOT_DIR}/p1phot_tests_perf.cpp
  ${P1PHOT_DIR}/p1phot_gaia_stub.cpp
  ${P1PHOT_PROD_SOURCES})
target_include_directories(p1phot_perf_test PRIVATE
  ${P1PHOT_DIR}
  ${P1PHOT_INC}
  ${P1PHOT_SRC}
  ${P1PHOT_LIB}/gaia_xpsd_client/src
  ${P1PHOT_LIB}/phase1/photometry
  ${P1PHOT_ROOT}/include)
target_link_libraries(p1phot_perf_test PRIVATE ${P1PHOT_OMP_TARGET})
add_test(NAME p1phot_performance COMMAND p1phot_perf_test)

# 故障注入必败自检 (验收: baseline 必 PASS + 注入必 FAIL 双向排除恒常)
add_executable(p1phot_selfcheck_test
  ${P1PHOT_DIR}/p1phot_tests_selfcheck.cpp
  ${P1PHOT_DIR}/p1phot_tests_units.cpp
  ${P1PHOT_DIR}/p1phot_tests_properties.cpp
  ${P1PHOT_DIR}/p1phot_tests_oracle.cpp
  ${P1PHOT_DIR}/p1phot_tests_negative.cpp
  ${P1PHOT_DIR}/p1phot_gaia_stub.cpp
  ${P1PHOT_PROD_SOURCES})
target_include_directories(p1phot_selfcheck_test PRIVATE
  ${P1PHOT_DIR}
  ${P1PHOT_INC}
  ${P1PHOT_SRC}
  ${P1PHOT_LIB}/gaia_xpsd_client/src
  ${P1PHOT_LIB}/phase1/photometry
  ${P1PHOT_ROOT}/include)
target_link_libraries(p1phot_selfcheck_test PRIVATE ${P1PHOT_OMP_TARGET})
add_test(NAME p1phot_selfcheck COMMAND p1phot_selfcheck_test)
