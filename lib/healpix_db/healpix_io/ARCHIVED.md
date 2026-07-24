# Archived: healpix_io

**归档日期**: 2026-07-16
**归档原因**: 按 architecture-refactor spec G1，healpix_io 源码合并入 aio (astro_image_io) 模块，统一 I/O 接口
**替代方案**: `lib/astro_image_io/`
- 头文件: `lib/astro_image_io/include/aio_healpix_io.h`
- 实现: `lib/astro_image_io/src/healpix/aio_healpix_io.cpp`
- Python 绑定: `lib/astro_image_io/python/aio_healpix_io.py`
- 格式规范: `lib/astro_image_io/docs/HEALPIX_FORMAT_SPEC.md`
- 测试: `lib/astro_image_io/tests/test_healpix_io*.py`
- 编译: `lib/astro_image_io/build.ps1` (含 HEALPix 条件编译宏 `AIO_ENABLE_HEALPIX`)

**API 命名变更**:
- `hiss_write` → `aio_hiss_write`
- `hiss_read` → `aio_hiss_read`
- `hcsd_write` → `aio_hcsd_write`
- `hcsd_read` → `aio_hcsd_read`
- `hcsd_read_leaf` → `aio_hcsd_read_leaf`
- `hio_free` → `aio_hio_free`
- `hiss_write_snr_model` → `aio_hiss_write_snr_model`
- `hiss_read_snr_model` → `aio_hiss_read_snr_model`
- `hio_free_snr_model` → `aio_hio_free_snr_model`

旧 API 通过 `aio_healpix_io.h` 中的向后兼容宏定义仍可使用（如 `#define hiss_write aio_hiss_write`）。

本目录代码不再用于编译或运行，仅供历史参考。
