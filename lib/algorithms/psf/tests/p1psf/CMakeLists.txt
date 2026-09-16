# ============================================================================
# P1-PSF-TEST (TEST-P1-PSF-001) — lib/algorithms/psf 可复用验证面
# ----------------------------------------------------------------------------
# 控制包任务: P1-PSF-TEST (queue 45, lock-P1-PSF; 依赖 P1-PSF-DOC 闭环)。
# 合同锚: docs/algorithms/STAR_PSF_ALGORITHMS.md §11 TEST-PSF-DESIGN-001
# (P1-PSF-DOC 冻结); lib/algorithms/psf/README.md r1 + module.yaml
# (CONTRACT_READY, 7 导出 C API, legacy lib/algorithms/psf)。
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
# 被测面: 现状唯一生产实现 dpsf_* C API 七导出 (lib/algorithms/psf/src/
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

# ----------------------------------------------------------------------------
# B4-5 共址回归锁 (M3b-F-01): p1psf_prodpath_centroid — 质心门必须按生产路径
# 定义 (sdet 系初值, 不用 gate2 的 "+0.5 offset 匹配真值" 口径), 并以
# SCI-P1-STAR-001 §1 冻结绝对门 |Δc| <= 0.3px (SNR>=20 域) 断言;
# converged-init 仅作拟合器本征精度旁证门 (T2)。构型含 findings 要求的
# "非整数真值中心 + sdet 系初值", T3 为偏移初值拉回对照 (排除停驻)。
# 被测面: dpsf_fit_batch_f32 生产实现 (dpsf_psf.cpp)。
# ----------------------------------------------------------------------------
add_executable(p1psf_prodpath_check_test
  ${P1PSF_DIR}/p1psf_prodpath_check.cpp
  ${P1PSF_PROD_SOURCES})
target_include_directories(p1psf_prodpath_check_test PRIVATE
  ${P1PSF_INC}
  ${P1PSF_SRC}
  ${P1PSF_DIR})
target_link_libraries(p1psf_prodpath_check_test PRIVATE ${ASTROCS_OMP_TARGET})
add_test(NAME p1psf_prodpath_centroid COMMAND p1psf_prodpath_check_test)

# ----------------------------------------------------------------------------
# G-P1-CENTROID-1 生产路径绝对位置门 (SCI-FIX-PSF 第 2 项, R-3 §4.1) —
# 修复前无任何门链接 sdet + dpsf: 编排写端对 PSF 支路重复 -0.5 使两条支路
# 相差恰 0.5000 px, 而 ipv 去重阈值是严格 < 0.5 px ⇒ 同星双份进 ipv。
# 本 target 链接 **两条支路的真实生产源** (sdet 5 TU + dpsf 2 TU) 与生产坐标
# 契约头 star_coord_contract.h (orchestrator 写端/读端唯一事实源)。
# 判据与阈值来源: docs/science/GATES_AND_TOLERANCES.md G-P1-CENTROID-1 行
# (median <= 0.1 px / p95 <= 0.3 px / 双支路分离 <= 0.05 px / dpsf 系 0.05 px)。
# 正例 p1psf_centroid_gate 必 PASS; 负例 p1psf_centroid_gate_neg 用修复前映射
# 重算, 必须以 >= 0.4 px 分离被检出 (能红能绿, ENGINEERING_SPEC §8)。
# ----------------------------------------------------------------------------
set(P1PSF_GATE_SDET_DIR ${P1PSF_DIR}/../../../star_detection)
set(P1PSF_GATE_ORCH_INC
  ${P1PSF_DIR}/../../../../infrastructure/pipeline/orchestrator/cpp/include)
find_path(P1PSF_GATE_GSL_INCLUDE_DIR gsl/gsl_multifit_nlinear.h
  HINTS /usr/include /usr/local/include)
find_library(P1PSF_GATE_GSL_LIB NAMES gsl
  HINTS /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib /usr/local/lib)
find_library(P1PSF_GATE_GSLCBLAS_LIB NAMES gslcblas
  HINTS /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib /usr/local/lib)
if(NOT P1PSF_GATE_GSL_INCLUDE_DIR OR NOT P1PSF_GATE_GSL_LIB
   OR NOT P1PSF_GATE_GSLCBLAS_LIB)
  message(FATAL_ERROR
    "G-P1-CENTROID-1: GSL (gsl_multifit_nlinear + libgsl/libgslcblas) not found; "
    "sdet/dpsf 生产源依赖 GSL trust-region LM, 无法编译本门")
endif()
set(P1PSF_GATE_SDET_SRC
  ${P1PSF_GATE_SDET_DIR}/src/sdet_api.cpp
  ${P1PSF_GATE_SDET_DIR}/src/sdet_detector.cpp
  ${P1PSF_GATE_SDET_DIR}/src/sdet_image.cpp
  ${P1PSF_GATE_SDET_DIR}/src/sdet_log.cpp
  ${P1PSF_GATE_SDET_DIR}/src/sdet_background.cpp)
add_executable(p1psf_centroid_gate_test
  ${P1PSF_DIR}/p1psf_centroid_gate.cpp
  ${P1PSF_PROD_SOURCES}
  ${P1PSF_GATE_SDET_SRC})
target_include_directories(p1psf_centroid_gate_test PRIVATE
  ${P1PSF_INC}
  ${P1PSF_SRC}
  ${P1PSF_DIR}
  ${P1PSF_GATE_SDET_DIR}/include
  ${P1PSF_GATE_SDET_DIR}/src
  ${P1PSF_GATE_ORCH_INC}
  ${P1PSF_GATE_GSL_INCLUDE_DIR})
