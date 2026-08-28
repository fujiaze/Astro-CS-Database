# QA-V19R7-A2-06 Evidence Index — Architecture 域审计（H+E）

- task: QA-V19R7-A2-06 | Spec §4.1 A2-06 (architecture 全仓)
- gate: G-QA A Gate
- status: DONE (只读)
- date: 2026-08-21 | branch: main | auditor: resident:code
- baseline: V19R6R2-W1 HEAD 05d7d6b

## 通过条件
- `reports/v19r7_quality/audit_findings_architecture.md` 已落盘，含概述/方法/发现表/统计
- 12 份 architecture + PUBLIC_API 与 lib/* include/线程/错误码/原子写/缓存/性能 实测一致性可追溯
- 不改 docs/lib（只读）

## 实际结果
- 审计文档: 13 份 (12 architecture + PUBLIC_API + 2 contracts 辅助)
- 审计代码: 约40文件抽样 约6000行 (phase2/include, astro_image_io/include, orchestrator.h, aio_upm/hips_writer, drizzle, gaia_client)
- 发现: 9 项 — P0 0 / P1 4 / P2 5
- 依赖环: 0 (astro_image_io 无反向, common 无上层反向, phase2 头文件无 aio 循环)
- 错误码: pass (orchestrator.h:111-134 与 ERROR_MODEL 逐值一致, V19R3 已修正)
- 缓存四要素: pass (4 缓存均有 capacity/identity/invalidation/thread)
- PUBLIC_API 预检: pass (p2_*/aio_* 16 符号均存在, machine_consistency_before.json pass=true)

## 产出清单
- `reports/v19r7_quality/audit_findings_architecture.md` — 主分表（ARC-001..009, P0 0/P1 4/P2 5）
- 本 `EVIDENCE_INDEX.md`
- 对偶索引: `evidence/QA-V19R7-A2-06/EVIDENCE_INDEX.md`

## 关键发现映射
| # | 合同 | 文档节 | 代码锚点 |
|---|---|---|---|
| ARC-001(P1) | ENG-IO-001 | IO_AND_ATOMICITY:4-5 | aio_upm.cpp:60-97 已原子 vs 文档“未原子”; hips_writer.cpp:172-260 非原子 |
| ARC-002(P1) | DEPENDENCY_RULES | DEPENDENCY_RULES:1-6, MODULE_MAP | orchestrator.cpp:36 #include astro_image_io.h ; phase2/include 无 aio 循环 |
| ARC-003(P1) | OWNERSHIP | OWNERSHIP:5-12 | aio_upm.cpp:21 thread_local g_upm_error ; upm.cpp:562 p2_upm_close ; aio_upm.cpp:448 dense guard |
| ARC-004(P1) | THREADING | THREADING_MODEL | drizzle_engine.cpp OpenMP ; grep omp_set_num_threads 0命中 |
| ARC-005..009(P2) | ERROR/CACHE/PERF/MODULE_MAP/PUBLIC_API | ERROR_MODEL, CACHE_POLICY, PERFORMANCE_MODEL, MODULE_MAP, PUBLIC_API.md | orchestrator.h:111-134 ; CACHE_POLICY 表4行 ; PERFORMANCE_MODEL hotpath |

## 关联
- 上游: A1-01/A1-02
- 下游: B3-02/B3-03/B3-05/B3-06/B3-07/B3-08/B3-09/B3-10
- Commit: `docs(qa): audit findings architecture [A2-06]` (待提交)

## 校验
- 文件存在且非空；SHA 见 D 阶段
