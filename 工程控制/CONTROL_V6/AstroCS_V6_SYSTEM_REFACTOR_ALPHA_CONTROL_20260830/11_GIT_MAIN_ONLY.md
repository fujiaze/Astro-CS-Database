# 仅 main 的原子提交与 Push 规范

## 1. 硬规则

- 只在 `main` 工作；不创建功能分支、临时分支或 worktree 分支。
- 每个 Ledger Task 一个可独立验证的 commit；通过后立即 push。
- 禁止 force push、历史重写、`reset --hard`、未经授权删除 tag/remote branch。
- 每个任务开始和结束都验证 `HEAD == main == origin/main`。
- 发现其他 Agent/用户修改时先登记；不得覆盖或“清理”。

## 2. Commit 类别不可混合

以下必须分开：

- `science:` SCI/ALG 定义变化；
- `architecture:` Runtime/接口/模块迁移；
- `fix:` 已确认 bug；
- `perf:` 有前置热点证据的性能修改；
- `test:` Oracle/fixture/checker；
- `docs:` 不改变合同语义的文档；
- `build:` CMake/toolchain/dependency；
- `release:` 产物/manifest/status。

任务标题已经规定 change_class；不得把别的类别塞入。

## 3. 提交信息

```text
<TASK-ID>: <动词+单一目的>

Change: <改了什么>
Science: unchanged | <SCI-ID change>
Contracts: <IDs>
Tests: <commands/report IDs>
Evidence: evidence/refactor/tasks/<TASK-ID>/TASK_RESULT.json
```

示例：`IO-002: 统一 AIO image RAII 释放路径`。

## 4. 每次 Push 前

- task-specific tests PASS；
- impact_analysis 列出的 tests PASS；
- contract/API checker PASS（若相关）；
- 只 stage 本任务文件；检查 `git diff --cached --stat` 和 diff；
- 没有 build/data/cache/credentials；
- Task result 写 start/end 预期；commit 后补 commit hash 时使用下一证据 commit会打破一任务一 commit，因此证据应记录 source tree hash/由 CI 绑定 commit，或使用 git notes/外部 evidence index，不做 amend/force。

推荐：任务证据主体随同代码 commit；push 后由不可变 CI/ledger index 记录实际 commit。禁止为了回填 hash 无限 amend。

## 5. 并发 Agent 协作

本控制包允许执行环境内部并行调查，但对 `main` 的写入必须串行化：

- 任务只有依赖通过后可进入 IN_PROGRESS；
- 同一文件不得由多个任务并行修改；
- 一个确定性 Git 管理者负责 stage/commit/push；
- worker 不直接 force/rebase/push；
- push 冲突时 fetch 并评估，不自动覆盖远端。

## 6. 错误提交

已 push 错误只用新的 `revert <TASK-ID>` 原子 commit 恢复；保留失败证据。随后新任务/同任务重试用新 commit 修复。任何回退都不能掩盖曾经失败的现场记录。
