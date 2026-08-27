# Git：仅 main、原子提交、立即 push

## 每个 Task 的固定流程

1. `git fetch origin --prune`。
2. 验证当前分支 `main` 且 `HEAD==origin/main`。
3. 验证没有来源不明修改。
4. 只修改当前 Task 授权文件。
5. 运行该 Task 的指定测试。
6. `git diff --check`；检查 staged 文件清单。
7. commit message：`<task_id>: <单一目的中文摘要>`。
8. 验证 commit 只含当前 Task。
9. `git push origin main`。
10. 验证 `HEAD==origin/main`，写入 `COMMITS.csv`。

## 禁止

- 创建任何开发/审计/prerelease 分支；
- force push、rebase 已推送 main、reset --hard、checkout 覆盖用户修改；
- 多 Task 合并为一个 commit；
- 测试失败仍 commit/push；
- 在 Fatduck 直接形成未同步的修复；
- 把 testdata、大产物、审核包、历史 archive 提交进仓库。

## 历史锚

仅允许：`git archive --format=tar <sha>` 导出到仓库外的临时目录。不得创建历史分支或在锚上开发。
