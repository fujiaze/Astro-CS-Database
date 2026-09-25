# ============================================================================
# P1-PHOT-TEST (TEST-P1-PHOT-001) — lib/algorithms/photometry 可复用验证面
# ----------------------------------------------------------------------------
# 控制包任务: P1-PHOT-TEST (lock-P1-PHOT; 依赖 P1-PHOT-DOC 闭环)。合同锚:
# docs/algorithms/PHOTOMETRIC_FIT.md §13.4 TEST-PHOT-DESIGN-001 (P1-PHOT-DOC
# 冻结, 2026-09-07); docs/science/PHOTOMETRY.md §11 (SCI-PHOT-001 FROZEN);
# 矩阵行 P1-PHOT (MOD-astrocs-phase1-photometry, API-PHOT-001 六导出)。
#
# 结构 (模块 eng/tests/{unit,properties,oracle,fixtures,negative,performance} 对应):
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
#   p1phot/p1phot_fixgates.cpp        — B4-2/B4-3 回归锁 (M3-C-001 参考星数门
#                                     |r_consistent|>=3 / |r_inliers|>=2;
#                                     M3-C-002 quality_flags 饱和有效域过滤)
#
# 被测面: 现状唯一生产实现 lib/algorithms/photometry/cpp/src 五 TU (pc_api/
# star_matcher/wcs_transform/image_corrector/spectrum_integrator, C ABI 六导出
# API-PHOT-001) + lib/algorithms/photometry/wrapper_phase1/photometer.cpp (F6 legacy 旧符号,
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
#   p1phot_fixgates     — B4-2/B4-3 回归锁 (参考星数门 + 饱和质量位门)
# ============================================================================

set(P1PHOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(P1PHOT_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp/src)
set(P1PHOT_INC ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp/include)
set(P1PHOT_LIB ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)
set(P1PHOT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)

# 被测域生产源 (原样编译, 零修改)
set(P1PHOT_PROD_SOURCES
  ${P1PHOT_SRC}/pc_api.cpp
  ${P1PHOT_SRC}/star_matcher.cpp
  ${P1PHOT_SRC}/wcs_transform.cpp
  ${P1PHOT_SRC}/image_corrector.cpp
  ${P1PHOT_SRC}/spectrum_integrator.cpp
  # PHOT-MXY-01: 低阶乘性空间增益 m(x,y) 的拟合 + 唯一空间场求值/施加实现
  # (spatial_gain.cpp 只 include photometry_apply.h 的基函数与内联求值; 单一实现)
  ${P1PHOT_SRC}/spatial_gain.cpp
  ${P1PHOT_LIB}/algorithms/calibration/src/photometry_apply.cpp
  # PHOT-MXY-01: 可辨识性判据 = 仓内唯一实现 (coverage/src/identifiability.cpp,
  # 与 astrocs_phase2 同一份源码; 测试 target 沿用「生产源零修改编入」先例)
  ${P1PHOT_LIB}/algorithms/coverage/src/identifiability.cpp
  # F6 legacy 旧符号 (README §9; 只读引用, 不改生产面)
  ${P1PHOT_LIB}/algorithms/photometry/wrapper_phase1/photometer.cpp
)

# PHOT-MXY-01 追加 include 面: calibration/src (photometry_apply.h 扁平头名)
# + coverage/include (astro/phase2/identifiability.h)
set(P1PHOT_EXTRA_INC
  ${P1PHOT_LIB}/algorithms/calibration/src
  ${P1PHOT_LIB}/algorithms/coverage/include)

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
  ${P1PHOT_DIR}/p1phot_tests_determinism.cpp
  ${P1PHOT_DIR}/p1phot_tests_spatial.cpp
  ${P1PHOT_DIR}/p1phot_gaia_stub.cpp
  ${P1PHOT_PROD_SOURCES})
target_include_directories(p1phot_tests PRIVATE
  ${P1PHOT_DIR}
  ${P1PHOT_INC}
  ${P1PHOT_SRC}
  ${P1PHOT_LIB}/infrastructure/gaia_xpsd_client/src
  ${P1PHOT_LIB}/algorithms/photometry/wrapper_phase1
  ${P1PHOT_EXTRA_INC}
  ${P1PHOT_ROOT}/lib/include)
