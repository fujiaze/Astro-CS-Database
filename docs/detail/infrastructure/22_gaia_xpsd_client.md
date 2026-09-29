# 插件文档：gaia_xpsd_client（本地星表查询）

> 上游：ASTROCS_DESIGN.md §8.1（顶层结构）

## 1. 职责与边界

- **职责**：解析 Gaia/XPSD **本地**星表文件（离线、零网络），提供锥形查询（含谱/测光变体）与两级进程内存缓存（查询级 + 块级）、坐标/数据集版本语义，供 platesolve 等消费。
- **不是**：不做解算（platesolve）；不做科学权重；**不联网**——无网络客户端、无外部服务依赖、无在线降级路径（最高设计 §3.3：星表只解析本地星表文件，离线、零网络，科学身份不依赖网络）。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §3.3（星表只解析**本地**星表文件，离线、零网络）、§8.4（`gaia_xpsd_client/` 本地星表解析）、§9（CPU 后端与资源：缓存复用、编排连续性）
- `docs/science/algorithms/GAIA_QUERY.md`（缓存契约/复杂度/并发模型/不变量正本）
- `docs/engineering/CACHE_POLICY.md`（Gaia 查询缓存行）
- `docs/detail/algorithms_phase1/05_platesolve.md`（消费方）
- Gaia DR3 数据模型：Gaia Collaboration et al. 2023, A&A 674, A1（Gaia DR3 发布与内容综述，DOI: 10.1051/0004-6361/202243940）；XPSD 本地编码合同 = `docs/engineering/STANDARDS_REGISTRY.md`（catalog 行）

## 3. 输入/输出数据合同

- **输入**：本地星表数据集目录 `catalog_dir`（GaiaDR3/GaiaDR3SP 文件集）、查询请求（`ra`/`dec`/`radius_deg`/`mag_low`/`mag_high`，谱/测光变体含 `match_radius_arcsec`）、`db_type`（AUTO/DR3/DR3SP）、`max_workers`。
- **输出**：星表行（位置、自行、parallax、光度、`source_id`）、数据集统计（file_count / file_entry_count / fail_count，`gaia_client.c` 暴露面）。
- config 键词表冻结于 `lib/infrastructure/gaia_xpsd_client/include/astrocs/gaia/types.h`（`ASTROCS_GAIA_CFG_KEY_*`）；op 词表：`cone_search` / `cone_search_for_solver` / `cone_search_with_spectrum` / `cone_search_with_photometry` / `query_spectrum_by_coords`（未知 op → PARAM，detail 100）。

## 4. 算法与公式要点

### 4.1 查询键与两级内存缓存

- 缓存键**精确匹配**（不做量化舍入）：`ra`/`dec`/`radius_deg`/`mag_low`/`mag_high`（double 逐位）+ 数据集身份（`db_type`/`file_count`）+ 缓存键版本 `GAIA_CACHE_VERSION=2`；命中 = 同一查询精确重复（正本：`GAIA_QUERY.md` §2.9 汇总表「缓存」行；实现 `lib/infrastructure/gaia_xpsd_client/src/gaia_client.c` 的 `query_cache_lookup`/`query_cache_insert` 与同文件的 `GAIA_CACHE_VERSION=2`）；
- 两级缓存均为**进程内存**实现：
  - **查询缓存 QueryCache**：64 条（`QUERY_CACHE_CAPACITY`），TTL 60 s，总字节上限 307,200,000 B，超限按 LRU（last_access）淘汰，事务性替换（先全分配成功再释放旧条目）+ 版本/过期校验失效；
  - **块缓存 BlockCache**：8192 槽/文件（2^n），模块级总预算 4 GB（`BLOCK_CACHE_MAX_MEMORY`，全部 XPSD 文件共享），内存压力淘汰 1/4 LRU；XPSD 文件 mmap 只读，块按需解压缓存（解压 O(B)、命中 O(1)）；
- 并发：`GaiaClient.cache_lock` 互斥（Win32 CRITICAL_SECTION / POSIX pthread_mutex）包裹查询缓存 lookup/insert；查询串行 + 缓存互斥（并发模型正本 = `GAIA_QUERY.md` §4；`gaia_client.h` 缓存锚注释）。

### 4.2 命中语义

- 同 client 同参数冷路径与缓存路径输出 **bitwise 一致**（`GAIA_QUERY.md` I3 缓存等价不变量）；
- 缓存不改变数据语义：命中与冷路径的输出逐字段一致。

### 4.3 坐标与剪枝

- J2000 坐标契约，与 platesolve 共享 TAN/SIP 坐标约定；锥形查询为球面角距判定（Haversine 余弦定理）；
- RA 环绕与极区保守剪枝：`dra>180°→360°−dra` 归一并以 cos(dec) 缩放判相交；极区 |cos(dec)|<0.01 保守返回相交；|dec|>45° 进入 AE 极冠平面剪枝，|dec|>85° 仍保守（false_negative=0）；
- 数据集身份（db_type/file_count）进缓存键，数据集变更即缓存失效。

## 5. 配置项

| 字段 | 说明 |
|---|---|
| `op` | 操作词表（见 §3） |
| `catalog_dir` | 本地星表数据集目录 |
| `db_type` | AUTO/DR3/DR3SP |
| `ra`/`dec`/`radius_deg` | 锥形查询天区（deg） |
| `mag_low`/`mag_high` | 星等窗 |
| `match_radius_arcsec` | 谱/测光变体匹配半径 |
| `max_workers` | 并行 worker 上限（并行轴 = 文件，1..file_count） |
| `ra_list`/`dec_list` | 批量查询坐标表 |

## 6. 接口/ABI

- 模块导出面 = `astrocs_module_query_v1`（12 个 legacy 符号经 `-DGAIA_EXPORT=` 本地化）；
- 生命周期 = `lifecycle_v1.h` 冻结时序（`module_entry.c`）：query → describe/validate_config/plan → execute；
- platesolve 经本模块获取匹配输入；查询缓存跨查询复用。

## 7. 错误与边界

- 数据集文件缺失/不可读 → 显式失败（统计面 file_load_fail_count / first_failed_path 如实报出）；
- 未知 op → PARAM（detail 100）；config 键缺失/非有限值 → detail 102/103；
- 版本不匹配或过期的缓存条目 → 失效重查，不返回过期数据；
- 全流程无网络 I/O：网络不可用/超时/在线降级路径在本模块不存在。

## 8. 测试与 Oracle

- 已知天区星表 → 与独立星表源交叉（位置/星等）；
- **缓存复用**：同一查询精确重复命中查询缓存；冷/热路径输出 bitwise 一致（I3）；
- LRU/字节预算：超限按最近最少使用淘汰，无无限增长；块缓存 total_memory ≤ 4 GB；
- RA 环绕/极区剪枝无假阴性（false_negative=0）；
- `max_workers=1` 与 N worker 输出一致；mmap 资源面 RSS 结束回落有解释高水位（`GAIA_QUERY.md` §资源）。
