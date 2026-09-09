# ============================================================================
# P1-PSF-TEST (TEST-P1-PSF-001) — lib/dynamic_psf 可复用验证面
# ----------------------------------------------------------------------------
# 控制包任务: P1-PSF-TEST (queue 45, lock-P1-PSF; 依赖 P1-PSF-DOC 闭环)。
# 合同锚: docs/algorithms/STAR_PSF_ALGORITHMS.md §11 TEST-PSF-DESIGN-001
# (P1-PSF-DOC 冻结); lib/dynamic_psf/README.md r1 + module.yaml
# (CONTRACT_READY, 7 导出 C API, legacy lib/dynamic_psf)。
#
# 结构 (模块 tests/{unit,properties,oracle,fixtures,negative,performance} 对应):
#   p1psf/p1psf_fixtures.hpp      — FIX-PSF-A..E/PREF 固定 seed fixture
#                                   (splitmix64; 解析 Moffat4 场, 不内嵌生产算法)
#   p1psf/p1psf_oracle.hpp        — 独立 oracle (旋转主轴投影直接公式, 与生产
#                                   p1/p2/p3 二次型无共享代码; 期望值不经被测函数)
#   p1psf/p1psf_test_main.hpp     — 执行器框架 (FaultRegistry + 组 runner)
#   p1psf/p1psf_tests_core.cpp    — units/properties/oracle/negative/boundary 五组
#   p1psf/p1psf_tests_perf.cpp    — performance 基线 (parity 4x + E.7 加速比 1.60)
#   p1psf/p1psf_tests_selfcheck.cpp — 故障注入必败自检 (baseline+injection 双向)
#
# CTest 注册 (单跑: ctest -R p1psf_):
#   p1psf_units / p1psf_properties / p1psf_oracle / p1psf_negative /
#   p1psf_boundary / p1psf_performance / p1psf_selfcheck
#
# 被测面: 现状唯一生产实现 dpsf_* C API 七导出 (lib/dynamic_psf/src/
# dpsf_psf.cpp + dpsf_log.cpp, 生产源零改动; 根构建无 dynamic_psf 生产
# target, 生产源独立编入测试目标 — 对齐 p1star 先例)。
# ============================================================================

set(P1PSF_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(P1PSF_SRC ${P1PSF_DIR}/../../src)
set(P1PSF_INC ${P1PSF_DIR}/../../include)

# 被测生产源 (独立编入测试目标, 不新建生产 target)
set(P1PSF_PROD_SOURCES
  ${P1PSF_SRC}/dpsf_psf.cpp
  ${P1PSF_SRC}/dpsf_log.cpp)

# core 可执行 (units/properties/oracle/negative/boundary 共享执行器框架)
add_executable(p1psf_tests
  ${P1PSF_DIR}/p1psf_tests_main.cpp
  ${P1PSF_DIR}/p1psf_tests_core.cpp
  ${P1PSF_PROD_SOURCES})
target_include_directories(p1psf_tests PRIVATE
  ${P1PSF_INC}
  ${P1PSF_SRC}
  ${P1PSF_DIR})
target_link_libraries(p1psf_tests PRIVATE ${ASTROCS_OMP_TARGET})
# 单组单跑: ctest -R "p1psf_(units|properties|oracle|negative|boundary)"
add_test(NAME p1psf_units      COMMAND p1psf_tests units)
add_test(NAME p1psf_properties COMMAND p1psf_tests properties)
add_test(NAME p1psf_oracle     COMMAND p1psf_tests oracle)
add_test(NAME p1psf_negative   COMMAND p1psf_tests negative)
add_test(NAME p1psf_boundary   COMMAND p1psf_tests boundary)

# performance 基线 (独立可执行, 时长敏感与 core 组隔离)
add_executable(p1psf_perf_test
  ${P1PSF_DIR}/p1psf_tests_perf.cpp
  ${P1PSF_PROD_SOURCES})
target_include_directories(p1psf_perf_test PRIVATE
  ${P1PSF_INC}
  ${P1PSF_SRC}
  ${P1PSF_DIR})
target_link_libraries(p1psf_perf_test PRIVATE ${ASTROCS_OMP_TARGET})
add_test(NAME p1psf_performance COMMAND p1psf_perf_test)

# 故障注入必败自检 (验收: baseline 必 PASS + 注入必 FAIL 双向排除恒常)
add_executable(p1psf_selfcheck_test
  ${P1PSF_DIR}/p1psf_tests_selfcheck.cpp
  ${P1PSF_DIR}/p1psf_tests_core.cpp
  ${P1PSF_PROD_SOURCES})
target_include_directories(p1psf_selfcheck_test PRIVATE
  ${P1PSF_INC}
  ${P1PSF_SRC}
  ${P1PSF_DIR})
target_link_libraries(p1psf_selfcheck_test PRIVATE ${ASTROCS_OMP_TARGET})
add_test(NAME p1psf_selfcheck COMMAND p1psf_selfcheck_test)
