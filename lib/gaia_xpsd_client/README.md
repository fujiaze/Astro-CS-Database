# lib/gaia_xpsd_client — astrocs.catalog.gaia（CAT-GAIA）

> 状态: CONTRACT_READY（CAT-GAIA-DOC 冻结，2026-09-05）｜doc revision: r2
> 本 README 由源码核对后全面重写（CAT-GAIA-DOC）：函数、单位、坐标、dtype、
> shape、invalid、错误、并发、内存、I/O 均以
> `src/gaia_client.h` / `src/gaia_client.c`（唯一生产源）为准；
> 旧版 README 中与源码不符的性能承诺与记录布局已修正或删除。

## 1. 身份

| 字段 | 当前值 |
|---|---|
| MOD ID / DLL target | `MOD-astrocs-catalog-gaia` / 现状产物 `gaia_client.dll`（模块 Makefile）；迁移目标 `astrocs_catalog_gaia.dll`（CAT-GAIA-IMPL 建立，尚未存在） |
| module / ABI / doc revision | `astrocs.catalog.gaia` / C ABI（无版本化 query 入口，迁移缺口） / r2 |
| owner / phase scope | SA-P1-W16 / service（wave W1） |
| 文档状态 | CONTRACT_READY（实现存在，模块化迁移未开始；不声明 IMPLEMENTED） |
| 上游来源 | PixInsight XPSD 格式客户端（历史上游 Gaia-DR3-DR3SP-Client-C，MIT；仅来源说明，非本合同权威） |

## 2. 负责范围

负责：本地 XPSD（Gaia DR3/DR3SP）解析与 mmap 只读加载；锥形搜索
（J2000/ICRS 度，球面角距）；星等窗过滤；BP/RP 测光与 343 点光谱量化解码；
两级缓存；极冠保守剪枝与赤道带 bbox 剪枝。

不负责：WCS/SIP 求解与像素投影（plate_solve/ipv）；星等自适应迭代与
F_syn 积分（photometric_calib）；星匹配/求解；网络访问（结构性零网络）；
数据目录整理/下载；全局线程池/ThreadLease（现状用 OpenMP 默认 team，
迁移后由 host 授予）；artifact store 写入；Phase 级行为（禁止整 Phase 编排）。

## 3. 输入与输出（DATA-GAIA-001）

输入 port：`catalog.xpsd_dir`（本地目录路径，≤32 个 `.xpsd`，魔数 `XPSD0100`，
LZ4 或 zlib+shuffle 压缩块；GaiaDR3 32B 记录 / GaiaDR3SP 384B 记录）。
模块不写输入文件、不联网。

输出行（单位/坐标/dtype/shape/invalid 全表见 docs/contracts/DATA_SEMANTICS.md §8）：

- `gaia_client_cone_search` → `GaiaStar[out_count]`：ra/dec/magG/magBP/magRP
  float64（deg, ICRS J2000 / mag）、parallax/pmra/pmdec float32
  **未初始化（禁用）**、source_id int64 恒 0（占位）。
- `cone_search_for_solver` → `out_ra/out_dec` float64[n]、`out_mag` float32[n]
  （内部固定 mag_low=-1.5）。
- `cone_search_with_photometry` → `GaiaPhotometryStar[n]`（5×float64；DR3 数据
  BP/RP=0 sentinel）。
- `cone_search_with_spectrum` / `query_spectrum_by_coords` →
  `GaiaSpectrumStar[n]`（ra/dec/magG float64 + flux_min/flux_mul float32）
  + `uint8 out_spectra[n × spec_n]` 行主序（`F(λ)=byte*flux_mul+flux_min`
  W·m⁻²·nm⁻¹，λ=spectrum_start+j·spectrum_step nm）+ match_idx int32
  （−1=未匹配，仅 by_coords）。
- `out_count=0`（含 NULL 数组）= 合法空结果；单文件候选 >200000 静默截断。

