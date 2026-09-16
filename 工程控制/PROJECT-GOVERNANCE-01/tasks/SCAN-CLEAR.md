# 任务：SCAN-CLEAR　台账清零核查（785 行逐条对当前树复核）

状态：NOT_STARTED　层：L1　依赖：阶段 0 收口（可并行开始，结论需在阶段 1 前完成）

## 背景（负责人指令）

> 「此前应该确认**台账上的清单全部清理干净**，把 bug 什么的修完。」

台账 = `问题扫描/REBASE_TABLE.md`（785 行；配套 26 个分片在 `reports/PROJECT-GOVERNANCE-01/root-scan/shards/`，索引 `DISPATCH_MATRIX.md`、`by_owner/*.txt`）。该台账是**重构前基线**的扫描结果，且研究线已证明其中**部分判据本身是错的**（R-1 判掉 CAR/AIT 与 leaf_order 两条；R-3 判掉 0.5px 量化门与 7×7 自证门；R-5 判掉 M7-A-203 对象错、V10-N-07 别题误挂；R-6 判掉 D-17 登记面与若干门）。

## 要做的

1. **逐条复核 785 行**（含已标 RESOLVED/VOID/UNVERIFIABLE 的），对当前树判定四类之一：
   - `FIXED`（已修：给出证据 = 代码/文档现状 + 复跑命令 + rc）；
   - `OPEN`（仍存在：给根因 + 修复方案 + 建议归属任务 + 验收门）；
   - `CRITERION-WRONG`（**判据本身错**：给出为什么错 + 正确口径 + 依据；此类不得按原处方整改）；
   - `NOT-APPLICABLE`（对象已不存在/已退役：给出退役记录）。
2. **旧→新路径翻译**：台账基于重构前路径，必须用 `cmake/ARCH-001-migration-manifest.md` + 当前树实际位置翻译（旧路径 100% 失效，直接照台账派活必错）。
3. **输出**：`reports/PROJECT-GOVERNANCE-01/scan-clear/REBASE_VERDICT.csv`（逐行：ID / 原判据摘要 / 判定 / 证据 / 归属）+ `SUMMARY.md`（四类计数 + 未清零清单按域分组 + 判据错清单）+ 逐条日志。
4. **不得**修改 `问题扫描/**`（隔壁挖掘线在用）；你的产物只落 `reports/PROJECT-GOVERNANCE-01/scan-clear/**` 与 `run/PROJECT-GOVERNANCE-01/SCAN-CLEAR/**`。

## 计数要求（防糊弄）

- 逐类计数之和必须 **= 785**（与台账行数一致）；与既有 `DISPATCH_MATRIX.md` 的 OPEN/RESOLVED/VOID 计数逐项对差并解释差异；
- 抽样可复跑：`CRITERION-WRONG` 每条都要有独立证据（标准原文/实验），`FIXED` 每条都要有可复跑命令。

## 验收门

- [ ] 785 行全覆盖，四类计数自洽；
- [ ] `OPEN` 清单每项含「根因 + 方案 + 归属 + 验收门」；
- [ ] `CRITERION-WRONG` 清单每项含「为什么错 + 正确口径 + 依据」；
- [ ] 零 git 写；日志与证据落上述目录。
