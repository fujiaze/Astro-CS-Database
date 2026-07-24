# P05 Stage 1 验证任务

按 `11_STAGE1_VALIDATION_SPEC.md` 从 READ_FITS 到 DRIZZLE 顺序执行。

每一节点：

1. 冻结输入/输出契约；
2. 合成真值；
3. 最小真实数据；
4. 失败路径；
5. 性能摘要；
6. 上下游契约回归。

Stage 1 E2E 前不得跳过真实主帧校准，也不得把退化路径当正式验收。
