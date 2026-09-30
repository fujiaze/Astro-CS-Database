# code/audit_rework — P5 审计重做三路 + 两个补实验的可复现脚本归档

**脚本快照位置**（本目录内，逐字节拷贝，文件名保持原样）：

| 子目录 | 快照位置 | 内容 |
|---|---|---|
| `route1/` | `route1/` | C1–C10 十个脚本＋其 run_all.sh |
| `route2/` | `route2/` | M1–M7 对应 e1–e10 十个脚本 |
| `route3/` | `route3/` | Q1–Q12 对应 exp01–exp12 十二个脚本 |
| `supp_507_relstep/` | `supp_507_relstep/` | ea（×5.07 扫描）、eb（rel_step_max 标定）、p5c_common |
| `supp_control_variance/` | `supp_control_variance/` | 有限 N、生产链、独立 seed 抽检三脚本（D-07 定稿口径） |

**k_corr 两因子查表补实验属 P3 单元承载**（`实验/裁决台账.md` D-08，负责人已批），不在本单元复现；
其固化读数拷贝于 `../../results/audit_rework/p3_kcorr/`（summary.csv、tables.md、g1/g2/g3/g4/g6）。

## 运行

```bash
bash code/audit_rework/run_all.sh          # 全量，单脚本 CPU ≤ 90 s，总计 < 30 min
# 或逐目录：
cd code/audit_rework/route1 && python3 c3_seam_gate.py   # 产物写在该目录 results/ 下
```

依赖：python3 + numpy 仅。无网络。零仓库 import。

## 固定 seed 说明

- **路线2 / 路线3 / 补实验（含 P2 control_variance 补实验）：seed = 20250926**
  （补实验-5.07与relstep 经 `p5c_common.SEED_BASE` 统一写死）。
- **路线1：seed 按主题分段写死于各脚本头**——接缝门 20260319、方差比 20260320、表示边界 20260325、
  其余 20260926 系（以各脚本源码头部标注为准）。
- 所有脚本 seed 均不可经命令行覆盖（防结果漂移）；重跑必须逐位复现已固化 JSON。
- 重跑产物落在本归档各子目录的 `results/` 下，**不会覆盖**单元 `results/audit_rework/` 的固化拷贝。

## 与论文/实验报告的对应

论文正文每个 [实验:...] 标注均回溯到本归档脚本＋`results/audit_rework/` 固化 JSON；
判据与订正以总编对账《分歧台账》D-07/D-08/A-P5-01…A-P5-12 为准。
