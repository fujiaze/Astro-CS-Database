# GOV-002 PLAN — 修复 gate 和依赖状态机

## 需求 (04_TASK_SPECIFICATIONS.md GOV-002)
- 修复上轮 `REVIEW_PENDING` 和"依赖未过但 release PASS"漏洞。
- validator 必须输出第一个和全部错误（聚合所有错误，不只第一个）。
- 不允许改 dependency 来使 ledger 通过。
- 负面 fixture：非法状态、两个 IN_PROGRESS、PASS 依赖 WAITING、环、缺任务、REL 提前 PASS、task count 不符。

## 现状证据
- R0-003 的 validate_task_ledger.py 已支持 7 状态 + 依赖检查（10 负例）。
- 上轮 ledger 存在 REL-001 PASS 而 WIN-006 WAITING_WINDOWS（F-034）与 REVIEW_PENDING 非法状态（F-035）。

## 影响文件
- tools/quality/validate_task_ledger.py（增强：全部错误聚合、--expected-tasks）
- evidence/v6_1_rework/tasks/GOV-002/{PLAN.md,TASK_RESULT.json,logs/*}

## 科学影响
无（纯治理）。

## 风险
- 聚合错误时跳过后续行解析需谨慎；已用 continue 保证结构错误仍逐项收集。

## 验收命令
1. `validate_task_ledger.py --selftest` → SELFTEST_PASS total=12（含 task_count_mismatch、all_errors_aggregated）
2. `validate_task_ledger.py evidence/v6_1_rework/TASK_LEDGER.csv --expected-tasks 67` → TASK_LEDGER_PASS
3. `--expected-tasks 66` → TASK_LEDGER_FAIL task count mismatch（负例）
