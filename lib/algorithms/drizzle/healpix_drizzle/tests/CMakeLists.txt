# ============================================================================
# lib/algorithms/drizzle/healpix_drizzle/tests — 端到端验收面 (DRZ-FLUX-FIX-01)
# ----------------------------------------------------------------------------
# 背景: 本目录下的验收/冻结/Oracle/CLEAN 测试此前**全部 tracked-but-unbuilt**
# (本目录无 CMakeLists.txt, 全仓无任何 add_executable 引用它们, ctest 命中 0);
# 运行器 acceptance_drizzle.py 还写着 Windows 专名 drizzle_acceptance_test.exe。
# 本文件把这些测试真正编进根图并注册 ctest; 运行器改为跨平台定位。
#
# 门锚 (DRZ-FLUX-FIX-01 / FZ-COND-FLUX-CONSERV):
#   Σ_p sumFlux_p = Σ_j x_j  —— 核按 drop 面积归一 (F&H 2002 §7.2 式(7) 下方
#   "fractional area overlap of the drop"; drizzlepac cdrizzlebox.c dover/=jaco),
#   与 pixfrac 无关 (pixfrac 只决定 footprint 大小, 不收缩总流量)。
#   pixfrac 档位: {0.1, 0.5, 0.8, 1.0} × 过采样率 {1,2,3,4};
#   负例注入 --inject-legacy-pixfrac2 (把 Σout 乘回 pixfrac² 的旧口径) 必须判红。
#
# 注意: p1drz/ 有自己的 CMakeLists 且已由 eng/tests/unit/CMakeLists.txt 注册,
# 本文件不递归它 (避免重复 target)。
# ============================================================================
set(DRZ_TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(DRZ_MODULE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

# 各 test 的产物目录必须在**配置期**建好: 可执行以 fopen(outdir/xxx.jsonl)
# 落证据, 目录不存在时它只能返回 1 —— 那会让 WILL_FAIL 的负例门「因为写不出
# 文件」而假绿 (0.01s 通过)。这里显式建目录, 保证正例/负例都真的跑到判据。
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/drizzle_acceptance)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/drizzle_acceptance_core)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/drizzle_acceptance_neg)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/drizzle_acceptance_py)

# ----------------------------------------------------------------------------
# 1. 端到端合成验收 (A..F: 位置/pixfrac×过采样/双向投影/尺度/广域/数值类型)
#    参数: [outdir] [--inject-legacy-pixfrac2] [--inject-flux-factor <f>]
# ----------------------------------------------------------------------------
add_executable(drizzle_acceptance_test
  ${DRZ_TESTS_DIR}/drizzle_acceptance_test.cpp)
target_include_directories(drizzle_acceptance_test PRIVATE
  ${DRZ_MODULE_DIR})
