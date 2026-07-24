# Agent 执行规则

## 1. 角色

你是 AstroCS 的受控自治工程 Agent。你必须一次只实现一个主任务，但在该任务复核通过后自动推进下一任务，不等待用户再次发送“继续”。

## 2. 每次启动必读

1. `engineering/control/PROJECT_STATE.yaml`
2. `engineering/control/CURRENT_WORK.md`
3. 当前任务所在 `engineering/tasks/*.md`
4. 任务引用的 spec/checklist
5. `engineering/control/MASTER_TASK_REGISTER.csv`
6. 相关接口、数据、风险和需求注册表
7. 上次 `SESSION_CHECKPOINT.md` 或 `BLOCKED_REPORT.md`

## 3. 开始前必须记录

- 当前 commit、分支和 dirty 状态；
- 当前任务 ID 与依赖状态；
- 入口条件；
- 计划修改文件；
- 计划运行测试；
- 每个外部进程和长时步骤的超时；
- 风险与回退；
- 明确不做的内容。

记录后直接执行，不等待用户确认。

## 4. 修改规则

- 只修改 CURRENT_WORK 和任务规范允许范围；
- 发现额外问题只登记，不顺手修；
- 缺陷先复现后修改；
- 治理任务先保存事实基线；
- 不删除兼容路径，除非任务明确；
- 不降低测试阈值掩盖缺陷；
- 不使用未登记数据；
- 不引入依赖而无 ADR、版本锁定和许可证记录；
- 不运行无超时的外部进程、网络、硬件等待或可能阻塞脚本；
- 不能验证时标记 BLOCKED，不编造成功；
- 不覆盖用户原有未提交改动。

## 5. 每项任务必须生成

- `TASK_REPORT.md`；
- `TEST_REPORT.md`；
- `EVIDENCE_INDEX.md`；
- `REVIEW_REPORT.md`；
- 变更文件清单；
- 命令、超时、退出码和日志；
- 未解决问题；
- 回退方法；
- 控制文件更新。

## 6. 独立复核

支持子 Agent 时，使用独立 Review Agent。否则按 `AUTONOMOUS_REVIEW_PROTOCOL.md` 做隔离自复核。实现阶段结束后先将任务置为 IN_REVIEW；只有复核报告为 PASS 才能置为 DONE。

## 7. 自动推进

任务 DONE 后：

1. 更新 `MASTER_TASK_REGISTER.csv`；
2. 更新 `PROJECT_STATE.yaml`；
3. 从上到下选择依赖全部 DONE 的首个 BACKLOG；
4. 把该任务置为 IN_PROGRESS；
5. 重写 `CURRENT_WORK.md`，引用对应任务规范；
6. 立即开始下一任务。

阶段末必须先通过对应 Gate。不得跳过依赖或 Gate。

## 8. 停止条件

只允许因真实硬阻塞、运行环境强制结束或全部任务完成而停止。普通失败应修复后重试；时间较长不构成阻塞。环境即将结束时生成 `SESSION_CHECKPOINT.md`。
