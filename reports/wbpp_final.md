# WBPP Final（V17 True Final Freeze）

## 冻结政策

```text
auto_policy              = wbpp_2_9_1（WBPP 2.9.1 bestRejectionMethod：
                           nominal<6 → percentile；6..15 → winsorized；
                           >15 → linear_fit；integration-group 层一次解析）
rejection_normalization  = astrocs_median_center_v1（判定工作域；mask 回
                           原始科学值；percentile 强制；rcr 强制 none）
large_scale_policy       = astrocs.large_scale_rejection.v1（默认关闭；
                           min_structure_pixels=8；low/high grow=2；
                           connected-component grow；非 PixInsight exact）
```

`wbpp_current` 仅为 migration alias：parser 规范化到 wbpp_2_9_1，
diagnostics/manifest 永远序列化版本化 ID（防止随用户后续安装 WBPP
版本漂移）。

## Feature matrix（V17 结论）

| feature | 状态 |
| --- | --- |
| WBPP_AUTO_ROUTING | SUPPORTED（wbpp_2_9_1，group 一次解析） |
| WBPP_PIXEL_REJECTION_METHODS | SUPPORTED/MAPPED（percentile / winsorized / linear_fit / sigma / averaged / ESD / median_sigma / minmax） |
| WBPP_LARGE_SCALE_REJECTION | SUPPORTED（astrocs.large_scale_rejection.v1；默认 off = WBPP largeScaleClip 默认一致） |
| WBPP rejectionNormalization | MAPPED（Scale → astrocs_median_center_v1） |
| PIXINSIGHT_EXACT_COMPATIBILITY | NOT_CLAIMED |

## 证据

- 本机 WBPP 2.9.1 源码 provenance（BPP-FrameGroup.js bestRejectionMethod /
  BPP-processing.js doIntegrate）见 evidence/wbpp_policy.json；
- 真实 16 帧 V17 重跑诊断：`rejection_resolved_methods={"16":
  "astrocs.linear_fit_siril_1_4_3.v1"}`，profile=wbpp_2_9_1，
  normalization=astrocs_median_center_v1；
- 受控 20 帧同源 case 与 frozen Siril 1.4.3 LinearFit harness 100% 一致。

```text
WBPP_FINAL = FROZEN（版本化；PIXINSIGHT_EXACT=NOT_CLAIMED）
```