## 4. 合同链接

- SCI: `SCI-AST-001`（docs/science/ASTROMETRY.md）
- ALG: `ALG-GAIA-001`（docs/algorithms/GAIA_QUERY.md）
- DATA: `DATA-GAIA-001`（docs/contracts/DATA_SEMANTICS.md §8）
- API: `API-GAIA-001`（docs/contracts/PUBLIC_API.md §gaia_client C API）
- ARCH: `ARCH-001`（docs/contracts/ARCH-001.md）
- TEST: `TEST-GAIA-DESIGN-001`（GAIA_QUERY.md §5，设计冻结；可执行
  TEST-GAIA-* 由 CAT-GAIA-TEST 建立，当前未实现）
- 追溯: docs/traceability/TRACEABILITY_MATRIX.json `MOD-astrocs-catalog-gaia`

## 5. 实现事实（源码核对）

- Public entry（12 个 `GAIA_EXPORT`，签名以 src/gaia_client.h 为唯一权威）：
  `gaia_client_create`、`gaia_client_create_ex`、`gaia_client_destroy`、
  `gaia_client_cone_search`、`gaia_client_cone_search_for_solver`、
  `gaia_client_get_db_type`、`gaia_client_get_file_count`、
  `gaia_client_get_total_sources`、`gaia_client_cone_search_with_spectrum`、
  `gaia_client_query_spectrum_by_coords`、
  `gaia_client_cone_search_with_photometry`、`gaia_client_get_spectrum_params`。
- 主要内部符号（迁移清单）：`load_xpsd_file`、`close_xpsd_file`、
  `search_recursive`、`search_recursive_spectrum`、`search_recursive_photometry`、
  `bbox_intersects`、`polar_plane_intersects`、`unproject`、`read_leaf_block`、
  `lz4_decompress`、`byte_unshuffle`、`block_cache_lookup`、`block_cache_insert`、
  `query_cache_lookup`、`query_cache_insert`、`check_memory_pressure`、
  `collector_push`/`spec_collector_push`/`phot_collector_push`。
- 返回码：查询族 `0`=成功（含 0 结果）/`-1`=参数错误或分配失败；
  `get_spectrum_params` `1`=有光谱/`0`=无；`create*` 失败=NULL。
- 测试钩子（仅测试编译定义生效）：`GAIA_ALLOC_TEST`（malloc/calloc/realloc/
  free 包装注入）、`GAIA_POLAR_PRUNE_DISABLED`（极区剪枝 differential
  reference mode）；诊断 `ASTROCS_GAIA_TRACE=1`（stderr，per-query 统计）。
- DR3SP 记录布局（源码 1378-1387 / PCL EncodedStarSPData）：32B 头
  （dx@0 u32、dy@4 u32、magG_raw@20 u16、magBP_raw@22 u16、magRP_raw@24 u16、
  dra_raw@26 i16）+ flux_min f32@32 + flux_mul f32@36 + uint8 spectrum[343]@40
  + 1B 填充 = 384B（旧版"28-39 保留"与 fluxMin/fluxMul 冲突，以本条为准）。

### 5.1 配置 schema（迁移合同；现状为 C 参数直传，无配置文件）

| 键 | 类型 | 默认 | 说明 |
|---|---|---|---|
| catalog.data_dir | string | 必填 | XPSD 数据目录 |
| catalog.db_type | enum | auto | auto/dr3/dr3sp（GaiaDbType 0/1/2） |
| catalog.ra / dec / radius_deg | double | — | 查询锥，度，有限值 |
| catalog.mag_low / mag_high | double | −1.5 / 数据上限 | 闭区间 |

线程/workers 不属于本模块配置（host ThreadBudget 授予，迁移后强制）。

## 6. 并发与资源

- 并行轴=文件：`#pragma omp parallel for schedule(dynamic)`，每文件独立
  collector/scratch/块缓存单写者；输出按文件序串接。
