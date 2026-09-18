# 变更 claim：FIX-SCI-S2-P3-001 — PHASE3_HIPS_TO_FITS §9a-10 正文与 DATA-UNC-001 更新块同步

- 控制包：RELEASE-02 / 任务 FIX-SCI 第 3 项（复核 SCI-S2 34 项订正中的冻结科学文档）
- 变更对象：`docs/science/PHASE3_HIPS_TO_FITS.md`（SCI-P3-001，FROZEN）§1 非目标、§9a-10
- 来源：`reports/RELEASE-01/science/SCI-S2-topics.md` §2 T11:155、§3-11、§6-4
- 日期：2026-09-18

## 1 问题

该文档顶部 **DATA-UNC-001 更新块**（`:10-19`）已把 variance/ivar 子产品输入由"不支持→显式拒绝"修订为"**必须显式消费传播**（输出 VARIANCE/IVAR 扩展 HDU），两者皆无时显式 unavailable"；但正文 §1 非目标（`:32`）与 §9a-10（`:147`）仍写"variance/weight/ivar 输入产品 → 显式拒绝"，**supersession 未落到正文**，同一冻结合同内两处互斥（SCI-S2 §6-4 登记为 UNRESOLVED）。

## 2 证据（文件:行）

- `docs/science/PHASE3_HIPS_TO_FITS.md:10-19`（更新块，权威依据 = `docs/science/UNCERTAINTY_AND_COVARIANCE.md` Phase3 节 DATA-P3-UNC-001 + `docs/contracts/DATA_SEMANTICS.md` §30.4）。
- 同文 `:32`、`:147`（旧正文）。
- `docs/science/UNCERTAINTY_AND_COVARIANCE.md:10` 等：Phase3 方差/ivar 传播口径。

## 3 diff 摘要

| 位置 | before | after |
|---|---|---|
| §1 非目标 | 拒绝项含 "variance/weight/ivar 输入产品" | 拒绝项改为 "weight/support 输入产品"，并注明 "variance/ivar 子产品为例外，按 §9a-10 必须显式消费传播（DATA-UNC-001 更新块）" |
| §9a-10 | "variance/weight/support 输入：不支持→显式拒绝；输出仅 S+coverage" | "variance/ivar 子产品必须显式消费传播（输出 VARIANCE/IVAR 扩展 HDU），皆无时显式 unavailable；weight/support tile 仍拒绝；flux-per-pixel 仍拒绝（§9a-8）" |

## 4 影响面

- 文档：`docs/science/PHASE3_HIPS_TO_FITS.md`（§1/§9a-10）。§2/§5/§8 的公式、容差、投影清单**零改动**。
- 实现：无（Phase3 alpha 未实现，本合同为施工边界；改动使施工边界与在役权威一致）。
- 未触碰三命令 / JSON 结构 / HiPS 数据模型。

## 5 回归

- `python3 tools/science_contract_lint.py docs/science/PHASE3_HIPS_TO_FITS.md` → PASS（15 节）。
- `CHK-SCI-REF`/`CHK-DANGLING`：见 `reports/RELEASE-02/FIX-SCI-report.md` §5。

## 6 状态

- **已落地**（doc-only；消除同合同内互斥）。
