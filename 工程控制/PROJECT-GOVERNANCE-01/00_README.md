# PROJECT-GOVERNANCE-01 — 基于新设计文档集的逐层治理控制包

状态：`READY_FOR_OWNER_EXECUTION_ORDER`（已编制，未执行）
基线提交：`a861d8f63a1f6c17dea2f201349006f6a6bd1ad2`（编制时 HEAD = main = origin/main 三者一致）
上位权威：`ASTROCS_DESIGN.md`
执行规范：`AGENTS.md`、`ENGINEERING_SPEC.md`、`CONTROL_PACK_SPEC.md`、`docs/ci/`
本包目的：把新文档集与当前仓库的偏差，按「权威与基线 → 数据合同 → 架构基建 → CLI → 三条科学产品链 → CPU/观测 → 集成/CI/发行验证」逐层清零。

## 1. 本包包含什么

| 文件 | 内容 |
|---|---|
| `00_README.md` | 本文：目的、治理事实、执行硬规则、入口 |
| `TASK_LIST.md` | 26 个任务的总览表、依赖图、并行组、提交规则 |
| `tasks/*.md` | 每任务一份：目标 / 基线状态 / 权威依据 / 文件域 / 步骤 / 验收门 / 证据命令 |
| `GAP_AUDIT.md` | 差异审计：基线红灯 + GAP-001..GAP-020（含可复跑证据） |
| `ACCEPTANCE.md` | 验收台账（逐任务状态，现全部 NOT_STARTED） |
| `SUMMARY.md` | 阶段汇总（执行期间更新） |
| `DISPATCH.md` | **派发提示词**：复制粘贴给执行 Agent / 验证 Agent |
| `OPERATOR.md` | 调度员手册：派发→独立验收→原子提交→台账维护 |

## 2. 当前已确认的治理事实（编制时实测）

- **机器门是红的**：`tools/check_agents_gov.py` rc=1、`tools/doccheck/check_engineering_constraints.py` rc=1、`tools/doccheck/check_doc_index.py --strict` rc=1——三者仍以旧治理体系为判据（详见 `GAP_AUDIT.md §0`）。
- 新文档要求用户命令为 `normalize/mosaic/export`，当前 CLI 仍注册 `phase1/phase2/phase3 …`，而旧检查器 `tools/check_cli_command_layer.py` 反而以旧命令层为 PASS 判据（rc=0）。
- 新文档要求 `lib/algorithms/` 与 `lib/infrastructure/` 两个源码根，当前源码仍分散在 31 个 `lib/*` 旧目录与根 `cli/`、`runtime/`、`providers/`、`modules/`。
- 新文档要求根 `config/defaults.json` 与 `config/filters.json`，当前没有 `config/`。
- 新文档要求 `python3 ci/run_checks.py` 与 `CHK-*` 注册语义，当前入口是 `ci/run.py`，`ci/checks.json` 145 项仍为旧 ID，且 `VERSION-CONSISTENCY` 以 `--expected 0.11.0-alpha.2` 绑定旧版本号。
- 旧宪章、旧工程约束、旧活动文档与旧控制包引用仍在活动面（GAP-001/GAP-002/GAP-019），只能在任务内收敛。
- 工作区在编制前已有大量预存修改与未跟踪文件；执行者不得 reset/stash/clean，也不得混入任务提交——先做 BASE-001。

## 3. 执行硬规则

1. 开工前按 `AGENTS.md §1` 阅读全部相关权威；未读不开工。
2. SubAgent 零 git 写权限；前台（调度员）独立验收后按任务原子提交，一个任务一个 commit。
3. 每任务只改该任务声明的文件域；根 `CMakeLists.txt`、中央 registry、`ci/checks.json` 只能由 INT-001 / CI-001 / QA-001 串行修改。
4. 科学公式、默认容差、冻结 SCI/ALG 只读；发现冲突登记 `BLOCKED` 并请求负责人。
5. 迁移先证明等价（bitwise 或既有冻结容差），不得为了迁移目录同时改科学语义。
6. 不得执行破坏性 Git；不得覆盖本包创建前的预存工作区改动。
7. 状态只用 `NOT_STARTED / IN_PROGRESS / PASS / FAIL / BLOCKED`；`PASS` 仅由前台独立复跑后写入。
8. 未完成 Linux 真实数据、Windows 复验和图像审核，不得宣称控制包完成；任何发布决定只属负责人。

## 4. 入口

1. 先读 `TASK_LIST.md` 的依赖图与并行组；
2. 用 `DISPATCH.md §1` 的通用提示词从 `BASE-001` 开始派发；
3. 验收与提交按 `OPERATOR.md` 执行；
4. 每完成一个任务：更新 `ACCEPTANCE.md` → 解锁下游 → 继续派发。

> 本包只定义任务，不代表任何任务已经开始、通过或完成。负责人向其他 Agent 明确发送「执行本控制包 / 执行 <任务ID>」后方可执行。
