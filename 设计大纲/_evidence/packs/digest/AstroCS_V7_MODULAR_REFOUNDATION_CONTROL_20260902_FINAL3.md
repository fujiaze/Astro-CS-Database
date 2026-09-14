# 包取证摘要：AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3

身份由脚本归一（去复原前缀与 .zip 后缀）。共 1 个实例。

## 【worktree】工程控制/AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3
- 文件 99 | 388.5 KB
- git 事件: {'first': None, 'last': None, 'dels': None, 'adds_n': 0, 'mod_n': None, 'touch_last': None}
- 台账 TASK_LEDGER.csv: 行 191 状态列 None 分布 {} 任务号样例 CTL-001, ID-001, ID-002, ID-003, AUD-001, GOV-001, GOV-002, GOV-003, GOV-004, GOV-005, DOC-001, TST-001, ARC-001, BLD-001, BLD-002, BLD-003
- 规格文件: 00_READ_FIRST.md, 01_OWNER_FROZEN_CONSTRAINTS.md, 02_CURRENT_BASELINE_AUDIT.md, 03_TARGET_PRODUCT_AND_ARCHITECTURE.md, 04_FOREGROUND_AGENT_RUNBOOK.md, 06_TASK_GRAPH_AND_WAVES.md, 07_CHECKPOINTS_AND_GATES.md, 08_GIT_MAIN_ONLY_INTEGRATION.md, 09_WINDOWS_TOOLCHAIN_LOCK.md, 10_LINUX_CONTROL_NODE.md, 11_MODULE_SOURCE_TEST_STANDARD.md, 12_DLL_ABI_AND_LOADER_STANDARD.md, 13_DATA_PIPELINE_AND_ARTIFACT_STANDARD.md, 14_RUNTIME_SCHEDULER_AND_TRACE_STANDARD.md, 15_CPU_PROVIDER_AND_RESOURCE_STANDARD.md, 16_SCIENCE_DOCUMENT_AND_TRACEABILITY_STANDARD.md, 17_INDEPENDENT_REVIEWER.md, 18_AUDIT_PACKAGE_SPEC.md, 19_MACHINE_CHECKS.md, 20_RELEASE_GATES.md, 21_PRIMARY_REFERENCES.md, 22_FAILURE_ESCALATION.md, 23_GRAPH_AND_DOC_TOOL_POLICY.md
- tasks/ 共 12: 00_TASK_EXECUTION_CONTRACT.md, 01_W0_GOVERNANCE_TASKS.md, 02_ABI_BUILD_CLI_TASKS.md, 03_RUNTIME_DATA_IO_TASKS.md, 04_CPU_RESOURCE_TASKS.md, 05_PHASE1_TASKS.md, 06_PHASE2_TASKS.md, 07_PHASE3_TASKS.md, 08_QA_LINUX_TASKS.md, 09_WINDOWS_RELEASE_TASKS.md, 10_AUDIT_PACKAGING_TASKS.md, MODULE_MIGRATION_TEMPLATE.md
- schemas/: artifact_manifest.schema.json, audit_summary.schema.json, dispatch.schema.json, evidence_record.schema.json, finding.schema.json, gate_result.schema.json, module_manifest.schema.json, task_result.schema.json, task_state.schema.json
- templates/: AGENTS.md, DISPATCH.json, EVIDENCE_RECORD.json, INDEPENDENT_FINDING.json, L0_OWNER_REVIEW.md, MODULE_README.md, REVIEWER_SIGNOFF.md, TASK_RESULT.json, TASK_STATE.json
- 包内 SHA256SUMS 核对: {"file": "SHA256SUMS", "ok": 97, "mismatch": 1, "missing": 0, "mismatch_list": ["TASK_LEDGER.csv"]}
- START_PROMPT 开头: 解压校验控制包，执行00_READ_FIRST.md；前台仅调度、验收、串行集成、原子推送和白名单打包，长任务交固定SubAgent，自动推进，不逐关停工。 ⏎ 
- 00_READ_FIRST 标题: # AstroCS Alpha 模块化重构控制包：先读我 ; ## 1. 本轮唯一目标 ; ## 2. 执行者第一小时必须完成 ; ## 3. 权限模型 ; ### 前台 Agent 可以 ; ### 前台 Agent 不可以 ; ### SubAgent 可以 ; ### SubAgent 不可以 ; ## 4. 状态与不停工规则 ; ## 5. 验收结论词义 ; ## 6. 交付
