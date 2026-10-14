# 断点续做说明（RESUME NOTES）

分支 `audit/acsd-science-docs-p5`，基线 `e31b2be66ffe37d547e67a4ba007e7257e49789d`。
本文件供后续会话或负责人**直接续做**，不重复已完成工作的过程。

## 一、已闭合（无需重做）

| 项 | 落点 |
|---|---|
| S00 真实帧索引 | `实验/shared/data_catalog.json` |
| S01 引文一致性审计 | `实验/shared/references/{CORE_REFERENCES_VERIFIED.md,S01_CITATION_AUDIT.json}` |
| P3 生产缺陷（两处近简并叉积） | 代码 `lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp`；证据 `实验/healpix-polar/**`（附录 A/B/C + `docs/S02_P3_*.json` + 4 图） |
| P5 生产缺陷（逐分量 gauge） | 代码 `lib/algorithms/coverage/src/sky_plane.cpp`；证据 `实验/additive-sky-seamless/**`（附录 S03/S03-exec/S03-裁决 + `docs/S03_P5_COMPONENT_evidence*.json`） |
| P1 实验 | `实验/photometric-magnitude/**`（§11 + `code/s04/` + 7 图） |
| P2 实验 | `实验/absolute-snr/**`（6 节 + `docs/{S05_P2_evidence.json,S05_REPORT.md}` + `code/s05/` + 4 图） |
| P4 实验 | `实验/dense-snr-reconstruct/**`（§9 + `docs/S06_P4_evidence.json` + `code/s06/` + 4 图） |
| P5 乘性实验 | `实验/additive-sky-seamless/**`（附录 S07 + `docs/{S07_P5_GAIN_evidence.json,S07_REPORT.md,S07_results/}` + `code/s07_*.py` + 4 图） |
| P1/P3/P4 论文更新 | 三篇 `REPORT_paper.md`（提交 `09b14b13d`） |
| 验收对照表 | `实验/shared/FIVE_PAPER_EXPERIMENT_PLAN.md` §九 |
| S09 零回归 + 端到端 | `S09_TEST_REGRESSION_BASELINE.md`、`S09_E2E_RUN.md`、`S09_E2E_STATUS.md` |

## 二、唯一剩余实质缺口

**S08 的 P2 与 P5 两篇 `REPORT_paper.md` 正文尚未更新。**

依据已齐备，无需再跑实验：

- **P2**：`实验/absolute-snr/docs/S05_P2_evidence.json`（8 条）、`S05_REPORT.md`、`code/s05/`、`figures/`（4 图）、
  `REPORT_experiment.md` 的 6 节新章。
- **P5**：`实验/additive-sky-seamless/docs/S07_P5_GAIN_evidence.json`、`S07_REPORT.md`、`S07_results/`（5 件）、
  `figures/`（4 图，含生成脚本）、`REPORT_experiment.md` 的附录 S03 / S03-exec / S03-裁决 / S07。

**体例**：照已提交的 `实验/healpix-polar/REPORT_paper.md`（P3）与
`实验/dense-snr-reconstruct/REPORT_paper.md`（P4）——`表 1 相关工作与本文区别` / `表 2 数据、构建与受控构型` /
`表 3 实验设计` / `§5.1 实测数字(表 4)` / `§5.2 图` / `§5.3 证据级别` / `§5.4 适用域与诚实限制` / `§5.5 跨篇误差传递反例`。

**必写诚实限制**（详见 `SCIENCE_HANDOFF_REPORT.md` §5 与 §8）：P2 的 `k_corr` `UNCALIBRATED`（生产几何
`rho≈0.83` 下真值 `2.05–2.33` 而冻结回退 `1.4`）、`2.7928` 须改为分层读数 + 经验地板、`sparse_snr_layer`
`NOT_IMPLEMENTED`、`sigma_F` 为条件方差；P5 的乘性 **`FAIL`**（阴性世界假阳性）、**判据分裂**、
跨分量绝对零点不可辨识且**需外部锚**、`S9`/`S13` 失败关闭是有意的、乘性**次序缺陷**、边界 `196→114`。

**提交前自查**（前次教训）：① 论文正文**不得**出现日期/版本号/任务编号/commit/「本轮」/历史叙事
（AGENTS §5）；② 每张表每一行的**来源文件必须真的含有该行所列数值**（提交前用脚本逐行核对）；
③ 证据级别只用六级、**不得**用不在六级内的 `PRODUCTION_CHECKED`、**不得**写 `RELEASE_VERIFIED`；
④ 无实测数据的图只留图题 + 「尚未运行」，**不得**画示意图冒充结果。

## 三、完成后收尾

1. 提交 P2/P5 两篇（各一次提交，或一次提交含两篇 + 各自 `figures/`）。
2. 刷新 `SCIENCE_HANDOFF_REPORT.md`：表 A 提交清单加到最后一次提交；§3/§5 复核；**移除 `DRAFT` 标记**。
3. 回传完整 HEAD SHA。

## 四、明确不在本任务范围（不要顺手改）

`k_corr` 修复、`idw_power` 统一、乘性次序修复、跨分量外部锚、P3 未扫像元域、`docs/science/**` 判据文字、
29 项既存测试失败（`module_adapters.cpp` 锚点漂移）、`.gitignore` 例外、`main` 合并与 PR（禁止）。
