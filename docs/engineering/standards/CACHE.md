# 缓存策略

上游：最高设计的软件架构与 CPU 后端与资源两章。

生产缓存的清单与「容量、身份、失效、线程模型」四要素的正本。

本清单只登记生产缓存。满足「容量 + 身份 + 失效 + 线程模型」四要素的对象才可成为缓存（最高设计 ）。HiPS Browser 属工具分类（非发布，最高设计 ），其页面级瓦片缓存不在本清单。

## 缓存清单

| 缓存 | 容量 | 身份 | 失效 | 线程 |
| --- | --- | --- | --- | --- |
| UPM dense cache（文件） | 受磁盘限制 | model_hash + target_order | 打开时 source_hash 校验，stale = 2 | 单写 / 并发读安全 |
| Gaia 查询缓存 | 有界 / 键化 | 查询键精确匹配（`query_cache_lookup`，`lib/infrastructure/gaia_xpsd_client/src/gaia_client.c`：ra/dec/radius/mag_low/mag_high + db_type/file_count + version） | 事务性替换 + 键校验 | 互斥 |
| Drizzle geometry cache | 8192（LRU；`lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.h` `TargetGeomCache(capacity=8192)` 默认，侵入式双向链表 LRU + unordered_map 索引，命中路径 O(1)） | target ipix（`TargetPixelGeometry {center,boundary4}`） | 每次 drizzleTiled run 起始 clear，随模型 / NSIDE 重建 | 线程私有（thread_local 仅本线程；命中/未命中 `hits`/`misses` 可观测） |

## 规则

- cache 必须具备容量、身份、失效与线程模型四要素；
- stale cache 必须拒绝（返回显式状态）；重算结果经显式状态标记后方为有效；
- cache 键只由目标身份与配置版本决定，与路径、容器遍历顺序无关。

## 机器判据

缓存身份 / 失效 / 线程模型的可核面 = `lib/infrastructure/gaia_xpsd_client/src/gaia_client.c`（查询键与事务性替换）、`lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.h`（LRU 容量与线程私有）、`lib/algorithms/resample/p3_resample.cpp`（P3 tile 负缓存与热缓存，见 `../architecture/DATA_FLOW.md` ）。偏差登记面 = 已知限制台账。

## 外部查询合并判据

对外部星表服务的同组查询，一次运行内的**外部请求计数为 1**：查询在客户端侧合并，缓存为两级（运行内复用 + 落盘复用）。

- 该计数是 L2 合成性能验收的缓存复用判据（../testing/VALIDATION_EVIDENCE.md ），与命中率一同归档；
- 判定看的是**外部请求数**而不是缓存命中率：命中率高而外部请求数大于 1，说明查询未合并，属性能缺陷；
- 两级缓存各自满足本文件「缓存清单」的四要素；失效策略不得让外部请求数随帧数线性增长。

## 参考文献

[1] 内部文档 `docs/ACSD_DESIGN.md，最高设计`，上位来源。
[2] 内部文档 `docs/engineering/architecture/ARCHITECTURE.md`，同层相关正本。
