# B5｜Export 目标 WCS、支持面积与产品语义审查（2026-11-11）

> 受检代码：`src/projection/export_projection.cpp`、`src/workflows/fits_writer.cpp`、`src/workflows/export_workflow.cpp`、`src/workflows/product*.cpp`。基线 `4e92cddab`，本轮修复提交见页末。未改任何真值/容差。

## 1. 定义与代码对应

任务定义的 \(b_{ip}=\Omega(E_i\cap H_i\cap U_p)\) 由 `compute_overlap(target=U_p, order=leaf order, pixfrac=1)` 的 `overlap_area` 给出；目标像元从不二次收缩。三种请求：

| 请求 | 代码系数 | 单位 |
|---|---|---|
| `supported_mean` | \(b_{ip}/\sum_i b_{ip}\) | 无量纲（面亮度均值） |
| `supported_flux` | \(b_{ip}\) | sr（积分量） |
| `extrapolated_full_flux` | \(\Omega(E_i)b_{ip}/\sum_i b_{ip}\)，且必须 `assume_uniform_uncovered` | sr |

`Q` 是 CSR 行算子（行=目标像元，列=叶），同时用于 signal、response 与 covariance：`export_estimate->noise = compose(Q, noise->influence)`，再改写 `input_output.domain/codomain` 为目标像元索引、`variance_unit = signal_unit²`。同一叶被多个像元引用时，Q 的共享列在协方差里保留，不拆成独立像元。

**本轮修正（关键）**：旧代码取 `overlap_area.lower_sr` 作为点系数，系统性把每个导出通量压低。Q 是点算子，现取认证区间的中点 `0.5(lower+upper)`；区间仍保留给数值界。

## 2. 失败状态

- 无支持像元：`pixel_supported=0`，导出 signal 写 NaN（不是 0）。代码在写出前逐像素 `if (!pixel_supported) values=NaN`。
- 负信号/零信号：不 clamp、不 abs、不重归一。
- 非 `tolerance_met` 的几何：按 `work/precision/unsupported` 具名失败，不发布近似 payload。
- `extrapolated_full_flux` 无 `assume_uniform_uncovered`：`invalid_request`。
- 输入 noise 不可重开：`estimate.noise` 为 null 且 issue 保留；导出 noise 为 unknown，不填零。
- 输出 dtype 仅 f64/f32；其他 `unsupported`。

## 3. 外部 FITS/WCS 核验（Astropy 7.0.1）

对 2 帧全协方差链的导出 `signal.fits`：

```
shape (128,128), dtype >f8
BUNIT='ADU', CTYPE1='RA---TAN', CTYPE2='DEC--TAN'
CRPIX=(64.5,64.5), CRVAL=(83.82208,-5.39111)
CD=diag(-2.777778e-4, +2.777778e-4)
WCS.has_celestial=True; 角点/中心 all_pix2world 有限
all_world2pix 往返最大 6.75e-11 pixel
data finite=16384, NaN=0, min=2279.258470995607, max=2279.2591292967427
```

输入帧约 2800 ADU/pixel；导出 `supported_flux` 约 2279 ADU（背景扣除后），量级合理。**这是本轮才发现并修正的**：此前 `apply_scale` 使用绝对 \(1/a\)，产品 signal 达 \(5\times10^{29}\) ADU/sr，导出约 \(6\times10^{19}\)；现使用相对标度 gauge，normalize signal 约 \(8.8\times10^{13}\) ADU/sr。

BUNIT 只写标准单位；内部 scale 进 `ACSUNIT`，非标准时再写 `ACSSTD='nonstandard_model_unit'`（`fits_writer.cpp`）。properties 同步 `hips_unit/acsunit/acsunit_standard`。

## 4. 新进程重开与协方差

- 2 帧全协方差链：normalize 生成 full 产品（含 `LinearAction/NoiseRoot/NoiseModel/CovarianceAction`），新进程 mosaic 重开并应用 sky，新进程 export 重开 mosaic 协方差并通过 Q；均退出 0。
- `validation/product/roundtrip_probe.cpp`：发布 full covariance 产品，重开后 `C·1=4·1`。
- 旧失败 `the action cannot be rebuilt`、`the signal cannot be reopened` 已消失。

## 5. 仍缺（不得写 PASS）

- 未用外部独立解析球面面积/高精度积分核验 `b_ip` 本身；当前只核验了 FITS 头/量级/重开链。2×2/3×3 非对称 CRPIX、旋转/镜像非对角 CD、跨 RA/12 face、极细支持、高阶目标像元、重复 pixfrac 的专项反例尚未跑。
- `supported_mean`/`supported_flux` 在部分支持像素上的分母语义、`effective_area_approx` 的近似误差未与外部真值比较。
- HiPS 层级“不是独立曝光”只在文档语义，无自动检查。
- `export_science.json` 的 Q/identity 未做产品级逆向追踪（Q 目前通过 covariance influence 间接保存）。
- 绝对测光标度仍不可辨识；`signal_only` 与 `full` 都只在相对标度下成立。

## 6. 裁决

Export 的 Q 定义、NaN/负/零语义、单位与 full 协方差重开已可运行并有外部 FITS 检查；但 B5 要求的独立面积真值与全部边界反例未完成，**B5 不能标 PASS**。本轮修正了两个会直接导致产品量级错误的缺陷（下界系数、绝对标度 gauge），并把协方差链首次跑通 normalize→mosaic→export。
