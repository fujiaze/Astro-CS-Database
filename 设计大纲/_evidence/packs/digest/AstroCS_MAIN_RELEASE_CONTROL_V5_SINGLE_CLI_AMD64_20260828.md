# 包取证摘要：AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828

身份由脚本归一（去复原前缀与 .zip 后缀）。共 2 个实例。

## 【recovered_from_history】history/f99e80d8/工程控制/RELEASE_V5/AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828
- 文件 38 | 98.1 KB
- git 事件: {'first': ('3ca7f974b039f87bd6a37c2a0023539e265918fc', '2026-08-30T17:07:02+08:00'), 'last': ('f99e80d8574dd19b305312c0e66006cc01584a41', '2026-08-28T20:58:26+08:00'), 'dels': [], 'adds_n': 6, 'mod_n': 210, 'touch_last': None}
- 台账 02_TASK_LEDGER.csv: 行 98 状态列 status 分布 {"PASS": 1, "NOT_STARTED": 97} 任务号样例 BASE-001, GOV-001, VER-001, TRACE-001, DOC-001, SCI-001, SCI-002, SCI-003, SCI-004, SCI-005, SCI-006, SCI-007, ALG-001, ALG-002, ALG-003, ALG-004
- 规格文件: 00_READ_FIRST.md, 01_PRODUCT_ARCHITECTURE.md, 03_TASK_DETAILS.md, 04_CLI_COMMAND_AND_PROTOCOL_CONTRACT.md, 05_CPU_BACKEND_ABI_AND_PACKAGING.md, 06_BENCHMARK_AND_PROFILE_SPEC.md, 07_RESOURCE_MONITOR_AND_UTILIZATION_GATE.md, 08_SCIENCE_SYNTHETIC_AND_EXTERNAL_REVIEW.md, 09_LINUX_WINDOWS_BUILD_RELEASE.md, 10_GIT_REVIEW_CAPSULE_AUDIT_PACKAGE.md, 11_AGENTS_MD_REQUIRED_BLOCK.md, 12_V3_V4_MIGRATION.md, 13_ALPHA_VERSION_AND_PHASE3.md, 14_V4_COVERAGE_MATRIX.md, 15_CONTINUOUS_CHECKPOINTS.md
- scripts/: package_final.py, validate_control.py, validate_final_package.py
- schemas/: cli_event.schema.json, cpu_profile.schema.json
- templates/: BUILD_RESULTS.csv, CHECKPOINTS.csv, COMMITS.csv, CPU_PROFILE_RESULTS.csv, FINDINGS.csv, LARGE_ARTIFACTS.csv, RELEASE_ARTIFACTS.csv, RESOURCE_RESULTS.csv, REVIEW_CAPSULE_INDEX.csv, SCIENCE_CLAIMS.csv, SUMMARY.json, TEST_RESULTS.csv, TRACEABILITY.csv
- 包内 SHA256SUMS 核对: {"file": "SHA256SUMS", "ok": 35, "mismatch": 1, "missing": 0, "mismatch_list": ["02_TASK_LEDGER.csv"]}
- START_PROMPT 开头: 在当前仓库外解压 V5 控制包，先运行 scripts/validate_control.py，再严格执行 00_READ_FIRST.md 与 02_TASK_LEDGER.csv。只在 main 原子 commit 并立即 push；不得创建分支、不得反复运行历史版本、不得接入 ACR/GPU。实现 Windows/Linux 单一 astrocs CLI、Phase1/2/3 内部调用、amd64 私有 CPU backend、逐内核 benchmark/profile 和强制资源门禁；无 profile 走 baseline 但仍按 affinity 多线程。Linux 连续完成静态/文档/合成/构建，Fatduck 在线后完成 Windows 正式 benchmark、合成、小真实数据及当前候选唯一一次 32R。除最终发布审核外不得停等外部批准；低利用率、内存异常增长、未验证或报告矛盾一律不得 PASS。 ⏎ 
- 00_READ_FIRST 标题: # AstroCS V5 预发布控制包：单一 CLI / amd64 CPU 自适应 ; ## 0. 本包的唯一目标 ; ## 1. 不可解释、不可放宽的硬约束 ; ## 2. 首次启动后 30 分钟内 ; ## 3. 连续执行状态机 ; ## 4. 唯一允许停止的条件 ; ## 5. 最终 PASS 含义

