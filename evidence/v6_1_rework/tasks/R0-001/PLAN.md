# R0-001 PLAN — 冻结当前 main 身份

## 需求
- 记录 remote（脱敏）、分支列表、HEAD/main/origin/main、最近 100 提交摘要、submodule、工作区状态。
- `git merge-base --is-ancestor b16d422... HEAD` 必须为真。
- 三 SHA 相同，仅 main；工作区变化全部可解释。
- 产物：`SOURCE_IDENTITY.json`、`WORKTREE_CLASSIFICATION.md`。

## 现状证据（起点）
- HEAD == main == origin/main == `30a3516ae8a3a1736bf8a470de56909997b1a219`
- b16d422 是祖先（ancestor check exit=0）
- 仅 main 分支；remote 无 token
- 205 项 dirty/untracked，全部来源已知（见 WORKTREE_CLASSIFICATION.md）

## 影响文件
- evidence/v6_1_rework/tasks/R0-001/SOURCE_IDENTITY.json
- evidence/v6_1_rework/tasks/R0-001/WORKTREE_CLASSIFICATION.md
- evidence/v6_1_rework/tasks/R0-001/logs/identity.log
- evidence/v6_1_rework/tasks/R0-001/PLAN.md
- evidence/v6_1_rework/tasks/R0-001/TASK_RESULT.json

## 科学影响
无（纯治理/身份任务，不动科学代码）。

## 风险
- 无。只读操作 + 证据记录。

## 验收命令
1. `git fetch origin --prune` exit=0
2. `git rev-parse HEAD main origin/main` 三值相等
3. `git merge-base --is-ancestor b16d422 HEAD` exit=0
4. `git branch -a` 仅 main
5. remote 无 token
6. 全部 dirty/untracked 有分类
