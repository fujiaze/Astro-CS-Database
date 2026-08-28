# EVIDENCE_INDEX — QA-V19R7-A2-03 (drizzle 域)

> 任务: `工程控制/tasks/QA-V19R7-QUALITY-OPTIMIZATION.md` A2-03 | Spec: `工程控制/docs/29_QUALITY_OPTIMIZATION_V19R7_SPEC.md`  
> 审计分表: `reports/v19r7_quality/audit_findings_drizzle.md` | 基线: V19R6R2-W1 | 只读

## 1. 输入

- Spec/Task: `工程控制/docs/29_QUALITY_OPTIMIZATION_V19R7_SPEC.md`, `工程控制/tasks/QA-V19R7-QUALITY-OPTIMIZATION.md`
- 科学: `docs/science/DRIZZLE.md` (SCI-DRZ-001/014/015/016)
- 算法: `docs/algorithms/DRIZZLE_GEOMETRY.md` (ALG-DRZ-CAND/OVERLAP/GEOM-CACHE), `docs/algorithms/HEALPIX_MAPPING.md` (ALG-HEALPIX-*)
- 追溯: `docs/TRACEABILITY.csv` SCI-DRZ-001, SCI-DRZ-014, ALG-DRZ-GEOM-CACHE-001
- 代码: `lib/healpix_db/healpix_drizzle/drizzle_engine.h:50-64,96-118` `drizzle_engine.cpp:40-293,728-1880` `spherical_overlap.h:301-337` `spherical_overlap.cpp:40,1383-1685` `healpix_core.h/.cpp (×2)` `astro_sphere_sink.*`
- 测试: `lib/healpix_db/healpix_drizzle/tests/candidate_oracle_test.cpp` `variance_propagation_test.cpp` `control_median_mc_test.cpp` `kcorr_matrix_test.cpp`
- 机器初扫: `reports/v19r7_quality/machine_consistency_before.json` `audit_stats.json` `traceability_broken.json` (A1, 待复核)

## 2. 产出

- 本审计分表: `reports/v19r7_quality/audit_findings_drizzle.md` (P0 1 / P1 3 / P2 4)
- 证据索引: 本文件

## 3. 方法 (只读)

对照 L1→L2→TRACE→CODE：几何缓存/RunGen/false_negative/variance sumVarNum/k_corr provenance/operation_counts/HEALPix NESTED 唯一性/plate 重复逐项核验；`grep` 符号表 + `diff` 双 healpix_core + `read` 文档-代码逐行比对。

## 4. 关键证据摘录

- **DRZ-01 P0 双实现**: `lib/common/healpix/healpix_core.{h,cpp} (namespace astrocs, 322L)` vs `lib/healpix_db/healpix_drizzle/healpix_core.{h,cpp} (class HealpixCore, 679L)` API 分叉，`HEALPIX_MAPPING.md` 指 common、`DRIZZLE_GEOMETRY.md` 指 healpix_drizzle。
- **DRZ-02 P1 缓冲分层**: `spherical_overlap.cpp:40 HP_CIRCUMRADIUS_FACTOR=1.25`, `query_candidate_pixels:1491 buffer=3.0×`, `query_candidate_pixels_fast:1.25+1.15畸变`, 文档仅写 2.0。
- **DRZ-03 P1 方差**: `drizzle_engine.h:50 sumVarNum += v·w²`, `drizzle_engine.cpp:1529 acc.sumVarNum += v*w2`, 归一在 `astro_sphere_sink.cpp:100 + aio_hips_writer finalize`。
- **DRZ-04 P1 零漏选**: `spherical_overlap.cpp:1538-1556 double 中心`, 9003 oracle 例, TRACE 缺 `TEST-DRZ-CAND-001`。
- **RunGen**: `drizzle_engine.cpp:242 target_cache_run_gen`, `285-293 thread_local cache+last_gen`, `1652 s_target_cache_gen.fetch_add(1)+1`, `compute_overlap_area_g_ctx_cached` 仅 nside≥256。

## 5. 缺口标注

- A1 machine_consistency 未在本会话重跑，本文结论待 `tools/docs_machine_consistency.py` 复核 (B5 前 0 broken)。
- `file_audit` 工具缺失，替代统计 873 文件 (A1-02 P1)。

## 6. 下步

B4-01 去重 NESTED 单源；B1-07/B2-07 收口缓冲/方差归一文档；B5 补 TRACE `TEST-DRZ-CAND-001` + ERROR_MODEL。

---
*只读审计，未改 `docs/lib`。*
