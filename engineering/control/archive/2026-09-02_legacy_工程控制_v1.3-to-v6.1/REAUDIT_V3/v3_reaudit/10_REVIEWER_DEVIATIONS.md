# 10_REVIEWER_DEVIATIONS 审核人要求的范围限定/偏离记录

> 本文件记录**审核人**（外部审核/现场审核）明确给出的范围限定或偏离决定。
> 这些决定优先于任务默认执行范围；审核包中须同步记录对应要求原文。
> 更新：2026-08-27。

## DEV-ACR-001：ACR CPU/GPU 异构计算跳过，只做纯 CPU

- **审核人要求（原文/语义）**：
  > ACR 相关的 CPU/GPU 异构计算先跳过，只做纯 CPU。
- **适用范围**：`lib/acr`（ACR 模块）的 CPU/GPU 异构（Mixed）部分。
- **受影响的控制包任务**：
  - `ACR-001`（CPU GPU Mixed route activation）
  - `ACR-002`（CPU GPU Mixed scientific equivalence）
  - `ACR-003`（GPU CPU utilization and fallback evidence）
- **执行决定**：以上三项**跳过**（不执行 GPU/Mixed 三路验证）。置状态 **BLOCKED**
  （审核人范围限定/跳过的阻断条件，按规范 "GPU 不可用记 BLOCKED，不能 PASS" 处理，
  且不得 PASS 以保持技术真实性）。仅保留 **纯 CPU** ACR 路径（CPU dispatcher / CPU 构建/测试）。
- **同步记录**：审核包 `reports/REAUDIT_V3/v3_exec/REVIEWER_REQ_ACR_hetero_skip.md`
  记录本要求为审核人指令。
- **备注**：如后续审核重新开放 GPU/Mixed，须在本文件追加修订并更新任务状态。
