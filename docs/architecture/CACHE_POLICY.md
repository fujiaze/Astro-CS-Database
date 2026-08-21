# Cache Policy

## 缓存清单

| 缓存 | 容量 | 身份 | 失效 | 线程 |
| --- | --- | --- | --- | --- |
| UPM dense cache（文件） | 受磁盘限制 | model_hash + target_order | 打开时 source_hash 校验，stale=2 | 单写/并发读安全 |
| Gaia 查询缓存 | 有界/键化 | 查询键精确匹配（见 lib/gaia_xpsd_client/src/gaia_client.c:键精确匹配锚点 — ra/dec/radius/mag_low/mag_high + db_type/file_count + version） | 事务性替换 + 键校验 | 互斥 |
| Drizzle geometry cache | 模型内 | tile/cell 键 | 随模型重建 | OpenMP 只读 |
| Browser tile cache | 页面级 | tile ipix | LRU/容量 | 后台线程 |

## 规则

- cache 必须 capacity + identity + invalidation + thread model；
- stale cache 必须拒绝（返回显式状态）不得静默重算为有效；
- cache 键不得依赖路径/容器遍历顺序。

## 契约

ENG-CACHE-001..002（S2 注册）。
