# R0-003 PLAN — 建立 V6.1 证据与台账

## 需求
- 新建 `evidence/v6_1_rework/`，不修改 `evidence/refactor/` 历史。
- 原样复制本包 ledger，保存控制包和 ledger SHA。
- 实现 `tools/quality/validate_task_ledger.py`：状态仅允许
  NOT_STARTED/IN_PROGRESS/PASS/FAIL/BLOCKED/WAITING_WINDOWS/WAITING_DATA。
- 依赖不全 PASS 时当前任务不得 PASS；唯一例外 Owner 任务保持 NOT_STARTED。
- result schema 一致（`check_task_result_schema.py` 提供独立验证）。

## 现状证据
- evidence/v6_1_rework/ 已建；TASK_LEDGER.csv 从控制包复制（当前 hash 随状态推进变化，冻结原件 hash=c6f21304...）。
- 控制包 zip SHA-256=903018212bfb584b1aaf0dd05b318d8171d1a5b3af43014ab020834e9359ede6。
- 上轮 evidence/refactor/ 未被修改（git 确认仅新增 v6_1_rework）。

## 影响文件
- tools/quality/validate_task_ledger.py（新，10 负例 selftest PASS）
- tools/quality/check_task_result_schema.py（新，schema 一致性检查器）
- evidence/v6_1_rework/TASK_LEDGER.csv（工作台账，冻结原件不动）
- evidence/v6_1_rework/tasks/R0-003/{PLAN.md,TASK_RESULT.json,ledger_state.json,logs/*}

## 科学影响
无（纯治理）。

## 风险
- BLOCKED 需要 BLOCKED_REASON：采用可选列（不在 REQUIRED_COLUMNS 内），不破坏控制包列集。

## 验收命令
1. `python3 tools/quality/validate_task_ledger.py --selftest` → SELFTEST_PASS total=10
2. `python3 tools/quality/validate_task_ledger.py evidence/v6_1_rework/TASK_LEDGER.csv` → TASK_LEDGER_PASS
3. `python3 tools/quality/validate_task_ledger.py 工程控制/.../03_REWORK_TASK_LEDGER.csv --baseline-sha256 c6f213...` → 冻结台账 hash 未变
4. `python3 tools/quality/check_task_result_schema.py` → R0-003 自身 PASS（R0-001/002 历史缺字段由 GOV-001 修复）
