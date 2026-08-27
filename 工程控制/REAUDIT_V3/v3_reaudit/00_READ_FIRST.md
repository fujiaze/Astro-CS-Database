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
- B：`83471979a`（预定接缝基线，必须完成运行后才可成为数值 oracle）；
- C：V3 起点 `origin/main`；
- D：V3 完成候选 `origin/main`。

## 立即停止条件

- 不在 `main`；
- `HEAD != main != origin/main`；
- 存在来源不明的 tracked 修改；
- 任务前一 Checkpoint 未获外部 PASS；
- 生产计算运行时 `max_threads < 2`；
- 2 核 Linux CPU 密集探针平均 CPU < 150%；
- 单任务修改跨越未授权模块；
- 测试失败却准备 commit/push；
- 审核包超过 25 MiB 或含禁入文件；
- 报告之间的计数、SHA、状态不一致。
