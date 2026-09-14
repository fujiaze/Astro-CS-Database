# 包取证摘要：AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905

身份由脚本归一（去复原前缀与 .zip 后缀）。共 3 个实例。

## 【recovered_from_history】history/a4fdee3f/engineering/control/active/AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905
- 文件 57 | 377.7 KB
- git 事件: {'first': ('a4fdee3f446de08cda50889a223740237b2da4a0', '2026-09-05T17:51:10+08:00'), 'last': ('a4fdee3f446de08cda50889a223740237b2da4a0', '2026-09-05T17:51:10+08:00'), 'dels': [], 'adds_n': 6, 'mod_n': 0, 'touch_last': None}
- 规格文件: 00_READ_FIRST.md, 01_FROZEN_CONSTRAINTS.md, 02_CURRENT_STATE_AUDIT.md, 03_VM_AND_CI_ARCHITECTURE.md, 04_FOREGROUND_AGENT_RUNBOOK.md, 06_TASK_GRAPH_AND_GATES.md, 07_CI_MACHINE_CONTRACT.md, 08_MAIN_ONLY_GIT_PROTOCOL.md, 09_WINDOWS_AND_FATDUCK_VALIDATION.md, 10_SECURITY_AND_RUNNER.md, 11_EVIDENCE_AND_AUDIT_PACKAGE.md, 12_FAILURE_POLICY.md, 13_PRIMARY_REFERENCES.md, 14_TOKEN_EFFICIENT_EXECUTION.md, 15_SUPERSESSION_NOTICE.md
- tasks/ 共 4: 01_WORKSPACE_ADOPTION_TASKS.md, 02_CI_TASKS.md, 03_P0_REMEDIATION_TASKS.md, 04_FATDUCK_AND_RELEASE_TASKS.md
- templates/: AGENTS_VM_APPEND.md, DISPATCH.json, FATDUCK_HARNESS_CONTRACT.md, OWNER_REVIEW_ISSUE.md, fatduck-realdata.yml, linux-ci.yml, windows-ci.yml
- 包内 SHA256SUMS 核对: {"file": "SHA256SUMS", "ok": 51, "mismatch": 3, "missing": 0, "mismatch_list": ["baseline/REVIEW_CLAIMED_TASK_STATE.csv", "baseline/REVIEW_FINDINGS.csv", "baseline/V7_1_STATIC_TASK_LEDGER.csv"]}
- START_PROMPT 开头: 严格执行00_READ_FIRST.md：在现有main工作区原地接入双平台CI和Fatduck终验；禁止克隆、迁移、分支及worktree。 ⏎ 
- 00_READ_FIRST 标题: # 00｜唯一执行入口 ; ## 目标 ; ## 先读顺序 ; ## 不得违反 ; ## 包自检 ; ## 完成定义

## 【worktree】工程控制/AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905
- 文件 56 | 377.6 KB
- git 事件: {'first': None, 'last': None, 'dels': None, 'adds_n': 0, 'mod_n': None, 'touch_last': None}
- 规格文件: 00_READ_FIRST.md, 01_FROZEN_CONSTRAINTS.md, 02_CURRENT_STATE_AUDIT.md, 03_VM_AND_CI_ARCHITECTURE.md, 04_FOREGROUND_AGENT_RUNBOOK.md, 06_TASK_GRAPH_AND_GATES.md, 07_CI_MACHINE_CONTRACT.md, 08_MAIN_ONLY_GIT_PROTOCOL.md, 09_WINDOWS_AND_FATDUCK_VALIDATION.md, 10_SECURITY_AND_RUNNER.md, 11_EVIDENCE_AND_AUDIT_PACKAGE.md, 12_FAILURE_POLICY.md, 13_PRIMARY_REFERENCES.md, 14_TOKEN_EFFICIENT_EXECUTION.md, 15_SUPERSESSION_NOTICE.md
- tasks/ 共 4: 01_WORKSPACE_ADOPTION_TASKS.md, 02_CI_TASKS.md, 03_P0_REMEDIATION_TASKS.md, 04_FATDUCK_AND_RELEASE_TASKS.md
- templates/: AGENTS_VM_APPEND.md, DISPATCH.json, FATDUCK_HARNESS_CONTRACT.md, OWNER_REVIEW_ISSUE.md, fatduck-realdata.yml, linux-ci.yml, windows-ci.yml
- 包内 SHA256SUMS 核对: {"file": "SHA256SUMS", "ok": 54, "mismatch": 0, "missing": 0, "mismatch_list": []}
- START_PROMPT 开头: 严格执行00_READ_FIRST.md：在现有main工作区原地接入双平台CI和Fatduck终验；禁止克隆、迁移、分支及worktree。 ⏎ 
- 00_READ_FIRST 标题: # 00｜唯一执行入口 ; ## 目标 ; ## 先读顺序 ; ## 不得违反 ; ## 包自检 ; ## 完成定义

