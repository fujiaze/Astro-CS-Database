# WBPP / ImageIntegration Feature Matrix（V17 True Final Freeze）

来源：本机 PixInsight `C:\Program Files\PixInsight\src\scripts\BatchPreprocessing\`
（WBPP 2.9.1 / PCL 2.9.4，Released 2026-01-16T11:44:34Z）。

## Light integration 关键源码证据（BPP-processing.js doIntegrate）

```js
case ImageType.LIGHT:
   II.normalization = AdditiveWithScaling;                    // 组合前归一化
   II.rejectionNormalization = Scale;                         // 判定域归一化
   II.weightScale = BWMV;
   II.winsorizationCutoff = 5.0;
   II.clipLow = true; II.clipHigh = true;
   II.largeScaleClipLow = false; II.largeScaleClipHigh = false; // 默认关
   II.subtractPedestals = true; II.truncateOnOutOfRange = true;
```

Light 方法默认参数（BPP-parameters.js 740-748）：
`percentileLow=0.2 / percentileHigh=0.1 / sigmaLow=4.0 / sigmaHigh=3.0 /
linearFitLow=5.0 / linearFitHigh=3.5 / ESD_Outliers=0.3 /
ESD_Significance=0.05 / RCR_Limit=0.1`。

## Feature Matrix

| feature | WBPP source evidence | AstroCS status | semantic mapping | test |
| --- | --- | --- | --- | --- |
| Auto routing | bestRejectionMethod()（n<6 percentile / 6-15 winsorized / >15 linear_fit） | exact（planning 层） | p2_reject_plan_resolve | V15AutoPlanResolvesByNominal + oracle auto-policy |
| Auto scope | doIntegrate() group 层一次 | exact（wbpp_2_9_1 group-level once；wbpp_current 仅 migration alias） | stage2 group_plan | real16 E2E diagnostics nominal=16→linear_fit once |
| rejection normalization | rejectionNormalization=Scale | mapped（astrocs_median_center_v1 per-pixel；UPM 已对齐帧，PIXINSIGHT_EXACT=NOT_CLAIMED） | plan.normalization | V16NormalizationTransparentAndNegativeSafe |
| combination normalization | normalization=AdditiveWithScaling | mapped（Phase2 加权积分在原始科学值；UPM 承担校准） | integrate.cpp | R5 weight truth gate |
| percentile defaults | pcClipLow=0.2 pcClipHigh=0.1 | exact（config 默认 0.2/0.1） | pct_low/high_fraction | config_consistency PASS |
| sigma defaults | sigmaLow=4.0 sigmaHigh=3.0 | exact | sigma.lower/upper | oracle sigma_vs_astropy |
| linear fit defaults | linearFitLow=5.0 linearFitHigh=3.5 | exact（V16 从 4/3 修正） | linear_fit.lower/upper | linear_fit_oracle（Siril harness） |
| winsorization cutoff | winsorizationCutoff=5.0 | mapped（Siril 1.4.3 winsorize 1.134 迭代；cutoff 语义并入 winsorized） | winsorized 实现 | G6WinsorizedDiffersFromSigma |
| ESD params | ESD_Outliers=0.3 ESD_Significance=0.05 | mapped（alpha=0.05；max_outliers 绝对数而非分数） | esd.alpha/max_outliers | G6EsdNistRosner54 |
| RCR | RCR_Limit=0.1 | mapped（SS_MEDIAN_DL 冻结链；limit 语义由官方链承担） | rcr.technique | rcr_oracle_compare exact |
| clipLow/High | true | exact（低/高阈值拒绝） | rejected_low/high | V15LowHighThresholdSemantics |
| large-scale rejection | largeScaleClipHigh/Low=false（默认关）；growth/layers 参数存在 | supported（astrocs.large_scale_rejection.v1：connected-component grow，min structure size，low/high 独立半径；默认关=与 WBPP 默认一致；非 PixInsight exact） | rejection.large_scale | V17LargeScaleGrowsTrailNotCosmic / E2E（satellite grown=3079，cosmic grown=0） |
| subtractPedestals | true | mapped（UPM 零锚/平滑承担） | upm zero_anchor | V13/14 UPM gates |
| weight scale | WeightScale_BWMV | mapped（support×snr² 冻结权重） | weight_mode | R5 weight truth gate |

## 结论

- Auto routing + 默认参数与 WBPP 2.9.1 一致；
- rejection normalization / large-scale / BWMV 为 mapped（AstroCS 语义或
  默认等价），`PIXINSIGHT_EXACT_COMPATIBILITY = NOT_CLAIMED`；
- large-scale rejection 已实现为 astrocs.large_scale_rejection.v1
  （默认关闭 = WBPP largeScaleClip 默认一致；启用后细/宽卫星线、飞机线
  结构被 grow，compact cosmic/星点不生长）；仍不宣称 PixInsight exact。
