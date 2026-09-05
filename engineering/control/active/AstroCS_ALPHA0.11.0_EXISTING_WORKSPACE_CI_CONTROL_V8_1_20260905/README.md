# AstroCS Alpha 0.11 现有工作区 CI 控制包 V8.1

本包承接 V7.1 和审核快照 `257ae1f4efbaf5822ea6d7c42882e15f2c416b36`，目标产品版本为 `0.11.0-alpha.2`。

本包不迁移、不创建、不克隆仓库。它在服务器现有 AstroCS `main` 工作区中原地接入 GitHub Linux/Windows CI、修复已确认 P0 问题、连接 Fatduck 真实数据终验，并恢复尚未完成的 Alpha 任务。

执行入口：`00_READ_FIRST.md`。机器入口：`CONTROL_TASK_LEDGER.csv`、`GATE_REQUIREMENTS.csv`、`ci/checks.seed.json`。

V8.0 中固定 `/srv/astrocs/main`、创建主机目录和 fresh clone 的设计全部作废，不得执行。
