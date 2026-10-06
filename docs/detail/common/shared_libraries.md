# 共享基础库

跨阶段公共机制之一，对应最高设计软件架构一章的模块与 ABI 条目，以及数据对象与配置一章的精度归属与帧标识条目。

## 职责

共享权威基础库：HEALPix 核心（NESTED 唯一实现）+ SHA-256 + 标量精度抽象。作为全链路通用基础设施，被上层科学/IO/浏览器复用，不承载业务科学语义。

## 非职责

不做图像校准/星点/PSF/plate solve/测光/噪声/Drizzle/UPM/rejection/integration 等科学处理；不做 FITS/XISF/HiPS I/O（由 astro_image_io 承担）。

## 接口签名

| 头 | 前缀/类型 | 函数/类型(签名节选) | 要点 |
|---|---|---|---|
| `lib/algorithms/shared/healpix/healpix_core.h` | `acsd::healpix` | `ang2pix_nest/pix2ang_nest/nested_local_to_xy/xy_to_nested_local/parent_nest/child_nest/query_disc/neighbors/leaf_to_tile_nest` | NESTED 唯一实现；被 healpix_drizzle / astro_image_io / healpix_browser_qt 复用，全仓唯一一套（依赖单源纪律，正本 = `docs/engineering/standards/DEPENDENCY.md`） |
| `lib/algorithms/shared/crypto/sha256.h` | `acsd::crypto` | `sha256_hex/Sha256 {update,final_hex}` | DATA-FRAME-ID-001 frame_id 唯一实现（truncated-64 SHA-256） |
| `lib/algorithms/shared/include/astro_scalar.h` | `AstroScalarType` | `FP32/FP64, AstroScalarTraits, DISPATCH` | 双精度 ABI 标量分发 |
| `lib/algorithms/shared/include/precision_context.h` | `PrecisionContext` | `set_scalar_type/scalar_type/is_fp32/is_fp64` | 全链路精度单例（启动写入、数据阶段只读无锁，默认 FP32） |

实现文件：`lib/algorithms/shared/healpix/healpix_core.cpp`, `lib/algorithms/shared/crypto/sha256.cpp`。

## 数据对象与块的读写

- HEALPix `order K → nside=2^K`，`ang2pix` 内归一 `ra` 任意值 `dec∈[-90,90]`，非法 `pix2ang` 返回 `0`；NESTED leaf local 18 bits `interleave(x,y)`，FITS index `(511-x)*512+y` 由 CDS Hipsgen oracle 冻结（`docs/science/unified/DATA_SEMANTICS.md`「坐标语义」一节）。
- frame_id = truncated-64(canonical SHA-256 of science payload)（`DATA-FRAME-ID-001`）。
- 标量精度 `FP32/FP64` 经 `aio_set_precision_mode` 跨 DLL 传递；全局 `AstroScalarType` 是**计算路径**的精度开关，**发布面** dtype 另按《ACSD 最高设计》的「精度归属」一节与 `docs/engineering/standards/NUMERIC.md` 逐数据形态归属（稠密发布面 FP32、稀疏与元数据 FP64）——两者是不同面，不互相覆盖。

## 所有权与生命周期

- `lib/algorithms/shared/healpix/healpix_core.h` header + `lib/algorithms/shared/healpix/healpix_core.cpp` 实现；`lib/algorithms/shared/crypto/sha256.h` + `lib/algorithms/shared/crypto/sha256.cpp` 编译单元（静态库，非纯 header-only）。
- `Sha256` 增量对象由调用方持有，`final_hex` 为终态（`update` 的调用面随 `final_hex` 结束）；`sha256_hex` 纯函数无所有权转移。

## 线程安全

- `healpix_core` 无状态纯函数，线程安全无锁；
- `PrecisionContext` 启动阶段写入、数据阶段只读无锁；跨 DLL 显式传递不依赖全局可变；
- `Sha256` 实例非线程安全（调用方线程内使用）。

## 错误处理

- `nside` 非 2 的幂 → 拒绝；`pix2ang` 越界 → `(0,0)`；
- `Sha256::final_hex` 后再 `update` → 未定义（合同约定 = `final_hex` 为终态）。

## 配置项与默认值来源

- `precision` (fp32/fp64) 由 `lib/infrastructure/pipeline/orchestrator/configs/stage1.schema.json` 经 orchestrator 透传。

## 对应一级文档条目

HEALPix 几何条目与帧标识条目，详见球面映射科学与天光平面科学分册，以及数据语义分册的溯源最小集一节与逐算法推导中的球面映射算法 [4]。

## 与相邻模块的关系

本库被全部科学模块、输入输出层与球面浏览器复用，只提供无状态几何、哈希与精度原语，不调用任何上层模块。依赖方向是单向的，上层可以依赖本库，本库不反向依赖上层。

## 调试与验证入口

共享基础库的验证落点目前尚未在本库登记。帧标识与产品的一致性由日志事件合同与各模块的数据合同约束保证。

## 已知限制与参考文献

- 仅支持 NESTED ordering（ring 未迁移）；
- 当前仅 Linux x86_64 字节序路径（`lib/algorithms/coverage/src/sampler.cpp` payload 字节语义）；
- 本库无共址测试目录，上述三项验证的落点未在本库登记。

## 参考文献

[1] Gorski K. M., Hivon E., Banday A. J., Wandelt B. D., Hansen F. K., Reinecke M., Bartelmann M. HEALPix: A framework for high-resolution discretization and fast analysis of data distributed on the sphere. The Astrophysical Journal, 2005, 622(2): 759–771. https://doi.org/10.1086/427976
[2] Fernique P., Allen M. G., Boch T., Burke D., Castro-Ginard A., Davidson J., Durand D., Kreckel K. Hierarchical progressive surveys: Visualisation and streaming of astronomical images and catalogues with HiPS. Astronomy and Astrophysics, 2015, 578: A114. https://doi.org/10.1051/0004-6361/201526075
[3] Eastlake D., Jones P. US Secure Hash Algorithms (SHA and SHA-based HMAC and HKDF). RFC 6234, 2011. https://www.rfc-editor.org/rfc/rfc6234
[4] 统一观测模型与数据对象细节页，本文common目录。
[5] 最高设计软件架构与数据对象条目，以及工程依赖与数值标准合同。

## 源文件

以 `lib/algorithms/shared/` 为根的 7 件：`dirent_win.h`、`healpix/healpix_core.h`、`healpix/healpix_core.cpp`、`crypto/sha256.h`、`crypto/sha256.cpp`、`include/astro_scalar.h`、`include/precision_context.h`。
