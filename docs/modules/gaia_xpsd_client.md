# Module: gaia_xpsd_client

## 职责

Gaia DR3 星表/光谱查询、极区保守裁剪、本地缓存。

## 非职责

不做匹配/求解。

## Public API

gaia_client DLL（查询/缓存键）。

## Data contract

RA/Dec/半径/带通 → 星表行；缓存键精确。

## Ownership

结果 buffer 调用方释放；缓存事务性替换。

## Thread safety

查询串行 + 互斥缓存（V18R3 审计）。

## Errors

网络/超时（TIMEOUT）；缓存 stale → 拒绝。

## Science IDs

SCI-AST-001（输入侧）；ALG-GAIA-*。

## 性能特征

dec 排序索引；极区 prune false negative=0。

## Tests

极区全枚举对照；缓存键等价性。

## Source files

lib/gaia_xpsd_client/。