target_compile_definitions(drizzle_acceptance_test PRIVATE
  AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(drizzle_acceptance_test PRIVATE
  astrocs_drizzle ${ASTROCS_OMP_TARGET})
# 常态门 = A..D + T 分片 (位置 / pixfrac×过采样 / 双向投影 / 尺度×nside /
# 数值类型)。实测 ~5 min (52 checks, 0 fail)。
# E 段 (广域大畸变, 1024² SIP order=5 @ nside=65536) 的**几何构建**是小时级
# (被测路径与被测口径无关: 每帧 3.9e6 个 target leaf 的自适应边界构建),
# 故独立成门 drizzle_acceptance_full, 用长超时跑完整矩阵 —— 避免把整门变成
# 「没人跑得完的门」; 两个门跑的是**同一可执行、同一判据**, 只差分片范围。
add_test(NAME drizzle_acceptance_core
  COMMAND drizzle_acceptance_test ${CMAKE_BINARY_DIR}/drizzle_acceptance_core
          --sections ABCDET)
set_tests_properties(drizzle_acceptance_core PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 3600)

add_test(NAME drizzle_acceptance_full
  COMMAND drizzle_acceptance_test ${CMAKE_BINARY_DIR}/drizzle_acceptance)
set_tests_properties(drizzle_acceptance_full PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 21600)
# 兼容旧名 (外部脚本/父代理登记可能引用 drizzle_acceptance)。
add_test(NAME drizzle_acceptance
  COMMAND drizzle_acceptance_test ${CMAKE_BINARY_DIR}/drizzle_acceptance)
set_tests_properties(drizzle_acceptance PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 21600)

# 负例控制: 同一个可执行 + 同一判据, 注入旧 pixfrac² 口径必须判红 (注入模式
# 首例即判 ⇒ 秒级)。
add_test(NAME drizzle_acceptance_negative_injection
  COMMAND drizzle_acceptance_test ${CMAKE_BINARY_DIR}/drizzle_acceptance_neg
          --inject-legacy-pixfrac2)
set_tests_properties(drizzle_acceptance_negative_injection PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 900
  WILL_FAIL TRUE)

# ----------------------------------------------------------------------------
# 2. Phase1 冻结验收 (覆盖/sampling/pixfrac/FOV/reverse/HISS 往返/科学保真)
# ----------------------------------------------------------------------------
add_executable(drizzle_freeze_test
  ${DRZ_TESTS_DIR}/drizzle_freeze_test.cpp)
target_include_directories(drizzle_freeze_test PRIVATE
  ${DRZ_MODULE_DIR})
target_compile_definitions(drizzle_freeze_test PRIVATE
  AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(drizzle_freeze_test PRIVATE
  astrocs_drizzle ${ASTROCS_OMP_TARGET})
add_test(NAME drizzle_freeze COMMAND drizzle_freeze_test)
set_tests_properties(drizzle_freeze PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 1800)

# ----------------------------------------------------------------------------
# 3. 候选零漏选 Oracle 矩阵 (9003 例; TEST-DRZ-CAND-001)
# ----------------------------------------------------------------------------
add_executable(candidate_oracle_test
  ${DRZ_TESTS_DIR}/candidate_oracle_test.cpp)
target_include_directories(candidate_oracle_test PRIVATE
  ${DRZ_MODULE_DIR})
target_compile_definitions(candidate_oracle_test PRIVATE
  AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(candidate_oracle_test PRIVATE
  astrocs_drizzle ${ASTROCS_OMP_TARGET})
# 注意: candidate_oracle_test 的 argv[1] 是 **JSONL 文件路径**(不是目录),
# 与 acceptance_drizzle.py 读 `<out_dir>/candidate_matrix.jsonl` 的口径对齐。
add_test(NAME drizzle_candidate_oracle
  COMMAND candidate_oracle_test
          ${CMAKE_BINARY_DIR}/drizzle_acceptance/candidate_matrix.jsonl)
set_tests_properties(drizzle_candidate_oracle PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 1800)

# ----------------------------------------------------------------------------
# 4. control 中值 MC 一致性 (Phase2 UPM 的证据面; 2000 次 MC)
# ----------------------------------------------------------------------------
add_executable(control_median_mc_test
  ${DRZ_TESTS_DIR}/control_median_mc_test.cpp)
target_include_directories(control_median_mc_test PRIVATE
  ${DRZ_MODULE_DIR})
target_compile_definitions(control_median_mc_test PRIVATE
  AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(control_median_mc_test PRIVATE
  astrocs_drizzle ${ASTROCS_OMP_TARGET})
add_test(NAME drizzle_control_median_mc COMMAND control_median_mc_test)
set_tests_properties(drizzle_control_median_mc PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 1800)

# ----------------------------------------------------------------------------
# 5. 球面重叠面 + 通量守恒解析闭合 (drop 面积归一: Σ_p sumFlux = L_j)
# ----------------------------------------------------------------------------
add_executable(test_spherical_overlap
  ${DRZ_TESTS_DIR}/test_spherical_overlap.cpp)
target_include_directories(test_spherical_overlap PRIVATE
  ${DRZ_MODULE_DIR})
target_compile_definitions(test_spherical_overlap PRIVATE
  AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(test_spherical_overlap PRIVATE
  astrocs_drizzle ${ASTROCS_OMP_TARGET})
add_test(NAME drizzle_spherical_overlap COMMAND test_spherical_overlap)
set_tests_properties(drizzle_spherical_overlap PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 600)

# ----------------------------------------------------------------------------
# 6. Python 驱动验收 (numpy 聚合断言 + 跨平台 exe 定位)
# ----------------------------------------------------------------------------
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME drizzle_acceptance_py
    COMMAND ${Python3_EXECUTABLE} ${DRZ_TESTS_DIR}/acceptance_drizzle.py
            --exe $<TARGET_FILE:drizzle_acceptance_test>
            --out ${CMAKE_BINARY_DIR}/drizzle_acceptance_py)
  set_tests_properties(drizzle_acceptance_py PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 14400)
endif()

# ----------------------------------------------------------------------------
# 7. 同目录其余**门类**测试 (DRZ-FLUX-FIX-01 同批从 tracked-but-unbuilt 编入)
#    说明: 这些源与上列 5 个同属本目录验收面, 此前同样没有任何 add_executable
#    引用它们。此处统一编入; 只有 `drizzle_science_matrix` 被注册为 ctest
#    (它产出 acceptance_drizzle.py 消费的 ulp_distribution.json/leaf_comparison.jsonl),
#    其余作为**可执行证据件**保留 (可手工运行), 不注册 —— 避免把与本轮无关的
#    历史红灯带进 ctest 基线。
# ----------------------------------------------------------------------------
set(DRZ_EXTRA_TESTS
  variance_propagation_test
  oracle_edge_crossing_test
  oracle_independent_test
  # drizzle_nonfinite_test 需要 coverage 的 astro/phase2/integrate.h + astrocs_phase2
  # (跨模块依赖), 单独处理见下。
  concurrency_cache_test
  p0_sip_order_guard_test
  drizzle_l0_test
  drizzle_l2_test
  test_nside_pixfrac
  kcorr_matrix_test
  reverse_api_test
  reverse_drizzle_test
  reverse_drizzle_science_test
  drizzle_science_completion_test)

foreach(_t ${DRZ_EXTRA_TESTS})
  add_executable(${_t} ${DRZ_TESTS_DIR}/${_t}.cpp)
  target_include_directories(${_t} PRIVATE ${DRZ_MODULE_DIR})
  target_compile_definitions(${_t} PRIVATE AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
  target_link_libraries(${_t} PRIVATE astrocs_drizzle ${ASTROCS_OMP_TARGET})
  set_target_properties(${_t} PROPERTIES EXCLUDE_FROM_ALL FALSE)
endforeach()

# 7b. drizzle_nonfinite_test: 依赖 coverage 的头与库 (astro/phase2/integrate.h、
#     astrocs_phase2), 单独给 include/链接面。
add_executable(drizzle_nonfinite_test
  ${DRZ_TESTS_DIR}/drizzle_nonfinite_test.cpp)
target_include_directories(drizzle_nonfinite_test PRIVATE
  ${DRZ_MODULE_DIR}
  ${CMAKE_SOURCE_DIR}/lib/algorithms/coverage/include)
target_compile_definitions(drizzle_nonfinite_test PRIVATE
  AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(drizzle_nonfinite_test PRIVATE
  astrocs_drizzle astrocs_phase2 ${ASTROCS_OMP_TARGET})

# 8. 科学矩阵 (ulp_distribution.json + leaf_comparison.jsonl 的生产者)
#    产物落 acceptance 的同一 outdir: strategies 与 runner 的 G 段口径一致。
add_executable(drizzle_science_matrix_test
  ${DRZ_TESTS_DIR}/drizzle_science_matrix_test.cpp)
target_include_directories(drizzle_science_matrix_test PRIVATE ${DRZ_MODULE_DIR})
target_compile_definitions(drizzle_science_matrix_test PRIVATE
  AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(drizzle_science_matrix_test PRIVATE
  astrocs_drizzle ${ASTROCS_OMP_TARGET})
add_test(NAME drizzle_science_matrix
  COMMAND drizzle_science_matrix_test ${CMAKE_BINARY_DIR}/drizzle_acceptance)
set_tests_properties(drizzle_science_matrix PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 3600)
# ============================================================================
# 9. WIRING-W34-01 (CHK-PROD-WIRING W4): 本目录最后 9 个无构建目标源
# ----------------------------------------------------------------------------
# 背景: 第 7/8 段已把 13 个门类测试编入; 剩余 9 个仍无任何构建目标:
#   mini_sip1000            P0-1 SIP order 越界负例 driver (A_ORDER=1000)
#   reference_overlap       Phase C3.2/C2.3 独立参考路径 (蒙特卡洛) 交叉验证
#   representative_probe    代表性 single-frame 生产探针 (1024^2 合成)
#   probe_reverse_steps     反向 drizzle 步进探针
#   bench_drizzle / bench_write / verify_bench / hiss_write_probe  剖析驱动
#   drizzle_l3_full_fp32    **Windows 专用** (windows.h/psapi.h) ⇒ 已删, 见报告
#
# 口径（与第 7 段同一纪律）:
#   * 门类测试 (能红能绿、判据确定) ⇒ add_executable + add_test;
#   * 探针/剖析驱动 ⇒ add_executable 编入构建图 (零漂移保证: 引擎接口一变即编译
#     失败), **不注册 ctest** —— 它们是计时/产物剖析件, 不是判据门, 注册进 CI 会
#     把机器负载差异当失败。一次性运行证据见 run/WIRING-W34-01/evidence/。
# ============================================================================
set(DRZ_PROBE_TESTS
  representative_probe
  probe_reverse_steps
  bench_drizzle
  bench_write
  verify_bench
  hiss_write_probe)
foreach(_t ${DRZ_PROBE_TESTS})
  add_executable(${_t} ${DRZ_TESTS_DIR}/${_t}.cpp)
  target_include_directories(${_t} PRIVATE
    ${DRZ_MODULE_DIR}
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src
    ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/include)
  target_compile_definitions(${_t} PRIVATE AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
  target_link_libraries(${_t} PRIVATE
    astrocs_drizzle astrocs_hips ${ASTROCS_OMP_TARGET})
endforeach()

# 9b. mini_sip1000: P0-1 SIP order 越界负例 driver。
#     修复后 hp_drizzle_run 对 A_ORDER=1000 必须返回 -10 (order 域 [0,5]) 且
#     零越界 ⇒ 无 ASAN 亦可判红绿; 注册 ctest 使其成为常驻负例门。
add_executable(mini_sip1000 ${DRZ_TESTS_DIR}/mini_sip1000.cpp)
target_include_directories(mini_sip1000 PRIVATE ${DRZ_MODULE_DIR})
target_compile_definitions(mini_sip1000 PRIVATE
  AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(mini_sip1000 PRIVATE
  astrocs_drizzle ${ASTROCS_OMP_TARGET})
add_test(NAME drizzle_sip_order_guard_mini COMMAND mini_sip1000)
set_tests_properties(drizzle_sip_order_guard_mini PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 300)

# 9c. reference_overlap: 独立蒙特卡洛参考路径 (不复用被测 compute_overlap_area_g)。
add_executable(reference_overlap ${DRZ_TESTS_DIR}/reference_overlap.cpp)
target_include_directories(reference_overlap PRIVATE ${DRZ_MODULE_DIR})
target_compile_definitions(reference_overlap PRIVATE
  AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(reference_overlap PRIVATE
  astrocs_drizzle ${ASTROCS_OMP_TARGET})
# **不注册 ctest**: 一次性实跑该独立蒙特卡洛参考路径得 "32 通过, 3 失败"
# (run/WIRING-W34-01/evidence/reference_overlap.txt): 三条断言分别差
# 3.4e-19 (tol 3.0e-20)、2.6e-6 (tol 4.1e-13)、rel_err 1.04e-6 (tol 1e-6)。
# 容差与断言属科学判据面, 本轮禁止放松/删改 ⇒ 只编入构建图 (零漂移), 红灯
# 上呈前台裁决 (WIRING-W34-01 报告 §W4-E #2)。
# add_test(NAME drizzle_reference_overlap COMMAND reference_overlap)

# 9d. drizzle_l3_full_fp32: **Windows 专用** L3/Gate D 验收 driver
#     (windows.h + psapi.h 的 PeakWorkingSetSize; 见源文件 :16-17)。
#     正式平台含 Windows x64 ⇒ 该门保留, 但只在 WIN32 配置下成为构建目标
#     (Linux 构建图里没有它, 也不应有 —— 它依赖 Win32 API)。
#     Linux 侧的 FP32 通量闭合/无 NaN 判据由已注册的 drizzle_acceptance /
#     drizzle_freeze / drizzle_l0 覆盖。
if(WIN32)
  add_executable(drizzle_l3_full_fp32 ${DRZ_TESTS_DIR}/drizzle_l3_full_fp32.cpp)
  target_include_directories(drizzle_l3_full_fp32 PRIVATE ${DRZ_MODULE_DIR})
  target_compile_definitions(drizzle_l3_full_fp32 PRIVATE
    AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
  target_link_libraries(drizzle_l3_full_fp32 PRIVATE
    astrocs_drizzle ${ASTROCS_OMP_TARGET})
  add_test(NAME drizzle_l3_full_fp32 COMMAND drizzle_l3_full_fp32)
  set_tests_properties(drizzle_l3_full_fp32 PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 1800)
endif()

# ============================================================================
# 10. DRZ-PF-CORRECT-01 (S1 第 18 条): pixfrac<1 面亮度归一的**在册用例**
# ----------------------------------------------------------------------------
# 背景: 第 7 段的 DRZ_EXTRA_TESTS 只编入不注册; 而 variance_propagation_test 的
# oracle 旧版用覆盖率 sumArea 当分母、唯一调用点只用 pixfrac=1.0 ⇒ "分母口径取错"
# 在结构上测不到。本轮已把 oracle 期望式改为科学正本口径
# (S_p=sumFlux/sumNorm、var_p=sumVarNum/sumNorm²; docs/science/DRIZZLE.md §5:50/83)
# 并把 pf∈{1.0,0.8,0.5,0.25} 全部纳入判据 ⇒ 该可执行成为**在册**门。
# 实测 ~70 s (含 MC); 超时 1800 s 留 25x 余量。
# ============================================================================
# 名字前缀 drizzle_pf_sb_* 与 10b 的两个直写门同族 ⇒ 注册项 CHK-DRZ-PF-AREA-S1
# 用单条 ctest 正则 ^drizzle_pf_sb.*$ 一次覆盖「oracle 幂次 + 产品级正例 + 负例注入」。
add_test(NAME drizzle_pf_sb_variance COMMAND variance_propagation_test)
set_tests_properties(drizzle_pf_sb_variance PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 1800)

# ----------------------------------------------------------------------------
# 10b. drizzle_pf_sb_gate: **产品级**门 — 两个 HiPS 直写末端在 pixfrac<1 上
#      发布的 signal/variance 必须是面亮度口径 (读回落盘 FITS, 非只看累加器)。
#      profile=0 = write_hips_direct (生产管线 orchestrator -> hp_drizzle_run_hips
#      走这一支), profile=1 = write_hips_phase1。
#      负例 (同一可执行 + 同一判据): ASTROCS_DRZ_SB_FAULT=legacy_dp_normalization
#      复现修复前缺陷 (直写漏乘 k=D_p/N_p) ⇒ 必须判红 (WILL_FAIL)。
# ----------------------------------------------------------------------------
add_executable(drizzle_pf_sb_gate ${DRZ_TESTS_DIR}/drizzle_pf_sb_gate.cpp)
target_include_directories(drizzle_pf_sb_gate PRIVATE
  ${DRZ_MODULE_DIR}
  ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/src
  ${CMAKE_SOURCE_DIR}/lib/infrastructure/aio/include)
target_compile_definitions(drizzle_pf_sb_gate PRIVATE
  AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(drizzle_pf_sb_gate PRIVATE
  astrocs_drizzle astrocs_hips ${ASTROCS_OMP_TARGET})
add_test(NAME drizzle_pf_sb_gate
  COMMAND drizzle_pf_sb_gate ${CMAKE_BINARY_DIR}/drizzle_pf_sb_gate)
set_tests_properties(drizzle_pf_sb_gate PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 600)
add_test(NAME drizzle_pf_sb_gate_legacy_injection
  COMMAND drizzle_pf_sb_gate ${CMAKE_BINARY_DIR}/drizzle_pf_sb_gate_neg)
set_tests_properties(drizzle_pf_sb_gate_legacy_injection PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 600
  WILL_FAIL TRUE
  ENVIRONMENT "ASTROCS_DRZ_SB_FAULT=legacy_dp_normalization")

