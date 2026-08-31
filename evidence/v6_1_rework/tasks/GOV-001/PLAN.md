# GOV-001 PLAN — 修复任务证据与 commit 绑定

## 需求 (04_TASK_SPECIFICATIONS.md GOV-001)
- `parent_commit` = 任务开始 SHA；`result_commit` 只能由打包器从 `git log --first-parent main` 提取。
- `COMMITS.csv` 字段固定：task_id、parent_commit、result_commit、commit_subject、committed_utc、pushed_origin_main、changed_paths_sha256、task_result_sha256。
- 验证：每个 commit subject 只出现一个 task ID；parent/result 形成连续链；每个 result commit 是最终 source commit 祖先；origin_main 证明存在；故意篡改 SHA、交换任务、缺 push 均失败。
- 同时修复 F-040（旧 TASK_RESULT source_commit==start_commit 语义无法标识结果 commit）与 R0-001/002 结果 schema 合规。

## 现状证据
- R0-001..004 已提交并 push（连续链 a3d16d8→31b10ca→d7b202e→5067382）。
- R0-001/R0-002 TASK_RESULT 缺 started_utc/duration_seconds/log_sha256（schema 不合规）。

## 影响文件
- tools/quality/gen_commits_csv.py（新）
- tools/quality/check_commits_csv.py（新）
- evidence/v6_1_rework/COMMITS.csv（生成）
- evidence/v6_1_rework/tasks/R0-001/TASK_RESULT.json（schema 修复）
- evidence/v6_1_rework/tasks/R0-002/TASK_RESULT.json（schema 修复）
- evidence/v6_1_rework/tasks/GOV-001/{PLAN.md,TASK_RESULT.json,logs/*}

## 科学影响
无（纯证据治理）。

## 风险
- 修复后的 TASK_RESULT 文件 hash 变化 → COMMITS.csv 的 task_result_sha256 需重新生成（已做）。

## 验收命令
1. `gen_commits_csv.py` → COMMITS_GEN_PASS tasks=4
2. `check_commits_csv.py --commits ...` → COMMITS_CHECK_PASS（真实链通过）
3. `check_commits_csv.py --selftest` → SELFTEST_PASS（tamper/swap/unpushed 全被抓）
4. `check_task_result_schema.py` → SCHEMA_CHECK_PASS checked=4
