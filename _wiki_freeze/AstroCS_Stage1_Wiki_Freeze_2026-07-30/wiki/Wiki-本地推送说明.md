# Wiki 本地推送说明

## 1. 是否可以由本地 Agent 直接推送

可以，前提是：

- 仓库已经启用 Wiki；
- GitHub 网页上已经创建至少一个初始 Wiki 页面；
- 本地 Agent 使用的 Git 凭据对主仓库具有写权限；
- 当前 GitHub 计划支持该仓库的 Wiki；
- 网络和远端地址可访问。

GitHub Wiki 是独立 Git 仓库，地址通常为：

```text
https://github.com/OWNER/REPOSITORY.wiki.git
```

或 SSH：

```text
git@github.com:OWNER/REPOSITORY.wiki.git
```

## 2. 推荐流程

```bash
git clone https://github.com/OWNER/REPOSITORY.wiki.git AstroCS.wiki
cd AstroCS.wiki

# 将本包 wiki/ 下的 Markdown 页面复制到此目录

git status
git diff
git add .
git commit -m "docs(wiki): freeze AstroCS Stage1 specification"
git pull --rebase
git push
```

## 3. 安全规则

- 推送前先 `git pull --rebase`；
- 不删除未知的现有 Wiki 页面；
- 更新 `_Sidebar.md` 前检查原内容；
- Agent 只提交文档，不把源码或大数据复制进 Wiki；
- 已冻结页面发生变更时，commit message 必须说明改变了哪项用户决策；
- Agent 不得自行把 `WAITING_FOR_USER_REVIEW` 改为 `USER_APPROVED`。

## 4. 首次初始化

如果 `.wiki.git` 尚不存在：

1. 打开主仓库的 Wiki 页面；
2. 创建 Home 页面；
3. 保存；
4. 再执行本地 clone。

## 5. 自动脚本

本交付包 `tools/Push-Wiki.ps1` 提供受控推送脚本。

默认只准备和提交，不推送；显式传入 `-Push` 才会执行远程 push。
