# Module: common

## 职责

共享权威基础库：HEALPix 核心（NESTED 唯一实现）+ SHA-256 + 标量精度抽象。作为全链路通用基础设施，被上层科学/IO/浏览器复用，不承载业务科学语义。

## 非职责

不做图像校准/星点/PSF/plate solve/测光/噪声/Drizzle/UPM/rejection/integration 等科学处理；不做 FITS/XISF/HiPS I/O（由 astro_image_io 承担）。

## Public API

| 头 | 前缀/类型 | 函数/类型(签名节选) | 要点 |
|---|---|---|---|
| `lib/common/healpix/healpix_core.h` | `astrocs::healpix` | `ang2pix_nest/pix2ang_nest/nested_local_to_xy/xy_to_nested_local/parent_nest/child_nest/query_disc/neighbors/leaf_to_tile_nest` | NESTED 唯一实现；被 healpix_drizzle / astro_image_io / healpix_browser_qt 复用，禁止第二套（B4-01 去重） |
| `lib/common/crypto/sha256.h` | `astrocs::crypto` | `sha256_hex/Sha256 {update,final_hex}` | DATA-FRAME-ID-001 frame_id 唯一实现（truncated-64 SHA-256） |
| `lib/common/include/astro_scalar.h` | `AstroScalarType` | `FP32/FP64, AstroScalarTraits, DISPATCH` | 双精度 ABI 标量分发 |
| `lib/common/include/precision_context.h` | `PrecisionContext` | `set_scalar_type/scalar_type/is_fp32/is_fp64` | 全链路精度单例（启动写入、数据阶段只读无锁，默认 FP32） |

实现文件：`lib/common/healpix/healpix_core.cpp`, `lib/common/crypto/sha256.cpp`。

## Data contract

- HEALPix `order K → nside=2^K`，`ang2pix` 内归一 `ra` 任意值 `dec∈[-90,90]`，非法 `pix2ang` 返回 `0`；NESTED leaf local 18 bits `interleave(x,y)`，FITS index `(511-x)*512+y` 由 CDS Hipsgen oracle 冻结（`docs/contracts/DATA_SEMANTICS.md §2-3`）。
- frame_id = truncated-64(canonical SHA-256 of science payload)（`DATA-FRAME-ID-001`）。
- 标量精度 `FP32/FP64` 经 `aio_set_precision_mode` 跨 DLL 传递。

## Ownership

- `healpix_core.h` header + `healpix_core.cpp` 实现；`sha256.h` + `sha256.cpp` 编译单元（静态库，非纯 header-only —— 见 ENG-C-04 澄清）。
- `Sha256` 增量对象由调用方持有，`final_hex` 后禁止再 `update`；`sha256_hex` 纯函数无所有权转移。

## Thread safety

- `healpix_core` 无状态纯函数，线程安全无锁；
- `PrecisionContext` 启动阶段写入、数据阶段只读无锁；跨 DLL 显式传递不依赖全局可变；
- `Sha256` 实例非线程安全（调用方线程内使用）。

## Errors

- `nside` 非 2 的幂 → 拒绝；`pix2ang` 越界 → `(0,0)`；
- `Sha256::final_hex` 后再 `update` → 未定义（文档禁止）。

## Config

- `precision` (fp32/fp64) 由 `lib/orchestrator/configs/stage1.schema.json` 经 orchestrator 透传。

## Science IDs

SCI-DRZ-* / SCI-UPM-*（HEALPix 几何）；DATA-FRAME-ID-001（frame_id）；详见 `docs/science/DRIZZLE.md` / `PHASE2_UPM.md` / `docs/contracts/DATA_SEMANTICS.md` §5 + `docs/algorithms/HEALPIX_MAPPING.md` B4-01。

## Tests

`lib/common/healpix/tests`（Hipsgen oracle 对照、`query_disc` 保守性）、`astro_scalar` 分发；上游通过 `docs_machine_consistency` frame_id / product 校验。

## Known limitations

- 仅支持 NESTED ordering（ring 未迁移）；
- 当前仅 Linux x86_64 字节序路径（`sampler.cpp:250-364` payload 字节语义）。

## Source files

`lib/common/{healpix/healpix_core.h,healpix_core.cpp,crypto/sha256.h,sha256.cpp,include/astro_scalar.h,include/precision_context.h,Makefile}`。
