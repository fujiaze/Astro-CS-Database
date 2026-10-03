# gaia_xpsd_client（本地星表查询，acsd.catalog.gaia / CAT-GAIA）

> 上游：docs/ACSD_DESIGN.md §8.4（顶层结构）、§8.5（模块与 ABI）、
> §3.3（星表只解析本地星表文件，离线、零网络）、§8.4（`gaia_xpsd_client/` 本地星表
> 解析）、§9（CPU 后端与资源：缓存复用、编排连续性）
> 科学正本：docs/science/ASTROMETRY.md（SCI-AST-001）
> 算法正本：docs/science/algorithms/GAIA_QUERY.md（ALG-GAIA-001；§2.9 汇总表
> 「缓存」行、§4 并发模型、§5 测试设计、§资源）
> 数据正本：docs/science/DATA_SEMANTICS.md §8（DATA-GAIA-001）
> API 正本：docs/engineering/PUBLIC_API.md（API-GAIA-001，gaia_client C API 节）
> 架构正本：docs/engineering/ARCH-001.md
> 缓存：docs/engineering/CACHE_POLICY.md（Gaia 查询缓存行）
> XPSD 本地编码合同：docs/engineering/STANDARDS_REGISTRY.md（catalog 行）
> 消费方：registry/acsd.phase1.wcs-platesolve.md
> 引用文献：见文末「参考文献」（角标用全角 `［N］`）

模块级事实以 lib/infrastructure/gaia_xpsd_client/src/gaia_client.{h,c} 为准。

## 1. 职责与边界

- **职责**：解析 PixInsight XPSD 格式的 Gaia DR3 / DR3SP **本地**星表文件（离线、
  零网络），提供锥形查询（含谱 / 测光变体）与两级进程内存缓存（查询级 + 块级）、
  坐标 / 数据集版本语义，供 platesolve 等消费；DR3SP 光谱量化解码（值 = 字节码
  × 增益 + 偏移）与 BP / RP 测光输出；极冠保守剪枝（false_negative = 0）与赤道带
  bbox 剪枝。
- **不是**：不做解算（platesolve）；不做科学权重；WCS / SIP 求解与像素投影
  （wcs-platesolve 域）；星等自适应迭代与合成光谱积分（photometry 域）；匹配
  求解；目录整理 / 下载；线程池创建（迁移后由 host executor / ThreadLease 授予）；
  artifact store 写入。
- **不联网**：无网络客户端、无外部服务依赖、无在线降级路径；「network errors
  never alter scientific identity」由**零网络 I/O** 结构性满足。

## 2. 输入/输出数据合同

- **输入**：本地星表数据集目录 `catalog_dir`（GaiaDR3 / GaiaDR3SP 文件集，≤ 32
  个文件，LZ4 / zlib + shuffle 数据块）；查询请求（`ra` / `dec` / `radius_deg` /
  `mag_low` / `mag_high`，谱 / 测光变体含 `match_radius_arcsec`）、`db_type`
  （AUTO / DR3 / DR3SP）、`max_workers`。
- **输出**：星表行（RA / Dec 度 ICRS J2000、mag、自行、parallax、光度、
  `source_id`，可选 343 点光谱 uint8 / 星）、数据集统计（file_count /
  file_entry_count / fail_count，gaia_client.c 暴露面）。
- config 键词表冻结于
  `lib/infrastructure/gaia_xpsd_client/include/acsd/gaia/types.h`
  （`ACSD_GAIA_CFG_KEY_*`）。

## 3. 查询键、两级内存缓存与剪枝

- 缓存键**精确匹配**（不做量化舍入）：`ra` / `dec` / `radius_deg` / `mag_low` /
  `mag_high`（double 逐位）+ 数据集身份（`db_type` / `file_count`）+ 缓存键版本
  `GAIA_CACHE_VERSION = 2`；命中 = 同一查询精确重复（实现 =
  gaia_client.c 的 `query_cache_lookup` / `query_cache_insert` 与同文件的
  `GAIA_CACHE_VERSION`）。
- 两级缓存均为**进程内存**实现：
  - **查询缓存 QueryCache**：64 条（`QUERY_CACHE_CAPACITY`），TTL 60 s，总字节
    上限 307,200,000 B，超限按 LRU（last_access）淘汰，**事务性替换**（先全
    分配成功再释放旧条目）+ 版本 / 过期校验失效；
  - **块缓存 BlockCache**：8192 槽 / 文件（2 的幂），模块级总预算 4 GB
    （`BLOCK_CACHE_MAX_MEMORY`，全部 XPSD 文件共享），内存压力淘汰 1/4 LRU；
    XPSD 文件 mmap 只读，块按需解压缓存（解压 O(块大小)、命中 O(1)）；
- **命中语义**：同 client 同参数的冷路径与缓存路径输出 **bitwise 一致**
  （GAIA_QUERY.md I3 缓存等价不变量）；缓存不改变数据语义，命中与冷路径的输出
  逐字段一致。
- **坐标与剪枝**：J2000 坐标契约，与 platesolve 共享 TAN / SIP 坐标约定；锥形
  查询为球面角距判定（Haversine 余弦定理）；RA 环绕与极区保守剪枝 —— 经差大于
  180° 归一后以 cos(dec) 缩放判相交；极区 |cos(dec)| < 0.01 保守返回相交；
  |dec| > 45° 进入极冠平面剪枝，|dec| > 85° 仍保守（false_negative = 0）；
  数据集身份进缓存键，**数据集变更即缓存失效**。
