# ============================================================================
# P1-NOISE-TEST (TEST-P1-NOISE-001) — lib/algorithms/noise_snr (astrocs.p1.noise-snr)
# 可复用验证面
# ----------------------------------------------------------------------------
# 控制包任务: P1-NOISE-TEST (SA-P1N-T, queue 41, lock-P1-NOISE; 依赖
# P1-NOISE-DOC 闭环)。合同锚: docs/algorithms/NOISE_ESTIMATION.md §13.4
# TEST-NOISE-DESIGN-001 (P1-NOISE-DOC 冻结, 2026-09-07, wave W1); 矩阵行
# P1-NOISE (MOD astrocs.p1.noise-snr); 上游 SCI-NOISE-001..015 (NOISE_MODEL.md
# FROZEN T104 2026-08-23, 不改)。
#
# 结构 (模块 eng/tests/{unit,properties,oracle,fixtures,negative,performance} 对应):
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
# 被测面: noise 方差模型唯一生产实现 lib/algorithms/noise_snr/cpp/src/noise_model.cpp
# (snr_noise_model_v1(+_f64)/fill/free/scale_law/gain_variance; 根构建无
# snr_estimator CMake 目标 — README §1, 故本测试面从生产源独立编译
# EXCLUDE_FROM_ALL 静态目标 astrocs_p1_noise_prod, 生产源零改动);
# lib/algorithms/noise_snr/ 为 P1-NOISE 迁移目标目录 (P1-NOISE-IMPL 落码后本测试面
# 直接复用, 冻结容差不变)。
# ============================================================================

