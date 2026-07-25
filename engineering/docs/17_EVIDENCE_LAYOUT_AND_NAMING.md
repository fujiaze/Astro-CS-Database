# 17 证据目录与命名

每个任务使用：

```text
evidence/<TASK_ID>/
  TASK_REPORT.md
  TEST_REPORT.md
  EVIDENCE_INDEX.md
  REVIEW_REPORT.md
  commands/
  logs/
  metrics/
  outputs/
  diffs/
```

真实数据不复制到证据目录，只保存逻辑 ID、路径、hash、inspect 摘要和必要的小型数值表。大型 HISS/HCSD 保存路径与 SHA-256。

重复检测专项建议证据：

```text
metrics/detector_call_count.json
metrics/star_det_hashes.json
metrics/wcs_ab_comparison.csv
metrics/psf_f32_vs_u16.csv
logs/stage1_new_path.jsonl
```
