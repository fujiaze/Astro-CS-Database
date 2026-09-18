# 变更 claim：FIX-SCI-S2-PHOT-001 — PHOTOMETRY.md 行锚订正 + F_syn 绝对刻度降级

- 控制包：RELEASE-02 / 任务 FIX-SCI 第 3 项（复核 SCI-S2 34 项订正中的冻结科学文档）
- 变更对象：`docs/science/PHOTOMETRY.md`（SCI-PHOT-001，FROZEN）§2 符号表、§5、§6
- 来源：`reports/RELEASE-01/science/SCI-S2-topics.md` §3-10、§3-12（T4 主题）
- 日期：2026-09-18

## 1 问题

1. **行锚失准**：`mag_tolerance=3.0` 的实际传入点是 `lib/algorithms/photometry/cpp/src/pc_api.cpp:139` 与 `:398`（`cleanAndScale(..., 3.0, ...)`）；文档 §2:24 与 §5:65 标 `star_matcher.cpp:241`，而该行是 `psf_valid` 诊断（`star_matcher.cpp:241-245`）。
2. **F_syn 绝对标度表述过强**：§6 写"Gaia XP **绝对**分光刻度，CALSPEC 溯源"；但 `F_syn=∫F_λTQλdλ` 不含 `1/(hc)` 等绝对归一，常数被 IRLS `location` 吸收，模型只提供**相对**通带刻度。

## 2 证据（文件:行）

- `lib/algorithms/photometry/cpp/src/pc_api.cpp:137-142`、`:396-401`：`cleanAndScale(raw_matches, &match_rows, nullptr, 3.0 /* mag_tolerance */, ...)`。
- `lib/algorithms/photometry/cpp/src/star_matcher.cpp:241-245`：仅 `out_diag->psf_valid = valid_idx.size()` 诊断，非 mag_tolerance 传入点。
- `docs/science/PHOTOMETRY.md` §5:44 `r_i = log10(F_instr/F_syn)`、§5:59 `scale = 10^{−location}`：F_syn 的常数归一被 `location` 吸收（相对刻度）。

## 3 diff 摘要

| 位置 | before | after |
|---|---|---|
| §2 符号表 `mag_tolerance` | `star_matcher.cpp:241` | `pc_api.cpp:139,398` |
| §5:65 锚 | `star_matcher.cpp:21-27,241-248,435-525,552-559` 及 `pc_api.cpp` | `star_matcher.cpp:21-27,435-525,552-559`（IRLS/Tukey/scale）及 `pc_api.cpp:139,398`（mag_tolerance 传入点；`star_matcher.cpp:241-248` 仅 psf_valid 诊断） |
| §6 假设 | "Gaia XP 绝对分光刻度，CALSPEC 溯源" | "模型通带**相对**刻度，锚 Gaia XP 光谱形状；`F_syn` 不含 `1/(hc)` 等绝对归一，常数由 `location` 吸收，**不宣称绝对通量刻度**" |

## 4 影响面

- 文档：`docs/science/PHOTOMETRY.md`（§2/§5/§6）。公式、常数、容差**零改动**。
- 实现：无。
- 与 `tools/docs_machine_consistency.py` 无交集（该工具只校验 MAD/trimmed-mean 常数；本次未触碰）。

## 5 回归

- `python3 tools/science_contract_lint.py docs/science/PHOTOMETRY.md` → PASS（15 节）。
- `CHK-SCI-REF`/`CHK-DANGLING`：见 `reports/RELEASE-02/FIX-SCI-report.md` §5（CHK-SCI-REF 的 DOC-LINE-ANCHORS 红来自并行分片 `sampler.cpp` 的 P2SMP-CELLSIDE，与本 claim 无关）。

## 6 状态

- **已落地**（doc-only）。
