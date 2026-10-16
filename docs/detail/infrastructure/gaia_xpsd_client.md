# 本地 Gaia XPSD 查询

本模块离线解析 Gaia DR3/DR3SP 本地 XPSD，供天测与测光使用。查询几何、量化与数值风险见[Gaia 算法](../../science/algorithms/common/GAIA_QUERY.md)，坐标历元见[天体测量](../../science/detection/ASTROMETRY.md)，资源规则见[缓存](../../engineering/standards/CACHE.md)。

## 1. 实际入口与消费者

公开头为 `lib/infrastructure/gaia_xpsd_client/src/gaia_client.h`，实现为同目录 gaia_client.c。`gaia_client_create[_ex]` 建立 client，cone_search、cone_search_for_solver、cone_search_with_spectrum、cone_search_with_photometry 和 query_spectrum_by_coords 分别返回不同字段；destroy 回收 mmap、块缓存与查询缓存。

模块 DLL 的 module_entry.c 经 acsd_module_query_v1 提供描述、配置、规划和执行适配；CLI 构建内调用与该动态入口分别核对，见[模块地图](../../engineering/architecture/MODULE_MAP.md)。IPV 使用求解候选，WCS 后测光使用位置/光谱匹配；查询器不执行板解、SIP或合成测光。

## 2. 输入、输出与单位

| 接口对象 | 合同 |
|---|---|
| data_dir/db_type | 本地目录与 AUTO/DR3/DR3SP；没有网络下载或在线回退 |
| ra/dec/radius_deg | 天球方向与锥半径，度；有限性、范围和RA回绕按入口核对 |
| mag_low/mag_high | G星等窗，mag；量化边界不声称逐位精确 |
| match_radius_arcsec | 按位置光谱匹配半径，角秒 |
| GaiaStar | ra/dec、G/BP/RP、parallax、pmra/pmdec、source_id；ICRS与历元分开解释 |
| 光谱行与字节数组 | 样本 `flux=byte×flux_mul+flux_min`，W m⁻² nm⁻¹；波长由 get_spectrum_params 查询，不硬编码343点 |
| out_match_idx | 与请求位置关联的索引，未匹配与空结果分别处理 |

结果容量和计数与头文件对应。无光谱不等于真实零光谱；DR3字段缺省/哨兵不能按0mag或0误差解释。星表位置的参考历元与观测时刻须在消费者保留，不能因注释写 J2000 就省略自行与历元核对。

## 3. 缓存与并发

查询键包含请求的双精度方向、半径、星等窗及当前数据集相关字段和缓存版本；具体比较方式按 query_cache_lookup 核对，不能仅凭注释称浮点逐位身份。查询缓存和块缓存为进程内存，容量、TTL、共享字节预算由代码配置；不把客户端绑定等同于完整内容哈希身份。

缓存命中在 client 锁内复制结果，成本随返回行数增长。文件分片有各自 collector/scratch；外层并发调用与同一文件缓存的同步需单独验证。destroy 必须等待在途查询结束，不能凭缓存锁推断释放安全。

头文件已有 gaia_set_worker_lease 和 gaia_set_cancel_checkpoint 注入；文件循环检查与调用者实际接线分别核对。无注入的直调路径可使用旧OpenMP默认team，不等于所有调用已遵守Runtime租约。模块不另建私有线程池，trace遵循唯一[开发开关](../../engineering/architecture/DEV_MODE.md)。

## 4. 完整性、所有权与错误

当前 MAX_FILES=32，单文件 collector 有 MAX_STARS_RESULT=200000 上限。超结果容量按已记录截断语义解释，不能把“查询成功”称所有恒星都已返回。目录装载失败、超文件数、记录损坏、解压失败、分配失败、取消和真实无结果分别报告。

接口结果数组由库分配，现有调用方按头/实现约定 free；跨DLL时需特别核对CRT和分配器，不把该legacy约定推广为任意模块ABI规则。client 的create/destroy成对，借用输入数组覆盖同步调用。GaiaCreateDiagnostics提供最近线程的装载计数/首个失败；快照在后续create时可能更新，调用方及时复制。

当前查询通常0成功、−1失败，空结果可成功；ABI错误码与进程退出码分别映射，不能因数值相同就套用CLI表。空目录的跨平台返回还需实际测量，上层必须检查真实装载数。错误时清空输出，不把残缺结果当完整。

## 5. 验证

独立解包微型XPSD并球面全枚举，检查赤道/极冠、RA回绕、星等量化边界、谱解码、坏块、分配失败、容量截断、冷/热缓存和合法释放顺序。剪枝局部推导和有限差分不证明所有锥零漏星；缓存字段一致不证明数据集输入完整。具体命令、原始字节、分母和失败记录随实验保存，本页没有新运行结论。旧参数表与实现自陈完整保留于[来源档案](../../../实验/documentation-refactor/docs/SPATIAL_NOTES_SOURCE.txt)。


## 6. 原始 191 篇内容保全：缓存、剪枝及对象寿命的函数级复核（2026-10-17）

本节从 `63eccbb6:docs/detail/infrastructure/gaia_xpsd_client.md` 原文 147 行逐项归纳，经现行 `src/gaia_client.{c,h}` 关键函数与 [Gaia 查询算法](../../science/algorithms/common/GAIA_QUERY.md) 对照，恢复当前正文曾省略但仍有实现依据的边界细节。**以下为静态源码合同，不是新执行的 PASS；旧原文的“已证明零漏星/缓存逐位等价”主张在缺独立 oracle 时不能照抄。**

### 6.1 两级缓存的真实限制及准确性

