# 数据来源与产品布局清单（DATA-SOURCES）

只读。全部路径相对仓库根；本单元不写任何产品目录。

## 1. 帧产品（`run/RELEASE-05/vis/out/m42_p1_t2`、`m42_p1_t3`）

| 文件 | 用到的字段 |
|---|---|
| `p1_phot.json` | `frames[].frame_key/status/photometry_applied`；`photscale_detail[<fk>].k_photo/n_matched/n_psf_domain/n_psf_skipped/sigma_residual_dex/source`；`photscale_fit[<fk>].zero_point_mag/zero_point_n_stars/zero_point_scatter_mag`；`photscale_spread_dex/spread_warn/spread_gate/photscale_source/pixel_scaling/photscal` |
| `p1_snr.json` | `frames[].file/background`（帧级经验天光，ADU；文件名用 `@` 分隔时刻） |
| `p1_flux.json` | `frames[].results[]`（`id/x/y/flux/flux_error/snr/background/valid/failure_reason`）；逐帧测光结果数 380–1463（T2 501–1418 / T3 379–1463）；`background` p05/p50/p95 = 156.2/197.4/640.9 ADU（T2）、166.6/205.7/440.3（T3）；`snr` p05/p50/p95 = 20.0/70.9/596.2（T2）、18.2/61.6/519.4（T3） |
| `p1_stack.json` | `n_healpix_pixels=23928505`、`hp_res_arcsec=0.8051921277697045`、`oversample_factor=1.1678579067744754`、`pixfrac=1.0`、`photappl=1`、`bunit=ASTROCS_RELATIVE_FLUX` |
| `p1_wcs.json` | `cd11=4.5538533590127475e-06`、`cd12=2.686040426927428e-04`、`cd21=−2.6854517634069836e-04`、`cd22=4.683582346738102e-06`、`crpix=(2048.5,2048.5)`、SIP order 3 / ap 5、`rms_arcsec=0.11010627041089632` ⇒ 0.9670115215027271″/px |
| `<fk>/signal/Norder9/Dir<d>/Npix<ipix>.fits` | float32，512×512；Norder9 ⇒ `nside = 2^18 = 262144`，`A_cell = 4π/(12·nside²) = 1.5238729992366245e-11 sr` |
| `<fk>/support/...` | float32，`support = D_p/A_cell`，clamp 到 [0,1] |
| `<fk>/variance/...` | float32；中位 `1.047e9`、`corr(signal,variance)=0.0040` |
| `<fk>/ivar/...` | float32；`ivar·variance = 1.0` 精确成立 |

**HiPS Dir 命名**：`Dir<floor(ipix/10000)*10000>`（IVOA HiPS 1.0）。
**tile 内序**：FITS 序 `fits_index = (511 − x)·512 + y`，`(x,y) = nest_to_xy(local, 9)`。

## 2. 马赛克产品（`run/RELEASE-05/vis/out/m42_p2`）

