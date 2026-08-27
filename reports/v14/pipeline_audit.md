# Phase1–Phase2 全链路审计（V14 G1）

逐项核对：实现 / 测试 / 文档 / 生产 CLI 一致。

## Phase1

| 步骤 | 实现 | 测试 | 文档 | 生产 CLI |
| --- | --- | --- | --- | --- |
| FITS/metadata 读取 | astro_image_io aio_fits | aio tests | docs/PIPELINE | orchestrator stage1 |
| 校准 A/B/C | lib/calibration | 模块 tests | 工程控制 | stage1 |
| plate solve / WCS | lib/plate_solve ipv | stage1 日志 rms 0.27″ | 工程控制 | stage1 |
| PSF / photometry / SNR | dynamic_psf/photometric/snr | stage1 verify | 工程控制 | stage1 |
| Drizzle (NESTED) | healpix_drizzle | V11 oracle | DATA_SEMANTICS | stage1 |
| HiPS 512-tile 映射 | healpix_core | hips_mapping_oracle 全像素 | DATA_SEMANTICS §3 | stage1 |
| hierarchy | aio writer | Hipsgen 外部验证 | DATA_SEMANTICS | stage1 |
| manifest/provenance | aio writer | properties 校验 | DATA_SEMANTICS §5 | stage1 |

## Phase2

| 步骤 | 实现 | 测试 | 文档 | 生产 CLI |
| --- | --- | --- | --- | --- |
| manifest / frame identity | stage2_common | frame_id 测试 | DATA_SEMANTICS §5 | stage2 |
| coverage union | coverage.cpp | synthetic gate | PIPELINE | stage2 |
| control geometry | sampler.cpp（几何/接受分离） | synthetic gate | sampler_design | stage2 |
| background-clean 采样 | sampler.cpp Stage A–E | v13_synth_test 多星 truth | sampler_design | stage2 |
| UPM 全几何 + 平滑 | upm.cpp build_geo | GC V13 逐位等价 | smooth_continuation | stage2 |
| standardized Huber | upm.cpp z=r/sigma | synthetic | robust_loss | stage2 |
| component 语义 | upm.cpp G3 | GC data=1/geometry=1 | SCIENCE_FREEZE | stage2 |
| 7 种 rejection | stage2 + integrate | synthetic gate | 工程控制 | stage2 |
| weighted integration | integrate.cpp | 权重语义测试 | seam 报告 | stage2 |
| no-valid/denominator | integrate status 1/2 | sanitize 测试 | DATA_SEMANTICS §4 | stage2 |
| HiPS serialization | stage2 (V11) | Hipsgen oracle | DATA_SEMANTICS §3 | stage2 |
| CPU/ACR 等价 | acr kernels | 0-diff 测试 | 工程控制 | stage2 |

## Cross-stage contract

Phase1 输出 → Phase2 消费：signal/support/MOC/SNR/quality/frame_id/manifest
（`docs/contracts/DATA_SEMANTICS.md` §7）。契约测试：
`phase2_synthetic_gate`（真实 HiPS 采样）+ GC 全流程回归（V13 逐位等价锚）。

## 审计结论

- 无 test-only science path 冒充 production（stage2 生产路径 = 唯一实现）；
- 实现/测试/文档/CLI 四者一致；
- V13 基线保持（C/M maxdiff=0）。
