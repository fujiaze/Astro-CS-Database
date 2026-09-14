# 包取证摘要：AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830

身份由脚本归一（去复原前缀与 .zip 后缀）。共 3 个实例。

## 【recovered_from_history】history/4b1b948e/工程控制/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830
- 文件 47 | 196.5 KB
- git 事件: {'first': ('4b1b948e3b3710cbd7e84030fbb1a363128f0da2', '2026-08-30T23:20:14+08:00'), 'last': ('4b1b948e3b3710cbd7e84030fbb1a363128f0da2', '2026-08-30T23:20:14+08:00'), 'dels': [], 'adds_n': 4, 'mod_n': 0, 'touch_last': None}
- 台账 05_TASK_LEDGER.csv: 行 88 状态列 status 分布 {"NOT_STARTED": 88} 任务号样例 BAS-001, BAS-002, BAS-003, BAS-004, VER-001, DOC-001, SCI-001, SCI-002, SCI-003, DATA-001, ARCH-001, API-001, TEST-001, BLD-001, CORE-001, CORE-002
- 规格文件: 00_READ_FIRST.md, 01_ASTROCS_ENGINEERING_CONSTRAINTS.md, 02_BASELINE_AUDIT.md, 03_TARGET_ARCHITECTURE.md, 04_MIGRATION_AND_GATES.md, 06_TASK_SPECIFICATIONS.md, 07_SCIENCE_AND_TEST_MATRIX.md, 08_CPU_PARALLEL_BACKEND.md, 09_DOCUMENTATION_AND_MACHINE_CHECKS.md, 10_LINUX_WINDOWS_EXECUTION.md, 11_GIT_MAIN_ONLY.md, 12_AUDIT_PACKAGE_SPEC.md, 13_RELEASE_ACCEPTANCE.md, 14_PRIMARY_REFERENCES.md, 15_GATE_CHECKLISTS.md
- scripts/: package_audit.py, selftest.py, validate_audit.py, validate_control.py, validate_task_graph.py
- schemas/: audit_summary.schema.json, cpu_profile.schema.json, data_artifact.schema.json, module_descriptor.schema.json, pipeline_ir.schema.json, task_result.schema.json
- templates/: ALG_CONTRACT.md, CHANGE_REVIEW.md, LARGE_ARTIFACT_MANIFEST.csv, MODULE_README.md, OWNER_REVIEW.md, RESOURCE_SUMMARY.csv, SCI_CONTRACT.md, TASK_REPORT.md, TEST_SUMMARY.csv
- 包内 SHA256SUMS 核对: {"file": "SHA256SUMS", "ok": 44, "mismatch": 0, "missing": 0, "mismatch_list": []}
- MANIFEST.json: {"schema": "astrocs.control-manifest/v1", "control_version": "V6.0", "target_version": "0.10.0-alpha.1", "baseline_commit": "587fe0e341a780da726917f40ed77f610de0c73f", "engineering_constraints_sha256": "dc47fbbbeccb239fc1834a985b357d6ded50080977708b6ea5c9811bf4de4b92", "generated_utc": "2026-08-30T15:06:37Z", "files": "[44 项] [{'path': '00_READ_FIRST.md', 'size': 4736, 'sha256': 'ef821216c25c6e7a6f16b2648b449ab1962d25b77cf3097f32ae7f119376d99c'}, {'path': '01_ASTROCS_ENGINE"}
- START_PROMPT 开头: 校验控制包，严格按00入口和任务DAG连续重构main；一任务一提交推送，未过硬门不得称完成。 ⏎ 
- 00_READ_FIRST 标题: # AstroCS V6 系统重构控制包：执行入口 ; ## 1. 任务目标 ; ## 2. 文档优先级 ; ## 3. 执行前必须完成 ; ## 4. 连续执行规则 ; ## 5. 修改纪律 ; ## 6. 绝对禁止项 ; ## 7. 完成定义

