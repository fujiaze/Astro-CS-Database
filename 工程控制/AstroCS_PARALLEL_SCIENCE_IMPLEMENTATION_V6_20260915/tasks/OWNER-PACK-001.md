# OWNER-PACK-001 — 负责人交付封装

## 强制纪律

- 必读根冻结宪章、AGENTS.md、本包 00_READ_FIRST、任务卡及其上位科学文档。
- 子代理只写 write_scope；共享文件和根 CMake/README 只有明确归属任务可改。
- 子代理不 commit、不 push、不建分支/worktree、不 stash/reset/clean、不再派发子代理。
- 所有命令检查退出码；零测试、skip-only、同实现自证不得 PASS。
- 返回列：修改文件、关键公式/符号、验证命令与 rc、未决风险、建议状态。

## 任务正文

根据最终 findings 形成负责人审核包，状态仅 READY_FOR_OWNER_REVIEW/NOT_READY/AWAITING_WINDOWS_VALIDATION/PACKAGE_INVALID。

## 调度合同

- depends_on: FINAL-AUDIT-001
- wave: 14
- write_scope: reports/v6/owner-package/, artifacts/v6/owner-package/

## 验收

- 任务正文逐项完成且不越界；
- 科学/算法/接口/代码/测试在本任务范围内一致；
- 独立 Oracle 或结构证据充分，负向门能红；
- 建议状态只能 PASS、FAIL 或 REVIEW_REQUIRED。
