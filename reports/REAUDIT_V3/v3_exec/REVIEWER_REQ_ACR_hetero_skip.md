# 审核人要求：ACR CPU/GPU 异构计算跳过（只做纯 CPU）

> 记录来源：**审核人**直接指令（本次会话）。
> 状态：已采纳并同步到控制包 `10_REVIEWER_DEVIATIONS.md`。
> 更新：2026-08-27。

## 要求原文（语义）

> ACR 相关的 CPU/GPU 异构计算先跳过，只做纯 CPU。

## 决定与影响

- **跳过**：ACR CPU/GPU **异构（Mixed）** 部分，即控制包任务
  - `ACR-001`（CPU GPU Mixed route activation）
  - `ACR-002`（CPU GPU Mixed scientific equivalence）
  - `ACR-003`（GPU CPU utilization and fallback evidence）
- 以上三项置 **BLOCKED**（审核人范围限定；按规范 "GPU 不可用记 BLOCKED，不能 PASS" 处理，
  不 PASS 以保持技术真实）。
- **保留**：ACR **纯 CPU** 路径（CPU dispatcher、CPU 构建/测试）——仍按正常任务推进。

## 依据

- 规范 §F「ACR-001..003：同一数据、同一精度依次强制 CPU、GPU、Mixed，不允许 auto 冒充
  三路验证」；本次审核人明确将 GPU/Mixed 从范围中剔除，故三路验证不再需要对 Mixed/GPU 执行。

## 执行记录

- 控制包：`工程控制/REAUDIT_V3/v3_reaudit/03_TASK_SPECIFICATIONS.md` 未改（保留规范原文），
  新增 `10_REVIEWER_DEVIATIONS.md`（DEV-ACR-001）记录审核人要求。
- 控制包 ledger：ACR-001/002/003 置 BLOCKED。
- 审核包：本文件记录该要求为**审核人指令**。
