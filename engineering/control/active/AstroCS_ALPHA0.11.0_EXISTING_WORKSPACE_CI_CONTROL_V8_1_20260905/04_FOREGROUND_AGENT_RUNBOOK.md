# 04｜前台 Agent 运行手册

## 前台职责

前台只负责：定位现有工作区、读取机器台账、派发固定任务、管理单写租约、机器验收、原子 commit/push、更新证据和打包。实现与长任务交给固定子 Agent。

## 每轮启动

```bash
ASTROCS_ROOT="$(git rev-parse --show-toplevel)"
cd "$ASTROCS_ROOT"
test "$(git branch --show-current)" = main
git status --porcelain=v2
git remote -v
git fetch origin --prune
git rev-list --left-right --count HEAD...origin/main
git worktree list --porcelain
```

要求：

- remote 输出进入证据前删除 URL 中凭据。
- 记录现有 worktree 列表，但不得删除历史 worktree；本轮禁止新增。
- 不要求工作树一开始为空。所有既有改动写入 `PREEXISTING_CHANGES.md`，标注来源、任务和处置。
- 工作树干净且只落后远端时才执行 `git pull --ff-only`。
- 本地领先可以继续验收并正常 push。
- 发生分叉或既有改动与远端冲突时，返回具体 SHA/文件；禁止自动 rebase/reset/stash。

然后从控制包目录运行：

```bash
python3 validators/next_tasks.py --ledger CONTROL_TASK_LEDGER.csv --state "${ASTROCS_ROOT}/<existing-evidence-dir>/TASK_STATE.json"
```

`<existing-evidence-dir>` 必须通过项目当前 `AGENTS.md`、`memory.md` 或文档索引确定；不得假设服务器绝对路径。

## 派发格式

派发必须包含：task_id、固定 owner、目标、允许路径、禁止路径、输入、输出、验收命令、超时和 heavy 标志。使用 `templates/DISPATCH.json`。子 Agent 不得改变范围、容差、API 或科学定义；遇到冲突返回文件、符号和合同 ID。

## 单写租约

租约写入当前项目证据目录的 `WRITE_LEASE.json`，仅含 `task_id/owner/base_sha/acquired_utc`。它是机器互斥，不是人工 checkpoint。

- 只读审查和不写 tracked 文件的测试可并行。
- 任一时刻只允许一个 tracked 文件写任务。
- 前台检查实际 diff 严格属于任务 allowlist 后再验收。
- 预先存在的修改不得混入新任务提交；若属于同一任务，先在证据中建立归属。

## 原子提交

```bash
git diff --check
python3 ci/run.py --profile fast --changed-from HEAD
git add -- <任务允许路径>
git diff --cached --check
git commit -m "<type>(<module>): <单一目的> [<TASK-ID>]"
git push origin main
```

随后验证 GitHub `linux-ci` 和 `windows-ci` 对 pushed SHA 的状态。失败时在同一 task_id 下追加最小修复提交，不 amend，不开始无关写任务。

## 自动继续

- Fatduck 离线：记录 `FATDUCK_PENDING`，继续所有开发和托管 CI。
- 旧证据缺失：能在当前 SHA 快速重建就重建。
- 可选工具缺失：记录并继续不依赖项；安装只遵循现有项目环境，不重新配置服务器。
- 检查失败：生成具体 finding 并派发修复，不停在人工审核点。

只有缺少必要权限/凭据、远端分叉无法安全快进或最终发布裁定才询问用户。

## Token约束

只读当前任务规范和关联合同；机器输出 JSON/CSV；成功只汇总命令、退出码、耗时、SHA 和关键指标。禁止重复复述全局规则或为未复现问题设计大量兜底。
