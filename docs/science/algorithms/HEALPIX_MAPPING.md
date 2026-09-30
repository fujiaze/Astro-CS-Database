# HEALPix Mapping

> 上游：ACSD_DESIGN.md §6.3（投影算法）、§3.1（数据对象）

关联：SCI-DRZ-001（科学正本 = `docs/science/DRIZZLE.md`）；core 实现正本 = `lib/algorithms/shared/healpix`（pix2ang/ang2pix/nested_local_to_xy 唯一实现，drizzle 依赖该单源）；tile/leaf 层级拆解实现面 = `lib/algorithms/coverage/src/upm.cpp` 与 `lib/infrastructure/aio/src/aio_upm.cpp`。

## 输入

RA/Dec 或 NESTED leaf/tile。

**科学量五件事**（`ACSD_DESIGN.md` §3.3「单位、坐标系、归一化、精度要求、
有效有限域」）—— 冻结在实现头，本文档只作**指针级承接**：

| 量 | 单位 | 坐标系 | 归一化 | 精度要求 | 有效有限域 | 锚 |
|---|---|---|---|---|---|---|
| `ra_deg` | 度 | ICRS 赤经 | **任意值，内部归一化**（调用方不预处理） | f64 | 内部归一到 [0,360) | lib/algorithms/shared/healpix/healpix_core.h |
| `dec_deg` | 度 | ICRS 赤纬 | 无 | f64 | **[−90, 90]** | lib/algorithms/shared/healpix/healpix_core.h |
| `nside` | 无量纲 | — | **必须 2 的幂**（禁静默向上取整） | u32 | 2^k，k ≤ 29 | lib/algorithms/shared/healpix/healpix_core.h |
| `ipix` | 无量纲 | NESTED | — | u64 | [0, 12·nside²) | lib/algorithms/shared/healpix/healpix_core.h |
| `local`/`x`/`y` | 无量纲 | NESTED 局部 / tile 二维 | 位交错：x=偶数位、y=奇数位 | u32/u64 | [0,4^shift)、[0,2^shift) | lib/algorithms/shared/healpix/healpix_core.h |

**非法输入的显式失败面**（不返哨兵、不静默改写）：`nside` 非 2 的幂 →
`std::invalid_argument`（lib/algorithms/shared/healpix/healpix_core.h）；`leaf_order < tile_order` →
`std::invalid_argument`（同一头）；`child_nest` 移位溢出 →
`std::overflow_error`（healpix_core.h + lib/algorithms/shared/healpix/healpix_core.cpp）；`tile_to_leaf_nest`
移位溢出 → `std::overflow_error`（同一头）。**唯一不抛的退化面** =
`pix2ang_nest` 的 ipix 越界（ra=dec=0，调用方约定，同一头）。

## 输出

NESTED 层级映射：tile ipix、local xy、leaf ipix（`lib/algorithms/coverage/src/upm.cpp`）；pix2ang/ang2pix（core）。

## Preconditions

order ≤ 29（`lib/algorithms/shared/healpix/healpix_core.cpp` 合法域注释）；输入范围校验。

## Postconditions

round-trip 误差 ≤ 1e-12 度（FP64）；NESTED 父子一致性。

**外部 Oracle 承接**（三个载体**已实存**，此前本文件零承接）：

| 面对 | 可执行载体 | Oracle / 判据 |
|---|---|---|
| 全天空 ang2pix | `lib/algorithms/shared/healpix/tests/test_healpix_oracle.cpp` | astropy-healpix（BSD-3-Clause）生成的 JSONL 逐行对拍；**硬门 mismatch == 0** 且往返角距 ≤ `1.2 × hp_res + 1e-9`（判据写在该测试源内） |
| NESTED 邻域 | `lib/algorithms/shared/healpix/tests/test_healpix_neighbors.cpp` | 邻域闭包一致性 |
| HiPS tile 排列 | `lib/algorithms/shared/healpix/tests/test_hips_tile_mapping.cpp` | CDS Hipsgen MAPTILES 逐像素 |
| 生成器（非生产码） | `lib/algorithms/shared/healpix/tests/gen_spatial_fuzz.py` | 空间模糊输入生成 |

Oracle 规模与覆盖域（`lib/algorithms/shared/healpix/healpix_core.h` 头注释冻结）：1,000,000 全天随机点
+ 12 base face + 极区 + RA 跨界锚点（order 0..22），**mismatch=0**；
不得在本模块之外维护第二套 ang2pix/pix2ang（同一头文件另有声明）。

## Invariants

tile_shift=9、mask=(1<<18)-1（`lib/infrastructure/aio/src/aio_upm.cpp`、`lib/algorithms/coverage/src/upm.cpp`）；nested_local_to_xy 单调（core）。

## 复杂度

O(1)。

## 数值风险

极区；order 上限溢出 → checked。

## ID

ALG-HEALPIX-*（实现正本 = `lib/algorithms/shared/healpix`，唯一单源）；
TEST-HEALPIX-*。

**TEST-HEALPIX-* 的指针级承接**（此前仅通配符；子项豁免登记 =
`独立审计/证据/AUD-101-DA02-算法推导.md`）：

| ID 面 | 可执行载体 |
|---|---|
| TEST-HEALPIX-ORACLE | `lib/algorithms/shared/healpix/tests/test_healpix_oracle.cpp` |
| TEST-HEALPIX-NEIGHBORS | `lib/algorithms/shared/healpix/tests/test_healpix_neighbors.cpp` |
| TEST-HEALPIX-TILEMAP | `lib/algorithms/shared/healpix/tests/test_hips_tile_mapping.cpp` |

**负例面（可判红、非恒真）**：`nside` 非 2 的幂必须抛
`std::invalid_argument`（lib/algorithms/shared/healpix/healpix_core.h）；`child_nest`
超域移位必须抛 `std::overflow_error`（healpix_core.h +
lib/algorithms/shared/healpix/healpix_core.cpp）；oracle 面 `mismatch != 0` 或往返角距超
`1.2 × hp_res + 1e-9` 必须判红（`lib/algorithms/shared/healpix/tests/test_healpix_oracle.cpp`）。

## 参考文献与参考代码库（含许可证）


- HEALPix 定义/order/NESTED：Górski et al. 2005, ApJ 622, 759（DOI 10.1086/427976）。
- 独立实现对照：astropy-healpix（BSD-3-Clause）、healpy（GPL-2.0）。
- HiPS 层级与 tile：IVOA HiPS 1.0（https://www.ivoa.net/documents/HiPS/）；Fernique et al. 2015, A&A 578, A114。
- round-trip 1e-12 度容差：Project-defined（本文件 Postconditions）。

参考代码库（含许可证）正本 = docs/engineering/SCIENTIFIC_REFERENCES.md §M。

