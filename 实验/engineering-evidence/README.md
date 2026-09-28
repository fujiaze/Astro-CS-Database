# engineering-evidence —— 工程实测留档

本目录收留**工程侧实测类证据留档**（测量结果 / 基准数值 / 质量实测 / 审计时点证据）。
依据负责人裁决（2026-09-28，artifacts 重组）：**实测类内容全部归 `实验/` 留档**；
`artifacts/evidence/` 只保留机器门基线与台账（门功能消费，非实测）；
正式文档只写设计不引外部锚（唯一例外 = 科学链路引 `实验/`），实验结论写成文档规范条款。

## 归属口径

- 放：一次性测量/审计/验收/发布的时点证据快照——历史记录**不改写**，目录内部结构保持原样；
- 不放：科学实验单元（五创新点与 M42 腿等自包含单元在 `实验/<单元>/`，有独立 REPORT 与 code/results 结构）；
- 不放：机器门基线与台账（`artifacts/evidence/{doc-hygiene,known-limitations-ledger,truthful-conclusion-01}`，门功能消费，原位保留）；
- 不放：CI 运行产物（`artifacts/ci/<sha>/`）。

## 内容（迁移映射，2026-09-28）

| 目录 | 性质 | 自何处迁入 |
|---|---|---|
| `prerelease-v5/` | ISA 变体性能实测（ISA-001/002/003 MEASUREMENTS.csv） | `artifacts/evidence/prerelease-v5/` |
| `v19r7-quality/` | V19R7 全仓质量审计实测（audit_findings_*、audit_stats.json） | `artifacts/evidence/v19r7-quality/` |
| `science/` | 科学数值工件（kcorr_matrix.json；归属单元待定，暂以原名留档） | `artifacts/evidence/science/` |
| `v6/` | V6 产品族 QA 设计档案与裁决/性能证据（qa-design 机器实现、review-audit、performance 等） | `artifacts/evidence/v6/` |
| `compress-01/` | HiPS 瓦片与内存累加器压缩评估（final_numbers.json、复现脚本、SHA-256 清单） | `artifacts/evidence/compress-01/` |
| `audit-2026-01/` | 2026-01 审计修复台账与裁决存档（FIX_LEDGER.csv、OWNER_DECISIONS.md、ROOT_CAUSES.md） | `artifacts/evidence/audit-2026-01/` |
| `reaudit-v3/` | V3 复审计执行结论（CON006/009/010） | `artifacts/evidence/reaudit-v3/` |
| `release-01/` | RELEASE-01 视觉验收证据（VIS-001 L3/L4 对比与裁剪件） | `artifacts/evidence/release-01/` |
| `release-05/` | RELEASE-05 审计与门验证留档（GATE-501 验证、FAILCLOSED 普查表、D-14 清理记录） | `artifacts/evidence/release-05/` |
| `l2_performance/` | L2 性能门实测读数（冻结门证据 gates/、worker_balance/、alloc_reports/、机器指纹） | `artifacts/acceptance/l2_performance/` |

## 机器消费锚（迁后改接）

- `eng/ci/check_frozen_gate.py` `--replay` 回放 `l2_performance/gates/*_gate.json`；
- `eng/ci/check_worker_balance.py` `--replay-archived` 读 `l2_performance/`；
- `docs/validation/v6/` 的 QA 复跑入口指向 `v6/qa-design/oracle/`；
- `docs/research/COMPRESSION_CODEC_RESEARCH_PACK.md` 复现命令指向 `compress-01/`；
- `docs/science/NOISE_MODEL.md` 数值实验脚本副本在 `release-01/science/exp_math.py`（科学链路例外，允许引 `实验/`）。

## 上游

上游：负责人裁决（2026-09-28，artifacts 重组）；AGENTS.md §7；ENGINEERING_SPEC.md §7。
