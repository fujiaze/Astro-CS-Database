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
`obs=0` 源于 sampler(phase2/src/sampler.cpp)经 `p2_frame_id(hips_path)`(line 316)从**每个 HiPS 的 `properties`/provenance** 构建 observation/frame。obs=0 说明 drizzle 产出的 HiPS **未携带 sampler 所需的逐帧观测/provenance 元数据**(`frame_drizzle_provenance` line 115 读 properties 时拿不到帧 WCS/观测信息)。属 **drizzle→phase2 观测链路线缺**: phase2 期待每观测一个 HiPS(含 properties 帧信息), 而 `drizzle` 合并输出单一 HiPS, 其 properties 未逐帧标记观测。 属 WIN-008 科学验证范畴, 需:
- 让 drizzle 输出的 HiPS 携带逐帧观测 provenance(properties 写 帧WCS/obs)，或
- 以单帧 HiPS 作为 phase2 的多个 `hips_paths` 输入, 或
- 明确 phase2 `upm`/投影/STF 配置后重试。

## 5. 结论
- production 链在真实银心数据上 phase1+drizzle 全通(核心场景达成)。
- phase2 UPM(obs=0)为真实 blocker, 留在 WIN-006/WIN-008 范畴; 记录不宣称 PASS, 不宣称 release。
