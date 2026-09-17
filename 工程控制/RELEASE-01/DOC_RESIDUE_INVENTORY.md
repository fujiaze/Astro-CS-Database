# DOC-001 产出：文档树残留清单（移交 DOC-002）

> 本清单由 DOC-001 步骤 4 的全局搜索产生，**只登记不清理**。清理属 DOC-002 文件域（"非科学类文档的表述订正：风格、指向、过时残留"）；`docs/science/**`、`docs/algorithms/**` 中的科学表述订正属 SCI-001。
>
> 权威文档集（负责人上传 36 篇）已在 DOC-001 清零（见 `ACCEPTANCE.md` §2）。下表为**其余仓库文档**的残留量化。

## 1. 量化汇总（`grep -rIlE <pattern> docs/ *.md`，按文件数）

| 残留类型 | 活动文档（不含 docs/archive、*/v6） | 归档/冻结（docs/archive、*/v6） |
|---|---|---|
| 治理 ID `GAP-\d+` | 4 | 1 |
| 治理 ID `W\d-A\d+` | 16 | 0 |
| 日期 `2026-\d\d-\d\d` | 129 | 67 |
| 旧世代串 `V19` | 31 | 24 |
| 旧世代串 `v6` | 14 | 58 |
| 头部元信息块（首 6 行内 `状态/日期/版本/作者/审核/负责人/文档ID/Commit:`） | 8 | — |

## 2. 含治理/任务 ID 的活动文档（34 个）

```text
docs/algorithms/anchors/check_doc_line_anchors.py
docs/algorithms/GATES_AND_TOLERANCES.md
docs/algorithms/HIPS_WRITER.md
docs/algorithms/NOISE_ESTIMATION.md
docs/algorithms/PHASE2_COVERAGE.md
docs/algorithms/PHASE2_INTEGRATION.md
docs/algorithms/PHASE2_SESSION.md
docs/algorithms/PHASE3_FITS_IMPL.md
docs/algorithms/PHASE3_PROJ_IMPL.md
docs/algorithms/PLATESOLVE.md
docs/algorithms/STAR_DETECTION_ALGORITHMS.md
docs/architecture/doc_symbol_namespaces.json
docs/architecture/MODULE_MAP.md
docs/contracts/CONFIG_CONTRACT.md
docs/contracts/DATA_SEMANTICS.md
docs/contracts/PUBLIC_API.md
docs/contracts/UNIFIED_OBJECTS.md
docs/DOCUMENT_INDEX.yaml
docs/modules/phase3_proj.md
docs/modules/registry/astrocs.phase3.wcs.md
docs/modules/registry/module_id_migration_baseline.json
docs/owner/RELEASE_STATUS.md
docs/science/ASTROMETRY.md
docs/science/CALIBRATION.md
docs/science/NOISE_MODEL.md
docs/science/PHASE2_UPM.md
docs/science/PHASE3_HIPS_TO_FITS.md
docs/science/PHOTOMETRY.md
docs/science/REJECTION.md
docs/science/STAR_DETECTION.md
docs/science/UNCERTAINTY_AND_COVARIANCE.md
docs/standards/checks/check_standards_registry.py
docs/standards/STANDARDS_REGISTRY.md
docs/traceability/traceability_warn_baseline.json
```

## 3. 含头部元信息块的活动文档（8 个）

```text
docs/design/PHASE1_DETAILED_DESIGN.md
docs/design/PHASE2_DETAILED_DESIGN.md
docs/design/PHASE3_DETAILED_DESIGN.md
docs/owner/PROJECT_SPEC.md
docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md
docs/references/SCIENTIFIC_REFERENCES.md
docs/science/PSF_SIGNAL_WEIGHT.md
docs/science/UNIFIED_SCIENCE_MODEL.md
```

## 4. 清理约束（移交时必读）

- `docs/science/**`、`docs/algorithms/**` 的公式/推导**语义不得改**；只清治理 ID/日期/头部块等非语义痕迹（SCI-001 复核）。
- `docs/architecture/doc_symbol_namespaces.json`、`docs/modules/registry/module_id_migration_baseline.json`、`docs/traceability/traceability_warn_baseline.json` 是**机器消费的登记面**（含 `task` 字段），改动前必须核对消费方检查器，禁止为清痕迹而破坏判据。
- `docs/standards/checks/*.py`、`docs/algorithms/anchors/*.py` 中的 `W#-A#` 出现在**代码注释**里，属注释卫生（ENGINEERING_SPEC §2），需与检查器判据同步。
- 归档目录（`docs/archive/**`）是否保留为历史档案、还是按"无历史叙事"一并清理，**需负责人确认**（DOC-002 前置裁决项）。

## 5. 复跑方式

```bash
# 治理 ID
grep -rIlE 'GAP-[0-9]+|W[0-9]-A[0-9]+|LEDGER-[A-Za-z]+|SC-FIX' docs/ *.md | grep -vE '^docs/archive/|/v6/'
# 日期
grep -rIlE '2026-[0-9]{2}-[0-9]{2}' docs/ *.md | grep -vE '^docs/archive/|/v6/' | wc -l
# 头部元信息块
for f in $(find docs -name '*.md'; ls *.md); do case "$f" in docs/archive/*|*/v6/*) continue;; esac; head -6 "$f" | grep -qE '^(状态|日期|版本|作者|审核|负责人|文档 ?ID|Commit|commit)[:：]' && echo "$f"; done
```
