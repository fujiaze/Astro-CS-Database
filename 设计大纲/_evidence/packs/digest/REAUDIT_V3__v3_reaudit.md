# 包取证摘要：REAUDIT_V3/v3_reaudit

身份由脚本归一（去复原前缀与 .zip 后缀）。共 2 个实例。

## 【recovered_from_history】history/3703650d/工程控制/REAUDIT_V3/v3_reaudit
- 文件 23 | 54.1 KB
- git 事件: {'first': ('6a947f518963b41e4546dd05d6219d06250ffd8c', '2026-08-28T19:29:31+08:00'), 'last': ('3703650df136a5dff27361f3ef8232509a7628c7', '2026-08-27T23:08:54+08:00'), 'dels': [], 'adds_n': 2, 'mod_n': 27, 'touch_last': None}
- 台账 02_TASK_LEDGER.csv: 行 62 状态列 status 分布 {"PASS": 10, "IN_PROGRESS": 1, "NOT_STARTED": 51} 任务号样例 ID-001, ID-002, ID-003, CON-001, CON-002, CON-003, CON-004, CON-005, CON-006, CON-007, CON-008, CON-009, CON-010, SCI-001, SCI-002, SCI-003
- 规格文件: 00_READ_FIRST.md, 01_WORKFLOW_AND_GATES.md, 03_TASK_SPECIFICATIONS.md, 04_CHECKPOINT_CHECKLISTS.md, 05_NUMERICAL_AND_PERFORMANCE_GATES.md, 06_GIT_MAIN_ONLY.md, 07_AUDIT_PACKAGE_SPEC.md, 08_V2_INHERITED_FINDINGS.md, 09_CONTINUOUS_EXECUTION_ORDER.md
- scripts/: package_audit.py, validate_audit_package.py, validate_control.py
- templates/: BUILD_RESULTS.csv, CHECKPOINT_RESULTS.csv, COMMITS.csv, FINDINGS.csv, LARGE_ARTIFACT_MANIFEST.csv, PERF_RESULTS.csv, SUMMARY.schema.json, TEST_RESULTS.csv, TRACEABILITY.csv
- 00_READ_FIRST 标题: # AstroCS MAIN 预发布重审控制 V3 ; ## 唯一目标 ; ## 不可变规则 ; ## 执行入口 ; ## 当前证据起点 ; ## 立即停止条件

## 【worktree】engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/REAUDIT_V3/v3_reaudit
- 文件 23 | 55.5 KB
- 父包目录: engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1
- git 事件: {'first': ('b7b2dea70dbcdacdcf6eb762609a908abdeab697', '2026-09-02T22:15:41+08:00'), 'last': ('b7b2dea70dbcdacdcf6eb762609a908abdeab697', '2026-09-02T22:15:41+08:00'), 'dels': [], 'adds_n': 0, 'mod_n': 2, 'touch_last': None}
- 台账 02_TASK_LEDGER.csv: 行 62 状态列 status 分布 {"PASS": 47, "FAIL": 1, "IN_PROGRESS": 3, "BLOCKED": 4, "NOT_STARTED": 7} 任务号样例 ID-001, ID-002, ID-003, CON-001, CON-002, CON-003, CON-004, CON-005, CON-006, CON-007, CON-008, CON-009, CON-010, SCI-001, SCI-002, SCI-003
- 规格文件: 00_READ_FIRST.md, 01_WORKFLOW_AND_GATES.md, 03_TASK_SPECIFICATIONS.md, 04_CHECKPOINT_CHECKLISTS.md, 05_NUMERICAL_AND_PERFORMANCE_GATES.md, 06_GIT_MAIN_ONLY.md, 07_AUDIT_PACKAGE_SPEC.md, 08_V2_INHERITED_FINDINGS.md, 09_CONTINUOUS_EXECUTION_ORDER.md, 10_REVIEWER_DEVIATIONS.md
- scripts/: package_audit.py, validate_audit_package.py, validate_control.py
- templates/: BUILD_RESULTS.csv, CHECKPOINT_RESULTS.csv, COMMITS.csv, FINDINGS.csv, LARGE_ARTIFACT_MANIFEST.csv, PERF_RESULTS.csv, SUMMARY.schema.json, TEST_RESULTS.csv, TRACEABILITY.csv
- 00_READ_FIRST 标题: # AstroCS MAIN 预发布重审控制 V3 ; ## 唯一目标 ; ## 不可变规则 ; ## 执行入口 ; ## 当前证据起点 ; ## 立即停止条件
