# 工程控制 / RELEASE-02 差距审计（GAP_AUDIT）

## 1. 输入基线

RELEASE-02 不重做全量审计，以 RELEASE-01 审计结果为修复输入：

- P0 清单（16 条 + 二轮增量 P0-17/18）：仓库 `工程控制/RELEASE-01/GAP_AUDIT.md` §1、§7.5；
- P1=87、P2=35 明细：`reports/RELEASE-01/audit/AUD-A1-normalize.md`（54 条）、`AUD-A2-mosaic.md`（25 条）、`AUD-A3-export.md`（27 条）、`AUD-A4-infrastructure.md`（41 条）；
- 科学侧：`reports/RELEASE-01/science/SCI-S1-snr-psfsw.md`、`SCI-S2-topics.md`；
- 测试侧：`reports/RELEASE-01/tests/TST-001-report.md`；
- 性能侧：`reports/RELEASE-01/perf/PERF-001-timing.md`；
- 视觉侧：`reports/RELEASE-01/vis/VIS-001-report.md`。

修复任务映射见 `TASK_LIST.md` 与 tasks/FIX-*.md。

## 2. 修复中新增差距（增量登记）

> 修复过程中新发现的问题在此登记，格式同 RELEASE-01：编号、位置（文件:行）、违背条款、级别、归属任务、归宿。

| 编号 | 位置 | 问题 | 级别 | 归属 | 归宿/证据 |
|---|---|---|---|---|---|
|  |  |  |  |  |  |

## 3. 自决研究记录（§3a）

> 科学方法、门禁判据的自决研究在此汇总，详细证据入 reports/RELEASE-02/。

| 主题 | 子代理 | 查阅的文献/开源 | 结论 | 落地位置 |
|---|---|---|---|---|
|  |  |  |  |  |

## 4. 闭合核验

全部修复完成后，前台对照 RELEASE-01 GAP_AUDIT §1 与本文件 §2 逐条核验归宿，结果写入 `ACCEPTANCE.md` §2。
