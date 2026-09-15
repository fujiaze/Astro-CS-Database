# RUNTIME-CI-001 — 运行时与 CI 收口

## 强制纪律

- 必读根冻结宪章、AGENTS.md、本包 00_READ_FIRST、任务卡及其上位科学文档。
- 子代理只写 write_scope；共享文件和根 CMake/README 只有明确归属任务可改。
- 子代理不 commit、不 push、不建分支/worktree、不 stash/reset/clean、不再派发子代理。
- 所有命令检查退出码；零测试、skip-only、同实现自证不得 PASS。
- 返回列：修改文件、关键公式/符号、验证命令与 rc、未决风险、建议状态。

## 任务正文

统一资源预算、并行确定性、CLI mode、CI 门和负向 mutation；不重写科学实现。

## 调度合同

- depends_on: P1-INTEGRATE-001, P2-INTEGRATE-001, P3-INTEGRATE-001
- wave: 9
- write_scope: runtime/, lib/core/, cli/, tools/v6/, ci/, .github/, tests/system/v6_runtime/

## 验收

- 任务正文逐项完成且不越界；
- 科学/算法/接口/代码/测试在本任务范围内一致；
- 独立 Oracle 或结构证据充分，负向门能红；
- 建议状态只能 PASS、FAIL 或 REVIEW_REQUIRED。
