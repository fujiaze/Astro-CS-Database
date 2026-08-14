# Docs / API / Config 一致性（V17）

## 修复的 stale docs

| 文档 | V16 问题 | V17 修复 |
| --- | --- | --- |
| docs/contracts/PUBLIC_API.md | 残留 p2_rejection_workspace_create/free | 删除；补 V16/V17 API（collector/weight validation/large_scale/integrate status） |
| docs/validation/SCIENCE_FREEZE.md | V15 状态 + 20-exposure gate + ROUND0-6 PASS | 更新 V17；freeze=CANDIDATE（Round6 终验后 PASS） |
| reports/rejection_semantics.md | MinMax 表格含 max_iterations；API 清单含 workspace | 移除；补 V17 status/normalization/large_scale/observed 命名 |
| docs/development/CONFIG_SCHEMA.md | wbpp_current canonical；deprecation adapter；linear_fit 4/3；percentile 0.1/0.1 | wbpp_2_9_1 + alias；large_scale；WBPP Light 5.0/3.5、0.2/0.1 |
| reports/api_inventory.md / wbpp_policy.md / wbpp_feature_matrix.md / satellite_v2.md / satellite_rejection.md / final_status.md / performance.md / full_e2e.md / rejection_normalization.md | 旧 profile/normalization 命名；false reject | 全部 canonical 化 + observed 命名 |

## Machine checks（evidence/api_doc_consistency.json）

```text
public_header_api_vs_docs   PASS（deleted API absent；新 API present）
semantic_ids_vs_docs        PASS（11 canonical IDs 全部出现）
status_enums_vs_docs        PASS（P2_STATUS_* / P2_INTEGRATE_*）
profile_canonical           PASS（wbpp_current 仅 alias 标注）
observed_rejection_naming   PASS（无未标注 false reject）
freeze_version_vs_report    PASS（V17 + CANDIDATE 字面量一致）
schema_vs_parser            PASS
defaults_vs_template        PASS（含 large_scale 四字段）
```

`tools/config_consistency_check.py`：checked_keys 全量（含
large_scale.enabled/min_structure_pixels/low|high_grow_radius_pixels），
mismatches=[]。

```text
DOC_CONTRACT_CONSISTENCY = PASS
```