## 【worktree】engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830
- 文件 46 | 196.1 KB
- 父包目录: engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1
- git 事件: {'first': ('b7b2dea70dbcdacdcf6eb762609a908abdeab697', '2026-09-02T22:15:41+08:00'), 'last': ('b7b2dea70dbcdacdcf6eb762609a908abdeab697', '2026-09-02T22:15:41+08:00'), 'dels': [], 'adds_n': 0, 'mod_n': 4, 'touch_last': None}
- 台账 05_TASK_LEDGER.csv: 行 88 状态列 status 分布 {"NOT_STARTED": 88} 任务号样例 BAS-001, BAS-002, BAS-003, BAS-004, VER-001, DOC-001, SCI-001, SCI-002, SCI-003, DATA-001, ARCH-001, API-001, TEST-001, BLD-001, CORE-001, CORE-002
- 规格文件: 00_READ_FIRST.md, 01_ASTROCS_ENGINEERING_CONSTRAINTS.md, 02_BASELINE_AUDIT.md, 03_TARGET_ARCHITECTURE.md, 04_MIGRATION_AND_GATES.md, 06_TASK_SPECIFICATIONS.md, 07_SCIENCE_AND_TEST_MATRIX.md, 08_CPU_PARALLEL_BACKEND.md, 09_DOCUMENTATION_AND_MACHINE_CHECKS.md, 10_LINUX_WINDOWS_EXECUTION.md, 11_GIT_MAIN_ONLY.md, 12_AUDIT_PACKAGE_SPEC.md, 13_RELEASE_ACCEPTANCE.md, 14_PRIMARY_REFERENCES.md, 15_GATE_CHECKLISTS.md
- scripts/: package_audit.py, selftest.py, validate_audit.py, validate_control.py, validate_task_graph.py
- schemas/: audit_summary.schema.json, cpu_profile.schema.json, data_artifact.schema.json, module_descriptor.schema.json, pipeline_ir.schema.json, task_result.schema.json
- templates/: ALG_CONTRACT.md, CHANGE_REVIEW.md, LARGE_ARTIFACT_MANIFEST.csv, MODULE_README.md, OWNER_REVIEW.md, RESOURCE_SUMMARY.csv, SCI_CONTRACT.md, TASK_REPORT.md, TEST_SUMMARY.csv
- 包内 SHA256SUMS 核对: {"file": "SHA256SUMS", "ok": 44, "mismatch": 0, "missing": 0, "mismatch_list": []}
- MANIFEST.json: {"schema": "astrocs.control-manifest/v1", "control_version": "V6.0", "target_version": "0.10.0-alpha.1", "baseline_commit": "587fe0e341a780da726917f40ed77f610de0c73f", "engineering_constraints_sha256": "dc47fbbbeccb239fc1834a985b357d6ded50080977708b6ea5c9811bf4de4b92", "generated_utc": "2026-08-30T15:06:37Z", "files": "[44 项] [{'path': '00_READ_FIRST.md', 'size': 4736, 'sha256': 'ef821216c25c6e7a6f16b2648b449ab1962d25b77cf3097f32ae7f119376d99c'}, {'path': '01_ASTROCS_ENGINE"}
- START_PROMPT 开头: 校验控制包，严格按00入口和任务DAG连续重构main；一任务一提交推送，未过硬门不得称完成。 ⏎ 
- 00_READ_FIRST 标题: # AstroCS V6 系统重构控制包：执行入口 ; ## 1. 任务目标 ; ## 2. 文档优先级 ; ## 3. 执行前必须完成 ; ## 4. 连续执行规则 ; ## 5. 修改纪律 ; ## 6. 绝对禁止项 ; ## 7. 完成定义

