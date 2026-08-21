# QA-V19R7 A2-03 Drizzle 域审计分表

> Spec: `工程控制/docs/29_QUALITY_OPTIMIZATION_V19R7_SPEC.md` | Task: `工程控制/tasks/QA-V19R7-QUALITY-OPTIMIZATION.md` A2-03  
> 基线: V19R6R2-W1 | 模式: 只读审计 | A1 machine_consistency 独立完成标注待复核  
> 域: `docs/science/DRIZZLE.md + docs/algorithms/DRIZZLE_GEOMETRY.md + docs/algorithms/HEALPIX_MAPPING.md` vs `lib/healpix_db/healpix_drizzle/* + lib/common/healpix/* + lib/common/crypto/*`  
> 追溯: `docs/TRACEABILITY.csv` SCI-DRZ-001/014, ALG-DRZ-GEOM-CACHE-001, ALG-DRZ-CAND/OVERLAP/VAR

## 1. 概述

Drizzle 是 Stage1 FITS → HiPS 的核心几何+光度引擎，冻结语义为 Fruchter & Hook 线性重建、通量守恒 `F_p=Σ x_j w_jp`、`w_jp=a_jp/A_drop`、方差传播 `variance_p=sumVarNum/D_p²`、候选保守 `false_negative=0`。V19R3 新增 `DRIZZLE_TARGETED` 定点优化：bounded target-ipix geometry cache (LRU 8192, run generation 清空) 与 operation counters。HEALPix 生产路径 NESTED 唯一，tile_depth=9 (512×512 leaf)。

审计结论：几何/方差/计数语义与 L1/L2 基本一致，未发现阻断性 P0 科学错；遗留 1 项 P0 架构重复、3 项 P1 文档-代码口径偏差、4 项 P2 建议。`false_negative=0` 由 oracle 9003 例保证，但文档缓冲系数与代码实现存在口径分叉需在 B1/B2 收口。

A1 `tools/docs_machine_consistency.py` 未在本会话重跑，已独立审计；本文 `machine_consistency` 结论标记 **待 machine_consistency 复核**，不阻塞 B1 进入但 B5 前必须 0 broken 复核。

## 2. 方法

- 文档精读：`DRIZZLE.md` (SCI-DRZ-001/014/015/016, ALG-DRZ-CAND/OVERLAP/VAR), `DRIZZLE_GEOMETRY.md` (V19R3 cache + operation_counts + oracle), `HEALPIX_MAPPING.md` (ALG-HEALPIX-*), `TRACEABILITY.csv` (SCI-DRZ-*, ALG-DRZ-GEOM-CACHE-001)。
- 代码核验：`drizzle_engine.h/.cpp` (PixelAccumulator sumVarNum, TileLeafAccumulator, DrizzleConfig, DrizzleStats counters, DrizzleRunContext run generation, run_target_cache), `spherical_overlap.h/.cpp` (HP_CIRCUMRADIUS_FACTOR, TargetGeomCache, compute_overlap_area_g_ctx_cached, query_candidate_pixels/fast, spherical_polygon_area Eriksson, planar fast path), `healpix_core.h/.cpp` 双份对比, `astro_sphere_sink.*`, 测试 `candidate_oracle_test.cpp / variance_propagation_test.cpp / control_median_mc_test.cpp / kcorr_matrix_test.cpp`。
- 交叉：`lib/common/healpix` vs `lib/healpix_db/healpix_drizzle/healpix_core.*` 重复度、`lib/common/crypto/sha256.*` 唯一性、`operation_counts` 文档-代码字段对齐、`sumVarNum` 写盘路径到 `aio_hips_writer`。
- 仅只读，不改 `docs/` `lib/`；证据落 `reports/v19r7_quality/` + `evidence/QA-V19R7-A2-03/`。

## 3. 发现表（P0/P1/P2 | 合同 | 文档节 | 代码位置 | 描述）

