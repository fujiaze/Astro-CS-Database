# CONTRACT-FREEZE-001 — 合同整合与实施放行

## 强制纪律

- 必读根冻结宪章、AGENTS.md、本包 00_READ_FIRST、任务卡及其上位科学文档。
- 子代理只写 write_scope；共享文件和根 CMake/README 只有明确归属任务可改。
- 子代理不 commit、不 push、不建分支/worktree、不 stash/reset/clean、不再派发子代理。
- 所有命令检查退出码；零测试、skip-only、同实现自证不得 PASS。
- 返回列：修改文件、关键公式/符号、验证命令与 rc、未决风险、建议状态。

## 任务正文

由独立集成 Agent 统一七份规格，只有无 SCI 冲突且审核通过才放行实现波次。

## 调度合同

- depends_on: DATA-DESIGN-001, ALG-P1-001, ALG-P2-POINT-001, ALG-P2-PSFSW-001, ALG-P2-SURF-001, ALG-P3-001, QA-MATRIX-001
- wave: 4
- write_scope: docs/science/v6/frozen/, docs/contracts/v6/frozen/, docs/algorithms/v6/frozen/, reports/v6/contract-review/

## 验收

- 任务正文逐项完成且不越界；
- 科学/算法/接口/代码/测试在本任务范围内一致；
- 独立 Oracle 或结构证据充分，负向门能红；
- 建议状态只能 PASS、FAIL 或 REVIEW_REQUIRED。
