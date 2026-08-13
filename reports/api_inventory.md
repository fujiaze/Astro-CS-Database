# Public / Internal API Inventory（V14 G5）

## Exported C ABI

```text
lib/astro_image_io: aio_fits_* aio_hips_* aio_upm_* aio_pipeline_*
lib/phase2: p2_frame_id p2_coverage_build/free p2_sample_controls
           p2_upm_build p2_upm_build_geo p2_upm_save/open p2_upm_close
           p2_upm_calibrate_block p2_upm_evaluate_c p2_upm_raw_weight
           p2_upm_normalized_weights p2_upm_geometry_hash
           p2_stats_median/mad p2_integrate_pixel p2_reject_stack
           p2_block_plan p2_upm_materialize_dense / dense read APIs
```

规则：extern "C"、不抛异常、0=OK、err 只做日志。

## Public C++ API

```text
astrocs::healpix: ang2pix_nest pix2ang_nest nested_local_to_xy
                  xy_to_nested_local nested_local_to_fits_index
                  fits_index_to_nested_local leaf_to_tile tile_to_leaf
astrocs::crypto: sha256_hex
astrocs::compute::phase2: kOpMosaicReject（ACR）
```

## Internal module API

- phase2 src：sampler（CellStat/Stage A–E）、upm（Model/build_impl）、
  integrate、coverage、stage2_common。
- astro_image_io src/hips：aio writer/reader、tile_rel_path。

## Tool / CLI

```text
astrocs-stage2.exe <config.json>
orchestrator.exe <stage1.json>
healpix_browser_qt.exe --hips/--standard-hips/--view/--lod/--screenshot/--exit
browser_cli.exe --hips/--refrender/--benchmark
toolchain.ps1 check|build|run|review
phase2_synthetic_gate.exe / calibrated_pair_diag.exe / rejection_cli.exe
```

## JSON schemas

- stage2 config（model/integration/output/diagnostics）。
- upm_sparse `astrocs-upm-v2`（含 component/geometry 统计，V14）。
- controls_accept.json（V13 overlay 诊断）。

## On-disk

- HiPS products（signal/support/snr, IVOA 1.4）。
- manifest.json / diagnostics.json / upm_dense.cache（checksum）。

## 命名/风格

- C ABI 保留 `aio_*` / `p2_*`（有调用方，不破坏）；C++ 命名空间
  `astrocs::phase1/phase2/hips/acr` 逐步引入，不强制大改名。