| # | 级别 | 合同 | 文档节 | 代码位置 | 描述 | 建议 |
|---|------|------|--------|----------|------|------|
| DRZ-01 | **P0** | SCI-DRZ-001 / ALG-HEALPIX-* / ENG-OWN-001 | `HEALPIX_MAPPING.md` "模块: lib/common/healpix" / `DRIZZLE_GEOMETRY.md` "lib/healpix_db/healpix_drizzle" | `lib/common/healpix/healpix_core.h:1` + `lib/common/healpix/healpix_core.cpp:322L` vs `lib/healpix_db/healpix_drizzle/healpix_core.h:679L` + `healpix_core.cpp` | **HEALPix NESTED 唯一性违背：两套独立实现并存**。`lib/common` 提供 `namespace astrocs::healpix::ang2pix_nest / nested_local_to_xy` 冻结独立实现 (astropy-healpix 1e6 点验证)；`lib/healpix_db` 提供 `class healpix::HealpixCore (ang2pix/pix2ang/radec2pix/queryDisc)`。头 guard、命名空间、API 完全分叉，`docs` 指向分裂：`HEALPIX_MAPPING.md` 指向 common，`DRIZZLE_GEOMETRY.md` 指向 healpix_drizzle。违反 Spec 4.2 B1-04/B4-01 "HEALPix NESTED 唯一映射去重" 与 `MODULE_MAP` 单一权威要求。当前数值一致性靠手工同步，无编译期阻断。 | B4-01 收口为单源：drizzle 转依赖 `lib/common/healpix` 或抽 `healpix_core` 到 `lib/common` 唯一实现，另一份标记 deprecated shim + 机器校验禁止第二套 `ang2pix` 符号。 |
| DRZ-02 | P1 | ALG-DRZ-GEOM-CACHE-001 / SCI-DRZ-001 | `DRIZZLE_GEOMETRY.md` §V19R3 cache: "容量 8192 LRU 线程私有，run generation 递增 clear" / "查询圆 = drop 包围圆 + 2.0×hp_res" | `drizzle_engine.cpp:242,285-293,1652,1668-1670` `spherical_overlap.h:301-328` `spherical_overlap.cpp:1383-1424,1427-1443` | **run generation 与缓存语义正确，但文档缓冲系数口径分叉**。代码：`TargetGeomCache capacity 8192` 默认正确，`thread_local cache + thread_local last_gen` 按 `rctx.target_cache_run_gen = s_target_cache_gen.fetch_add(1)+1` (atomic) 递增清零，`compute_overlap_area_g_ctx_cached` 仅 `nside>=256` 缓存 `center+boundary4`，科学等价与保序 (quick-reject 在边界构建前) 均实现。文档写 "查询圆 2.0×hp_res 保守上界" 与代码实际三处不一致：`spherical_overlap.cpp:40 HP_CIRCUMRADIUS_FACTOR=1.25` (quick-reject 用 `1.25×hp_res`)、`query_candidate_pixels` 用 `3.0×hp_res` 保守圆盘、`query_candidate_pixels_fast` 用 `1.25× + 1.15畸变系数 + 边界回退` 分层。文档未同步该分层修正，读者按 2.0 复现与实测 `k_corr` 不一致。 | B2-07/B1-06 同步：文档改 "分层保守缓冲：overlap quick-reject 1.25×, candidate 圆盘 3.0×, fast 1.25×+1.15畸变+极冠回退" 并引 `spherical_overlap.cpp:40` 与 scan 证据。 |
| DRZ-03 | P1 | SCI-DRZ-014 / ALG-DRZ-VAR-* | `DRIZZLE.md` §方差传播 "sumVarNum+=v_j×w_jp², variance=sumVarNum/D_p², ivar=1/variance, α²v" | `drizzle_engine.h:50-51,64` `drizzle_engine.cpp:1525-1529,1763,1824-1827` `astro_sphere_sink.cpp:100` | **方差传播分子正确，归一化位置与文档一致但端到端可追溯性断**。累加器 `PixelAccumulator.sumVarNum / TileLeafAccumulatorT<Scalar>.sumVarNum` 按 `w=overlap/drop_area` 计算 `acc.sumVarNum += v * w²` (double 提升) 正确，`D_p = sumArea = Σ a_jp` 存储正确。`sumVarNum/D²` 归一由 `astro_sphere_sink` / `aio_hips_writer` 在 tile finalize 时执行 (`dense_var[local]=acc.sumVarNum`)，符合 `variance=Σ v w² / D²`。但 `DRIZZLE.md` 未声明 `sumVarNum` 为分子中间量、未说明最终 `variance/ivar` 在 HISS 哪层计算，导致 L2→L3→L4 追溯缺 `implementation_symbols: aio_hips_writer::finalize_tile` 链。`TRACEABILITY SCI-DRZ-014` 仅指向 `drizzleTiled`，缺 writer 符号。 | B1-07 补 `DRIZZLE.md` "variance 归一在 sink/writer finalize" 段；B5 `TRACEABILITY SCI-DRZ-014 implementation_symbols += aio_hips_writer::finalize_tile`。 |
| DRZ-04 | P1 | SCI-DRZ-001 / ALG-DRZ-CAND-001 | `DRIZZLE_GEOMETRY.md` §候选包围圆 L37-45 "false_negative=0 由 candidate_oracle_test 9003 例全枚举保证" | `spherical_overlap.cpp:1459-1513,1524-1685` `drizzle_engine.cpp:1480-1494,1652` `tests/candidate_oracle_test.cpp` | **`false_negative=0` 语义实现完备，测试门存在但文档测试 ID 缺失**。代码：`max_angle = max angular_distance(center, corner)` (double) + `buffer 3.0/1.25`，`query_candidate_pixels_fast` 对极冠/RA跨0 `boundary_fallback` + 盒触极回退，包围圆用 double 中心避免 float 舍入误拒，符合零漏选。`candidate_oracle_test` 9003 例 (12 face × 边/角 × pixfrac × 尺度 × nside) 已落仓。但 `docs/TRACEABILITY` 无 `TEST-DRZ-CAND-001` 对应 `test_ids` 行，`DRIZZLE_GEOMETRY.md` ID 段仅 `ALG-DRZ-CAND-001` 无 `TEST-*` 映射，machine_consistency 误判为 0 broken 实为覆盖缺。 | B5 增 `TEST-DRZ-CAND-001` 行 (关联 `candidate_oracle_test.cpp`)；文档补 `ID: TEST-DRZ-CAND-001`。 |
| DRZ-05 | P2 | SCI-DRZ-001 / ALG-DRZ-GEOM-CACHE-001 | `DRIZZLE_GEOMETRY.md` §操作计数模型 L63-77 | `drizzle_engine.h:96-118` `drizzle_engine.cpp:246-280,1824-1877` | **operation_counts 文档与代码基本一致，命名微分叉**。文档列 `source_pixels/candidates/true_overlaps/quick_rejects/pix2radec/boundary_builds/geometry_builds/target_boundary_builds/target_geometry_builds/geometry_cache_hits/geometry_cache_misses/sh_calls/tile_lookups/heap_allocations` 13 项；代码 `DrizzleStats` 13 项 `op_*` + 运行时 `DrizzleOpCounters` 同步 13 项，含 `op_target_*` / `op_geometry_cache_*`，统计在 `merge_op_counters` 汇总、`profile_overlap_path_counts` 门控于 `ASTROCS_DRIZZLE_FINE_PROFILE`。差异仅命名：文档 `geometry_cache_hits` vs 代码 `op_geometry_cache_hits`，`ops` 日志行 `[ops]` 小图实测值与文档示例一致 (400/3463/1221, hit 91.7%)。 | B2-07 统一命名表 (文档加 `op_` 前缀对照) + 引用 `drizzle_engine.h:96`。 |
| DRZ-06 | P2 | SCI-DRZ-014 / ENG-ERR-001 | `DRIZZLE.md` §失效条件 / `DRIZZLE_GEOMETRY.md` §数值风险 | `drizzle_engine.cpp:728-763,852-888` | **失效域显式化已实现，错误码未进 TRACEABILITY**。`pixfrac∈(0,1]`、`nested==true`、`channels==1` 均硬拒绝并返 `error_msg`；`WCS 无/非法、WcsSip 失败` 返错；`varianceData==nullptr` 跳过传播 (未强制)。符合 "几何退化 → NO_DATA, conservative false_negative=0"。但 `error_codes` 列空，未与 `docs/architecture/ERROR_MODEL.md` 的 `ERR-DRZ-*` 对齐。 | B3-07 补 `ERROR_MODEL` 中 `ERR-DRZ-001..004` 并回填 TRACEABILITY `error_codes`。 |
| DRZ-07 | P2 | ALG-DRZ-OVERLAP-* | `DRIZZLE_GEOMETRY.md` §球面几何推导 L19-32 | `spherical_overlap.cpp:556-608 HP_ADAPTIVE_MAX_DEPTH=8, hp_epsilon=hp_res*1e-6` `spherical_overlap.cpp:849-894 WCS_ADAPTIVE_MAX_DEPTH=12, wcs_epsilon=max(src_scale*1e-12,1e-11)` | **球面几何数值路径文档化充分，阈值有解释但分散**。WCS 自适应 `wcs_epsilon` 相对阈值 + 下限 `1e-11 rad` (防 TAN 数噪永不收敛) 与 `spherical_overlap.cpp:934` 注释一致；HEALPix 边 `hp_epsilon=hp_res*1e-6` 与 `subdivide_healpix_edge:574` 一致；三处 `1e-3 rad` 切平面 fast path (`planar_polygon_area_n`) 与 `compute_overlap_area` 分支一致。阈值分散三处，建议集中表。 | B2-07 增 "数值阈值总表" 小节集中三阈值与出处行号。 |
| DRZ-08 | P2 | DATA-HIPS-* / ALG-HEALPIX-* | `HEALPIX_MAPPING.md` L3 "关联 SCI-DRZ-001 模块 lib/common/healpix" | `lib/common/include/astro_scalar.h` `lib/common/healpix/tests/test_healpix_oracle.cpp` | **HEALPix 映射文档过薄**。`HEALPIX_MAPPING.md` 仅 35 行，无输入/输出/不变量/伪代码/复杂度/oracle，与 Spec B2-08 要求不符。代码侧 `astro_scalar.h` 精度上下文、`nested_local_to_xy` 位交错、`parent_nest/child_nest` 层级正确但未在文档落地。 | B2-08 按模板补全 HEALPIX_MAPPING 输入/输出/不变量/伪代码/oracle (1e6 随机 + 12 face)。 |