- 互斥：client 级 `cache_lock` 包裹查询缓存 lookup/insert；命中路径持锁完成
  O(N) 输出构造（命中查询串行化）。
- lease：现状 OpenMP 默认 team（**未接入 host ThreadLease，迁移缺口**）；
  min/max workers 迁移后为 1..file_count（≤32）。
- 内存：mmap 全部 XPSD 只读 + 解压块缓存 ≤4GB（8192 槽 LRU 1/4 淘汰）+
  查询缓存 64 条/60s TTL + 每线程 scratch max(max_block_size, 65536)；
  可用内存 <4GB 时自适应停止缓存（check_memory_pressure）。
- I/O：目录枚举（Win32 FindFirstFileA / POSIX opendir）+ 只读 mmap
  （MapViewOfFile / mmap）；按需解压；trace 走 stderr；无网络、无写盘。
- cancel/checkpoint：无取消检查点（迁移后=文件循环边界，host 传播）；
  数据目录内容在 client 生命周期内不得变更（缓存不检测文件变化）。

## 7. provider 能力与 fallback

纯 C99 + OpenMP + zlib/lz4，无 CPU provider/ISA 分层；不使用 AVX 编译选项
（历史 Makefile `-march=native` 仅为上游本地构建，非 AstroCS 生产 target）。
fallback：无（baseline 单路径）；ISA 合同=迁移 bitwise 等价。

## 8. 验证（TEST-GAIA-DESIGN-001）

- 设计已冻结（GAIA_QUERY.md §5）：合成 fixture（固定 seed，4 树/LZ4+Zlib/
  含极区与 RA 环绕）、独立 Python oracle（暴力全枚举 + 标准解压库）、
  不变量 I1 无假阴性 / I2 无假阳性 / I3 缓存 bitwise 等价 / I4 星等闭区间 /
  I5 截断上限、负面（NULL、坏魔数、GAIA_ALLOC_TEST 5 失败点）、1/N worker、
  ISA bitwise、资源（RSS 回落、块缓存 ≤4GB）。
- 冻结容差：迁移等价 bitwise；oracle 数值位置 |Δ|≤5e-10 deg、星等
  |Δ|≤1e-9 mag、光谱字节恒等。
- **当前无可执行测试、无 PASS 声明**（CAT-GAIA-TEST 建立；历史 V18R3 Gate
  结论为上游/前代证据，不作为本轮验收）。

## 9. 构建与已知限制

- 构建（现状，Linux 技术预览）：
  `make -C lib/gaia_xpsd_client`（gcc -O2 -march=native -fopenmp -lz）；
  Windows MSVC/MinGW 命令见历史上游说明。**无 CMake target**（BLD-001 显式
  target 缺口，CAT-GAIA-IMPL 建立 `astrocs_catalog_gaia` DLL + adapter）。
- 已知限制/未实现（如实登记，不得静默使用）：
  1. C ABI adapter / 版本化 query 入口 / plan()/cancel() 不存在；
  2. OpenMP 默认 team，未接 host ThreadLease；
  3. `GaiaStar.parallax/pmra/pmdec` 输出未初始化；`source_id` 恒 0；
  4. 空数据目录 Windows 返回 NULL、POSIX 返回空 client（平台差异）；
  5. 单文件 200000 星静默截断；
  6. 缓存不检测数据文件内容变化（目录内容 client 生命周期内须不变）；
  7. NaN/Inf 参数不显式校验（前置条件：有限值）；
  8. 可执行测试未建立（设计冻结于 TEST-GAIA-DESIGN-001）。
- 平台范围：Windows x64（生产）/ Linux amd64（当前构建验证）；最后核对
  基线：CAT-GAIA-DOC 于源码 2255 行逐函数核对（run/local/agent_cat_gaia_doc/）。

## 10. 许可

MIT（历史上游 Gaia-DR3-DR3SP-Client-C；本仓库内修改遵循 AstroCS 合同）。
