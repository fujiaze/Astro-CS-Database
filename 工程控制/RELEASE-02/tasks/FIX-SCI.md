# FIX-SCI 冻结科学文档变更 claim（负责人已批准）

## 目标

RELEASE-01 SCI-001 发现两项 P0 科学文档问题，位于冻结文档，负责人已明确裁决"订正成正确的"。按 ENGINEERING_SPEC §3 走变更 claim 落地，并做一致性回归。

## 订正项

1. **DRIZZLE.md §5（归一化）与 §7（不变量）互斥**：两处对 drizzle 归一化的口径不能同时成立。派子 agent 重读 Fruchter & Hook 2002（PASP 114,144）原文、SWarp/本仓库 drizzle 实现，确认归一化因子在"像素保留面积加权"与"通量守恒不变量"之间的正确关系，订正错误的一节，使 §5/§7 自洽；同步受影响的 `docs/plugins/algorithms_phase1/08_drizzle.md`、`PHASE1_DETAILED_DESIGN.md` 引用；
2. **ASTROMETRY.md §5a 的 1px 平移口径**与 Zackay & Ofek Paper I / 当前实现不一致。派子 agent 核对 Paper I 关于配准残差/亚像素平移的处理、SCAMP/astrometry.net 开源实现与本仓库 platesolve/resample 代码，订正文档口径到与科学正确且与实现一致（若实现错了则改实现，以科学正确为准）；
3. 复核 SCI-S2 报告的 34 项订正（P1 8 / P2 24）中，凡属**冻结科学文档**的，一并按同一流程落地；非冻结文档的订正 RELEASE-01 已完成，核对无回退即可。

## 流程（每个订正）

派子 agent 研究 → 一手文献/开源代码证据（文件:行、式号）→ 变更 claim（编号、问题、证据、订正前后 diff、影响面）→ 改文档 → 受影响代码/测试同步 → 一致性回归（CHK-SCI-REF、CHK-DANGLING、相关 Oracle）→ 前台独立复核。

## 验收门

- 两项 P0 有完整变更 claim 记录，订正后文档自洽（内部无互斥条款）；
- 文档-实现一致：订正涉及的代码路径有合成测试守护；
- `tools/science_contract_lint.py`、CHK-SCI-REF、CHK-DANGLING 全绿；
- 新增文献进入 SCIENTIFIC_REFERENCES 且编号/DOI 核验真实（Crossrun/Crossref 或出版社页面）。
