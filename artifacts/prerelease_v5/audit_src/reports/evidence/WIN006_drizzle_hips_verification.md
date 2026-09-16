# WIN-006 生产 Drizzle→HiPS 直链验证

## 结论
CLI（统一单二进制）现已**完整打通生产 Phase1 Final Closure：Drizzle → AIO HiPS 直写**，
与 orchestrator 使用同一生产 API `hp_drizzle_run_hips`，**无 HISS 中转**。已按当前 SHA 构建并复验。
相关依赖边：WIN-006(phase2/3 未再被"HIPS 链缺失"阻塞)。

## 根因（打回意见 #3）
此前 `hp_drizzle_run_hips`（`lib/healpix_db/healpix_drizzle/hp_drizzle_api.h`，生产 Drizzle→AIO HiPS
直写 API）只被 orchestrator 动态加载使用，**统一 CLI 完全未链接该链**：`drizzle_engine`、`hp_drizzle_api`、
`astro_sphere_sink`、`fits_reader`、`wcs_sip`、`poly_clip`、`spherical_overlap`、`reverse_drizzle`、
`snr_evaluator` 及 hiss 写器（`hiss_tile_model/writer/common/transform/codec/reader/stream_writer`）、
`aio_healpix_io`、`aio_pipeline/aio_pipeline_engine`（提供 `aio_frame_*`）均不在 CLI 构建内。
于是 CLI 到 "HiPS" 处是断的 —— phase1 只产出校准 FITS，不存在生产 HiPS 直写路径；
`tests/backend/phase2_fixture_main.cpp` 里那个"HiPS"是**伪造的常量通量 tile**（
`aio_hips_product_begin/write_signal_support_tile/finalize`），**非生产链**，不得当作生产路径。

## 修复
1. `cli/CMakeLists.txt`（commit `199a1c3`）：把上述生产链全部源文件接入 `astrocs` 目标；
   补 hiss/HEALPix/CFITSIO/`aio_pipeline` include 与 `-DAIO_ENABLE_HEALPIX`、`-DAIO_ENABLE_FITS`。
   链接期逐层补齐缺失符号：`hiss::HissWriter*`/`hiss::compute_tile_*`、`aio_frame_get_block/kv_*`。
2. `cli/main.cpp`（commit `cba67e5`）：新增 CLI 子命令
   `astrocs drizzle --config <path> [--nside N] [--pixfrac P]`。
   用 CLI 自有的 `aio_read`（与 phase1 同一读取器）读校准 FITS → 构建 `PipelineFrame`
   （`data` 块 + WCS/SIP/PHOTSCAL/CTYPE/PRECISION 等 header KV）→ 调 `hp_drizzle_run_hips` **直写 HiPS**。
   命令树、旗标白名单（新增 `--nside`/`--pixfrac`）同步注册；config 沿用 `schema_version=1` 全量校验。

## 当前 SHA 复验（commit `cba67e562b8a971be8adfff00af4966e5435c3c5`）
二进制版本：`astrocs 0.9.0-alpha.1+gcba67e562b8a.dirty`；`doctor --json` = PASS。

### 1) 合成 WCS 帧端到端（256×256 合成帧, `--nside 512 --pixfrac 0.8`）
```
$ astrocs drizzle --config /tmp/upm_tmp/dz.json --nside 512 --pixfrac 0.8
{"drizzle":"ok","frames":1,"hips_dir":"/tmp/upm_tmp/hips",
 "n_healpix_pixels":27,"n_source_pixels":65536}    exit=0
```
关键日志：
- `frame_create: ok` / `add_block: 'data' ... 65536 (262144 bytes)`
- `WCS CRVAL=(272.825665,-13.131811) CRPIX=(128.500,128.500) CD=[...]`
- `测光元数据 photscal=1.000000 photappl=1`
- `[drizzle_engine] 完成: 65536 源像素 → 27 HEALPix 像素 (1 Tile)`
- `[sink] HiPS 直写完成: 1 tiles -> /tmp/upm_tmp/hips`
- `[hp_drizzle_api] hp_drizzle_run: HiPS 已直写 /tmp/upm_tmp/hips (无 HISS 中转)`

产物结构（完整 IVOA HiPS 产品）：
```
/tmp/upm_tmp/hips/
  manifest.json   (hips v1.4, nside 512, tile_width 512, float32, products signal/support/snr)
  operation_counts.json
  signal/  Moc.fits  metadata.fits  properties  Norder0/Dir0/Npix7.fits
  support/ Moc.fits  metadata.fits  properties  Norder0/Dir0/Npix7.fits
```

### 2) 受影响 oracle 回归（drizzle 平面原语不变量）
`tests/backend/test_drizzle_oracle.py`：**6/6 OK**（OVERLAP 解析、support/coverage、ACCUMULATE、
NORMALIZE 守卫、亚像素 shift、flux/brightness 守恒），无回归。

## 限制 / 遗留
- `drizzle` 子命令要求输入为**已校准(测光已应用)且含 WCS/SIP + PHOTSCAL>0** 的帧；无该信息时
  `hp_drizzle_run_hips` 会硬失败（如 `photscal 非法` / `nside 非法`），与正式 Stage1 约束一致。
- HiPS 精度默认 FP32；`--nside` 必须为正的 2 的幂（缺省 2048）。
- PAR-002 sampler 并行**读**仍受 `lib/astro_image_io` 的 cfitsio 后端并发 `_IO_fread` SIGSEGV 影响
  （见 `PAR002_blocker.md`），见 PAR-002 独立记录；与本 WIN-006 写链解耦。
- 未用真实数据/32R 验证；仅合成 WCS 帧 + oracle 单帧。真实 32R 需 Fatduck。
