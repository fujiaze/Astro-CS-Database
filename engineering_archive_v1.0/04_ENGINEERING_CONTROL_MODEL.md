# 04 工程控制模型

## 1. 单一事实来源

| 信息 | 唯一文件 |
|---|---|
| 当前阶段与 Gate | `control/PROJECT_STATE.yaml` |
| 当前唯一工作 | `control/CURRENT_WORK.md` |
| 所有任务 | `control/MASTER_TASK_REGISTER.csv` |
| 需求到测试追踪 | `control/REQUIREMENTS_TRACEABILITY.csv` |
| 接口状态 | `control/INTERFACE_REGISTER.csv` |
| 数据集状态 | `control/DATASET_REGISTER.csv` |
| 风险 | `control/RISK_REGISTER.csv` |
| 架构决策 | `control/DECISION_LOG.md` + ADR |
| 变更规则 | `control/CHANGE_CONTROL.md` |

旧的 `memory.md` 可保留历史，但不再作为当前状态唯一依据。

## 2. 状态机

任务状态只能是：

- `BACKLOG`：未排期；
- `READY`：入口条件已满足；
- `IN_PROGRESS`：当前唯一执行任务；
- `BLOCKED`：有明确阻塞与解除条件；
- `IN_REVIEW`：实现完成，等待独立复核；
- `DONE`：证据齐全、Gate 通过；
- `REJECTED`：方案被否决，保留记录；
- `OBSOLETE`：因架构决策失效。

同一时间最多一个 `IN_PROGRESS` 主任务。可有只读审计子任务，但不得同时修改同一代码域。

## 3. Gate

| Gate | 通过条件 |
|---|---|
| G0 基线可信 | 源码/依赖/环境可定位，审计事实已复核 |
| G1 可复现构建 | 干净环境一键构建，产物清单与 smoke test 通过 |
| G2 数据契约冻结 | PipelineFrame/HISS/HCSD schema 与兼容测试通过 |
| G3 接口契约冻结 | ABI、内存、错误码、线程、配置 schema 通过 |
| G4 测试平台可用 | 根级测试入口、数据集、报告与 CI 可运行 |
| G5 Stage 1 可信 | 每节点和端到端数值验收通过 |
| G6 Stage 2 可信 | 合成+真实多帧、接缝/通量/权重/离群验证通过 |
| G7 稳定性可信 | 性能、内存、恢复、长时间运行通过 |
| G8 可发布 | 版本、产物、说明、回滚与浏览器兼容通过 |

## 4. Agent 工作闭环

每个任务严格执行：

1. **读取**：当前任务、关联 spec、接口/数据注册表；
2. **预检**：确认基线、依赖和测试可运行；
3. **计划**：列变更文件、测试、风险、回退；
4. **红灯**：先得到失败测试或可重复的缺陷证据；
5. **实现**：最小改动；
6. **绿灯**：相关测试通过；
7. **回归**：受影响模块与上游/下游契约测试；
8. **证据**：命令、环境、日志、结果、哈希；
9. **复核**：独立 Agent 或人工按 checklist 审查；
10. **收尾**：更新控制文件并切换下一任务。

## 5. 禁止行为

- 一次处理多个 P0/P1 问题；
- 未验证旧审计就直接批量修；
- 仅修改文档把缺陷“解释掉”；
- 只看程序退出码，不检查数值结果；
- 测试失败后降低阈值以通过；
- 使用未登记的真实数据；
- 在测试中读取作者电脑的绝对路径；
- 外部进程无超时；
- 删除失败证据；
- Agent 自行决定跨阶段架构重构。

## 6. 证据目录约定

建议每个任务建立：

```text
engineering/evidence/<TASK_ID>/
  TASK_REPORT.md
  TEST_REPORT.md
  EVIDENCE_INDEX.md
  commands.txt
  environment.json
  logs/
  metrics/
  artifacts.sha256
```

大体积 FITS/HISS/HCSD 不进入 Git，但其路径、SHA-256、大小、来源和用途必须进入证据索引。
