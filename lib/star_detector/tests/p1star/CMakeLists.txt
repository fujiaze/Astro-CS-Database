# ============================================================================
# P1-STAR-TEST (TEST-P1-STAR-001) — lib/star_detector 可复用验证面
# ----------------------------------------------------------------------------
# 控制包任务: P1-STAR-TEST (SA-P1ST-T, queue 49, lock-P1-STAR; 依赖
# P1-STAR-DOC 闭环)。合同锚: docs/algorithms/STAR_DETECTION_ALGORITHMS.md §11.4
# TEST-STAR-DESIGN-001 (P1-STAR-DOC 冻结, 2026-09-08, wave W3); 矩阵行
# P1-STAR (MOD astrocs.phase1.star_detection, TEST-P1-STAR-001); 冻结合同
# lib/star_detector/README.md (CONTRACT_READY)。
#
# 结构 (模块 tests/{unit,properties,oracle,fixtures,negative,performance} 对应;
# 结构先例 P1-HIPS-TEST commit c19b4a59 六组 CTest 对齐):
#   p1star/p1star_fixtures.hpp     — FIX-STAR-A..G 固定 seed fixture generator
#                                    (mt19937_64/splitmix64, 容差来源注记=§11.4)
#   p1star/p1star_oracle.hpp       — 独立 oracle (不调用被测函数; 构造参数即
#                                    期望 + 解析积分 + 全序比较器复算)
#   p1star/p1star_test_main.hpp    — 执行器框架 (FaultRegistry + 组 runner)
#   p1star/p1star_tests_core.cpp   — units/properties/oracle/negative 四组
#                                    (自带 main; negative 组 OOM 注入为
#                                    fork+execv /proc/self/exe 子进程扫描)
#   p1star/p1star_tests_perf.cpp   — performance 基线 (独立可执行自带 main;
#                                    parity/trend 比值哨兵 + bitwise)
#   p1star/p1star_tests_selfcheck.cpp — 故障注入必败自检 (baseline + 双注入点;
#                                    argv 子进程模式: execve /proc/self/exe
#                                    <group> 重入, 与 p1hips_selfcheck 同构)
#
# CTest 注册 (单跑: ctest -R p1star_):
#   p1star_units        — F1 召回/虚警/质心/FWHM (SNR 分谱) + F2 饱和/混合
#   p1star_properties   — F3 OMP 1/2/4 bitwise + mag 全序 + NaN 末尾 +
#                         maxStars 保最亮 + F4 u16 |Δc|≤0.5px
#   p1star_oracle       — F4 独立 oracle (0.05px/1e-3) + F6 回归锚
#   p1star_negative     — F5 合同负例 + OOM 注入扫描 (LD_PRELOAD
#                         sdet_oom_interposer.so, 批次 R 事务化 malloc 负例)
#   p1star_performance  — perf 基线哨兵 (parity 4x/trend 0.4x/bitwise)
#   p1star_selfcheck    — 故障注入必败自检 (baseline PASS + 注入 A/B 双注入点
#                         必败, 三阶段 rc 双向判定, 无 WILL_FAIL 依赖)
#
# 域基线: HEAD dc2f5f3e (批次 R sdet malloc 事务化发布, 17 处静默失败全修);
# 与 tests/unit/p1_stars_test.cpp (P1-003 桥接层 astrocs::phase1::StarDetector)
# 互补不重复 — 本验证面为生产 sdet_* C API。
#
# 注: 生产 5 src 不在根 CMake 构建 (独立 Makefile) — 此处直接编入测试目标,
# 不新建生产 target (边界: 仅测试面)。sdet_log 写 CWD 相对日志
# lib/star_detector/logs/ → 全部 add_test 带 WORKING_DIRECTORY 隔离。
# ============================================================================

