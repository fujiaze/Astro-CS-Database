# V17 True Final Freeze — 最终状态

日期：2026-08-14 ｜ 分支：main ｜ 控制包：AstroCS_TrueFinal_Freeze_Control_Package_V17
（SHA256 10ab2cddb534005fbf63b2ffe5e3695e1fbcc5976865df6e27b327bee29e30ed）

## V17 处理（V16 未冻结项 → V17 修复）

| V17-RJ | 问题 | 处理 | 证据 |
| --- | --- | --- | --- |
| C01 | 非 finite 权重 → OK+NaN | integration eligibility：finite(weight)>0 强制；INVALID_INPUT 显式状态；validate_candidate_weights | V17NonFiniteWeightInvalid / V17NonFiniteSupportInvalid |
| C02 | rejection INVALID_* 被当可积分 | Stage2/ACR/CLI 只接受 OK/UNDERDETERMINED，其余 hard fail | V17InvalidMethodStatus + stage2 wiring |
| C03 | support 三套 reduction | 唯一 canonical reducer = max(accepted support)，Stage2/ACR 只消费 | integrate.h + stage2/ACR wiring |
| C04 | integration status 语义不自洽 | 显式枚举 OK/NO_CANDIDATES/ALL_REJECTED/ZERO_VALID_WEIGHT/INVALID_INPUT | V17StatusesExplicit |
| C05 | weight 命名混用 | upm.robust_control_weight.v1 vs stack.support_x_snr2.v1 / stack.equal.v1 | integrate.h + PUBLIC_API |
| W01 | wbpp_current 永久漂移 | canonical=wbpp_2_9_1；wbpp_current 仅 migration alias，诊断序列化版本化 | real16 重跑 diagnostics |
| W02 | routing/normalization 混名 | auto_policy / rejection_normalization / large_scale_policy 三政策分开 | wbpp_policy.md + schema |
| D1/D2 | 真实 reject 冒充 false reject | 受控零离群 truth 测 true FPR（1.88%，与 frozen Siril 100% 一致）；真实 16 帧只报 observed（0.54%） | controlled_rejection_metrics.json / real_rejection_metrics.json |
| E | Large-Scale 未实现 | astrocs.large_scale_rejection.v1（CC grow；min size；low/high 独立半径）实现+测试 | V17LargeScale* + E2E（satellite grown=3079，cosmic grown=0） |
| F | healpix_stack/legacy Stage2 仍在 active tree | 移入 archive/legacy/，orchestrator 接线删除，no_legacy gate | no_legacy_production_reference.py PASS |
| G | 旧 config aliases 静默迁移 | parser 删除 low/high/max_iterations/min_samples（硬错误）+ migration tool | migrate_stage2_config.py |
| H | Phase1 canonical source 未完整交付 | canonical_core 含 Phase1+Phase2+shared+Browser | 审核包 source/canonical_core |
| I | 150s/frame 未解释 | 分段 profile（Drizzle 78s 主导）+ 65s 差异解释 + warm catalogue 优化 | phase1_performance.md |
| J | stale docs | PUBLIC_API/SCIENCE_FREEZE/rejection_semantics/CONFIG_SCHEMA 修复 + machine check | api_doc_consistency.json PASS |

## Gate 状态（V17）

```text
G1 Core correctness   : PASS（74/74 synthetic gate；non-finite/invalid-status/support 单路径）
G2 Semantic IDs       : PASS（wbpp_2_9_1 / astrocs_median_*_v1 / large_scale 独立）
G3 Rejection quality  : PASS（受控 truth FPR+Siril 100% 一致；注入 recall 1.0；observed 命名）
G4 WBPP completeness  : PASS（pixel methods 完整 + Large-Scale 已实现并测试）
G5 Single path        : PASS（legacy 移出 active tree；no_legacy PASS；duplicate=0）
G6 Config/API         : PASS（old aliases 删除；migration tool；machine consistency）
G7 Phase1 audit       : PASS（canonical_core 完整 + call graph + 无隐藏路径）
G8 Performance        : PASS/见 phase1_performance.md（3+ runs before/after）
G9 Docs               : PASS（无 stale freeze/API/minmax；support/weight 语义 exact）
G10 Round0-6          : RUNNING（self_review/ V17 六轮；Round6 clean-tree 终验后置 PASS）
```

## 如实标注

- `PIXINSIGHT_EXACT_COMPATIBILITY = NOT_CLAIMED`；
- `ASTROCS_FOUNDATION_FINAL_FREEZE`：G10 完成前为 **CANDIDATE**（本文件
  Round6 终验通过后翻转 PASS，与 SCIENCE_FREEZE.md 同步）；
- 真实 16 帧只报 `observed_sample_rejection_rate`（0.54%）与
  `observed_pixel_any_rejection_rate`（6.58%），不叫 false reject；
- V16 曾报 9.45%（CLI 未 honor normalization 的测量口径 bug），V17 修复
  后与生产语义一致；
- Phase1 ≈150 s/frame 的 65s 差异已解释（Drizzle 计数/数据集差异），
  性能基线在 G8 3-runs 后更新。

## 关键交付

- integration/rejection 最后 correctness 清零（C01-C05 + INVALID_* 契约）；
- 受控 clean rejection truth（true FPR / 星点 / PSF / 结构 / 噪声效率 /
  satellite/cosmic/streak recall）；
- astrocs.large_scale_rejection.v1（实现 + 5 单元测试 + E2E）；
- legacy 多路径/旧 config aliases 彻底移除；
- Phase1 分段 profile + warm catalogue 优化；
- docs/API/config machine 一致性 + Round0-6 增强自审。