## 【worktree】engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/RELEASE_V5/AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828
- 文件 37 | 97.1 KB
- 父包目录: engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1
- git 事件: {'first': ('b7b2dea70dbcdacdcf6eb762609a908abdeab697', '2026-09-02T22:15:41+08:00'), 'last': ('b7b2dea70dbcdacdcf6eb762609a908abdeab697', '2026-09-02T22:15:41+08:00'), 'dels': [], 'adds_n': 0, 'mod_n': 40, 'touch_last': None}
- 台账 02_TASK_LEDGER.csv: 行 98 状态列 status 分布 {"PASS": 88, "BLOCKED": 1, "REVIEW_PENDING": 1, "IN_PROGRESS": 1, "NOT_STARTED": 7} 任务号样例 BASE-001, GOV-001, VER-001, TRACE-001, DOC-001, SCI-001, SCI-002, SCI-003, SCI-004, SCI-005, SCI-006, SCI-007, ALG-001, ALG-002, ALG-003, ALG-004
- 规格文件: 00_READ_FIRST.md, 01_PRODUCT_ARCHITECTURE.md, 03_TASK_DETAILS.md, 04_CLI_COMMAND_AND_PROTOCOL_CONTRACT.md, 05_CPU_BACKEND_ABI_AND_PACKAGING.md, 06_BENCHMARK_AND_PROFILE_SPEC.md, 07_RESOURCE_MONITOR_AND_UTILIZATION_GATE.md, 08_SCIENCE_SYNTHETIC_AND_EXTERNAL_REVIEW.md, 09_LINUX_WINDOWS_BUILD_RELEASE.md, 10_GIT_REVIEW_CAPSULE_AUDIT_PACKAGE.md, 11_AGENTS_MD_REQUIRED_BLOCK.md, 12_V3_V4_MIGRATION.md, 13_ALPHA_VERSION_AND_PHASE3.md, 14_V4_COVERAGE_MATRIX.md, 15_CONTINUOUS_CHECKPOINTS.md
- scripts/: package_final.py, validate_control.py, validate_final_package.py
- schemas/: cli_event.schema.json, cpu_profile.schema.json
- templates/: BUILD_RESULTS.csv, CHECKPOINTS.csv, COMMITS.csv, CPU_PROFILE_RESULTS.csv, FINDINGS.csv, LARGE_ARTIFACTS.csv, RELEASE_ARTIFACTS.csv, RESOURCE_RESULTS.csv, REVIEW_CAPSULE_INDEX.csv, SCIENCE_CLAIMS.csv, SUMMARY.json, TEST_RESULTS.csv, TRACEABILITY.csv
- 包内 SHA256SUMS 核对: {"file": "SHA256SUMS", "ok": 35, "mismatch": 1, "missing": 0, "mismatch_list": ["02_TASK_LEDGER.csv"]}
- START_PROMPT 开头: 在当前仓库外解压 V5 控制包，先运行 scripts/validate_control.py，再严格执行 00_READ_FIRST.md 与 02_TASK_LEDGER.csv。只在 main 原子 commit 并立即 push；不得创建分支、不得反复运行历史版本、不得接入 ACR/GPU。实现 Windows/Linux 单一 astrocs CLI、Phase1/2/3 内部调用、amd64 私有 CPU backend、逐内核 benchmark/profile 和强制资源门禁；无 profile 走 baseline 但仍按 affinity 多线程。Linux 连续完成静态/文档/合成/构建，Fatduck 在线后完成 Windows 正式 benchmark、合成、小真实数据及当前候选唯一一次 32R。除最终发布审核外不得停等外部批准；低利用率、内存异常增长、未验证或报告矛盾一律不得 PASS。 ⏎ 
- 00_READ_FIRST 标题: # AstroCS V5 预发布控制包：单一 CLI / amd64 CPU 自适应 ; ## 0. 本包的唯一目标 ; ## 1. 不可解释、不可放宽的硬约束 ; ## 2. 首次启动后 30 分钟内 ; ## 3. 连续执行状态机 ; ## 4. 唯一允许停止的条件 ; ## 5. 最终 PASS 含义
