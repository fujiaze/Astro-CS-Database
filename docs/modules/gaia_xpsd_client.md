# Module: gaia_xpsd_client（astrocs.catalog.gaia / CAT-GAIA）

> 状态: CONTRACT_READY（CAT-GAIA-DOC 冻结，2026-09-05；本节为事实修订版，
> 全部条目以 lib/gaia_xpsd_client/src/gaia_client.c/.h 为准，旧版与源码冲突
> 的陈述已删除）。

## 职责

- 解析 PixInsight XPSD 格式本地星表（Gaia DR3 / DR3SP，离线、零网络）；
- 锥形搜索（J2000/ICRS，度，球面角距判定）与星等窗过滤；
- DR3SP 光谱量化解码（`F(λ)=byte*fluxMul+fluxMin`）与 BP/RP 测光输出；
- 两级查询缓存（结果缓存 60s TTL 精确键 + 解压块缓存 8192 槽 ≤4GB）；
- 极冠保守剪枝（false_negative=0）与赤道带 bbox 剪枝。

不负责：WCS/SIP 求解与像素投影（plate_solve/ipv）、星等自适应迭代与
F_syn 光谱积分（photometric_calib）、匹配求解、目录整理/下载、线程池创建
（迁移后由 host executor/ThreadLease 授予）、artifact store 写入。

## 合同 ID

- SCI: SCI-AST-001（docs/science/ASTROMETRY.md）
- ALG: ALG-GAIA-001（docs/algorithms/GAIA_QUERY.md）
- DATA: DATA-GAIA-001（docs/contracts/DATA_SEMANTICS.md §8）
- API: API-GAIA-001（docs/contracts/PUBLIC_API.md §gaia_client C API）
- ARCH: ARCH-001（docs/contracts/ARCH-001.md）
- TEST: TEST-GAIA-DESIGN-001（GAIA_QUERY.md §5，设计冻结；可执行
  TEST-GAIA-* 由 CAT-GAIA-TEST 建立）

## Public API

12 个 `GAIA_EXPORT` C 符号（create/create_ex/destroy、cone_search、
cone_search_for_solver、get_db_type、get_file_count、get_total_sources、
cone_search_with_spectrum、query_spectrum_by_coords、cone_search_with_photometry、
get_spectrum_params）；签名/返回码/所有权以 API-GAIA-001 为准。

## Data contract

输入 `.xpsd` 目录（≤32 文件，LZ4/zlib+shuffle 数据块）；输出星表行
（RA/Dec 度 ICRS J2000、mag、可选 343 点光谱 uint8/星）；缓存键精确
（double 逐位 + dataset identity + version=2）。详见 DATA-GAIA-001。

## Ownership

`client` create/destroy 配对；全部 `out_*` 数组模块 malloc、调用方 free；
缓存条目模块内部事务式替换（先分配成功再释放旧条目）。

## Thread safety

文件级 OpenMP parallel for（每文件独立 collector/scratch/块缓存单写者）+
client 级缓存互斥（Win32 CRITICAL_SECTION / POSIX mutex）；命中路径持锁
完成 O(N) 输出构造；destroy 不得与在途查询并发。现状使用 OpenMP 默认
team（迁移后由 host ThreadLease 授予）。

## Errors

- `0` 成功（含空结果）；`-1` 参数错误/分配失败/内部错误；
- `create` 失败返回 NULL（注意平台差异：空数据目录 Windows=NULL，
  POSIX=空 client，file_count=0）；
- 分配故障路径全 checked（V18R3），cache 替换事务式；
- **无网络**：不存在 TIMEOUT/网络错误码（旧版"网络/超时"陈述有误，已删）；
  "network errors never alter scientific identity" 由零网络 I/O 结构性满足。

## 性能特征

四叉树投影索引（非 dec 排序索引）；极冠剪枝可证明保守；单查询
O(树深 + 命中叶块解压 + 输出)；文件级并行；缓存命中近乎零解压。
数量级参考见模块 README（历史实测，非本合同承诺）。

## Tests

- 设计冻结：TEST-GAIA-DESIGN-001（GAIA_QUERY.md §5——合成 fixture、独立
  oracle、不变量 I1-I5、负面/分配注入、1/N worker、ISA bitwise、资源）；
- 可执行测试：未实现（CAT-GAIA-TEST 落地，tests/{unit,properties,oracle,
  fixtures,negative,performance}），当前无 PASS 声明。

## Source files

- lib/gaia_xpsd_client/src/gaia_client.h / gaia_client.c（唯一生产源）
- lib/gaia_xpsd_client/module.yaml / README.md（CAT-GAIA-DOC 冻结）
- 追溯行：docs/traceability/TRACEABILITY_MATRIX.json
  `MOD-astrocs-catalog-gaia`
