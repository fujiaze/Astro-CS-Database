# Module: healpix_drizzle

## 职责

球面 Drizzle：HEALPix 网格重投影 + 通量守恒 + 方差/ivar 传播 +
操作计数诊断。

## 非职责

不做多帧统计合并（Phase2）。

## Public API

hp_drizzle_api（extern "C"）；候选/重叠/几何接口。

## Data contract

输入帧（像素+WCS）→ 目标 NESTED tile 产品（signal/variance/ivar）。

## Ownership

句柄级 RAII。

## Thread safety

OpenMP 源帧并行；geometry cache 只读。

## Errors

几何退化/无覆盖 → NO_DATA；candidate 保守。

## Science IDs

SCI-DRZ-001/014/015/016；ALG-DRZ-CAND-001；ALG-DRZ-OVERLAP-001；
ALG-DRZ-VAR-*。

## 性能特征

geometry cache 复用；计数 METRIC-P1-DRZ-CANDIDATES 等。

## Tests

candidate/overlap/variance oracle（evidence/drizzle/*.json）；
Monte Carlo 方差（SNR-011/012）。

## Source files

lib/healpix_db/healpix_drizzle/。