# P5-SNR: noise_model.cpp 的 snr_phot_cal_quality 现消费 snr_science.cpp 的
# snr_calib_zero_point_standard_error (零点标准误 1.253*sigma/sqrt(N)), 故生产源闭包
# 增加 snr_science.cpp (唯一权威实现, 零本地副本)。
set(P1NOISE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
# ---------------------------------------------------------------------------
# python3 解析必须在**任何** add_test 使用 ${P1NOISE_PYTHON3} 之前：旧位置在同文件
# :157（aba_layout 门之后）⇒ 全新 build 目录首次 configure 时该变量为空，命令退化成
# 裸 .py 路径（无执行位）⇒ p1noise_abi_layout = BAD_COMMAND 假红；再 configure 一次
# （缓存已写入）后 100% 通过 —— 绿色只取决于目录是否被 configure 过。
# 判据由 eng/tools/quality/check_cmake_usebeforedef.py 常设扫描（C1 use_before_define）。
# 缺 python3 一律 fail-closed（禁止静默退化）。
find_program(P1NOISE_PYTHON3 NAMES python3 python)
if(NOT P1NOISE_PYTHON3)
  message(FATAL_ERROR
    "p1noise 门需要 python3（p1noise_abi_layout / p1noise_numpy_oracle / "
    "p1noise_saturation_wiring）；请安装 python3 或显式传 -DP1NOISE_PYTHON3=<path>")
endif()
set(P1NOISE_PROD_SRC
  ${CMAKE_SOURCE_DIR}/lib/algorithms/noise_snr/cpp/src/noise_model.cpp
  ${CMAKE_SOURCE_DIR}/lib/algorithms/noise_snr/cpp/src/snr_science.cpp)
set(P1NOISE_PROD_INC ${CMAKE_SOURCE_DIR}/lib/algorithms/noise_snr/cpp/include)

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
# n12: 消费侧方差面可用性策略头 astrocs/noise/variance_plane_policy.h
target_include_directories(p1noise_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/algorithms/noise_snr/include)
target_link_libraries(p1noise_tests PRIVATE p1noise_under_test)
if(UNIX)
  target_link_libraries(p1noise_tests PRIVATE m)
endif()
# 单组单跑: ctest -R p1noise_units / p1noise_properties / p1noise_oracle /
#            p1noise_negative / p1noise_scale_law / p1noise_fill / p1noise_mask
add_test(NAME p1noise_units COMMAND p1noise_tests units)
add_test(NAME p1noise_properties COMMAND p1noise_tests properties)
add_test(NAME p1noise_oracle COMMAND p1noise_tests oracle)
add_test(NAME p1noise_negative COMMAND p1noise_tests negative)
add_test(NAME p1noise_scale_law COMMAND p1noise_tests scale_law)
add_test(NAME p1noise_fill COMMAND p1noise_tests fill)
# MASK-002 (claim SC-009): 源污染 oracle (含星帧) + 掩膜半径单调不变量。
# 旧 oracle/negative 面只有无星纯高斯帧 ⇒ 原理上发现不了半径错误
# (实测 r=0/8/60 px 输出逐位相同), 本组为结构性补强 (新增门, 不放宽任何旧断言)。
add_test(NAME p1noise_mask COMMAND p1noise_tests mask)
# ABI 版本化证据门 (claim SC-009): 结构布局探针 + 锁定文件比对 + 突变自检
add_test(NAME p1noise_abi_layout
         COMMAND ${P1NOISE_PYTHON3} ${P1NOISE_DIR}/p1noise_abi_layout_check.py)

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
target_include_directories(p1noise_selfcheck_test PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/algorithms/noise_snr/include)
target_link_libraries(p1noise_selfcheck_test PRIVATE p1noise_under_test)
if(UNIX)
  target_link_libraries(p1noise_selfcheck_test PRIVATE m)
endif()
add_test(NAME p1noise_selfcheck COMMAND p1noise_selfcheck_test)
# ============================================================================
# P5-SNR (2026-09-14, 负责人授权): 逐源 SNR 科学修正回归锁 (TEST-P5-SNR)
#   被测面 = snr_science.cpp (Horne 1986 / CCD 方程 / 5-sigma 深度 / 零点标准误)
#            + snr_estimator.cpp 生产控制点路径 (snr_extract_model_v3)
#   oracle = 测试内独立长双精度暴力实现 + NumPy oracle 锚值
#            (run/perf-fix/P5-snr/harness/snr_oracle.py)
#   OpenMP: snr_estimator.cpp 的 <omp.h> 已由 _OPENMP 守卫; 本目标无 OMP。
# ============================================================================
set(P5SNR_SRC
  ${CMAKE_SOURCE_DIR}/lib/algorithms/noise_snr/cpp/src/snr_science.cpp
  ${CMAKE_SOURCE_DIR}/lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp)
add_library(p5snr_under_test STATIC ${P5SNR_SRC})
target_include_directories(p5snr_under_test PUBLIC ${P1NOISE_PROD_INC})
set_target_properties(p5snr_under_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON)
if(UNIX)
  target_link_libraries(p5snr_under_test PUBLIC m)
endif()

add_executable(p1snr_science_test ${P1NOISE_DIR}/p1snr_science_test.cpp)
target_link_libraries(p1snr_science_test PRIVATE p5snr_under_test)
add_test(NAME p1snr_science_units COMMAND p1snr_science_test units)
add_test(NAME p1snr_science_oracle COMMAND p1snr_science_test oracle)
add_test(NAME p1snr_science_negative COMMAND p1snr_science_test negative)
add_test(NAME p1snr_science_production COMMAND p1snr_science_test production)
add_test(NAME p1snr_science_determinism COMMAND p1snr_science_test determinism)
# SCI-501 (FIX-407): sigma_sky 语义 / 读噪双计负例保护（MC 3sigma 双向）
add_test(NAME p1snr_science_skysource COMMAND p1snr_science_test skysource)

# ============================================================================
# SAT-001（claim SC-008）：饱和域门 —— 「饱和过滤在默认配置下必须生效，
#   若未提供电平则必须显式声明」。被测面 = 生产源 noise_model.cpp（零改动）
#   + 策略头 astrocs/noise/saturation_policy.h；负例 = 未提供电平（level=0）
#   时必须检出污染控制点与帧权重崩塌（gate 不得恒真）。
#   selfcheck 组证明判据双向可假（无源帧不报污染 / 有源帧必报污染）。
# ============================================================================
add_executable(p1noise_saturation_test ${P1NOISE_DIR}/p1noise_saturation_test.cpp)
target_link_libraries(p1noise_saturation_test PRIVATE p1noise_under_test)
target_include_directories(p1noise_saturation_test PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/algorithms/noise_snr/include)
if(UNIX)
  target_link_libraries(p1noise_saturation_test PRIVATE m)
endif()
add_test(NAME p1noise_saturation COMMAND p1noise_saturation_test contract)
add_test(NAME p1noise_saturation_selfcheck COMMAND p1noise_saturation_test selfcheck)

# ============================================================================
# P1-002 / M3-F-001: docs/science/NOISE_MODEL.md §11:117 + §15:140 承诺的
#   "**Python 参考**：NumPy 对同 data 的 median/MAD/5σ裁剪/平面最小二乘
#    复算 variance/ivar（rtol 1e-9）"
# 独立 Oracle（ENGINEERING_SPEC §5.1「不调用生产实现的独立 Oracle」）：
#   Python/NumPy 侧从同一 data 第一性原理复算，生产源仅作被测对象（零改动）；
#   numpy 或 g++ 缺失时脚本自身 SKIP 并返回 0。
# ============================================================================
# find_program(P1NOISE_PYTHON3 ...) 已上移到文件顶部（首次 configure 即需有效，
# 见该处注释与 eng/tools/quality/check_cmake_usebeforedef.py）。
if(P1NOISE_PYTHON3)
  add_test(NAME p1noise_numpy_oracle
           COMMAND ${P1NOISE_PYTHON3} ${P1NOISE_DIR}/noise_model_numpy_oracle.py)
  set_tests_properties(p1noise_numpy_oracle PROPERTIES LABELS "p1noise")

  # SAT-001 生产接线门（源级 + 默认值登记 + 文档一致性；负例注入见 SAT-001 自证摘要 §4）
  add_test(NAME p1noise_saturation_wiring
           COMMAND ${P1NOISE_PYTHON3} ${P1NOISE_DIR}/check_saturation_wiring.py)
  set_tests_properties(p1noise_saturation_wiring PROPERTIES LABELS "p1noise")
endif()
