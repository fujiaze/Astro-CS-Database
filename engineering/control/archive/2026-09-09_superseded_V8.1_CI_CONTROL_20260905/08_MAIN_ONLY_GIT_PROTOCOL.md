# 08｜现有工作区 main 提交流程

## 允许

- 只使用 `git rev-parse --show-toplevel` 找到的现有仓库。
- 只在当前 `main` 开发并 push `origin/main`。
- 一个任务一个提交；失败修复使用同 task_id 的后续提交。
- 每个提交以完整 40 位 SHA 关联 CI 和证据。
- 工作树干净且仅落后远端时执行 `git pull --ff-only`。

## 禁止

- `git clone`、`git init` 或复制出第二个开发仓库。
- `git branch` 创建任务分支、`git switch -c`、`git checkout -b`。
- `git worktree add`。
- 移动/重命名现有仓库根目录或重新规划服务器目录。
- 自动 `git stash`、`git clean`、`git reset`、merge、rebase。
- `commit --amend`、force push、filter-repo、BFG 和历史重写。

## 既有修改

初始 dirty 状态不是自动失败。必须先将每个文件归入：当前任务成果、用户修改、生成证据或来源未知。来源未知只阻止与其重叠的写任务，不阻止其他只读检查和不相交任务。不得为了得到 clean status 清除内容。

## 提交门禁

1. 当前分支为 main；记录 HEAD 与 origin/main 的关系。
2. staged 文件只在任务 allowlist，不含预存无关修改、大文件、build、testdata 或历史包。
3. 任务验收和 `ci-fast` 通过。
4. 提交消息只含一个任务 ID。
5. push 后同 SHA 的 `ci-main` 通过。

若远端前进且本地无提交/修改，允许 `pull --ff-only`；否则精确报告分叉和重叠文件，不自动处理历史。
