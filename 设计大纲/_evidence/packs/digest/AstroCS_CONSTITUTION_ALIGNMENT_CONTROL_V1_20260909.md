# 包取证摘要：AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909

身份由脚本归一（去复原前缀与 .zip 后缀）。共 1 个实例。

## 【worktree】工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909
- 文件 74 | 287.7 KB
- git 事件: {'first': ('f5f944a512017642193f687356e6a8b83c20997e', '2026-09-12T18:52:52+08:00'), 'last': ('e9547c02389d305532a7675a90903eeeda664d0e', '2026-09-10T15:14:01+08:00'), 'dels': [], 'adds_n': 6, 'mod_n': 39, 'touch_last': None}
- 台账 TASK_LEDGER.csv: 行 57 状态列 status 分布 {"PASSED": 29, "IN_PROGRESS": 5, "NOT_STARTED": 23} 任务号样例 BASE-001, GOV-001, GOV-002, WCS-001, WCS-002, WCS-003, PSF-001, DATA-001, AIO-001, AIO-002, P1-001, P2-001, P2-002, P3-001, P3-002, P1-HIPS-DIGEST-001
- 台账 TASK_LEDGER_V1_ARCHIVE.csv: 行 30 状态列 status 分布 {"PASSED": 9, "PASSED_FINDING_OPEN": 1, "NOT_STARTED": 19, "IN_FLIGHT": 1} 任务号样例 BASE-001, GOV-001, GOV-002, WCS-001, WCS-002, WCS-003, PSF-001, DATA-001, BASE-UTIL-001, P1-HIPS-DIGEST-001, ARCH-AUDIT-P1, AIO-001, AIO-002, P1-001, P2-001, P2-002
- 规格文件: 00_READ_FIRST.md, 01_BASELINE_AUDIT.md, 02_GATES_AND_EXECUTION.md, 03_AUDIT_PACKAGE_SPEC.md, 04_OWNER_DECISIONS_20260910.md, 05_FINDINGS_REGISTER_20260911.md, 06_V2_PACK_DESIGN_20260911.md, 07_FRONT_DESK_RULINGS_20260912.md, 08_FRONTDESK_HANDOVER_20260912.md
- tasks/ 共 57: AIO-001.md, AIO-002.md, ARCH-AUDIT-P1.md, ARCH-TB-001.md, AUD-001.md, BASE-001.md, BASE-UTIL-001.md, CI-001.md, CI-001B.md, CI-002.md, CI-BACKEND-001.md, CI-BASELINE-001.md, CI-DATA-REG-001.md, CI-REG-002.md, CI-REPAIR-001.md, CI-REPAIR-002.md, CI-VER-CHK-001.md, CI-WIN-001.md, CLI-001.md, CLI-001B.md, CON-COMMENT-001.md, CORE-RACE-001.md, DATA-001.md, DOC-001.md, DOC-CONV-001.md, GOV-001.md, GOV-002.md, GOV-AGENTS-001.md, MOD-001.md, MOD-001A.md, MOD-001B.md, P1-001.md, P1-HIPS-DIGEST-001.md, P2-001.md, P2-002.md
- schemas/: evidence.schema.json
- control-pack.json: {"schema": "cprun/v4", "id": "astrocs-constitution-alignment-v1", "title": "AstroCS 宪章一致性整改与发布复核", "workspace": "../..", "max_parallel": 12, "lanes": "[11 项] {'repo-write': {'capacity': 1}, 'rw-docs': {'capacity': 1}, 'rw-tools': {'capacity': 1}, 'rw-lib': {'capacity': 1}, 'rw-tests': {'capacity': 1}, 'rw-c", "gates": "[5 项] {'G-CI-FIX': {'all': [{'task': 'CI-DATA-REG-001', 'state': 'passed'}, {'task': 'CI-VER-CHK-001', 'state': 'passed'}, {'task': 'WCS-PATH-001', 'state':", "tasks": "[57 项] [{'id': 'BASE-001', 'title': '冻结预存工作区与旧台账', 'spec': 'tasks/BASE-001.md', 'lane': 'read-only', 'priority': 50, 'review': 'foreground', 'retry': {'max_a"}
- 00_READ_FIRST 标题: # 00｜唯一执行入口 ; ## 1. 本轮目标 ; ## 2. 执行前硬门（G-GOV） ; ## 3. 基线保护 ; ## 4. 启动顺序 ; ## 5. 完成定义 ; ## 6. 修订记录 ; ## 7. V2 重编与启动快照制（rev5，2026-09-11） ; ## 8. rev6 重编（2026-09-12，前台接续执行）
