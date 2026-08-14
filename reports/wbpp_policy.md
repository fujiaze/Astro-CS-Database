# WBPP Auto 政策（V17 True Final Freeze，profile=wbpp_2_9_1）

## 本机安装证据（source provenance）

| 项 | 值 |
| --- | --- |
| PixInsight 安装目录 | `C:\Program Files\PixInsight` |
| PCL | 2.9.4（`include/pcl/Version.h`，Released 2025-03-31） |
| WBPP | 2.9.1（`src/scripts/BatchPreprocessing/BPP-defines.jsh`，Released 2026-01-16T11:44:34Z） |
| Auto 路由源码 | `BPP-FrameGroup.js` `bestRejectionMethod()` |
| Auto 解析时机 | `BPP-processing.js` `doIntegrate`（integration group 层，一次） |

源码 SHA-256（本次核验）：见 evidence/wbpp_source_provenance.json。

## WBPP 2.9.1 Auto 路由（逐字源码语义）

```js
// BPP-FrameGroup.js
this.bestRejectionMethod = function() {
   let n = this.activeFrames().length;
   if ( n < 6 ) return PercentileClip;
   if ( n <= 15 || BIAS || DARK ) return WinsorizedSigmaClip;
   return LinearFit;
};
```

即：

```text
nominal contributors < 6            -> percentile（astrocs.percentile_siril.v1）
6 <= nominal contributors <= 15     -> winsorized_sigma（astrocs.winsorized_sigma_siril_1_4_3.v1）
nominal contributors > 15           -> linear_fit（astrocs.linear_fit_siril_1_4_3.v1）
```

`nominal contributors` = integration group active independent exposure 数
（stage2 用 cfg.hips.size()，group-level 一次解析），不是 pixel effective
count。Auto 在 planning 层解析一次，pixel loop 只执行显式方法
（`p2_reject_plan_resolve`）。

## V17 版本化 profile（不再使用 wbpp_current 作为 canonical）

```text
wbpp_2_9_1      = FROZEN canonical（WBPP 2.9.1 路由 + WBPP Light 参数默认：
                  linearFit 5.0/3.5、percentile 0.2/0.1）
wbpp_current    = migration alias（parser 规范化到 wbpp_2_9_1；最终
                  diagnostics/manifest 永远序列化 wbpp_2_9_1，防止随
                  用户以后安装的 WBPP 版本漂移）
astrocs_adaptive= AstroCS 自有策略（tile nominal geometric depth 自适应；
                  独立命名，不冒充 WBPP exact）
```

## 三项政策分开（不再混成一个 "WBPP profile"）

```text
auto_policy            = wbpp_2_9_1（路由）
rejection_normalization = astrocs_median_center_v1（判定工作域；decision
                          作用于 working stack，mask 应用回原始科学值）
large_scale_policy     = astrocs.large_scale_rejection.v1（默认关闭；
                          connected-component grow；min structure size；
                          low/high 独立半径）
```

## Feature matrix（V17 结论）

```text
WBPP_AUTO_ROUTING               = SUPPORTED（wbpp_2_9_1，group 一次解析）
WBPP_PIXEL_REJECTION_METHODS    = SUPPORTED/MAPPED（percentile /
                                  winsorized / linear_fit / sigma /
                                  averaged / ESD / median_sigma / minmax）
WBPP_LARGE_SCALE_REJECTION      = SUPPORTED（astrocs.large_scale_rejection.v1，
                                  默认关闭 = WBPP largeScaleClip 默认 off）
WBPP rejectionNormalization     = MAPPED（Scale → astrocs_median_center_v1）
PIXINSIGHT_EXACT_COMPATIBILITY  = NOT_CLAIMED
```

## 生产证据（V17 重跑）

真实 16 帧队列（NGC1727 H-alpha）wbpp_2_9_1 诊断：
`rejection_resolved_methods={"16":"astrocs.linear_fit_siril_1_4_3.v1"}`——
全 run 单次解析，tile 不重选；normalization 序列化
`astrocs_median_center_v1`。受控 20 帧 truth 同源 case 与 frozen
Siril 1.4.3 LinearFit harness 100% 一致。