## 【zip】工程控制/_control_packs/AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905.zip
- 文件 56 | 377.6 KB
- sha256: 77a4b03a622c63cc3bf9622351dc2363a0f35adef46ca79aea4e749b48fd6eff
- git 事件: {'first': None, 'last': None, 'dels': None, 'adds_n': 0, 'mod_n': None, 'touch_last': None}
- zip 顶层: AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905
- zip 内 marker: 00_READ_FIRST.md, START_PROMPT.txt
- zip 内规格文件: 00_READ_FIRST.md, 01_FROZEN_CONSTRAINTS.md, 02_CURRENT_STATE_AUDIT.md, 03_VM_AND_CI_ARCHITECTURE.md, 04_FOREGROUND_AGENT_RUNBOOK.md, 05_FIXED_SUBAGENT_BINDINGS.yaml, 06_TASK_GRAPH_AND_GATES.md, 07_CI_MACHINE_CONTRACT.md, 08_MAIN_ONLY_GIT_PROTOCOL.md, 09_WINDOWS_AND_FATDUCK_VALIDATION.md, 10_SECURITY_AND_RUNNER.md, 11_EVIDENCE_AND_AUDIT_PACKAGE.md, 12_FAILURE_POLICY.md, 13_PRIMARY_REFERENCES.md, 14_TOKEN_EFFICIENT_EXECUTION.md, 15_SUPERSESSION_NOTICE.md
- 00_READ_FIRST 开头:

# 00｜唯一执行入口

## 目标

在服务器上已经存在且正在使用的 AstroCS `main` 工作区内原地继续。不得创建、克隆、移动或替换仓库。先冻结现有状态，再接入 GitHub 托管 Linux/Windows CI，修复已确认的架构、科学、文档和 CPU 并行问题；Fatduck 只用 CI 产出的 Windows 候选程序运行本地真实数据终验。

## 先读顺序

1. `01_FROZEN_CONSTRAINTS.md`
2. `02_CURRENT_STATE_AUDIT.md`
3. `03_VM_AND_CI_ARCHITECTURE.md`
4. `04_FOREGROUND_AGENT_RUNBOOK.md`
5. `CONTROL_TASK_LEDGER.csv`
6. 当前任务对应的 `tasks/*.md`
7. `GATE_REQUIREMENTS.csv`

## 不得违反

- 从当前目录运行 `git rev-parse --show-toplevel` 获取现有仓库根目录；禁止假定固定绝对路径。
- 只在现有 `main` 开发；禁止新分支、`git worktree`、额外 clone、仓库搬迁、目录重新规划和新建开发账户。
- 首先记录 `HEAD/main/origin/main`、remote、完整 `git status --porcelain=v2` 和现有任务证据。不得自动 `reset`、`stash`、`clean`、`rebase` 或删除现有文件。
- 前台 Agent 只分发、机器验收、提交、push、状态汇总和打包；固定子 Agent 执行任务。
- 只读任务可以并行；同一现有工作区的 tracked 文件写入必须串行。
- 一个任务一个原子 commit，直接 push `main`；禁止 amend、force push 和历史重写。
- 机器检查自动推进。除缺少权限/凭据、远端分叉无法快进、Fatduck 离线及最终发布裁定外，不等待用户签字。
- 不做历史版本全链路重算。数值正确性使用科学文档推导的合成 Oracle、解析解、守恒量和不变量。
- 当前生产只使用纯 CPU；ACR 保留但不可进入生产路由。
- heavy 计算必须动态并行，并采集 CPU、线程、内存、IO、进度和 worker 负载；单线程或持续低利用率直接失败。
- 不硬编码核心数、线程数或 SIMD 路径；benchmark 生成主机配置。没有配置时使用保守通用 amd64 路径。
- Phase1、Phase2、Phase3 独立调用、独立恢复、独立验收，不强制串行执行。

## 包自检

在控制包目录执行：

```bash
python3 validators/validate_control.py --

