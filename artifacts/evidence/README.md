# evidence 目录说明（机器门基线与台账）

本目录只收留**机器门消费的基线与台账**（门功能消费，非实测类）。
按负责人裁决（2026-09-28，artifacts 重组），实测类内容（测量结果 / 基准数值 / 质量实测 /
审计时点证据）已全部迁 `实验/engineering-evidence/` 留档，迁移映射见该目录 README；
正式文档只写设计不引外部锚（唯一例外 = 科学链路引 `实验/`），实验结论写成文档规范条款。

## 现行保留内容（每条都是机器门的功能输入，不得删除）

| 路径 | 保留理由 |
|---|---|
| `README.md`（本文件） | 本目录的锚（`eng/ci/impact_map.json` 路径域探针 + `eng/ci/root_manifest.json` required_dirs），不得删除 |
| `doc-hygiene/baseline.json` | `check_doc_hygiene.py` 棘轮基线（只减不增），路径硬编码于 `eng/tools/doccheck/check_doc_hygiene.py` |
| `known-limitations-ledger/LEDGER.md` | `docs/KNOWN_LIMITATIONS.md` 条目号解析台账（编号注册表 / 严重度 / 状态 / 发现任务 / 裁决出处）；CHK-DOC-HYGIENE D2d 判据要求其存在且 §1 编号表可解析 |
| `truthful-conclusion-01/file_audit.json` | `eng/tools/quality/check_conclusion_truth.py`（CHK-TRUTHFUL-CONCLUSION）判据依据（file_audit 覆盖率复算件） |

## 未决条目（原位保留，归属待负责人裁决）

| 路径 | 现状 |
|---|---|
| `governance-01/` | 治理证据锚：`retire/RETIREMENT_LEDGER.md`（退役工具复原坐标，`docs/ci/01_CHECKS.md` §2.2 与 root_manifest notes 引用）、`research/`（R-1..R-6 等研究件）、`security/EXPOSURE_NOTE.md`（root_manifest 登记）。非实测、非门基线，未入迁移清单 |
| `review-package/` | V3 后问题、推导和缺陷账本（`docs/references/SCIENTIFIC_REFERENCES.md` 引用）。未决裁决落点，非实测 |
| `v19r2/` | `evidence/quality/traceability_check.json`（CHK-SCI-REF 历史登记输出；该门 outputs 现已指 `run/ci/quality/traceability_check.json`）。处置待裁决 |

## 目录规则

- 新报告一律落 `run/<task>/`（gitignore，不入库）；实测类证据一律落 `实验/engineering-evidence/`；
  只有**机器门功能消费**的基线/台账才允许进本目录；
- 新增/删除本目录条目时，必须同步核对 `eng/ci/impact_map.json` 的路径域探针与
  `eng/ci/root_manifest.json` 的 `required_dirs`（本目录不得为空目录出库）。
