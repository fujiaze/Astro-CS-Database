# 包取证摘要：AstroCS_MAIN_PRERELEASE_REAUDIT_CONTROL_V3_20260827

身份由脚本归一（去复原前缀与 .zip 后缀）。共 1 个实例。

## 【zip】工程控制/_control_packs/AstroCS_MAIN_PRERELEASE_REAUDIT_CONTROL_V3_20260827.zip
- 文件 21 | 50.0 KB
- sha256: fcd96d697a58e13c16bc00045185a94ee9b3ee76b045de744100e3c8fa6e50e2
- git 事件: {'first': None, 'last': None, 'dels': None, 'adds_n': 0, 'mod_n': None, 'touch_last': None}
- zip 顶层: 00_READ_FIRST.md, 01_WORKFLOW_AND_GATES.md, 02_TASK_LEDGER.csv, 03_TASK_SPECIFICATIONS.md, 04_CHECKPOINT_CHECKLISTS.md, 05_NUMERICAL_AND_PERFORMANCE_GATES.md
- zip 内 marker: 00_READ_FIRST.md
- zip 内规格文件: 00_READ_FIRST.md, 01_WORKFLOW_AND_GATES.md, 02_TASK_LEDGER.csv, 03_TASK_SPECIFICATIONS.md, 04_CHECKPOINT_CHECKLISTS.md, 05_NUMERICAL_AND_PERFORMANCE_GATES.md, 06_GIT_MAIN_ONLY.md, 07_AUDIT_PACKAGE_SPEC.md, 08_V2_INHERITED_FINDINGS.md
- zip 内台账: 02_TASK_LEDGER.csv 表头 ['task_id', 'gate', 'depends_on', 'scope', 'required_commit', 'required_push', 'checkpoint', 'status'] 行 62
- 00_READ_FIRST 开头:

# AstroCS MAIN 预发布重审控制 V3

## 唯一目标

把 AstroCS `main` 收敛到可预发布状态，使：

`SCI 科学定义 -> ALG 算法推导 -> ARCH/API 软件合同 -> SRC 实现 -> TEST/运行证据`

逐项可追溯且相互一致，并消除生产重计算单线程、接缝回归和虚假质量门禁。

## 不可变规则

1. 只在 `main` 开发；禁止创建开发分支、PR 分支、审计分支或 prerelease 分支。
2. 当前任务开始时执行 `git fetch origin`，以当时 `origin/main` 为唯一候选起点并记录 SHA；不得假设仍是 `535e738`。
3. 历史锚只允许 `git archive <sha>` 导出到仓库外；禁止在历史锚上提交，禁止为历史锚创建分支。
4. 每个 Task 恰好一个原子 commit；验证通过后立即 push `main`。push 失败必须停止。
5. 禁止 force-push、reset --hard、改写历史、删除用户改动。
6. Agent 无权改变任务、阈值、状态词、豁免条件和执行顺序。
7. 唯一状态词：`NOT_STARTED / IN_PROGRESS / PASS / FAIL / BLOCKED`。
8. `BLOCKED` 必须给出外部阻塞证据；代码缺陷、测试失败、性能不达标只能记 `FAIL`。
9. 任何生产重计算阶段持续超过 1 秒时不得单线程；累计串行计算时间不得超过总计算时间 1%。
10. 并行门禁未通过前，禁止启动 32R 全量、历史 A/B/C 全量或其他超过 60 秒的科学运行。
11. 所有命令有 timeout，输出日志；禁止后台放任运行、轮询到无限期、反复重跑完整基准。
12. Agent 只能报告证据，最终 PASS/REJECT 由外部审核人裁决。

## 执行入口

1. 读取本文件。
2. 运行 `python3 scripts/validate_control.py .`；非零立即停止。
3. 逐行执行 `02_TASK_LEDGER.csv`，不得跳号。
4. 每个 Task 按 `03_TASK_SPECIFICATIONS.md` 执行并更新模板台账。
5. 到达 Checkpoint 时按 `04_CHECKPOINT_CHECKLISTS.md` 打包；等待外部审核，不得自行跨关。

## 当前证据起点

V2 审核时 main 为 `535e73879662346ee1f599d7a9cae96c6c23680d`，但该 SHA 只是历史证据起点。V3 实际起点必须在任务开始时重新冻结。

历史接缝锚：

- A：`b38b446e6`（辅助历史对照）；
- B：`83471979a`（预定