## 【zip】工程控制/_control_packs/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830.zip
- 文件 46 | 196.1 KB
- sha256: fdad8e40123a757cfcdd51429261ca2e9c915f640e69adba8eb449ee2850f689
- git 事件: {'first': None, 'last': None, 'dels': None, 'adds_n': 0, 'mod_n': None, 'touch_last': None}
- zip 顶层: AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830
- zip 内 marker: 00_READ_FIRST.md, MANIFEST.json, START_PROMPT.txt
- zip 内规格文件: 00_READ_FIRST.md, 01_ASTROCS_ENGINEERING_CONSTRAINTS.md, 02_BASELINE_AUDIT.md, 03_TARGET_ARCHITECTURE.md, 04_MIGRATION_AND_GATES.md, 05_TASK_LEDGER.csv, 06_TASK_SPECIFICATIONS.md, 07_SCIENCE_AND_TEST_MATRIX.md, 08_CPU_PARALLEL_BACKEND.md, 09_DOCUMENTATION_AND_MACHINE_CHECKS.md, 10_LINUX_WINDOWS_EXECUTION.md, 11_GIT_MAIN_ONLY.md, 12_AUDIT_PACKAGE_SPEC.md, 13_RELEASE_ACCEPTANCE.md, 14_PRIMARY_REFERENCES.md, 15_GATE_CHECKLISTS.md, 16_ACCEPTANCE_MATRIX.csv, 17_RISK_REGISTER.csv, 18_CURRENT_TO_TARGET_MAPPING.csv
- zip 内台账: AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/05_TASK_LEDGER.csv 表头 ['task_id', 'gate', 'title', 'depends_on', 'platform', 'change_class', 'heavy_compute', 'commit_required', 'scope', 'acceptance', 'status'] 行 88
- 00_READ_FIRST 开头:

# AstroCS V6 系统重构控制包：执行入口

控制包版本：`V6.0`  
目标软件版本：`0.10.0-alpha.1`  
冻结源码基线：`main@587fe0e341a780da726917f40ed77f610de0c73f`  
生成日期：`2026-08-30`  
工作分支：仅 `main`

## 1. 任务目标

在不改变已冻结科学语义的前提下，把当前由多套编排器、临时 CLI 路径和旧工具拼接的工程，迁移为：

`统一 CLI → Pipeline Runtime → 类型化数据管道 → 模块注册表 → 科学模块 → 纯 CPU 后端 → I/O/Artifact Store`

本轮不是增加功能，也不是再做历史版本全量对比。本轮必须建立以后可长期迭代的工程基座，使 SCI、ALG、DATA、ARCH、API、MOD、源码符号、TEST 和现场证据形成机器可检查的闭环。

## 2. 文档优先级

发生冲突时严格按下列顺序处理，不得自行折中：

1. `01_ASTROCS_ENGINEERING_CONSTRAINTS.md`：最高工程约束，禁止 Agent 修改或重新解释。
2. 本控制包的任务台账、合同和门禁。
3. 当前仓库已冻结的有效 SCI/ALG/DATA 合同。
4. 当前源码只作为“现状证据”，不因代码已经存在就自动成为正确设计。
5. 历史报告、HANDOVER、自报 PASS 只作线索，不作为验收证据。

若 SCI/ALG 文档互相冲突，标记 `SCIENCE_CONFLICT`，完成不依赖该冲突的任务；仅在冲突会导致不同科学结果时请求负责人决定。

## 3. 执行前必须完成

- 验证控制包：`python3 scripts/validate_control.py .`
- 读取 `01`、`02`、`03`、`04`、`05_TASK_LEDGER.csv` 和当前任务对应合同。
- `git fetch origin --prune`，确认 `HEAD == main == origin/main`；否则停止代码修改并报告。
- 禁止创建分支；禁止 force push、reset --hard、覆盖外部修改。
- 将任务台账复制为仓库内 `evidence/refactor/TASK_LEDGER.csv`，状态只能按脚本规则迁移。
- 冻结起点清单、源码 commit、工具链、机器与外部数据引用；不把真实数据复制进仓库。

## 4. 连续执行规则

检查点用于自动判定和生成证据，不是人工停工点。除下列真实阻塞外，Agent 必须连续推进所有可做任务：

- 权限、凭据或必需数据缺失；
- main 与 origin/main 无法安全对齐；
- 科学定义存在会改变数值结果的冲突；
- 必须执