## 4. 统计

- 审计文档：L1 `DRIZZLE.md` 1 + L2 `DRIZZLE_GEOMETRY.md` + `HEALPIX_MAPPING.md` 2 = 3 份；追溯 `TRACEABILITY.csv` 63 行中 drizzle 相关 4 行 (SCI-DRZ-001/014, ALG-DRZ-GEOM-CACHE-001, TEST-DRZ-* 缺 1)。
- 审计代码：`lib/healpix_db/healpix_drizzle` 14 文件 + `lib/common/healpix` 3 + `lib/common/crypto` 2 = 19 文件；核心核验 8 文件 (`drizzle_engine.*`, `spherical_overlap.*`, `healpix_core.*×2`)。
- 计数器：文档 13 项 vs 代码 13 项 `op_*`，一致 100% (命名差 P2)。
- 方差：`sumVarNum` 分子 `w²` 路径 FP32/FP64 双实例一致 (template Scalar)，归一化在 writer 层，语义与 `DRIZZLE.md:24-30` 一致。
- 发现：P0 1 / P1 3 / P2 4 = 8 项。阻断仅 DRZ-01 (NESTED 双实现)。
- machine_consistency：本次未重跑，标记 **待复核** (A1-01 `machine_consistency_before.json` 9 checks 0 broken 为初扫快照，B5 前需重跑 `tools/docs_machine_consistency.py`)。

## 5. 结论与下步

- 科学正确性：`false_negative=0`、`α²v`、`平面 1e-3 rad` fast path、`k_corr` provenance 预留均实现正确，无需语义改动。
- 架构债：首要收口 DRZ-01 (B4-01)，次要收口 DRZ-02/DRZ-03 文档-实现口径 (B1-07/B2-07)，B5 补 TRACEABILITY `TEST-DRZ-CAND-001` 与 `ERROR_MODEL`。
- 风险：双 HEALPix 实现若继续分叉，BASS 全量不同 NSIDE 下 `ang2pix` 1 ULP 差异可致 tile 缝；B4 前加 `grep` 门禁 "禁止第二套 ang2pix" 即可阻断。

---
*审计：resident:architecture (只读) | 证据：`evidence/QA-V19R7-A2-03/EVIDENCE_INDEX.md` | 待 machine_consistency 复核后进 B1。*
