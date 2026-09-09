# 继承基线说明

- `V7_1_STATIC_TASK_LEDGER.csv`：上一轮 191 项静态任务定义。
- `REVIEW_CLAIMED_TASK_STATE.csv`：审核包声称的中间状态，必须经 V81-ADOPT-003 与现有工作区对账后才能采用。
- `AstroCS_ALPHA3_MODULAR_REFOUNDATION_CONTROL_V7_1_20260902_FINAL3.zip`：上一轮完整 task spec 的只读归档，SHA256 为 `005448fb3b6892d3063d23149692fbd9226a892a2c1891e4cb8d14a80e8491c3`。

V7.1 只用于读取尚未完成任务的科学/功能目标和验收内容。以下旧执行规则全部作废，以 V8.1 为准：detached worktree、任何新增 git worktree、额外开发副本、Linux 2c2g 固定并发、Fatduck 现场编译、旧 WIN task 编排、旧人工 checkpoint。

前台恢复任务时必须将 V7.1 的 goal/acceptance 与 V8 的单一 main、GitHub 托管 CI、Fatduck 二进制验证合同合并成一条 `DISPATCH.json`；不得把旧包整份加载给执行子 Agent。
