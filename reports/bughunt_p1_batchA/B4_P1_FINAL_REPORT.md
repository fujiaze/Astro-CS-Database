# B4/batchA P1 修复最终报告（AstroCS bug 狩猎 R4）

状态: 5/5 全部完成, 已交付父 Agent。日期: 2026-09-08。

## 修复清单与依据

| 项 | 文件 | 修复 | 依据 |
|---|---|---|---|
| P1-1 | lib/dynamic_psf/src/dpsf_psf.cpp | `dpsf_nan_safe_less` 全序比较器(有限值上与 `a<b` 位级等价, NaN 排末尾)替换 4 处 `std::sort`; 采样循环 `isfinite` 过滤 + `m==0`→`DPSF_FIT_INVALID_PARAMS` | 消除严格弱序 UB; 保守方案=合法输入语义零变化 |
| P1-2 | lib/gaia_xpsd_client/src/gaia_client.c | `BcLock`(POSIX pthread_mutex/Win CRITICAL_SECTION) per-file 串行化 block_cache lookup/insert; `read_leaf_block` 返回缓存权威副本 | GAIA_QUERY.md §4: match 类查询并行轴=坐标, 同文件 cache 并发写=UAF |
| P1-3 | 同上 (spec_collector_push ~1150) | realloc 返回值立即回写 `sc->stars`(单一所有权); spectra 失败仅 return 不 free | realloc 失败路径悬垂→double free |
| P1-4 | lib/plate_solve/cpp/ipv/src/ipv_entry.cpp | `g_handle_mutex()` + 读端 lock_guard + 写端 internal setter; C ABI 签名零变更 | 全局句柄跨实例串扰 |
| P1-5 | lib/plate_solve/cpp/ipv/src/ipv_solver.cpp | 6 入口统一 `last_inliers_ = SolveInlierCache{}`; 覆盖全部 15 失败路径 | 失败后陈旧 inlier 诊断污染 WCS Gate v2 |

P1-5 清单: 入口重置 6 处 = solve(402)/solve_from_memory(791)/solve_post_select(1153)/
solve_from_detections_v1(1422)/solve_from_memory_with_callback(1491)/…_f64(1557)。
失败路径 15 处 (`*result = fail_result; return;`) = solve 4 / from_memory 4 / post_select 4 /
detections 1 / callback 1 / f64 1。成功路径 cache_last_inliers_ 重建 @746/1107/1374。

## 验证表

| 项 | 测试(新建共址) | 修复版 | 原版对照 | 工具 |
|---|---|---|---|---|
| P1-1 | test/test_dpsf_nan_sort.cpp | ALL PASS rc=0 (9 断言) | — (UB 无法对照) | g++ |
| P1-2 | test/test_gaia_race.c (8线程×60轮) | ALL PASS + **0 race** | **4× data race** rc=66 | TSAN |
| P1-3 | test/test_spec_collector_ownership.c | ALL PASS (12 断言) | **ASAN double-free** @orig:1083 | ASAN+GAIA_ALLOC_TEST 注入 |
| P1-5 | ipv test/test_last_inlier_reset.cpp | ALL PASS (8 断言) | — (静态审计) | g++ |
| 构建 | gaia/dpsf/ipv 13TU | 全 rc=0 | — | -Wall -Wextra |
| 回归 | tests/traceability (pytest) | **9 passed rc=0** | — | pytest |
| 回归 | test_kvector | PASS | — | — |
| 回归 | test_synthetic | FAIL 同 HEAD 基线 (PROSAC 早停平台差异, 与本批无关) | 基线对照 | — |

平台限制说明: Linux 下 ipv_select 主体在 `#ifdef _WIN32` 内且需注入 gaia/detector 句柄,
solve 无法走通成功路径, 故 P1-5 的"成功→失败清除陈旧值"场景以静态审计清单 + 失败路径
行为测试覆盖 (test_synthetic 亦绕过 IPVSolver, 属既有平台限制)。

## 影响范围

改动仅 4 个源文件 (+196/-36) + 4 个新建共址测试; 头文件 ABI 零变更; 未触碰 docs/、ci/、
.github/、lib/phase2/ 等禁区; 未 commit/push。git 工作区其余 modified 文件属其他并行任务。

## 产物 (run/local/bughunt_p1_batchA/)

- 构建: build_final.log / build_gaia.log / build_dynamic_psf.log / build_ipv.log
- 竞态/内存闭环: tsan_before_gaia.log / tsan_after_gaia.log / asan_before_gaia_p13.log / asan_after_gaia_p13.log
- 测试日志: test_dpsf_nan_sort.log / test_gaia_race.log / test_gaia_ownership.log / test_ipv_last_inlier.log
- 回归: traceability_pytest.log
