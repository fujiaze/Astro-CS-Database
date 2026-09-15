# W1 片4 · P2｜D_COMMENT

### W1-N-11（P2）`module_adapters.cpp:3170-3178` 欠采样告警在 **auto 分支**也触发，且文案硬编码归因为 explicit ⇒ 诊断面失实（数据面不错）
- `finest` 小于 0.0503 每像素时引擎钳位 `2^22` 使 `hp_res` 大于 `finest`，**auto 运行会打出「按用户显式值执行 (nside_source=explicit)」的失实归因日志**；且「合规应取 nside=auto_res.nside」的建议值 **等于本次实际采用值** ⇒ 建议自相矛盾。`p1_stack.json` 字段仍如实记 auto ⇒ **数据面对、诊断面失实**。related `V9-N-13`（归因失真族）、`C-17`、`M3-G-003`（日志合同）

### W1-N-13（P2）`drizzle_engine.cpp` 的 p22 剖面行 `out_sort=%.3f` **恒传字面量 0.0**（无实测来源）⇒ 自报观测面含伪指标
- `fine=false` 时整行虽不输出，但字段语义会误导运维读表（把常量当实测排序开销）。改法：删除该字段或补真实度量来源。related `M8-F-009`（profile 字段名与语义对不上）、簇 7、§10.5 自报面

