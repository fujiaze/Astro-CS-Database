# Gaia Query

关联：SCI-AST-001；模块：lib/gaia_xpsd_client。

## 输入

RA/Dec 视场 + 半径 + 星等范围 + 滤光带。

## 输出

星表子集（id, ra, dec, photometry, 误差列）。

## Preconditions

参数有限；坐标 J2000。

## Postconditions

- 极区查询 provably-conservative prune（V18R3）；
- cache 键精确匹配，事务性替换。

## Invariants

- 返回星不遗漏（false negative=0 由保守裁剪保证）；
- cache 命中必须键完全一致（含带通/半径）。

## 复杂度

O(log n) 索引 + O(k) 输出。

## 并行模型

查询串行（客户端），缓存互斥。

## 数值风险

极区 atan2/cos 奇点 → 保守 prune；浮点坐标键比较容差定义。

## fast/reference/oracle

直连查询 vs 缓存路径逐行等价；极区全枚举对照。

## ID

ALG-GAIA-*；TEST-GAIA-*。
