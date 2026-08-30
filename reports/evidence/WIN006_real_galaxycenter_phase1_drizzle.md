# WIN-006 真实数据代表链路 — 银心(Phase1+Phase2 里程碑) — 当前SHA

> 状态: **IN_PROGRESS**。真实银心数据 `phase1`(6帧) + `drizzle`(→真实 HiPS) 均 PASS; `phase2` UPM 构建 blocker(obs=0)。WIN-006 仍**未** PASS(phase2/3 链未达)。

## 1. 机器(Windows, 当前SHA b842899)
Fatduck `astrocs.exe` `0.9.0-alpha.1+gb842899eb8fb`(doctor PASS, 内置生产 Drizzle→HiPS 直链)。数据: `F:\Astro dev\Astro CS Normalization Database\testdata\Galaxy_Center_T4\`(银心 mosaic 三板块, `*-180S-Red.fts`, 4500×3600) + `T4 calibration files` 母版(xisf, **4500×3600**, 非 4096×4096)。

## 2. phase1 真实校准 PASS
`astrocs phase1 run --config <win006_gc/phase1_cfg.json> --events-jsonl`:
- 6 帧(panel1/2/3 各前 2 张 R 帧) + masterBias + masterDark(180s) + masterFlat(Red), 均 4500×3600。
- `stage calibrate ok: 6 frames`, **exit_code=0**, run_id `df75ccc1d650`, manifest sha256 `cc01f225c0cd60d686d56a4ff3ce0a01fdb05f75a9679b8e7e2c365a5598d0c0`。

## 3. phase1→drizzle→真实银心 HiPS PASS(核心里程碑)
`astrocs drizzle --config <win006_gc/drizzle_cfg.json> --nside 2048 --pixfrac 0.9`:
- `{"drizzle":"ok","frames":6,"hips_dir":".../win006_gc/hips","n_healpix_pixels":366781,"n_source_pixels":97200000}` **exit=0**。
- 完整 IVOA HiPS 产物: `signal/`(metadata.fits, Moc.fits, properties, Norder0/Npix7, Norder1/Npix28, Norder2/Npix112/113/115…) + `support/`(metadata.fits, Moc.fits, properties, Norder0/Npix7, Norder1/Npix28…) + `manifest.json` + `operation_counts.json`。
- **来自真实银心数据的生产链产物**(非合成), 证明 production Drizzle→HiPS 直链在真实数据上工作。

## 4. phase2 UPM 构建 BLOCKER
`astrocs phase2 run --config <win006_gc/phase2_cfg.json>` (hips_paths=['.../win006_gc/hips'], output_dir='.../phase2_out'):
- sampler 日志: `enter n_union=2 grid=8 n_frames=1 target_order=2`, `first tile 112 probe`, 但 **`obs=0 overlap_controls=0`**。
- `upm_build rc=1` → `phase2_failed`(exit_code=3)。

### 根因分析(已定位到 sampler)
`obs=0` 源于 sampler(phase2/src/sampler.cpp)经 `p2_frame_id(hips_path)`(line 316)用 `aio_hips_open(hips_path, AIO_HIPS_RD_SIGNAL)` + `aio_hips_get_properties` 读 **信号 properties**, 取 `creator_did/obs_title/obs_filter/obs_exptime/obs_date/hips_order/hips_pixel_scale/moc_sky_fraction` 等键。
**实测发现(已确认)**:
- drizzle 把这些 obs_* 键写到 **`support/properties`**(creator_did=ivo://astrocs/phase1, obs_title=AstroCS Phase1, hips_order=2, hips_pixel_scale=103.06, moc_sky_fraction=0.010417, hips_release_date=2026-08-30 …)。
- 但 **`signal/properties` 只有** hips_pixel_scale/hips_initial_fov/moc_sky_fraction/astrocs_* /hips_data_range, **缺失 creator_did/obs_title/obs_filter/obs_exptime/obs_date/hips_order/hips_release_date**。
- 而 `p2_frame_id` 读的是 **signal** properties(缺这些键) → 帧观测身份为空 → sample `obs=0`。
**这就是 drizzle→phase2 观测链路 metadata 不匹配的根因**(不是基础设施失败, 是 properties 写入/读取端不匹配)。

### 修复方向(win-006/008)
- **让 drizzle 把 obs_* 观测 provenance 同时写入 `signal/properties`**(aio_hips_writer/drizzle 的 properties 段), 使 `p2_frame_id` 读 signal properties 能取到帧观测身份; 或
- 让 phase2/sampler 改读 `support/properties`(已含 obs_*); 或
- 以逐帧单独 HiPS 作为 phase2 多个 `hips_paths` 输入; 或
- 明确 phase2 `upm`/投影/STF 配置后重试。

> 现状: phase1(6帧, exit0) + drizzle(→366781 HEALPix→完整HiPS, exit0) **PASS**; phase2 UPM(obs=0) **blocker**, 记录不宣称 PASS。

## 5. 结论
- production 链在真实银心数据上 phase1+drizzle 全通(核心场景达成)。
- phase2 UPM(obs=0)为真实 blocker, 留在 WIN-006/WIN-008 范畴; 记录不宣称 PASS, 不宣称 release。
