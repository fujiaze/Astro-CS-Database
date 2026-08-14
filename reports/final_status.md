# V16 Final Closure AuditFix — 最终状态

日期：2026-08-14 ｜ 分支：main ｜ 控制包：AstroCS_FinalClosure_AuditFix_Control_Package_V16
（SHA256 b189076e8d14c7001267e3c5083d8b24d83d7c75564855d8f273093d0e64b2b0）

## V15 未冻结项 → V16 处理

| V16-RJ | 问题 | 处理 | 证据 |
| --- | --- | --- | --- |
| RJ-01 | wbpp_current 冒名 tile-adaptive | 拆分 wbpp_current（group-level 一次解析）与 astrocs_adaptive（tile nominal-depth） | real16 诊断 nominal=16→linear_fit 单次；V16ProfileGroupVsAdaptive |
| RJ-02 | MinMax 迭代删到 min_kept | 一次性固定 rank 删除（low+high 各一次；n−low−high≥min_kept） | V16MinMaxFixedCountExact（50 bias (3,5)→42） |
| RJ-03 | 缺 rejection normalization | RejectionNormalizationPolicy（none/median_center/median_scale）；decision 作用 working，mask 回原始值积分 | V16NormalizationTransparentAndNegativeSafe |
| RJ-04 | percentile 负值域错 | percentile 必须 median_center（|median| 尺度）；否则 INVALID_CONFIGURATION | V16InvalidConfigurationCombos + 负 median 测试 |
| RJ-05 | satellite gate 证据无效 | V2：修 support 路径、truth baseline 用干净副本、四组对照、真实 16 exposure | satellite_v2_metrics.json |
| RJ-06 | eligibility 双路径 | p2_collect_candidate_stack（strided 生产收集器）CPU/ACR/compat 同一 policy | V16GatherStridedFp32Fp64 + stage2 wiring |
| RJ-07 | oracle NOT_RUN 签 PASS | averaged_sigma 改名 `astrocs.averaged_sigma.v1`，IRAF exact=NOT_CLAIMED；oracle_matrix 无 NOT_RUN | rejection.h + oracle_matrix.json |
| RJ-08 | depth 统计重复计数 | depth_0/depth_1/depth_ge_2 mutually exclusive | 诊断输出 |
| Large-Scale | WBPP parity 盘点 | wbpp_feature_matrix：large-scale 默认 off → unsupported（如实）；normalization/参数 mapped | wbpp_feature_matrix.md |

## Gate 状态

```text
G0 V15 baseline        : PASS（65/65 gate；V15 RJ bug 不回归）
G1 WBPP policy         : PASS（group/adaptive 分离；feature matrix 完整）
G2 Rejection normal    : PASS（explicit layer；负值门；决策域/积分域分离）
G3 Algorithm semantics : PASS（MinMax exact；percentile 负值安全；averaged 改名）
G4 Eligibility single  : PASS（CPU/ACR/compat 同一 collector）
G5 Satellite V2        : PASS（真实 16 exposure；recall=1.0；preservation）
G6 Diagnostics         : PASS（depth 互斥；profile/method/norm 显式）
G7 Full E2E            : PASS（真实 raw→Phase1→Phase2→mosaic→browser）
G8 Full source audit   : PASS（canonical_core + repo_source_manifest.csv）
G9 Performance         : PASS（fixed-scratch；3+ runs；等价）
G10 Round0-6           : PASS（升级 Red-Team 10 假设；clean-tree 真实 E2E）
```

## 如实标注

- `PIXINSIGHT_EXACT_COMPATIBILITY = NOT_CLAIMED`（WBPP profile 只提供
  Auto routing/参数映射；rejection normalization 为 mapped）；
- large-scale rejection 启用路径未实现（WBPP 默认 off，如实标注 unsupported）；
- averaged_sigma 无 IRAF 实际运行 oracle → 改名 `astrocs.averaged_sigma.v1`
  且不冻结为 IRAF-compatible；
- clean sample false reject（真实 16 帧）9.45%，如实报告。

## 关键交付

- 真实 16-exposure Phase1→Phase2 E2E + 卫星线门 V2（recall=1.0、背景/星点
  无净损伤）；
- rejection normalization 层 + percentile 负值安全 + MinMax 固定 rank +
  eligibility 单路径 + profile 拆分；
- 审核包：AstroCS_Review_FinalClosure_V16.zip（<25MiB，含 canonical_core
  与 repo_source_manifest.csv）。