| 对象/函数 | 从旧文恢复、经现行代码核实的事实 | 风险/未被证明的属性 |
|---|---|---|
| `query_cache_lookup/insert`（`gaia_client.c` 819/869） | 当前 `GAIA_CACHE_VERSION=2`；64 条槽位；TTL=60 s；字节预算 `64*200000*3*sizeof(double)=307200000` B，仅 3 列 RA/Dec/mag；按 last_access 时间做 LRU，先分配复制新三数组，确认成功后淘汰并插入；内存压力可不缓存 | 使用 `==` 逐项比较输入 `double`，**不是 bitwise** 比较：`-0.0==+0.0`，NaN 不会命中；不能仅凭原注释宣称逐位 identity。输出缓存另需冷/热实测 bitwise 及负零/NaN 的语义裁决 |
| 同一 QueryCache 身份 | 比较 `ra/dec/radius/mag_low/mag_high` 数值、`db_type_detected` 和 `file_count`，并检查版本/过期。query result 是内部缓存持有的三列数组，由调用链在锁内复制后才能给调用者长期使用 | `db_type + file_count` **不等于星表内容身份**：相同文件数但文件内容改变时，键本身无法区分。进程内 client 若绑定不可变数据集应明确此先验，并通过新建 client/内容哈希切换，不能无依据写“任意数据集变更即缓存失效” |
| `block_cache_init/lookup/insert`（596/672/772） | 每个 XPSD 文件一个开放寻址哈希表，8192 槽；`BlockCacheBudget` 对同一 GaiaClient 下全部文件共享 **4 GiB** 总数据预算；file `bc_lock` 后获得 budget lock，预留总字节时不跨文件偷取其他锁，已满则可拒绝缓存回到 scratch；内存压力在本文件内淘汰大约 1/4 LRU | 不能再按旧的“每文件 4 GiB”推算共享内存，亦不等于包含 mmap/collector/并行 scratch 的全进程 4 GiB 硬上限；跨文件实际压力、高并发锁序需可执行验证 |
| 装载/输出（`create_ex`，各 collector） | `MAX_FILES=32`；超过或部分文件装载失败须具名拒绝不完整数据集；每文件 `MAX_STARS_RESULT=200000` 是查询截断边界，不能把成功返回说成“全集” | 内部发生损坏叶/解压错误须由 drop ledger fail-closed；独立真实 XPSD 负例及调用者是否检查错误，尚未在本批重测 |

旧原文所述“查询缓存命中 O(1)”不准确：当前 `query_cache_lookup` 最多线性查 64 槽，命中还需由外围锁内复制 O(N_result)；`block_cache_lookup` 为线性探测哈希，均摊可能 O(1)、最坏 O(8192)。此外查询数组的 malloc/free 生存期、client destroy 与在途查询等待必须由调用者匹配，不能把 cache_lock 误当销毁时自动 quiescence。

### 6.2 球面剪枝：哪些界可严谨声明

赤道 `bbox_intersects`（`gaia_client.c` 1054）先比赤纬区间、再计算 RA差（大于180°则取 `360-dra`），`cos(dec)<0.01` 时不进一步剪枝；否则 `dra*cos(dec)>1.2*radius` 时拒绝。这里的 **1.2 是经验裕度**，未从一般球面矩形严格推导零假阴性，不能把旧文「整个实现必无假阴性」作为已证明结论。

极冠 `polar_plane_intersects`（约1108）选择以对应极点为中心的 Azimuthal Equidistant 映射，局部径向伸缩 1、切向伸缩 `θ/sin θ`；在 `θ≤π/4` 子冠用 `C45=π/(2√2)`，否则在半球使用 `C90=π/2` 对查询圆投影给出保守包含半径。边界 `θ_q>45°+r` 时整个极冠不交，`45°<θ_q≤45°+r` 保守不剪；若 `θ_q+r>90°` 也不采用已越出证明域的常数。原文 `|dec|>85°` 属旧描述，当前有效域应以真实 theta 与分支为准；这些局部数学保证**不代替**极冠树布局正确性/解码正确性/赤道路径证明。定义 `GAIA_POLAR_PRUNE_DISABLED` 可只关闭极冠剪枝作 differential reference，最终仍要另写独立全叶枚举 oracle 对照。

### 6.3 实际 API/配置与验证任务

原 147 行的五类操作继续是 `cone_search`、`cone_search_for_solver`、`cone_search_with_spectrum`、`cone_search_with_photometry`、`query_spectrum_by_coords`；输入目录/DB 类型、角度/星等窗口及 match 半径是 C 客户端的真实负担。恢复的确切模块层原则：`gaia_client_create[_ex]` 成功之后由调用者负责 destroy；输出由模块分配、调用方按现有 C 约定释放；各调用返回成功、空结果、目录缺失/加载失败、解码失败、取消、分配失败须区分。文档不将 v1 `acsd_module_query_v1` 同步调用与待实施 ARS-02 v2 的 operation 表混合。

独立回归包含：固定目录与文件内容 hash，测试冷/热、TTL边界、64槽、307200000 B 预算、跨文件 4 GiB 预算及压力、同文件多查询/多线程和 destroy；同样文件数但变更字节的 dataset 反例；RA 359.9→0.1、极冠 44.8°+1° 跨界、极点、宽半径、bbox 经验裕度反例搜寻与完整枚举比较。真实 XPSD 数据/多线程/故障注入在本批均 **NOT_RUN**。原文中的文献 DOI 与科学模型仍在现行算法科学页与原始 blob 中保留；本节不把每条引用已全文核查作为本批成果。
