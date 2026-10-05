# Module: common

> 上游：《ACSD 最高设计》的「软件架构」一章（模块与 ABI：版本化公开头、跨动态库不传 STL/异常/RTTI）、
> 「数据对象与配置」一章（精度归属：稠密大面默认单精度、稀疏与元数据全程双精度、JSON 显式指定以 JSON 为准；
> 数据对象：全链一套球面索引与一套帧标识）
> 依赖面正本：docs/engineering/standards/DEPENDENCY.md（`acsd_common` 可被任何模块单向依赖；
> 共享 HEALPix 核心单源 = `lib/algorithms/shared/healpix/healpix_core.cpp`）
> 精度边界正本：docs/engineering/standards/NUMERIC.md
> 承载位置：本库是跨全链路的共享基础库，不是流水节点，故不在 `registry/` 的生产模块登记面内。

## 职责

共享权威基础库：HEALPix 核心（NESTED 唯一实现）+ SHA-256 + 标量精度抽象。作为全链路通用基础设施，被上层科学/IO/浏览器复用，不承载业务科学语义。

## 非职责

不做图像校准/星点/PSF/plate solve/测光/噪声/Drizzle/UPM/rejection/integration 等科学处理；不做 FITS/XISF/HiPS I/O（由 astro_image_io 承担）。

## Public API

| 头 | 前缀/类型 | 函数/类型(签名节选) | 要点 |
|---|---|---|---|
| `lib/algorithms/shared/healpix/healpix_core.h` | `acsd::healpix` | `ang2pix_nest/pix2ang_nest/nested_local_to_xy/xy_to_nested_local/parent_nest/child_nest/query_disc/neighbors/leaf_to_tile_nest` | NESTED 唯一实现；被 healpix_drizzle / astro_image_io / healpix_browser_qt 复用，全仓唯一一套（依赖单源纪律，正本 = `docs/engineering/standards/DEPENDENCY.md`） |
| `lib/algorithms/shared/crypto/sha256.h` | `acsd::crypto` | `sha256_hex/Sha256 {update,final_hex}` | DATA-FRAME-ID-001 frame_id 唯一实现（truncated-64 SHA-256） |
| `lib/algorithms/shared/include/astro_scalar.h` | `AstroScalarType` | `FP32/FP64, AstroScalarTraits, DISPATCH` | 双精度 ABI 标量分发 |
| `lib/algorithms/shared/include/precision_context.h` | `PrecisionContext` | `set_scalar_type/scalar_type/is_fp32/is_fp64` | 全链路精度单例（启动写入、数据阶段只读无锁，默认 FP32） |

实现文件：`lib/algorithms/shared/healpix/healpix_core.cpp`, `lib/algorithms/shared/crypto/sha256.cpp`。

## Data contract

- HEALPix `order K → nside=2^K`，`ang2pix` 内归一 `ra` 任意值 `dec∈[-90,90]`，非法 `pix2ang` 返回 `0`；NESTED leaf local 18 bits `interleave(x,y)`，FITS index `(511-x)*512+y` 由 CDS Hipsgen oracle 冻结（`docs/science/unified/DATA_SEMANTICS.md`「坐标语义」一节）。
- frame_id = truncated-64(canonical SHA-256 of science payload)（`DATA-FRAME-ID-001`）。
- 标量精度 `FP32/FP64` 经 `aio_set_precision_mode` 跨 DLL 传递；全局 `AstroScalarType` 是**计算路径**的精度开关，**发布面** dtype 另按《ACSD 最高设计》的「精度归属」一节与 `docs/engineering/standards/NUMERIC.md` 逐数据形态归属（稠密发布面 FP32、稀疏与元数据 FP64）——两者是不同面，不互相覆盖。

## Ownership

- `healpix_core.h` header + `healpix_core.cpp` 实现；`sha256.h` + `sha256.cpp` 编译单元（静态库，非纯 header-only）。
- `Sha256` 增量对象由调用方持有，`final_hex` 为终态（`update` 的调用面随 `final_hex` 结束）；`sha256_hex` 纯函数无所有权转移。

## Thread safety

- `healpix_core` 无状态纯函数，线程安全无锁；
- `PrecisionContext` 启动阶段写入、数据阶段只读无锁；跨 DLL 显式传递不依赖全局可变；
- `Sha256` 实例非线程安全（调用方线程内使用）。

## Errors

- `nside` 非 2 的幂 → 拒绝；`pix2ang` 越界 → `(0,0)`；
- `Sha256::final_hex` 后再 `update` → 未定义（合同约定 = `final_hex` 为终态）。

## Config

- `precision` (fp32/fp64) 由 `lib/infrastructure/pipeline/orchestrator/configs/stage1.schema.json` 经 orchestrator 透传。

## Science IDs

SCI-DRZ-* / SCI-UPM-*（HEALPix 几何）；DATA-FRAME-ID-001（frame_id）；详见 `docs/science/drizzle/DRIZZLE.md` / `docs/science/PHASE2_UPM.md` / `docs/science/unified/DATA_SEMANTICS.md`「溯源最小集」一节 + `docs/science/algorithms/HEALPIX_MAPPING.md`。

## Tests

`lib/algorithms/shared/` 下当前**不承载共址测试目录**：HEALPix 几何的 Hipsgen oracle 对照、`query_disc` 保守性与 `astro_scalar` 分发的验证落点未在本库登记（待补，见「Known limitations」）；frame_id 与 product 的一致性由 `lib/infrastructure/observability/logging/log_event_v1.schema.json` 合同与各模块的数据合同约束保证。

## Known limitations

- 仅支持 NESTED ordering（ring 未迁移）；
- 当前仅 Linux x86_64 字节序路径（`lib/algorithms/coverage/src/sampler.cpp` payload 字节语义）；
- 本库无共址测试目录，上述三项验证的落点未在本库登记。

## Source files

以 `lib/algorithms/shared/` 为根的 7 件：`dirent_win.h`、`healpix/healpix_core.h`、`healpix/healpix_core.cpp`、`crypto/sha256.h`、`crypto/sha256.cpp`、`include/astro_scalar.h`、`include/precision_context.h`。
