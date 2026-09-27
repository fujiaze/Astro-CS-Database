
# docs/validation/v6 索引（QA-MATRIX-001）

> 上游：ASTROCS_DESIGN.md §12（验证体系）


| 文件 | 内容 |
|---|---|
| `QA_MATRIX.md` | 44 条门的规格（判据/容差来源/零用例即红/mutation/输入输出/单位/适用域/fail-closed），含机读渲染块 |
| `BASELINE_COMPARISON_MATRIX.md` | 三生产模式 × 文档基线 + 延迟模式的比较矩阵与声明规则 |
| `NEGATIVE_MUTATION_CATALOG.md` | 56 条 science/spec/doc mutation 目录 |
| `ORACLE_AND_ZERO_CASE_POLICY.md` | 独立 Oracle、自证来源独立、零用例/skip-only 即红政策 |
| `P0_GATE_FAMILY.md` | 历史根因 R1/R2/R4/R5/R10 与 785 合并层账本的 P0 门族（只登记） |

配套（tracked，`artifacts/evidence/v6/qa-design/`）：`artifacts/evidence/v6/qa-design/SUMMARY.md`、`qa_matrix.json`、`case_ledger.json`、
`data/*.json`、`oracle/*.py`、`evidence/`。

**条款 id 落点**：本目录各文引用的 `FZ-*` 条款 id 的
登记解析权威 = `docs/contracts/DATA_SEMANTICS.md` §31（§31.10 = 条款注册表与待签登记，
含 `CHG-2026-09-22-V6-CONTRACT-MERGE` 的 V6 合同层设计档案与冻结 JSON 出库清单）；
本目录内条款 id 一律按 §31.10 解析。

复跑：`python3 artifacts/evidence/v6/qa-design/oracle/run_all.py`（单 rc；证据落 `artifacts/evidence/v6/qa-design/evidence/`）。

状态：本目录为**验证设计规格**；生产实现与真实数据执行归 W5/W7/W8/W10/W11。未 commit/push、未宣布发布。
