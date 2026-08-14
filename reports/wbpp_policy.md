# WBPP Auto 政策（V15 核准，profile=wbpp_current）

## 本机安装证据（source provenance）

| 项 | 值 |
| --- | --- |
| PixInsight 安装目录 | `C:\Program Files\PixInsight` |
| PCL | 2.9.4（`include/pcl/Version.h`，Released 2025-03-31） |
| WBPP | 2.9.1（`src/scripts/BatchPreprocessing/BPP-defines.jsh`，Released 2026-01-16T11:44:34Z） |
| Auto 路由源码 | `BPP-FrameGroup.js` `bestRejectionMethod()` |
| Auto 解析时机 | `BPP-processing.js` `doIntegrate`（integration group 层，一次） |

源码 SHA-256（本次核验）：

```text
WBPP.js        : 见 evidence/wbpp_source_provenance.json
BPP-FrameGroup.js : 见 evidence/wbpp_source_provenance.json
BPP-processing.js : 见 evidence/wbpp_source_provenance.json
```

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

`nominal contributors` = 该 integration cohort / tile 几何上可贡献的独立
exposure 数（stage2 使用 tile 覆盖帧数 depth），**不是** pixel effective
count。Auto 在 planning 层解析一次（stage2 per-tile），pixel loop 只执行
显式方法（`p2_reject_plan_resolve`）。

## V14 旧实现 vs V15

| 项 | V14（错误/不完整） | V15 |
| --- | --- | --- |
| auto 解析位置 | kernel 内按每像素 effective count 路由（n<3 none / 3-5 winsorized / 6-10 averaged / >10 linear_fit） | planning 层按 nominal contributors（WBPP 2.9.1） |
| production 默认 | winsorized_sigma | auto + profile=wbpp_current |
| 像素不足处理 | 静默换算法 | UNDERDETERMINED（记录，不换算法） |

## PIXINSIGHT 兼容性声明

`PIXINSIGHT_EXACT_COMPATIBILITY = NOT_CLAIMED`：AstroCS 不宣称与
PixInsight 内核 bit-exact；WBPP profile 只负责 Auto routing 政策与参数
映射，算法实现为 AstroCS canonical semantic IDs（各带独立 oracle）。
