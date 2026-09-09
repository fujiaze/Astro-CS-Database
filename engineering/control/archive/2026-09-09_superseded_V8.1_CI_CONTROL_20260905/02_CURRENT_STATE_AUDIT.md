# 02｜当前状态审计（只作核对输入）

## 已知审核快照

- 审核包：`AstroCS_ALPHA0.11.0_REVIEW_20260905T060635Z_257ae1f4efba.zip`。
- 快照 SHA：`257ae1f4efbaf5822ea6d7c42882e15f2c416b36`。
- 快照产品版本：`0.11.0-alpha.1`。
- 快照声称：190 个任务，51 CLOSED，137 NOT_STARTED，`CPU-006` DISPATCHED，`WIN-002` WAITING_RESOURCE，46 个提交。
- V7.1 静态台账为 191 项，审核快照缺 `CLI-002`；这一差异必须对账。

## 权威顺序

审核包不是当前工作区替代品。执行时事实优先级为：

1. 服务器现有 Git 工作区的当前 `main`、工作树和可达提交；
2. 当前仓库内可复现的机器测试与证据；
3. 当前活动治理/科学/算法/接口文档；
4. 本包保存的审核快照和 V7.1 只读基线。

不得把工作区退回审核 SHA，也不得用审核包覆盖当前代码。

## 已知问题线索

- 审核时 `VERSION` 为 `0.11.0-alpha.1`，CMake `project()` 仍为 `0.10.0`。
- README 曾引用旧基线 `6affe300…`。
- 模块清单摘要曾显示 0 个模块。
- `.github/workflows` 当时尚未形成正式 CI。
- Phase2 可能存在多个 IR 节点绑定同一个全量 `session_run`、伪模块化和重复计算。
- Phase2 多处 heavy 循环仍可能串行；`P2_ENABLE_OPENMP` 曾默认关闭。
- worker 数曾硬编码为 2；benchmark/profile 与真实 provider 路由可能没有闭环。
- 资源监控曾依赖外部声明的 active/runnable/queue，而非真实线程与工作单元采样。

这些都是待现场确认的 finding，不允许直接宣称当前仍存在，也不允许忽略。

## 原地接管规则

1. 记录现有根目录、HEAD、main、origin/main、remote（脱敏）、status、submodule 和现有工作树列表。
2. 保存已有修改的文件清单、diff 统计和来源说明；只在其归属明确后继续写任务。
3. `git fetch origin --prune` 后分类：同步、仅本地领先、仅远端领先、分叉。
4. 工作树干净且仅远端领先时才允许 `git pull --ff-only`；本地领先则完成验证后正常 push；分叉或已有修改与远端冲突时精确报告，禁止自动 rebase/reset。
5. 当前仓库已有 `TASK_STATE`/证据优先；再与审核快照和 V7.1 台账按 task_id 外连接。
6. CLOSED 任务只做证据存在性和当前 SHA 快检，不跑历史全量计算。
7. `CPU-006` 等旧“运行中”状态只有在当前工作区存在可验证修改时才继承，否则回到 READY，不等待旧进程。

## 必须先生成

- `WORKSPACE_IDENTITY.json`
- `PREEXISTING_CHANGES.md`
- `REMOTE_RELATION.json`
- `STATE_RECONCILIATION.csv`
- `TASK_STATE.json`

这些文件是接管证据，不授权清理或重建工作区。
