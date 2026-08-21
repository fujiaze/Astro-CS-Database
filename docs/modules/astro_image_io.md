# Module: astro_image_io

## 职责

唯一 I/O 层：FITS/XISF/ahpx 读写、zstd/lz4 压缩、HiPS 读/写（signal/
support/variance/ivar）、UPM 模型文件容器（aio_upm sparse/dense）、
PipelineFrame/引擎。

## 非职责

不实现科学算法（几何/统计）；不写 testdata/。

## Production callers

所有科学模块 DLL、orchestrator、phase2（aio_upm/aio_hips_reader）、
浏览器（hips reader）。

## Public API

aio_* 系列（aio_fits/aio_xisf/aio_hips_reader/aio_hips_writer/
aio_upm/aio_compressor/aio_pipeline）；API-AIO-001..（S2 注册）。

## Data contract

FITS 标准 + IVOA HiPS；HiPS tile 语义：signal/support/variance/ivar
（DATA-HIPS-SIGNAL-001 等，S2 注册）；UPM sparse format astrocs-upm-v2。
HiPS 精度：`AioHipsDataType` 枚举 `AIO_HIPS_FLOAT32=0` / `AIO_HIPS_FLOAT64=1`
（`lib/astro_image_io/include/aio_hips.h:46-49`）透传至 `aio_hips_product_begin`
`data_type`，写盘 `BITPIX -32/-64` 对应 `CFITSIO TFLOAT/TDOUBLE`
（`src/hips/aio_hips_writer.cpp:222-225`）；科学精度优先 FP64 reference，FP32
仅显式等价路径（见 `docs/architecture/PERFORMANCE_MODEL.md`）。

## Ownership

读句柄 aio_*_open → aio_*_close；动态 buffer 由 aio_upm_read_all_dynamic
返回调用方 delete[]；g_upm_error thread_local。

## Thread safety

独立句柄可并行；同句柄顺序访问；dense cache 写/读分离。

## Errors

IO/CONFIG/INPUT_CORRUPT；aio_upm_last_error 提供消息；stale cache=2。

## Config

压缩级别；HiPS order 参数；无全局 config。

## Science/algorithm IDs

SCI-DRZ-014/016（产品语义）；DATA-HIPS-*。

## 性能特征

流式读写；compression 块级；HiPS 写先 tile 后 properties。

## 缓存

无进程级缓存（读路径句柄级）。

## Diagnostics

日志 run/logs/astro_image_io/；错误类别+消息。

## Tests/oracles

hiss_correctness、pipeline_frame_contract、checksum、drizzle_integration、
fuzz/sanitize driver；Python oracle（hips_mapping_oracle）；
`test_precision_dual.cpp` 覆盖 FP64 精度 oracle（DATA_TYPE FLOAT32/FLOAT64
双模式：precision_mode/signal_dtype 元数据与 `astrocs_signal_dtype` 一致性）。

## Known limitations

UPM sparse 已于 V19R6R2 temp+rename 已修复（F-V19R2-IO-001 已闭环，见 docs/architecture/IO_AND_ATOMICITY.md）；HiPS tiles 仍非原子（partial-file 策略：abort 尽力清理、finalize 写 CHECKSUM/DATASUM 后交付，单 tile 为 remove→create→write_chksum→close）；orchestrator 日志路径嵌套 bug（非阻断）。

## Source files

lib/astro_image_io/{include,src}/。
