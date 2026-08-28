# QA-V19R7-A2-08 Evidence Index — 审计总表汇总（H+E）

- task: QA-V19R7-A2-08 | Spec §4.1 A2 总表汇总 | Checklist QA_V19R7_QUALITY A2
- gate: G-QA A Gate（入 B 前）→ CONDITIONAL PASS
- status: DONE（只读汇总，不改 docs/lib）
- date: 2026-08-22 | baseline: V19R6R2-W1 HEAD 2767874/05d7d6b | auditor: resident:project

## 通过条件
- `reports/v19r7_quality/audit_findings.md` 存在且含 7 域汇总（总述/方法/按域统计/P0冻结/P1/P2/B1..B5映射/下游依赖）
- `reports/v19r7_quality/audit_stats.json` 为汇总版（含 total_findings/per_domain/per_level/p0_frozen/can_enter_B1，保留 before）
- `reports/v19r7_quality/evidence/QA-V19R7-A2-08/EVIDENCE_INDEX.md` 存在（本文件）
- 不改 `docs/` `lib/`（`git diff -- docs lib` 为空）
- `git add` 后可提交

## 实际结果
- 输入: 7 域分表 + `machine_consistency_before.json(9/9 PASS)` + `traceability 63/63` + `file_audit 873` + `standards 7 violations`
- 计数复核: `grep -c` 12+10+8+8+7+9+10 = 64 项（P0 4 / P1 24 / P2 36）
- P0 冻结: SC-01(flat钳位)/SC-02(IRLS)/NO-01(gain域)/DRZ-01(双HEALPix) → B1-02/B1-04/B1-05/B4-01
- A Gate: CONDITIONAL PASS — B1可进，B2需P0解除（`docs_machine_consistency 0 broken + oracle 9003 + synthetic_gate`）

## 产出清单
| 产出 | 路径 | 说明 |
|------|------|------|
| 总表 | `reports/v19r7_quality/audit_findings.md` | 7 域汇总（§1-10） |
| 统计 | `reports/v19r7_quality/audit_stats.json` | 汇总版（before 保留） |
| 7域分表 | `reports/v19r7_quality/audit_findings_{science,noise,drizzle,phase2,io,architecture,standards}.md` | 64 项行级锚点 |
| 机器 | `reports/v19r7_quality/machine_consistency_before.json` | 9 checks 0 broken pass=true |
| 追溯 | `docs/TRACEABILITY.csv` | 63 行 0 broken |
| 文件审计 | `reports/v19r7_quality/file_audit_before.json` | 873 文件，tool_missing=P1 |
| 标准扫描 | `reports/v19r7_quality/standards_violations.json` | 7 违规 CODE3/COMMENT4 |
| 证据索引 | `reports/v19r7_quality/evidence/QA-V19R7-A2-08/EVIDENCE_INDEX.md` | 本文件 |
| 对偶索引 | `evidence/QA-V19R7-A2-08/EVIDENCE_INDEX.md` | 双路径（如需） |

## A Gate 判定
| 项 | 结果 |
|----|------|
| machine_consistency | 9/9 PASS |
| traceability | 63/63 0 broken |
| file_audit | 873/~713+ 需 A1-02 找回工具后复核 |
| standards | 7 violations 已行级定位（A2-07）|
| P0 冻结 | 4 项（B1-02/04/05 + B4-01）|
| can_enter_B1 | true |
| can_enter_B2 | false（需 P0 解除）|

## 下游 B 优先级
- **P0 冻结（先做）**: DRZ-01(B4-01 去重) ⟶ B2-08 阻塞；SC-01(B1-02)/SC-02(B1-04)/NO-01(B1-05) ⟶ B1 科学冻结
- **P1 24项（B阶段并行）**: B1 8 + B2 6 + B3 9 + B4 4批 + B5 1批，详见总表 §7
- **P2 36项（增强）**: 表述/锚点/互引，B 阶段随文档收口

## 校验
```bash
ls -l reports/v19r7_quality/audit_findings.md reports/v19r7_quality/audit_stats.json reports/v19r7_quality/evidence/QA-V19R7-A2-08/EVIDENCE_INDEX.md
sha256sum reports/v19r7_quality/audit_findings.md reports/v19r7_quality/audit_stats.json
cat reports/v19r7_quality/audit_stats.json | python3 -m json.tool | head -n 80
git diff -- docs lib  # 应为空
git add reports/v19r7_quality/audit_findings.md reports/v19r7_quality/audit_stats.json reports/v19r7_quality/evidence/QA-V19R7-A2-08/EVIDENCE_INDEX.md evidence/QA-V19R7-A2-08/EVIDENCE_INDEX.md
git status --porcelain
```

## 关联
- 上游: A1-01/A1-02 + A2-01..07（7 域分表）
- 下游: B1-01..08 / B2-07..12 / B3-02..10 / B4-01/03/04/20..28 / B5-06 → C 阶段
- Commit: `docs(qa): audit rollup A2-08 (64 findings, P0=4 frozen)`（待提交）

## 限制
- 只读汇总，未重跑 `tools/docs_machine_consistency.py` 与全量测试；B 末需重跑并生成 D 阶段 SHA。
- `file_audit` shipping 分母 ~713+ 需工具找回后重算。
- 数值/并发未做 TSan/ASan 全量，建议 C 阶段矩阵复核。
