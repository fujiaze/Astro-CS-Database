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

极区 atan2/cos 奇点 → 保守 prune（极区 |dec|>45° 分支、|dec|>85° 仍保守 C=π/2/C45=π/(2√2)，平面 Lipschitz 盘 B(q,C·radius) 不相交则拒绝，false_negative=0；RA 环绕 dra>180→360-dra 并 cos(dec) 缩放判相交 lib/gaia_xpsd_client/src/gaia_client.c:polar_plane_intersects/bbox_intersects，头文件锚点见 lib/gaia_xpsd_client/src/gaia_client.h:1-14 与 B2-06 对齐；`|cos(dec)|<0.01` 保守返回相交；与 plate_solve 坐标契约无分叉——锥形查询为球面角距、SIP 仅 plate_solve 侧 WCS 畸变，见 docs/algorithms/PLATESOLVE.md 数值风险 / docs/science/ASTROMETRY.md 失效条件）；浮点坐标键比较容差定义。

## fast/reference/oracle

直连查询 vs 缓存路径逐行等价；极区全枚举对照。

## ID

ALG-GAIA-*；TEST-GAIA-*。
