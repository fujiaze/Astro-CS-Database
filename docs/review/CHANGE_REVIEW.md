# CHANGE_REVIEW.md — AstroCS 变更审查（L0 负责人层）

> 目标版本: 0.10.0-alpha.1  V6 重构
> 权威: `evidence/refactor/TASK_LEDGER.csv`、`evidence/refactor/tasks/*/TASK_RESULT.json`

## 变更纪律
- 仅 main 原子提交 + 立即 push; 每任务一个 commit, 前缀 `<TASK-ID>:`。
- 三 SHA 纪律: HEAD == main == origin/main; 禁 force push/reset/rebase。
- 外部修改禁 stage (ISA MEASUREMENTS.csv ×4, test_bench_cli.py)。

## 已执行变更摘要 (G4..G7)
| 门 | 任务 | 核心变更 |
|---|---|---|
| G4 | P1-001..009 | phase1 模块化 + synthetic fixtures (校准/星点/WCS/噪声/NSIDE/HiPS/IR/资源) |
| G5 | P2-001..008 | phase2 UPM/排异/block/输出语义 + workers 门禁 |
| G6 | P3-001..006 | phase3 正式化 + WCS/插值/coverage/FITS 验证/组装 |
| G7 | CLI/LEG | command 层 + run preset + test 接通 + 旧路径退出 |

## 待审查
- 每任务 TASK_RESULT.json (evidence/refactor/tasks/) 供负责人抽查。
- 最终视觉审查: 负责人 HiPS 图像核对 (REL-004)。
