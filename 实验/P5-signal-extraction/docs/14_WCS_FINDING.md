# 独立发现：6 帧的 `cleaned_*.fts` 缺 WCS 关键字（阶段一产出缺口）

## 1 现象与逐帧名单

对 `run/PHOT-FIX/p1_rerun/{t2,t3}/` 下全部 **49 帧** 的 `cleaned_*.fts` 逐帧读头，**43 帧有 WCS 块、6 帧没有**：

| # | 帧（`cleaned_` 去掉前后缀） | 目录 | `p1_wcs.json` | 头里 `CRVAL1` | 头键数 |
|---|---|---|---|---|---|
| 1 | `M42_M4_T2_flying_dutchman-20251216@050117-300S-Red` | t2 | **存在** | 无 | 67 |
| 2 | `M42_M4_T2_flying_dutchman-20251224@045919-300S-Red` | t2 | **存在** | 无 | 67 |
| 3 | `M42_M4_T2_flying_dutchman-20251228@044805-300S-Red` | t2 | **存在** | 无 | 67 |
| 4 | `M42_M2_T3_flying_dutchman-20251126@044822-300S-Red` | t3 | **存在** | 无 | 67 |
| 5 | `M42_M4_T3_flying_dutchman-20251128@062504-300S-Red` | t3 | **存在** | 无 | 67 |
| 6 | `M42_M4_T3_flying_dutchman-20251228@040115-300S-Red` | t3 | **存在** | 无 | 67 |

对照：其余 43 帧头键数 **117**，含 `CRVAL1/2`、`CRPIX1/2`、`CD1_1…CD2_2`、`CTYPE1/2`、`CDELT1/2`、
`CROTA1/2`、`EQUINOX`、`EPOCH`、`PLTSOLVD`、`FWHM`、`PA`、`ZMAG` 以及 SIP 系数 `TR1_0…TR2_14`。

## 2 解算状态：**不是没解出，是没写回产物**

6 帧各自的帧产物目录（`…/<帧名，@→_>/`）**都存在且完整**（`signal/ variance/ ivar/ support/ p1_final.json
p1_stack.json p1_wcs.json manifest.json`），`p1_wcs.json` 里是一份**有效的 SIP 解**：

| 帧 | `ctype1` | `sip_order` | `n_pairs` / `best_inliers` | `rms_px` | `rms_arcsec` | `samples` 表 |
|---|---|---|---|---|---|---|
| M4_T2_1216 | RA---TAN-SIP | 3 | 37 / 37 | 0.1077 | 0.1065 | 64 条（像素↔天球，往返残差 ~1e-11 px） |
| M4_T2_1224 | RA---TAN-SIP | 3 | 35 / 35 | 0.1188 | 0.1175 | 64 条 |
| M4_T2_1228 | RA---TAN-SIP | 3 | 37 / 37 | 0.1240 | 0.1226 | 64 条 |
| M2_T3_1126 | RA---TAN-SIP | 3 | 36 / 36 | 0.1289 | 0.1275 | 64 条 |
| M4_T3_1128 | RA---TAN-SIP | 3 | 35 / 35 | 0.1063 | 0.1052 | 64 条 |
| M4_T3_1228 | RA---TAN-SIP | 3 | 35 / 35 | 0.1493 | 0.1477 | 64 条 |

`p1_products.json`（t2 16 帧 / t3 33 帧，`count_consistent=true`）为这 6 帧登记了
`hips_path`、`n_tiles`（116 / 112）、`nside=262144` 与方差审计字段，状态正常。

## 3 已排除的解释

- **不是"没解出"**：6 帧都有 SIP 解与 35–37 个内点。
- **不是陈旧残留文件**：6 帧与对照帧的 `cleaned_/calibrated_/badcol_` 记为同一时刻（10-11 22:58），
  `photoapplied_` 同为 10-11 23:06，且四类输出**全部**是 67 键。
- **不是"输入无初值"**：对照帧的 `p1_wcs.json` 同样 `initial=false`（即输入本来就没有可用初值），
  但它们写回了 WCS，所以初值有无不是判据。

⇒ 现象是**阶段一帧级 FITS 输出的写头缺口**：这 6 帧的解算结果只进了 `p1_wcs.json` 与帧 HiPS 产品，
没有进入 `calibrated/cleaned/photoapplied/badcol` 四类平面 FITS 的头。

## 4 影响与处置（待裁决）

- 影响：任何按帧头 WCS 做几何消费的下游（含本单元）会**静默丢掉这 6 帧**。本单元已按
  「缺几何的帧单独登记、不猜不补」的原则剔除并在 `results/p5w/p5w_stage1.json` 的 `inputs.skipped` 列名。
- 可用的权威补救（**本轮未采用，等裁决**）：`p1_wcs.json` 的 `samples` 表（64 条像素↔天球对应）
  或 `initial` 为空时由 `samples` 反算 TAN+SIP。这属于"读该帧自己的权威解"，不是猜；
  但派单明确要求本轮**不补 WCS**，故只登记不动手。
- 建议的根治点：阶段一写帧级平面 FITS 时，应把**本次解算得到的** WCS/SIP 块写进头，而不只是沿用输入头。
