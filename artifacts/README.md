# artifacts

本目录是入库证据与产物的落位点：CI 运行产物与机器门基线/台账在这里归档。

## 职责边界

- 放：每次 CI 运行的产物（ci/）、机器门消费的基线与台账（evidence/，见下）。
- 不放：实测类证据（测量结果 / 基准数值 / 质量实测 / 审计时点证据）——按负责人裁决
  （2026-09-28，artifacts 重组）已全部迁 `实验/engineering-evidence/` 留档；
  一次性过程报告与日志（run/，gitignore）；块级运行输出（各命令的 output_dir）。
- 进入 evidence/ 的条目必须是机器门基线/台账（门功能消费，非实测），其增删要与
  eng/ci/impact_map.json 的路径域探针、eng/ci/root_manifest.json 的目录登记同步核对；
  evidence/ 的 README 是该目录的登记锚。

## 内容

- ci/ —— 每次 CI 运行一个独立子目录（以该次运行的提交标识命名），存放该次运行的检查与测试产物。
- evidence/ —— 机器门基线与台账：doc-hygiene/baseline.json（CHK-DOC-HYGIENE 棘轮基线）、
  known-limitations-ledger/LEDGER.md（CHK-DOC-HYGIENE D2 台账）、
  truthful-conclusion-01/file_audit.json（CHK-TRUTHFUL-CONCLUSION 判据依据）；
  另有未决条目 governance-01/、review-package/、v19r2/（归属待裁决，见该目录 README）。

## 上游

上游：docs/ASTROCS_DESIGN.md §12.4（验证层级与四层验收）；docs/ci/04_ARTIFACTS.md（产物与留存）；
ENGINEERING_SPEC.md §7（目录落位）。
