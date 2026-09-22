# ============================================================================
# IPV-FAILCLOSED-TEST — lib/algorithms/platesolve/cpp/ipv 共址回归锁
# ----------------------------------------------------------------------------
# B4-1 (M1a-C-002): extract_wcs_sip TRANS 线性奇异必须显式失败
#   (SCI-WCS-001 §8 / ALG-WCS-001 §11.1; 宪章 §6.3 禁静默降级 / §14.4 fail-fast)。
# 断言面直接是 extract_wcs_sip 本体 (不是 iter_trans_solve 等邻接函数)。
#
# 被测生产源: src/ipv_wcs.cpp + src/ipv_sip.cpp (零改动直编, 与 ipv Makefile
# SRCS 同名同选项; 不新建生产 target — 对齐 p1star/p1psf 先例)。
#
# 注册方式: 本文件位于模块自带测试目录, 由 eng/tests/unit/CMakeLists.txt 一条
#   add_subdirectory(../../lib/algorithms/platesolve/cpp/ipv/test p1ipvtest)
# 挂入。该钩子属 B2 写域 (eng/tests/unit/CMakeLists.txt), 见 REPORT "越域残余"。
# ============================================================================

set(P1IPVTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
# CLEAN-403: 被测生产源 ipv_*.cpp 经 ipv_log.h 依赖 aio 机制原语
# (aio_atomic_file.h, header-only) ⇒ 本目录测试目标需要 aio/src include 面。
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../infrastructure/aio/src)
set(P1IPV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_executable(ipv_extract_wcs_sip_failclosed_test
  ${P1IPVTEST_DIR}/test_extract_wcs_sip_failclosed.cpp
  ${P1IPV_DIR}/src/ipv_wcs.cpp
  ${P1IPV_DIR}/src/ipv_sip.cpp)
target_include_directories(ipv_extract_wcs_sip_failclosed_test PRIVATE
  ${P1IPV_DIR}/include)
add_test(NAME ipv_extract_wcs_sip_failclosed
         COMMAND ipv_extract_wcs_sip_failclosed_test)

# ============================================================================
# P4-magiter — 极限星等割线迭代回归锁 (test/test_mag_iter.cpp)
# ----------------------------------------------------------------------------
# 被测函数 estimate_mag_lim_iterative 在 src/ipv_select.cpp (生产源, 零改动直编);
# 直接链接生产静态库 astrocs_p1_ipv (含 gaia_client.c), 不新建生产 target。
# 全部用例使用固定 stub 查询序列 —— 不依赖网络 / Gaia 数据 / 真实图像。
#   收敛性 / 确定性(逐位+金标准) / 触顶防御 / N=0 步长 / 查询次数上界 /
#   参数可覆盖(宪章 §10.4 禁硬编码) / 异常路径。
# ============================================================================
add_executable(ipv_mag_iter_test
  ${P1IPVTEST_DIR}/test_mag_iter.cpp)
target_include_directories(ipv_mag_iter_test PRIVATE
  ${P1IPV_DIR}/include)
target_link_libraries(ipv_mag_iter_test PRIVATE
  astrocs_p1_ipv astrocs_p1_sdet astrocs_aio astrocs_cfitsio)
add_test(NAME ipv_mag_iter COMMAND ipv_mag_iter_test)

# ============================================================================
# P14-N-10 (RQS V2-N-10 缺陷 1) — mag-iter 交付面回归锁
# ----------------------------------------------------------------------------
# 锁 converged / query_failed / capped / n_queries / mag_lim_final / alpha
# 全部落到交付结构 StarSelection (唯一映射点 mag_iter_apply_to_selection,
# 4 条 ipv_select 路径共用)。旧实现丢 converged/query_failed 且触顶记收敛。
# ============================================================================
add_executable(ipv_mag_iter_delivery_test
  ${P1IPVTEST_DIR}/test_mag_iter_delivery.cpp)
target_include_directories(ipv_mag_iter_delivery_test PRIVATE
  ${P1IPV_DIR}/include)
target_link_libraries(ipv_mag_iter_delivery_test PRIVATE
  astrocs_p1_ipv astrocs_p1_sdet astrocs_aio astrocs_cfitsio)
add_test(NAME ipv_mag_iter_delivery COMMAND ipv_mag_iter_delivery_test)

# ============================================================================
# ALG-WCS-001 §4a — 图像侧选星有效域 + 空扫描 provenance 回归锁
# ----------------------------------------------------------------------------
# 被测面: select_image_stars / compute_fov_density / estimate_mag_lim_iterative
# (生产源 src/ipv_select.cpp, 经生产静态库 astrocs_p1_ipv 链接)。
# 判据: 饱和检测不得进入选星样本 (真实场型: 最亮 N 颗全饱和); 候选不足不回填;
#       rho_img 分子 = 实际样本基数; 全空扫描必须标记 empty_sweep。
# 负向对照在测试内: 旧规则 (统一排序含饱和) 必须使同一判据判红 (非恒真门)。
# ============================================================================
add_executable(ipv_select_domain_test
  ${P1IPVTEST_DIR}/test_select_domain.cpp)
target_include_directories(ipv_select_domain_test PRIVATE
  ${P1IPV_DIR}/include)
target_link_libraries(ipv_select_domain_test PRIVATE
  astrocs_p1_ipv astrocs_p1_sdet astrocs_aio astrocs_cfitsio)
add_test(NAME ipv_select_domain COMMAND ipv_select_domain_test)
set_tests_properties(ipv_select_domain PROPERTIES TIMEOUT 120)

# ============================================================================
# ALG-WCS-001 §4b — 对外可见错误串 UTF-8 归一回归锁
# ----------------------------------------------------------------------------
# 被测面: ipv::utf8_safe_copy (生产源 src/ipv_entry.cpp) + 公共 ABI 失败路径写出的
# IpvWcsResult.error_msg。判据 = 独立严格 UTF-8 校验器 (RFC 3629)。
# 负向对照在测试内: 修复前的「按字节截断 + 原样透传」实现必须被判红。
# ============================================================================
add_executable(ipv_error_utf8_test
  ${P1IPVTEST_DIR}/test_error_utf8.cpp)
target_include_directories(ipv_error_utf8_test PRIVATE
  ${P1IPV_DIR}/include)
target_link_libraries(ipv_error_utf8_test PRIVATE
  astrocs_p1_ipv astrocs_p1_sdet astrocs_aio astrocs_cfitsio)
add_test(NAME ipv_error_utf8 COMMAND ipv_error_utf8_test)
set_tests_properties(ipv_error_utf8 PROPERTIES TIMEOUT 120)

# ============================================================================
# P11-IPV-BUDGET — 三角形匹配搜索预算 fail-closed 回归锁
# ----------------------------------------------------------------------------
# 被测面: triangle_match (src/ipv_triangle.cpp, 生产源) 经生产静态库
# astrocs_p1_ipv 链接。锁定: n=60 生产路径预算不触发且确定性; n>=66 越界
# 输入进入 O(n_A_tri·n_B_tri) 枚举前即 fail-closed, 有明确 fail_reason,
# 且在有界时间内返回 (不分配/不枚举)。
# ============================================================================
add_executable(ipv_triangle_budget_test
  ${P1IPVTEST_DIR}/test_triangle_budget.cpp)
target_include_directories(ipv_triangle_budget_test PRIVATE
  ${P1IPV_DIR}/include)
target_link_libraries(ipv_triangle_budget_test PRIVATE
  astrocs_p1_ipv astrocs_p1_sdet astrocs_aio astrocs_cfitsio)
add_test(NAME ipv_triangle_budget COMMAND ipv_triangle_budget_test)
set_tests_properties(ipv_triangle_budget PROPERTIES TIMEOUT 120)

# ============================================================================
# P18 (RQS V2-N-01, P0) — IpvParams / IpvWcsResult C↔ctypes 布局一致性锁
# ----------------------------------------------------------------------------
# 根因: ipv_api.h::IpvParams 在 P4 批增删字段 (删 m_lim_step / +10 double /
# log_dir 96->168) 后, 诊断工具自带的 ctypes 镜像未同步 => Python 352 vs C 424,
# ipv_get_default_params 的 memset(params,0,sizeof(IpvParams)) 对 352 字节
# 缓冲越界写 72 字节 (内存安全, P0)。宪章 §8.6。
#
# 机械防护 (两层):
#   A) 布局锁 ipv_abi_layout_probe (只 include ipv_api.h, 无 windows.h) 输出
#      sizeof/offsetof JSON, 由 ipv_abi_layout_lock.py 与仓内唯一权威镜像
#      lib/algorithms/platesolve/tools/ipv_abi_mirror.py 逐字段比对; 偏移/大小/字段名/
#      顺序任一不一致 => FAIL。ipv_abi_layout_lock_selfcheck 用被破坏镜像
#      反向证明该门会失败 (非恒真)。
#   B) 运行时 fail-closed 锁 ipv_params_abi_failclosed_test: 五个公共入口在
#      struct_size/abi_version 不匹配时必须返回 0 且 error_msg 点名 ABI。
# ============================================================================
add_executable(ipv_abi_layout_probe
  ${P1IPVTEST_DIR}/ipv_abi_layout_probe.cpp)
target_include_directories(ipv_abi_layout_probe PRIVATE
  ${P1IPV_DIR}/include)

add_test(NAME ipv_abi_layout_lock
  COMMAND ${CMAKE_COMMAND} -E env
          IPV_LAYOUT_PROBE=$<TARGET_FILE:ipv_abi_layout_probe>
          python3 ${P1IPVTEST_DIR}/ipv_abi_layout_lock.py
          --probe $<TARGET_FILE:ipv_abi_layout_probe>)
set_tests_properties(ipv_abi_layout_lock PROPERTIES TIMEOUT 120)

add_test(NAME ipv_abi_layout_lock_selfcheck
  COMMAND ${CMAKE_COMMAND} -E env
          IPV_LAYOUT_PROBE=$<TARGET_FILE:ipv_abi_layout_probe>
          python3 ${P1IPVTEST_DIR}/ipv_abi_layout_lock.py
          --probe $<TARGET_FILE:ipv_abi_layout_probe> --selfcheck)
set_tests_properties(ipv_abi_layout_lock_selfcheck PROPERTIES TIMEOUT 120)

add_executable(ipv_params_abi_failclosed_test
  ${P1IPVTEST_DIR}/ipv_params_abi_failclosed_test.cpp)
target_include_directories(ipv_params_abi_failclosed_test PRIVATE
  ${P1IPV_DIR}/include)
target_link_libraries(ipv_params_abi_failclosed_test PRIVATE
  astrocs_p1_ipv astrocs_p1_sdet astrocs_aio astrocs_cfitsio)
add_test(NAME ipv_params_abi_failclosed COMMAND ipv_params_abi_failclosed_test)
set_tests_properties(ipv_params_abi_failclosed PROPERTIES TIMEOUT 120)

# ============================================================================
# P27 死字段防误用锁 (负责人裁决 A: 不改解算行为, 只标注 + 机器锁)
# ----------------------------------------------------------------------------
# 生产路径 phase1 run -> p1_op_wcs -> ipv_solve_from_memory_with_callback_d
# -> solve_from_memory_with_callback_f64 -> solve_post_select 中:
#   - triangle_match(U, W, 60, 60, 0.002, s0) 参数为**字面量常数**;
#   - p_adapt.img_n_target = 60 硬覆盖配置;
#   - polygon_match / polygon_match_adaptive / geometric_vote /
#     extract_consensus / prosac_verify 在 src/ 内零调用点;
#   - robust_refine_wcs 收到 RobustRefineParams{} 字面量默认构造。
# => IPVSolverParams / RobustRefineParams 的部分字段在生产路径完全不被读取,
#    改配置既不改行为也不报错 ("配置不生效" 型误用)。
#
# 机器锁把"死字段清单"(ipv_dead_params_manifest.json) 变成可执行断言:
#   1) 生产链函数闭包 (源码级调用图, 从生产入口可达) 内:
#      - dead 字段读取点数必须为 0; live/shadowed 字段必须等于冻结值;
#      - 出现未登记读取点 => FAIL (有人新接线);
#   2) 生产链内调用已判定不可达的匹配器 (dead_symbols) => FAIL;
#   3) 生产路径常数/默认值来源 (frozen_literals) 被换成配置 => FAIL;
#   4) 配置结构体字段集合与清单不一致 => FAIL;
#   5) 生产链闭包与清单不一致 => FAIL。
# 任一 FAIL 均提示"请同步更新清单与文档"。
#
# 阴性对照 ipv_dead_params_lock_selfcheck 证明锁非恒真:
#   P1 死字段新接线 / P2 真消费被删 / P3 死匹配器接线 / P4 常数换成配置
#   四类反例锁均变红, 恢复后变绿。
# 详见 run/perf-fix/P27-dead-params/REPORT.md。
# ============================================================================
add_test(NAME ipv_dead_params_lock
  COMMAND python3 ${P1IPVTEST_DIR}/ipv_dead_params_lock.py
          --manifest ${P1IPVTEST_DIR}/ipv_dead_params_manifest.json)
set_tests_properties(ipv_dead_params_lock PROPERTIES TIMEOUT 180)

add_test(NAME ipv_dead_params_lock_selfcheck
  COMMAND python3 ${P1IPVTEST_DIR}/ipv_dead_params_selfcheck.py
          --manifest ${P1IPVTEST_DIR}/ipv_dead_params_manifest.json)
set_tests_properties(ipv_dead_params_lock_selfcheck PROPERTIES TIMEOUT 300)


