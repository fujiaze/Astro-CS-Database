# Round 5 — Independent Red-Team Review（V16 升级）

针对 V15 审核决定列出的 10 个假设，逐一验证（DISPROVED_WITH_EVIDENCE 或
BUG_FOUND_AND_FIXED）：

| # | Hypothesis | 结论 |
| --- | --- | --- |
| 1 | WBPP group-level policy 被 tile-level adaptive 冒名 | DISPROVED：wbpp_current 在 run 开始一次解析（stage2 group_plan，real16 诊断 `resolved_methods={'16': linear_fit}` 单次）；astrocs_adaptive 独立命名，头文件/报告明确非 WBPP exact |
| 2 | MinMax 是否固定删 count | DISPROVED：一次性 rank 删除（V16MinMaxFixedCountExact (3,5)→42）；`max_iterations` 已删 |
| 3 | clean-rejection gate 拿"同样过度拒绝的 clean"当 truth | BUG_FOUND_AND_FIXED：V15 baseline 逻辑漏洞 + 配置误指 trail 副本 → V2 用干净副本 truth + 四组对照；修正后 injection@mask=+0.0033 |
| 4 | support metrics 是否真的读取 support | BUG_FOUND_AND_FIXED：V15 load_tile 读 signal → V16 读 support/（satellite_gate_real_metrics.py） |
| 5 | negative physical values + percentile | DISPROVED：median_center 工作域 + |median| 尺度；负 median 测试 PASS；违规组合 INVALID_CONFIGURATION |
| 6 | rejection normalization 是否缺失 | DISPROVED：RejectionNormalizationPolicy 已实现（none/median_center/median_scale），decision 在 working、积分在原始值 |
| 7 | canonical Eligibility 是否真被 production 调用 | DISPROVED：stage2 CPU 与 ACR 均调用 p2_collect_candidate_stack（grep 验证），compat 走同一 policy core |
| 8 | oracle matrix 是否 NOT_RUN 却签 PASS | DISPROVED：averaged_sigma 改名 + IRAF NOT_CLAIMED；oracle_matrix.json 无 NOT_RUN 条目（全部实际运行或明确映射） |
| 9 | diagnostics 名称与计数一致 | DISPROVED：depth_0/depth_1/depth_ge_2 互斥；diagnostics 输出 profile/method/normalization/group 显式 |
| 10 | Large-Scale rejection 遗漏却声称 parity | DISPROVED（如实）：feature matrix 标注 large-scale 默认 off → unsupported（启用未实现），不声称 parity |

额外：ScratchVec n>64 数据丢失崩溃 → BUG_FOUND_AND_FIXED（R1-P1-001）。

```text
10 项 V16 强制假设 + 1 项额外假设全部闭环
ROUND5=PASS
```