set(P1STAR_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SDET_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
set(SDET_SRC
  ${SDET_SRC_DIR}/sdet_api.cpp
  ${SDET_SRC_DIR}/sdet_detector.cpp
  ${SDET_SRC_DIR}/sdet_image.cpp
  ${SDET_SRC_DIR}/sdet_log.cpp
  ${SDET_SRC_DIR}/sdet_background.cpp)

# GSL (sdet_api.cpp 依赖 gsl_multifit_nlinear; 主树根 CMake 无 GSL target,
# 本域自探 — 与生产 Makefile LDLIBS=-lgsl -lgslcblas 同源, 不改生产构建)。
# name 带 gsl/ 相对子目录 (生产源 #include <gsl/...>, find_path 不递归子目录)。
find_path(SDET_GSL_INCLUDE_DIR gsl/gsl_multifit_nlinear.h
  HINTS /usr/include /usr/local/include)
find_library(SDET_GSL_LIB NAMES gsl
  HINTS /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib /usr/local/lib)
find_library(SDET_GSLCBLAS_LIB NAMES gslcblas
  HINTS /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib /usr/local/lib)
if(NOT SDET_GSL_INCLUDE_DIR OR NOT SDET_GSL_LIB OR NOT SDET_GSLCBLAS_LIB)
  message(FATAL_ERROR
    "P1-STAR-TEST: GSL (gsl_multifit_nlinear + libgsl/libgslcblas) not found; "
    "生产 sdet_api.cpp 依赖 GSL trust-region LM, 无法编译验证面")
endif()
set(SDET_GSL_LINK ${SDET_GSL_LIB} ${SDET_GSLCBLAS_LIB} m)

# core 组可执行 (units/properties/oracle/negative 共享执行器框架;
# main 与组注册表在 p1star_tests_main.cpp — selfcheck 目标共享 core TUs
# 但不共享 main, 唯一性拆分, 先例 p1hips_tests_main.cpp 同构)
add_executable(p1star_tests
  ${P1STAR_DIR}/p1star_tests_main.cpp
  ${P1STAR_DIR}/p1star_tests_core.cpp
  ${SDET_SRC})
target_include_directories(p1star_tests PRIVATE
  ${SDET_GSL_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${P1STAR_DIR})
target_link_libraries(p1star_tests PRIVATE ${SDET_GSL_LINK} ${CMAKE_DL_LIBS})
# OpenMP (生产 sdet 内 #pragma omp; 根 OpenMP::OpenMP_CXX 若未找到则依赖编译器默认)
if(OpenMP_CXX_FOUND)
  target_link_libraries(p1star_tests PRIVATE OpenMP::OpenMP_CXX)
endif()
# 单组单跑: ctest -R p1star_units / p1star_properties / p1star_oracle / p1star_negative
# WORKING_DIRECTORY 隔离 sdet_log 写盘; OMP_NUM_THREADS=1 固定 malloc 序
# (负面组 OOM 注入序确定性; properties 组内部自行 set 1/2/4)。
add_test(NAME p1star_units COMMAND p1star_tests units)
add_test(NAME p1star_properties COMMAND p1star_tests properties)
add_test(NAME p1star_oracle COMMAND p1star_tests oracle)
set_tests_properties(p1star_units p1star_properties p1star_oracle PROPERTIES
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/run
  ENVIRONMENT "OMP_NUM_THREADS=1")

# negative 组: LD_PRELOAD interposer (第 N 次 malloc 失败; interposer 源与本
# 目标同目录, 参照批次 R 样板 run/local/bughunt_batchR/oom_inject.c)。
add_library(p1star_oom_interposer SHARED ${P1STAR_DIR}/sdet_oom_interposer.c)
set_target_properties(p1star_oom_interposer PROPERTIES
  OUTPUT_NAME sdet_oom_interposer PREFIX "" SUFFIX ".so" LINKER_LANGUAGE C)
add_dependencies(p1star_tests p1star_oom_interposer)
target_link_options(p1star_tests PRIVATE "LINKER:--no-as-needed")

# add_test 需 .so 与 run/ 就绪 → 用 CMake 生成 launcher 脚本设置 LD_PRELOAD
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/run)
set(P1STAR_LAUNCHER ${CMAKE_CURRENT_BINARY_DIR}/p1star_negative_launcher.sh)
file(GENERATE OUTPUT ${P1STAR_LAUNCHER}
  CONTENT "#!/bin/sh
# P1-STAR-TEST negative launcher: OOM interposer + 单线程 malloc 序确定性
# LD_PRELOAD 无法携带含空格路径 (glibc ld.so 按空格分词, 无转义机制) →
# 运行时把 .so 复制到无空格临时目录再预载; 探测子进程 execv 继承该值
OOMDIR=/tmp/astrocs_p1star
mkdir -p \"\$OOMDIR\" && cp \"$<TARGET_FILE:p1star_oom_interposer>\" \"\$OOMDIR/sdet_oom_interposer.so\" &&
  export LD_PRELOAD=\"\$OOMDIR/sdet_oom_interposer.so\"
export OMP_NUM_THREADS=1
cd \"\$1\" && shift
exec \"$<TARGET_FILE:p1star_tests>\" negative
")
add_test(NAME p1star_negative
  COMMAND /bin/sh ${P1STAR_LAUNCHER} ${CMAKE_CURRENT_BINARY_DIR}/run)
set_tests_properties(p1star_negative PROPERTIES
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/run)

# performance 基线 (独立可执行, 时长敏感与 core 组隔离)
add_executable(p1star_perf_test
  ${P1STAR_DIR}/p1star_tests_perf.cpp
  ${SDET_SRC})
target_include_directories(p1star_perf_test PRIVATE
  ${SDET_GSL_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${P1STAR_DIR})
target_link_libraries(p1star_perf_test PRIVATE ${SDET_GSL_LINK})
if(OpenMP_CXX_FOUND)
  target_link_libraries(p1star_perf_test PRIVATE OpenMP::OpenMP_CXX)
endif()
add_test(NAME p1star_performance COMMAND p1star_perf_test)
set_tests_properties(p1star_performance PROPERTIES
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/run
  ENVIRONMENT "OMP_NUM_THREADS=1")

# 故障注入必败自检 (结构先例 P1-HIPS-TEST c19b4a59: p1hips_selfcheck 单 test
# 三阶段 — baseline 必 PASS 排除恒 FAIL 侧, 注入 A/B (units/oracle 双注入点)
# 经 fork+execve /proc/self/exe <group> argv 子进程模式必 FAIL 排除恒 PASS 侧;
# 共享 core TUs, 注入经 ASTROCS_P1STAR_FAULT FaultRegistry 断言翻转)。
add_executable(p1star_selfcheck_test
  ${P1STAR_DIR}/p1star_tests_selfcheck.cpp
  ${P1STAR_DIR}/p1star_tests_core.cpp
  ${SDET_SRC})
target_include_directories(p1star_selfcheck_test PRIVATE
  ${SDET_GSL_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${P1STAR_DIR})
target_link_libraries(p1star_selfcheck_test PRIVATE ${SDET_GSL_LINK})
if(OpenMP_CXX_FOUND)
  target_link_libraries(p1star_selfcheck_test PRIVATE OpenMP::OpenMP_CXX)
endif()
add_test(NAME p1star_selfcheck COMMAND p1star_selfcheck_test)
set_tests_properties(p1star_selfcheck PROPERTIES
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/run
  ENVIRONMENT "OMP_NUM_THREADS=1;ASTROCS_P1STAR_FAULT="
  PASS_REGULAR_EXPRESSION "SELFCHECK PASS")
