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

### 根因分析(已定位到 sampler, 修正)
phase2 实跑 `sample ok: obs=0 overlap_controls=0`。经查 `p2_sample_controls`(sampler.cpp):
- `n_frames = coverage->n_inputs` = 1(单个合并 HiPS)。
- `p2_frame_id(path)` 返回**有效非零 frame_id**(未触发 `"frame_id 0 invalid"` 错误), 故**不是** properties 缺失直接导致 obs=0(先前记录以 frame_id 判定的观测身份是通的)。
- `obs=0` 来自**后续 cell 接受门限**: 对单个合并 HiPS 帧跑 `pass1_cell`/catalog veto/`rejected_insufficient_support`/`min_samples` 等接受判据, 该单帧 coadd 产物在采样网格里没有产生被**接受的 control observation cell**(可能因 6 帧 coadd 成一帧、SNR/support/catalog veto 不满足, 或单帧无有效邻域)。
- **关键**: phase2 的观测模型是「每观测=独立 HiPS 帧」; 把 6 帧 coadd 成单一 HiPS 后 phase2 只看到 1 个观测, 而该观测未通过采样接受判据 → obs=0, 且无 overlap_controls。

### 修复方向(win-006/008)
- **逐帧独立 HiPS 作多个 `hips_paths` 输入**(每帧=每个观测, 最符合 phase2 观测模型; 6 帧 → n_inputs=6), 或
- 逐个 cell 调高 `min_samples`/放宽 veto(需科学验证); 或
- 明确 phase2 `upm`/投影/STF 配置后重试。
> 更可能正解: **drizzle 每帧单独 HiPS + phase2 用多个 hips_paths**, 使 phase2 看到 6 个独立观测并形成 overlap_controls = 32R/接缝的前提。

### 逐帧实验(已试, 推进一档)
- 逐帧 drizzle(每帧独立 HiPS, nside=256, 6 个 `hips_paths`) → phase2 **过了 config 校验**(n_inputs=6, 不再 `obs=0`)。
- 但下一档 blocker: **`coverage_build rc=1`**(phase2 首阶段 coverage)。6 个逐帧 nside=256 HiPS 的 MOC union 失败(单帧 4500×3600 只覆盖一小块, 逐帧 MOC 过小/分布散, coverage_build 无法 union)。
- 说明: 逐帧结构解决了「观测模型=每帧一 HiPS」的 obs=0, 但**帧间 MOC union/覆盖连续性**是新问题(需覆盖重叠/更大 nside, 或 coverage_build 对稀疏帧的处理)。

### ✅ 突破: 逐帧 nside=2048 → phase2 全链 PASS(当前SHA b842899)
- 逐帧 drizzle **nside=2048**(每帧独立 HiPS, 匹配正常 tile_width=512/hierarchy), 6 个 `hips_paths` → phase2 全链:
  - `stage sample ok: obs=96 overlap_controls=42`(96 观测 / 42 重叠控制 → UPM 有足够 overlap 可解)。
  - **`{"exit_code":0,"status":"ok","summary":"phase2 complete"}`**, run_id `84e1b6de317b`, manifest `status:"complete"`, `n_inputs=6, n_obs=96`, sha256 `98441aa3bed...`。
- **结论**: WIN-006 代表链路(phase1→逐帧 drizzle→phase2)在真实银心数据上**全链打通**。前提=逐帧 nside=2048 独立 HiPS 作多个 `hips_paths`(而非单帧 coadd)。
- 注意: 本次 phase2 未 `persist_upm`(默认 false), 故仅落 manifest, UPM 模型未落盘。WIN-008 需 persist UPM + phase3 + seam/flux/coverage + 资源/内存门控。

### 结论(阶段)
- phase1(6帧,exit0) + drizzle(→完整HiPS,exit0) **PASS**; phase2 在逐帧结构下推进到 coverage_build, 但 `coverage_build rc=1` 为新 blocker。记录不宣称 PASS/release。

## 5. 结论
- production 链在真实银心数据上 phase1+drizzle 全通(核心场景达成)。
- phase2 UPM(obs=0)为真实 blocker, 留在 WIN-006/WIN-008 范畴; 记录不宣称 PASS, 不宣称 release。
