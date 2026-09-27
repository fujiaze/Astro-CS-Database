# artifacts

本目录是入库证据与产物的落位点：验收读数、CI 运行产物与被正式文档引用的证据锚在这里归档。

## 职责边界

- 放：四层验收的证据记录（acceptance/）、每次 CI 运行的产物（ci/）、被正式文档或检查器反向引用的证据锚（evidence/）。
- 不放：一次性过程报告与日志（run/，gitignore）、块级运行输出（各命令的 output_dir）。
- 进入 evidence/ 的条目必须是活锚，且其增删要与 eng/ci/impact_map.json 的路径域探针、eng/ci/root_manifest.json 的目录登记同步核对；evidence/ 的 README 是该目录的登记锚。

## 内容

- acceptance/ —— 验收层证据：L2/、l2_performance/（性能门读数、探针、时序与机器指纹）、L3/、L4/ 各自的状态与证据文件。
- ci/ —— 每次 CI 运行一个独立子目录（以该次运行的提交标识命名），存放该次运行的检查与测试产物。
- evidence/ —— 证据锚库，按主题分子目录（v6/、release-01/、release-05/、compress-01/、known-limitations-ledger/ 等），供正式文档逐条引用。

## 上游

上游：docs/ASTROCS_DESIGN.md §12.4（验证层级与四层验收）；docs/ci/04_ARTIFACTS.md（产物与留存）；ENGINEERING_SPEC.md §7（目录落位）。