# HEALPix Mapping

> 上游：ASTROCS_DESIGN.md §6.3（投影算法）、§3.1（数据对象）

关联：SCI-DRZ-001（科学正本 = `docs/science/DRIZZLE.md`）；core 实现正本 = `lib/algorithms/shared/healpix`（pix2ang/ang2pix/nested_local_to_xy 唯一实现，drizzle 依赖该单源）；tile/leaf 层级拆解实现面 = `lib/algorithms/coverage/src/upm.cpp:321` 与 `lib/infrastructure/aio/src/aio_upm.cpp:524-525`。

## 输入

RA/Dec 或 NESTED leaf/tile。

## 输出

NESTED 层级映射：tile ipix、local xy、leaf ipix（`lib/algorithms/coverage/src/upm.cpp:321`）；pix2ang/ang2pix（core）。

## Preconditions

order ≤ 29（`healpix_core.cpp:456` 合法域注释）；输入范围校验。

## Postconditions

round-trip 误差 ≤ 1e-12 度（FP64）；NESTED 父子一致性。

## Invariants

tile_shift=9、mask=(1<<18)-1（`aio_upm.cpp:524-525`、`upm.cpp:321`）；nested_local_to_xy 单调（core）。

## 复杂度

O(1)。

## 数值风险

极区；order 上限溢出 → checked。

## ID

ALG-HEALPIX-*；TEST-HEALPIX-*。

## 参考文献与参考代码库（含许可证）


- HEALPix 定义/order/NESTED：Górski et al. 2005, ApJ 622, 759（DOI 10.1086/427976）。
- 独立实现对照：astropy-healpix（BSD-3-Clause）、healpy（GPL-2.0）。
- HiPS 层级与 tile：IVOA HiPS 1.0（https://www.ivoa.net/documents/HiPS/）；Fernique et al. 2015, A&A 578, A114。
- round-trip 1e-12 度容差：Project-defined（本文件 Postconditions）。

参考代码库（含许可证）正本 = docs/references/SCIENTIFIC_REFERENCES.md §M。

