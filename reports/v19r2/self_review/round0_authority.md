# Round0 — Authority

检查 science/algorithm/architecture/standard 文档互不冲突：

- L1 science 与 L2 algorithms 分层引用（NOISE_MODEL ↔ NOISE_ESTIMATION
  常数一致；DRIZZLE ↔ DRIZZLE_GEOMETRY 公式一致；PHASE2_UPM ↔
  UPM_SOLVER 绑定契约一致）；
- L4 standards 与 MASTER_CONTROL_SPEC §5 逐条对齐（MUST/SHOULD/MAY）；
- 历史文档（docs/history/v19/）标注非 current authority；
- 机器一致性 6/6 PASS（含常数/退出码/stage/产品契约）。

结论：PASS（无冲突 authority）。
