# HEALPix Mapping

关联：SCI-DRZ-001；模块：lib/algorithms/shared/healpix（权威实现）；B4-01 去重为单源（drizzle 转依赖 lib/algorithms/shared/healpix，另一份 deprecated shim + 机器门禁）。

## 输入

RA/Dec 或 NESTED leaf/tile。

## 输出

NESTED 层级映射：tile ipix、local xy、leaf ipix；pix2ang/ang2pix。

## Preconditions

order ≤ 29；输入范围校验。

## Postconditions

round-trip 误差 ≤ 1e-12 度（FP64）；NESTED 父子一致性。

## Invariants

tile_shift=9；mask=(1<<18)-1；nested_local_to_xy 单调。

## 复杂度

O(1)。

## 数值风险

极区；order 上限溢出 → checked。

## ID

ALG-HEALPIX-*；TEST-HEALPIX-*。

## 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动本文件任何公式、锚点、阈值与容差；原有条款全部保留。

- HEALPix 定义/order/NESTED：Górski et al. 2005, ApJ 622, 759（DOI 10.1086/427976）。
- 独立实现对照：astropy-healpix（BSD-3-Clause）、healpy（GPL-2.0）。
- HiPS 层级与 tile：IVOA HiPS 1.0（https://www.ivoa.net/documents/HiPS/）；Fernique et al. 2015, A&A 578, A114。
- round-trip 1e-12 度容差：Project-defined（本文件 Postconditions）。

参考代码库（含许可证；GPL 代码仅作行为/数值对照，不复制进本仓）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）；photutils（BSD-3-Clause，https://github.com/astropy/photutils）；astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）；ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）；reproject（BSD-3-Clause，https://github.com/astropy/reproject）。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）。
- SExtractor / PSFEx / SWarp / SCAMP（GPL-3.0，https://github.com/astromatic/）。
- healpy（GPL-2.0，https://github.com/healpy/healpy）；Siril（GPL-3.0，https://gitlab.com/free-astro/siril）；LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）；GSL（GPL-3.0，https://www.gnu.org/software/gsl/）。
- WCSLIB（LGPL-3.0）；CFITSIO（宽松许可，NASA/HEASARC，https://heasarc.gsfc.nasa.gov/fitsio/）。
- NumPy / SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

