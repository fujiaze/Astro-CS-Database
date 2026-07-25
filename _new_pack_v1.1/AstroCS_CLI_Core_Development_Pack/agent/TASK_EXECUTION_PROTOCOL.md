# Task Execution Protocol

- 在 `evidence/<TASK_ID>/` 创建四份报告。
- 命令必须记录工作目录、参数、超时、退出码和日志路径。
- 修改 ABI/格式前先增加失败的 contract test。
- 真实数据文件只登记 hash/路径，不提交仓库。
- 测试失败时保留首次失败证据，不覆盖。
- 任务通过后更新 `MASTER_TASK_REGISTER.csv` status=DONE，并把下一依赖满足任务写入 `CURRENT_TASK.md`。
