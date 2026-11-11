# M42 阶段一证据包（Issue #123）

> 执行记录与裁决见 [docs/audit/m42-phase1-2026-11-12.md](../../docs/audit/m42-phase1-2026-11-12.md)。
> **状态：阶段一未完成。** 正式 Normalize→Mosaic→Export 产品未产出；本目录交付的是一个可打开的、真实 M42 数据定标的诊断 FITS，并保留失败证据。

## 交付物

- [M42-S1_Halpha_256_calibrated.fits](deliverable/M42-S1_Halpha_256_calibrated.fits)：
  256×256 float32，BUNIT `ADU`，TAN+SIP（Gaia 独立解），NaN 32.8%（饱和核心）。
  SHA256 `321378161ae1395a0f34f1fa662e80c149dba75099ab4a00efa1ab3e2bd749f0`。
- [M42-S1_Halpha_256_calibrated.png](deliverable/M42-S1_Halpha_256_calibrated.png)：5–99.5% 线性拉伸预览。

该 FITS 是 T2/M5 2025-12-12 单帧 Hα 600 s、按 `x=(raw-bias-(dark-bias))/(flat/flat_normalization)` 定标后的结果；**不是**正式 Mosaic/Export 产品，不含正式 supported_mean/resampling/协方差。

## 输入与证据

| 文件 | 内容 |
|---|---|
| `inputs_manifest.json` | 196 FITS 只读 inventory（path/size/SHA256/头/中心 256² 读探测）与 3 帧冻结块 |
| `space.md` | 输入/可用空间、order 13/14 预算 |
| `calibration.json` | XISF master 解码细节、ADU 语义、公式与数值检查 |
| `response_ha.json` | 参数化 Astrodon 3 nm Hα top-hat（绝对 QE/透过率 unknown） |
| `astrometry/` | 3 帧 Gaia TAN+SIP 解、留出残差、独立 C++/astropy 对拍、互检 |
| `science/lowbg_photometry.json` | 低背景 128² 天区 4 颗共同星的孔径测光与两夜比值 |
| `science/metrics.json` | 诊断 FITS 读回、定标统计、整幅差/帧统计（核心区，仅诊断） |

## 关键真实指标

- 天测留出 RMS：0.307″ / 0.327″ / 0.394″（3 帧）；头部 PinPoint WCS 偏 5.2–5.7″。
- 低背景孔径测光：SNR 110–1641；4 颗共同星 frame2/frame1 通量比中位 **1.2766**，星间 scatter ~4% — 两夜 ~28% 相对零点差，必须在正式 mosaic 前处理。
- 物理 gain/read noise/SATURATE/QE 缺失；噪声与绝对定标保持 unknown，不以孔径 IVAR 冒充 scene variance。

## 未闭合（不得宣称通过）

1. Normalize exit 0 但 `signal` shape [122]、`support.index.size=122`、HiPS tiles 全 NaN，产品内容不自洽。
2. 256² 单/双帧 Mosaic 达 11.6–12.6 GiB，超 12 GiB 账本/17 GiB 主机。
3. Export 因 `LinearAction` 引用未存而 `corrupt_product`。
4. 亮星云单窗口常数背景使 joint photometry `nonconverged`。
5. 两夜 28% 相对零点差、master 时效/光路差未校正。

阶段一在这些项闭环并通过独立审核前不得推进阶段二。

## 复现路径（本机）

原始数据只读：`testdata/M42_T2T3_mosaic_Flying_dutchman`；主帧 `testdata/T2 calibration files`；Gaia `gaia/GaiaDR3{,SP}`。
运行配置与 trace 在 `run/m42-s1/`（大 FITS/中间产品不入 git）。