target_link_libraries(p1psf_centroid_gate_test PRIVATE
  ${P1PSF_GATE_GSL_LIB} ${P1PSF_GATE_GSLCBLAS_LIB} m ${ASTROCS_OMP_TARGET})
# sdet_log 写 CWD 相对日志 → WORKING_DIRECTORY 隔离; OMP_NUM_THREADS=1 固定
# 候选拟合归约序 (门要求逐位可复现)。
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/run)
add_test(NAME p1psf_centroid_gate COMMAND p1psf_centroid_gate_test)
add_test(NAME p1psf_centroid_gate_neg COMMAND p1psf_centroid_gate_test negative-injection)
set_tests_properties(p1psf_centroid_gate p1psf_centroid_gate_neg PROPERTIES
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/run
  # STAR_DETECTOR_LOG_LEVEL=3 (ERROR): 门只关心数值, 抑制 sdet 逐阶段 INFO 噪声
  ENVIRONMENT "OMP_NUM_THREADS=1;STAR_DETECTOR_LOG_LEVEL=3")
set_tests_properties(p1psf_centroid_gate_neg PROPERTIES
  PASS_REGULAR_EXPRESSION "NEGATIVE INJECTION DETECTED")

# ----------------------------------------------------------------------------
# G-P1-CENTER-CONTRACT-1 (W4-A1): psf 块像素中心口径门
# ----------------------------------------------------------------------------
# 缺陷背景 (OPEN_ITEMS A1 / 问题扫描 M1a-C-003): psf 块 cx/cy = dpsf 原始输出
# = index-is-center (0-based); 内部 0-based 与 FITS 1-based (CRPIX 1-based,
# Paper I §2.1.1) 之间**只允许一次** +1 桥接 (SCI-WCS-001 §3a/§5a)。修复前
# p1_op_wcs 把 0-based 数组下标直喂 1-based 契约的 WcsTan, 使发布工件
# p1_wcs.json.samples[] 恒差 1px, 且两道自检门与"独立参考解"共用同一原点
# ⇒ 对原点平移零判别力。
# 本门链接三条消费链的真实生产源 (photometry wcs_transform / snr snr_estimator
# +snr_science / 导出契约 wcs_tan) 与契约头 star_coord_contract.h; 判据
# [C1]..[C5] 见 p1psf_center_contract_gate.cpp 头注 (含原点鉴别力判据)。
# 负例 p1psf_center_contract_gate_neg 以修复前形态 (0-based 直喂 1-based)
# 注入, 必须以 >= 0.9 px 被检出 (能红能绿, ENGINEERING_SPEC §8);
# p1psf_center_contract_astropy 用 astropy.wcs 做第三方交叉 (fail-closed:
# astropy/二进制/导出缺失一律判红)。
# ----------------------------------------------------------------------------
set(P1PSF_CC_DIR ${P1PSF_DIR}/../../../photometry/cpp/src)
set(P1PSF_CC_SNR_DIR ${P1PSF_DIR}/../../../noise_snr/cpp)
set(P1PSF_CC_TAN_DIR ${P1PSF_DIR}/../../../platesolve/wrapper_phase1)
set(P1PSF_CC_ORCH_INC
  ${P1PSF_DIR}/../../../../infrastructure/pipeline/orchestrator/cpp/include)
set(P1PSF_CC_PROD_SOURCES
  ${P1PSF_CC_DIR}/wcs_transform.cpp
  ${P1PSF_CC_SNR_DIR}/src/snr_estimator.cpp
  ${P1PSF_CC_SNR_DIR}/src/snr_science.cpp
  ${P1PSF_CC_TAN_DIR}/wcs_tan.cpp)
add_executable(p1psf_center_contract_gate_test
  ${P1PSF_DIR}/p1psf_center_contract_gate.cpp
  ${P1PSF_CC_PROD_SOURCES})
target_include_directories(p1psf_center_contract_gate_test PRIVATE
  ${P1PSF_CC_DIR}
  ${P1PSF_CC_SNR_DIR}/include
  ${P1PSF_CC_TAN_DIR}
  ${P1PSF_CC_ORCH_INC}
  ${P1PSF_DIR})
target_link_libraries(p1psf_center_contract_gate_test PRIVATE m ${ASTROCS_OMP_TARGET})
add_test(NAME p1psf_center_contract_gate COMMAND p1psf_center_contract_gate_test)
add_test(NAME p1psf_center_contract_gate_neg
  COMMAND p1psf_center_contract_gate_test negative-injection)
set_tests_properties(p1psf_center_contract_gate_neg PROPERTIES
  PASS_REGULAR_EXPRESSION "NEGATIVE INJECTION DETECTED")
# astropy 第三方交叉 (独立实现, 零生产依赖; 自跑 C++ 门 --export 后对拍)。
# fail-closed: astropy 缺失 ⇒ 门判红 (不用 SKIP 掩盖)。
add_test(NAME p1psf_center_contract_astropy
  COMMAND ${CMAKE_COMMAND} -E env
          P1PSF_CENTER_GATE_BIN=$<TARGET_FILE:p1psf_center_contract_gate_test>
          python3 ${P1PSF_DIR}/p1psf_center_contract_astropy.py)
set_tests_properties(p1psf_center_contract_gate p1psf_center_contract_gate_neg
                     p1psf_center_contract_astropy PROPERTIES LABELS "p1psf")

