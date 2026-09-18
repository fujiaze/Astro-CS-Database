# per-doc: docs/algorithms/PHASE2_INTEGRATION.md

| 编号 | 行 | 主张 | 独立证据 | 判定 | 三分类 |
|---|---|---|---|---|---|
| R2-L-03(P1D-001) | 64,235-241 | 登记 DISP-P2INT-001「sup_max 在 w==0 continue 之后更新（:54-55）」 | 同文 §5:112/§7:174-179/§10.2 与实装 integrate.cpp:49-50 均表明 sup_max 在权重分支之前 → 自相矛盾 | WRONG | DESIGN-OK-DOC-WRONG |
| — | §3 | sup_max 语义 | 代码与 §5/§7 一致 | CORRECT | — |

## 处置
删除 §3:64 与 §11.3 的 DISP-P2INT-001 残留，保留"已修复"口径。
