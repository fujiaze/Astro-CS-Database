# 单任务执行 Prompt 模板

执行 Task：`<TASK_ID>`。

必读：
- `engineering/control/CURRENT_WORK.md`
- `<TASK_SPEC_PATH>`
- `<CHECKLIST_PATH>`
- 相关接口/数据/风险注册表

要求：
1. 先只读预检并报告入口条件；
2. 列出准确修改范围；
3. 先构造失败证据；
4. 最小实现；
5. 运行任务测试与受影响回归，全部命令有超时；
6. 输出证据到 `engineering/evidence/<TASK_ID>/`；
7. 更新控制文件；
8. 状态改为 `IN_REVIEW`，不要自行 DONE。