| 文件 | 规模 / 关键字段 |
|---|---|
| `p2_samples.json` | 145 MB；`controls` 33,472 项（`leaf_ipix/ra_deg/dec_deg`，每 tile 64 个）；`observations` 277,255；`stats`：accepted 277,255 / candidate 358,976；`sampler_config`：grid 8、`k_corr` 1.4、`patch_radius_leaf` 2 |
| `p2_coverage.json` | `n_union_cells = 523` |
| `p2_rejection.json` | `n_pixels = 137,101,312`；`tiles` 523 项（`tile_ipix/offset/depth/frame_slots/sample_mask_offset`）；`stats`：`accepted_pixels=137101312`、`rejected_high=13553303`、`rejected_low=3776275`、`rejected_samples=17329578`、`underdetermined_pixels=1265704`；`plan`：method 4、`minimum_n=4`、`nominal_n=49`、`underdetermined_n=3`；`low_n_policy="underdetermined_no_rejection"`、`geometric_n_source="frame_support_gt0"`；`files` 指向四个二进制 |
| `p2_integrated.json` | `n_pixels`、`weight_basis="per_sample_ivar"`、`nrej_total=17329578`、`tiles`（`tile_ipix/offset/n_pixels`） |
| `p2_integrated_signal.bin` | fp64，1,096,810,496 B = 137,101,312 × 8 |
| `p2_integrated_nused.bin` / `_nrej.bin` | int32，548,405,248 B each |
| `p2_rejection_candidates.bin` / `_nrej.bin` | uint16，274,202,624 B each |
| `p2_rejection_accepted.bin` | uint8，137,101,312 B |
| `p2_rejection_sample_mask.bin` | uint8，**1,470,365,696 B = 262,144 × 5,609**（`Σ_tiles depth = 5609`）；按 `tile_index·262144 + slot·262144 + p` 寻址 |
| `p2_corrected_<hash>.bin` | fp64，每帧 `n_tiles × 262,144 × 8`（如 241,172,480 B ⇒ 115 tile）；**FITS 序**（实测：与 `p2_integrated_signal` 相关系数 +0.96…+0.99；NESTED-local 序 ≈0） |
| `p2_corrected_var_<hash>.bin` | fp64，同上布局 |
| `p2_corrected.json` | `frames[]`（`data_file/var_file/frame_id/hips_path/n_tiles/tiles[{tile_ipix,offset}]`）；顶层加性语义：`additive_mode_effective="delta"`、`delta_subtracted=true`、`c_subtracted=false`、`additive_combination="raw_minus_delta"`、`sky_plane_mode="delta_to_B_ref"` |
| `p2_sky_plane.bin` | 8,464 B |
| `p2_upm_model.json` / `.bin` | 805 B / 26,981,932 B（`final_gauge=1`、`m_full_frame=1`） |
| `p2_final.json` | verdict FAIL；`covered_but_nonfinite_px = 466,515`（2.7806%）；`seam_ratio = 1.0096080530220828`（门 1.5）；7 列 + 7 行 seam |
| `p2_integrated_nused.json` 等 | 见上 |

## 3. 导出产品（`run/RELEASE-05/vis/out/m42_p3`）

| 文件 | 关键量 |
|---|---|
| `output_phase3.fits` | 4096×4096，4 HDU：PRIMARY(信号)/COVERAGE/VARIANCE/IVAR；有限像素占比 0.96900 |
| `p3_verify.json` / `p3_wcs.json` | 0.0005 deg/px = 1.8″/px，TAN |
| `vis_report.json` | `seam_ratio=1.0096080530220828`、`max_seam_ratio=1.5`、`seam_detail`（`seam_v=2.0038933143951e-06`、`inner_v=1.984823029488325e-06`、`seam_h=1.938373316079378e-06`、`inner_h=1.9610852177720517e-06`、7 列 + 7 行）、verdict FAIL、findings V1b、`coverage_crosscheck`（`covered_fraction=0.9966769814491272`、`phantom_data_px=0`、`covered_but_nonfinite_px=466515`） |

## 4. 几何与规模事实

- M42 天区：`dec ∈ [−7.0209°, −3.8168°]`，`|z| ∈ [0.06657, 0.12223]`；
  到最近极点 82.898°、到最近 `|z|=2/3` 接缝圆 34.708°、到 `z=0` 线 3.736°。
- 帧像元 0.9670″/px ⇒ `A_drop = 2.1979e-11 sr`；导出像元 1.8″/px ⇒ `7.6154e-11 sr`；
  `hp_res = 0.805192″`。
- 帧覆盖：每帧 112–119 个 Norder9 tile；49 帧 union = 523 tile；
  逐 leaf 覆盖重数中位 8、均值 8.324、max 33。
- 层级面积守恒：order 0..9 上 `Σ support·A_cell` 相对差 8.754e-09（`eps_fp32` 的 7.3%），
  order0 总面积 3.644293e-04 sr = 2.391468e+07 叶面积。
