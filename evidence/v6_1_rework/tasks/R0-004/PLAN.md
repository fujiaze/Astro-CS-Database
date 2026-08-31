# R0-004 PLAN — 复现并冻结 40 项已知问题

## 需求
- 逐项执行 F-001..F-040，每项写最小复现命令、stdout/stderr、文件/符号、结果。
- 无法因审核包缺文件而复现时，在完整 repo 复现；不能把"没找到"直接 CLOSED。
- 输出 `KNOWN_FAILURES_BASELINE.json`，40 项状态只能 REPRODUCED / NOT_REPRODUCED_WITH_EVIDENCE / SOURCE_CHANGED。
- P0/P1 不允许靠改文档关闭。

## 现状证据
- 控制包 `02_FINDINGS.csv` 40 项全 OPEN。
- 控制包 `known_failure_scan.py` 独立扫描命中 18/19 探测项。
- 本任务扩展扫描器 `tools/quality/known_failures_baseline.py` 全覆盖 40 项。

## 影响文件
- tools/quality/known_failures_baseline.py（新）
- evidence/v6_1_rework/tasks/R0-004/KNOWN_FAILURES_BASELINE.json（冻结基线）
- evidence/v6_1_rework/tasks/R0-004/{PLAN.md,TASK_RESULT.json,logs/*}

## 科学影响
无（只读复现，不改代码）。

## 风险
- 复现判定基于静态证据 + 现场命令；部分 finding（如 F-032/033/036/037 审核包内容）以仓库内 V5 审核包与上轮证据为准，属诚实登记。

## 验收命令
1. `python3 tools/quality/known_failures_baseline.py --repo . --output .../KNOWN_FAILURES_BASELINE.json`
   → findings=40 reproduced=40（全 REPRODUCED）
2. 控制包 `known_failure_scan.py` 独立验证 18 项命中
3. status 集合合法；P0/P1 无靠文档关闭