- **性能特征**：四叉树投影索引（非按 dec 排序的索引）；极冠剪枝可证明保守；单
  查询复杂度 = 树深 + 命中叶块解压 + 输出构造；文件级并行；缓存命中近乎零解压。
  性能量级参考见 `lib/infrastructure/gaia_xpsd_client/README.md` 的现状观测
  （非合同承诺）。

## 4. 配置项

| 字段 | 说明 |
|---|---|
| `op` | 操作词表：`cone_search` / `cone_search_for_solver` / `cone_search_with_spectrum` / `cone_search_with_photometry` / `query_spectrum_by_coords` |
| `catalog_dir` | 本地星表数据集目录 |
| `db_type` | AUTO / DR3 / DR3SP |
| `ra` / `dec` / `radius_deg` | 锥形查询天区（deg） |
| `mag_low` / `mag_high` | 星等窗 |
| `match_radius_arcsec` | 谱 / 测光变体匹配半径 |
| `max_workers` | 并行 worker 上限（并行轴 = 文件，1..file_count） |
| `ra_list` / `dec_list` | 批量查询坐标表 |

## 5. 接口/ABI

- 模块导出面 = `acsd_module_query_v1`（12 个 legacy 符号经 `-DGAIA_EXPORT=`
  本地化）：`create` / `create_ex` / `destroy` / `cone_search` /
  `cone_search_for_solver` / `get_db_type` / `get_file_count` /
  `get_total_sources` / `cone_search_with_spectrum` / `query_spectrum_by_coords` /
  `cone_search_with_photometry` / `get_spectrum_params`；签名 / 返回码 / 所有权
  以 API-GAIA-001 为准。
- 生命周期 = `lifecycle_v1.h` 冻结时序（`module_entry.c`）：query →
  describe / validate_config / plan → execute。
- platesolve 经本模块获取匹配输入；查询缓存跨查询复用。

## 6. 所有权、并发与错误

- **所有权**：`client` create / destroy 配对；全部 `out_*` 数组由模块 malloc、
  调用方 free；缓存条目由模块内部事务式替换（先分配成功再释放旧条目）。
- **线程安全**：文件级 OpenMP parallel for（每文件独立 collector / scratch /
  块缓存单写者）+ client 级缓存互斥（Win32 CRITICAL_SECTION / POSIX
  pthread_mutex，`GaiaClient.cache_lock` 包裹查询缓存 lookup / insert）；命中
  路径持锁完成 O(N) 输出构造；destroy 与在途查询串行执行。现状使用 OpenMP 默认
  team（迁移后由 host ThreadLease 授予）。
- **错误**：0 = 成功（含空结果）；−1 = 参数错误 / 分配失败 / 内部错误；`create`
  失败返回 NULL（**平台差异**：空数据目录 Windows = NULL，POSIX = 空 client、
  file_count = 0）；分配故障路径全 checked；cache 替换事务式。
  数据集文件缺失 / 不可读 → 显式失败（统计面 `file_load_fail_count` /
  `first_failed_path` 如实报出）；未知 op → PARAM（detail 100）；config 键缺失 /
  非有限值 → detail 102 / 103；版本不匹配或过期的缓存条目 → 失效重查，**不
  返回过期数据**。**无网络**：不存在 TIMEOUT / 网络错误码。
- 错误码与退出码唯一源 = lib/infrastructure/cli/exit_codes.h（本页不复制数值表）；
  域→码映射唯一源 = docs/engineering/LOG_AND_ERROR_CONTRACT.md §5。

## 7. 测试与 Oracle

设计冻结 = `TEST-GAIA-DESIGN-001`（GAIA_QUERY.md §5：合成 fixture、独立 oracle、
不变量 I1–I5、负面 / 分配注入、1/N worker、ISA bitwise、资源）；可执行
`TEST-GAIA-*` 待建（当前无 PASS 声明）。

- 已知天区星表 → 与独立星表源交叉（位置 / 星等）；
- **缓存复用**：同一查询精确重复命中查询缓存；冷 / 热路径输出 bitwise 一致（I3）；
- LRU / 字节预算：超限按最近最少使用淘汰，无无限增长；块缓存总内存 ≤ 4 GB；
- RA 环绕 / 极区剪枝无假阴性（false_negative = 0）；
- `max_workers = 1` 与 N worker 输出一致；mmap 资源面 RSS 结束回落有可解释
  高水位（GAIA_QUERY.md §资源）。

## 8. 已知限制

- 全局限制登记 = artifacts/evidence/known-limitations-ledger/LIMITATIONS.md。

## 9. 源文件

- `lib/infrastructure/gaia_xpsd_client/src/gaia_client.{h,c}`（唯一生产源）；
- `lib/infrastructure/gaia_xpsd_client/module.yaml` + `README.md`（CAT-GAIA-DOC
  冻结）；追溯行 = `MOD-acsd-catalog-gaia`。

## 参考文献

- ［1］ Gaia Collaboration; Vallenari, A.; et al. (2023). "Gaia Data Release 3: Summary of
  the Content and Survey Properties". *Astronomy and Astrophysics* 674, A1.
  DOI [10.1051/0004-6361/202243940](https://doi.org/10.1051/0004-6361/202243940)；
  预印本 [arXiv:2208.00211](https://arxiv.org/abs/2208.00211)
