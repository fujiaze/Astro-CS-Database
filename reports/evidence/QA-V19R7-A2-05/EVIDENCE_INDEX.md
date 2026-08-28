# QA-V19R7-A2-05 Evidence Index — IO 域审计（H+E）

- task: QA-V19R7-A2-05 | Spec §4.1 A2-05 (io+orchestrator)
- gate: G-QA A Gate (入 B 前)
- status: DONE (只读)
- date: 2026-08-21 | branch: main | auditor: resident:code
- baseline: V19R6R2-W1 HEAD 05d7d6b

## 通过条件
- `reports/v19r7_quality/audit_findings_io.md` 已落盘，含概述/方法/发现表(P0/P1/P2)/统计
- 发现与 `docs/contracts/DATA_SEMANTICS.md`, `docs/architecture/IO_AND_ATOMICITY.md/PIPELINE/DATA_FLOW/COMPATIBILITY` 及 `lib/astro_image_io` 代码位置可追溯
- 不改 `docs/*` 与 `lib/*`（只读）

## 实际结果
- 审计文档: 5 份 (DATA_SEMANTICS, PIPELINE, DATA_FLOW, IO_AND_ATOMICITY, COMPATIBILITY) + astro_image_io 模块文档
- 审计代码: 7 文件 约3400行 (aio_hips.h/.cpp, aio_hips_reader.cpp, aio_upm.h/.cpp, aio_pipeline.h/.cpp, healpix_core.h) + 2 测试抽样
- 发现: 7 项 — P0 0 / P1 3 / P2 4
- 机器一致性: `machine_consistency_before.json: checks=9 pass=true` (A1-01 复用)

## 产出清单
- `reports/v19r7_quality/audit_findings_io.md` — 主分表（概述/方法/发现表 IO-001..007/统计，P0 0/P1 3/P2 4，关键 P1: HiPS tiles 非原子与 UPM 已原子但文档滞后的双向不一致）
- 本 `EVIDENCE_INDEX.md` — 证据索引（本文件）
- 对偶索引: `evidence/QA-V19R7-A2-05/EVIDENCE_INDEX.md`（同内容，满足“同时建 evidence/QA-V19R7-A2-05/06/07”要求）

## 关键发现映射
| # | 合同 | 文档节 | 代码锚点 |
|---|---|---|---|
| IO-001(P1) | ENG-IO-001/IO_STANDARD | IO_AND_ATOMICITY:4-5 | aio_hips_writer.cpp:172-260 write_fits_image (非原子) ; aio_upm.cpp:60-97 (已原子 but 文档写未原子) |
| IO-002(P1) | DATA-HIPS-VAR/IVAR, SCI-DRZ-014 | DATA_SEMANTICS:4a | aio_hips.h:34-42,66-71 ; aio_hips_writer.cpp:295-330,558-700,665-700,119 add_var(…,0) |
| IO-003(P1) | DATA-FRAME-ID-001 | DATA_SEMANTICS:5, PIPELINE, DATA_FLOW | aio_pipeline.h:20,72-78 ; phase2/src/upm.cpp:890 frame_id_by_index |
| IO-004..007(P2) | V11冻结/NUMERIC/ALL标志/Engine线程 | DATA_SEMANTICS:3,6 | healpix_core.h ; aio_hips_writer.cpp:487,502,748,1092 ; aio_pipeline_engine.h:11 |

## 关联
- 上游: A1-01/A1-02 machine/file 扫描 (pass)
- 下游: B3-04/B3-08/B3-09 与 B4-03/B4-04 修复
- Commit: `docs(qa): audit findings io [A2-05]` (待提交，A2-08 汇总时同批)

## 校验
- `ls -l reports/v19r7_quality/audit_findings_io.md` 存在且非空
- `sha256sum reports/v19r7_quality/audit_findings_io.md` 可复现（见 D 阶段 SHA256SUMS）
