# 会话恢复规则

收到自治启动提示词后，不默认从 P00-001 重新开始。先读取：

- `control/PROJECT_STATE.yaml`
- `control/CURRENT_WORK.md`
- 当前证据目录下的 `SESSION_CHECKPOINT.md`
- Git 状态与最近提交

恢复优先级：

1. BLOCKED 且阻塞条件已解除：从阻塞报告指定步骤继续；
2. IN_REVIEW：先完成复核；
3. IN_PROGRESS 且有检查点：从检查点下一命令继续；
4. IN_PROGRESS 且无检查点：重新做只读预检后继续，不覆盖已有证据；
5. 没有当前任务：从注册表选择依赖满足的首个 BACKLOG；
6. 全部 DONE：执行最终 Gate 和发布完整性检查。
