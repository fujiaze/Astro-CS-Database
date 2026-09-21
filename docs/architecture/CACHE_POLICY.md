# Cache Policy

> ⚠ 下表**生产缓存 = 前 3 行**（UPM dense cache /
> Gaia 查询缓存 / Drizzle geometry cache）；**Browser tile cache 属「工具分类（非发布）」**，
> 不是生产缓存（最高设计 §7.1/§10.1：HiPS Browser 不进产品 manifest）。

## 缓存清单

| 缓存 | 容量 | 身份 | 失效 | 线程 |
| --- | --- | --- | --- | --- |
| UPM dense cache（文件） | 受磁盘限制 | model_hash + target_order | 打开时 source_hash 校验，stale=2 | 单写/并发读安全 |
| Gaia 查询缓存 | 有界/键化 | 查询键精确匹配（见 lib/infrastructure/gaia_xpsd_client/src/gaia_client.c:键精确匹配锚点 — ra/dec/radius/mag_low/mag_high + db_type/file_count + version） | 事务性替换 + 键校验 | 互斥 |
| Drizzle geometry cache | 8192 (LRU, lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.h:303 TargetGeomCache(capacity=8192) 默认, 侵入式双向链表 LRU + unordered_map 索引, 命中路径 O(1)) | target ipix (TargetPixelGeometry {center,boundary4}) | 每次 drizzleTiled run 起始 clear()，随模型/NSIDE 重建 | 线程私有 (thread_local 禁止跨线程共享, 命中/未命中 hits()/misses() 可观测) |
| ~~Browser tile cache~~ **工具分类（非发布）** | 页面级 | tile ipix | LRU/容量 | 后台线程 —— HiPS Browser 是未来可视化组件，**不进产品 manifest**（最高设计 §7.1/§10.1），**不是生产缓存** |

## 规则

- cache 必须 capacity + identity + invalidation + thread model；
- stale cache 必须拒绝（返回显式状态）不得静默重算为有效；
- cache 键不得依赖路径/容器遍历顺序。

## 契约

ENG-CACHE-001..002（S2 注册）。
