# 包取证摘要：archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1

身份由脚本归一（去复原前缀与 .zip 后缀）。共 1 个实例。

## 【worktree】engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1
- 文件 855 | 40408.5 KB | 容器（含子包 9 个）
- git 事件: {'first': ('b7b2dea70dbcdacdcf6eb762609a908abdeab697', '2026-09-02T22:15:41+08:00'), 'last': ('b7b2dea70dbcdacdcf6eb762609a908abdeab697', '2026-09-02T22:15:41+08:00'), 'dels': [], 'adds_n': 0, 'mod_n': 2, 'touch_last': None}
- PACKAGE_VERSION: 1.3.0 ⏎ 
- 台账 CONTROL_V4/AstroCS_MAIN_PRERELEASE_CONTROL_V4_CPU_ADAPTIVE_20260828/02_TASK_LEDGER.csv: 行 54 状态列 status 分布 {"NOT_STARTED": 54} 任务号样例 BASE-001, GOV-001, BASE-002, SCI-001, SCI-002, SCI-003, SCI-004, SCI-005, SCI-006, ALG-001, ALG-002, ALG-003, ALG-004, ALG-005, DOC-001, DOC-002
- 台账 CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/05_TASK_LEDGER.csv: 行 88 状态列 status 分布 {"NOT_STARTED": 88} 任务号样例 BAS-001, BAS-002, BAS-003, BAS-004, VER-001, DOC-001, SCI-001, SCI-002, SCI-003, DATA-001, ARCH-001, API-001, TEST-001, BLD-001, CORE-001, CORE-002
- 台账 REAUDIT_V3/v3_reaudit/02_TASK_LEDGER.csv: 行 62 状态列 status 分布 {"PASS": 47, "FAIL": 1, "IN_PROGRESS": 3, "BLOCKED": 4, "NOT_STARTED": 7} 任务号样例 ID-001, ID-002, ID-003, CON-001, CON-002, CON-003, CON-004, CON-005, CON-006, CON-007, CON-008, CON-009, CON-010, SCI-001, SCI-002, SCI-003
- 台账 RELEASE_V5/AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828/02_TASK_LEDGER.csv: 行 98 状态列 status 分布 {"PASS": 88, "BLOCKED": 1, "REVIEW_PENDING": 1, "IN_PROGRESS": 1, "NOT_STARTED": 7} 任务号样例 BASE-001, GOV-001, VER-001, TRACE-001, DOC-001, SCI-001, SCI-002, SCI-003, SCI-004, SCI-005, SCI-006, SCI-007, ALG-001, ALG-002, ALG-003, ALG-004
- tasks/ 共 85: P09-001.md, P09-002.md, P09-003.md, P10-001.md, P10-002.md, P10-003.md, P10-004.md, P10-005.md, P10-006.md, P11-001.md, P11-002.md, P11-003.md, P11-004.md, P11-005.md, P11-006.md, P12-001.md, P12-002.md, P12-003.md, P12-004.md, P12-005.md, P12-006.md, P13-001.md, P13-002.md, P13-003.md, P13-004.md, P14-001.md, P14-002.md, P14-003.md, P14-004.md, P14-005.md, P14-006.md, P14-007.md, P14-008.md, P15-001.md, P15-002.md
- schemas/: stage2.schema.json
- templates/: ADR.md, BLOCKED_REPORT.md, EVIDENCE_INDEX.md, P11_004_DECISION.md, REVIEW_REPORT.md, SESSION_CHECKPOINT.md, TASK_REPORT.md, TEST_REPORT.md
- 包内 SHA256SUMS 核对: {"file": "SHA256SUMS.txt", "ok": 122, "mismatch": 9, "missing": 1, "mismatch_list": ["AUTONOMOUS_ENTRY.md", "contracts/wcs_authoritative_pairs.schema.json", "control/CURRENT_TASK.md", "control/DECISION_REGISTER.md", "control/MASTER_TASK_REGISTER.csv", "control/PROJECT_STATE.yaml", "control/REQUIREMENTS_TRACEABILITY.csv", "control/RISK_REGISTER.csv", "docs/18_CODE_CHANGE_MAP.md"]}
- START_PROMPT 开头: 解压最新AstroCS修复开发包，读取AUTONOMOUS_ENTRY.md，迁移v1.2进度并持续执行，仅硬阻塞或全部完成时汇报。 ⏎ 
- 00_READ_FIRST 标题: # AstroCS V4：CPU 自适应预发布控制 ; ## 目标 ; ## 本包优先级 ; ## 硬规则 ; ## 启动顺序 ; ## 审核责任 ; ## 唯一允许状态