target_link_libraries(p1phot_tests PRIVATE ${P1PHOT_OMP_TARGET})
# 单组单跑: ctest -R p1phot_units / p1phot_properties / p1phot_oracle / p1phot_negative
add_test(NAME p1phot_units COMMAND p1phot_tests units)
add_test(NAME p1phot_properties COMMAND p1phot_tests properties)
add_test(NAME p1phot_oracle COMMAND p1phot_tests oracle)
add_test(NAME p1phot_negative COMMAND p1phot_tests negative)
# 到达顺序不变性 + 线程数扫描 (1/2/4/8): §13.4 I5 只扫线程数、样本序恒定 ⇒
# 本组把"样本序"变成被控自变量 (判据口径 = docs/contracts/TEST_MATRIX.md §2)。
add_test(NAME p1phot_determinism COMMAND p1phot_tests determinism)
# PHOT-MXY-01: 低阶乘性空间增益 m(x,y) —— 正例恢复 + 四条负例（真值无效应归零 /
# 分布不足声明降级 / 开关关闭逐位一致 / 判据能红能绿）。
add_test(NAME p1phot_spatial COMMAND p1phot_tests spatial)

# performance 基线 (独立可执行, 时长敏感与 core 组隔离)
add_executable(p1phot_perf_test
  ${P1PHOT_DIR}/p1phot_tests_perf.cpp
  ${P1PHOT_DIR}/p1phot_gaia_stub.cpp
  ${P1PHOT_PROD_SOURCES})
target_include_directories(p1phot_perf_test PRIVATE
  ${P1PHOT_DIR}
  ${P1PHOT_INC}
  ${P1PHOT_SRC}
  ${P1PHOT_LIB}/infrastructure/gaia_xpsd_client/src
  ${P1PHOT_LIB}/algorithms/photometry/wrapper_phase1
  ${P1PHOT_EXTRA_INC}
  ${P1PHOT_ROOT}/lib/include)
target_link_libraries(p1phot_perf_test PRIVATE ${P1PHOT_OMP_TARGET})
add_test(NAME p1phot_performance COMMAND p1phot_perf_test)

# 故障注入必败自检 (验收: baseline 必 PASS + 注入必 FAIL 双向排除恒常)
add_executable(p1phot_selfcheck_test
  ${P1PHOT_DIR}/p1phot_tests_selfcheck.cpp
  ${P1PHOT_DIR}/p1phot_tests_units.cpp
  ${P1PHOT_DIR}/p1phot_tests_properties.cpp
  ${P1PHOT_DIR}/p1phot_tests_oracle.cpp
  ${P1PHOT_DIR}/p1phot_tests_negative.cpp
  ${P1PHOT_DIR}/p1phot_tests_determinism.cpp
  ${P1PHOT_DIR}/p1phot_tests_spatial.cpp
  ${P1PHOT_DIR}/p1phot_gaia_stub.cpp
  ${P1PHOT_PROD_SOURCES})
target_include_directories(p1phot_selfcheck_test PRIVATE
  ${P1PHOT_DIR}
  ${P1PHOT_INC}
  ${P1PHOT_SRC}
  ${P1PHOT_LIB}/infrastructure/gaia_xpsd_client/src
  ${P1PHOT_LIB}/algorithms/photometry/wrapper_phase1
  ${P1PHOT_EXTRA_INC}
  ${P1PHOT_ROOT}/lib/include)
target_link_libraries(p1phot_selfcheck_test PRIVATE ${P1PHOT_OMP_TARGET})
add_test(NAME p1phot_selfcheck COMMAND p1phot_selfcheck_test)

# ----------------------------------------------------------------------------
# B4-2/B4-3 共址回归锁 (M3-C-001 SCI-PHOT 参考星数门 / M3-C-002 饱和质量位门):
#   p1phot_fixgates — |r_consistent|>=3 / |r_inliers|>=2 门 + quality_flags
#   有效域过滤 (饱和星不得进入零点拟合) + 对照用例排除恒真/恒假。
# 被测面: star_matcher.cpp 生产实现 (pc::StarMatcher::cleanAndScale)。
# ----------------------------------------------------------------------------
add_executable(p1phot_fixgates_test
  ${P1PHOT_DIR}/p1phot_fixgates.cpp
  ${P1PHOT_SRC}/star_matcher.cpp
  ${P1PHOT_SRC}/wcs_transform.cpp)
target_include_directories(p1phot_fixgates_test PRIVATE
  ${P1PHOT_DIR}
  ${P1PHOT_INC}
  ${P1PHOT_SRC}
  ${P1PHOT_ROOT}/lib/include)
target_link_libraries(p1phot_fixgates_test PRIVATE ${P1PHOT_OMP_TARGET})
add_test(NAME p1phot_fixgates COMMAND p1phot_fixgates_test)
