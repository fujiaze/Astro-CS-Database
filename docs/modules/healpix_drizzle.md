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

## V19R3 bounded target-ipix geometry cache

- TargetGeomCache（LRU，默认 8192，线程私有，run generation 切换清空）；
- 计数新增 target_boundary_builds / target_geometry_builds /
  geometry_cache_hits / geometry_cache_misses（DrizzleStats + [ops] 行）；
- 科学等价：candidate oracle 9003/0、freeze 42/42、UPMW-005 MC
  k_corr=1.3883 不变；详见 docs/algorithms/DRIZZLE_GEOMETRY.md。

## Tests

candidate/overlap/variance oracle（evidence/drizzle/*.json）；
Monte Carlo 方差（SNR-011/012）。

## Source files

lib/healpix_db/healpix_drizzle/。
