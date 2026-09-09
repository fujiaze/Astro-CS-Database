# V8.0 作废声明

`AstroCS_ALPHA0.11.0_CI_VM_MIGRATION_CONTROL_V8_20260905` 不得执行。

V8.0 中以下内容全部作废：

- 固定 `/srv/astrocs/main`；
- 创建服务器用户和统一主机目录；
- fresh clone 或用审核包重建工作副本；
- 要求删除已有分支/worktree才能继续；
- 把 Linux 托管 CI 工具链版本强加给现有 Agent 主机。

V8.1 是唯一有效续作包。它只允许在现有 `main` 工作区原地接管，并保护所有预存修改。
