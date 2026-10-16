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
