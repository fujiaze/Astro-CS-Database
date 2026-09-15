# P1-INTEGRATE-001 — Phase1 产品链集成

## 强制纪律

- 必读根冻结宪章、AGENTS.md、本包 00_READ_FIRST、任务卡及其上位科学文档。
- 子代理只写 write_scope；共享文件和根 CMake/README 只有明确归属任务可改。
- 子代理不 commit、不 push、不建分支/worktree、不 stash/reset/clean、不再派发子代理。
- 所有命令检查退出码；零测试、skip-only、同实现自证不得 PASS。
- 返回列：修改文件、关键公式/符号、验证命令与 rc、未决风险、建议状态。

## 任务正文

接线 Phase1 单帧产品，磁盘重开可供 Phase2 消费；不改已归属底层模块。

## 调度合同

- depends_on: IMPL-P1-CAL-001, IMPL-P1-PSFW-001, IMPL-P1-DRZ-001, IMPL-AIO-001, SCHEMA-INTEGRATE-001
- wave: 7
- write_scope: lib/phase1/, lib/phase1_session/, tests/integration/v6_p1/

## 验收

- 任务正文逐项完成且不越界；
- 科学/算法/接口/代码/测试在本任务范围内一致；
- 独立 Oracle 或结构证据充分，负向门能红；
- 建议状态只能 PASS、FAIL 或 REVIEW_REQUIRED。
