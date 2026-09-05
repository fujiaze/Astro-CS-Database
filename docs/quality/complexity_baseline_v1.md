# 复杂度基线 v1（complexity_baseline_v1）

- 任务：V8-CI-005（owner=SA-CI-32）　日期：2026-09-05
- 合同依据：`07_CI_MACHINE_CONTRACT.md`「Coverage 与复杂度」——第一次 deep CI
  只测量并记录基线，不设阈值；第二个原子任务再冻结每模块复杂度上限。
- 工具：`tools/quality/check_complexity.py`（正则近似：函数头计数 +
  全文分支 token；文件级粒度，不做函数体切分——namespace/class 花括号
  会把多函数吞并成假"巨型函数"，实测伪影 max≈504 已弃用该算法）。
- 度量域：`--paths lib,cli,include`；排除 `lib/acr`（ACR dormant，约束 §C）、
  `legacy`、`third_party`/`thirdparty`（vendored，QA-002 已 `-w` 隔离）。
- 阈值：**null（未冻结）**。检查 `DEEP-COMPLEXITY` 恒 exit 0（只记录，
  不做门禁判定）；正式 C++ 解析器与阈值冻结由后续质量任务实现。

## 首次基线（main @ d41f451618c1，本机实测）

| 指标 | 值 |
| --- | --- |
| files | 375 |
| lines | 139881 |
| functions_approx | 1858 |
| branch_tokens | 19182 |
| max_file_cyclomatic | 1048（`lib/orchestrator/cpp/src/orchestrator.cpp`） |

cli 域 top1：`cli/commands.cpp`（332）；include 域 top1：
`include/astrocs/core/contracts.h`（29）。

## hosted 后待补字段

- hosted linux-deep runner 上由 `DEEP-COMPLEXITY`（plan-only 可见）实跑复测，
  回填 `artifacts/ci/.../run 产物` 中 `run/ci/complexity/complexity.json` 的
  `generated_utc` 与 totals（启发式对文本内容确定性，数值应与本基线一致，
  除非源码已前进）。
- 阈值冻结时在此登记每模块上限与冻结 SHA。
