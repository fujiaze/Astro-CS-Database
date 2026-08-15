# HEALPix Mapping

关联：SCI-DRZ-001；模块：lib/common/healpix。

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
