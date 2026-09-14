# 包取证摘要：REAUDIT_V4/v4_reaudit

身份由脚本归一（去复原前缀与 .zip 后缀）。共 1 个实例。

## 【recovered_from_history】history/b12305ed/工程控制/REAUDIT_V4/v4_reaudit
- 文件 24 | 70.9 KB
- git 事件: {'first': ('b12305ed0df13318f9b0369d1d87e8c2e63edd28', '2026-08-28T20:54:30+08:00'), 'last': ('020cdc994dc42035c6eba7efd68c07e19d175415', '2026-08-28T20:36:03+08:00'), 'dels': [('b12305ed0df13318f9b0369d1d87e8c2e63edd28', '2026-08-28T20:54:30+08:00'), ('b12305ed0df13318f9b0369d1d87e8c2e63edd28', '2026-08-28T20:54:30+08:00')], 'adds_n': 2, 'mod_n': 1, 'touch_last': None}
- 台账 02_TASK_LEDGER.csv: 行 98 状态列 status 分布 {"PASS": 5, "NOT_STARTED": 93} 任务号样例 C0-001, C0-002, C0-003, C0-004, C0-005, C1-001, C1-002, C1-003, C1-004, C1-005, C1-006, C1-007, C1-008, C2-001, C2-002, C2-003
- 规格文件: 00_READ_FIRST.md, 01_WORKFLOW_AND_GATES.md, 03_TASK_SPECIFICATIONS.md, 04_CHECKPOINT_CHECKLISTS.md, 05_NUMERICAL_AND_PERFORMANCE_GATES.md, 06_GIT_MAIN_ONLY.md, 07_AUDIT_PACKAGE_SPEC.md, 08_V2_INHERITED_FINDINGS.md
- scripts/: make_capsule.sh, package_audit.py, validate_audit_package.py, validate_control.py
- templates/: BUILD_RESULTS.csv, CHECKPOINT_RESULTS.csv, COMMITS.csv, FINDINGS.csv, LARGE_ARTIFACT_MANIFEST.csv, PERF_RESULTS.csv, SUMMARY.schema.json, TEST_RESULTS.csv, TRACEABILITY.csv
- 00_READ_FIRST 标题: # AstroCS MAIN 预发布重审控制 V4 ; ## 唯一目标 ; ## 不可变规则 ; ## 执行入口 ; ## 当前证据起点（继承）
