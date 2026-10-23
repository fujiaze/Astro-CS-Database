# catalogue.access：本地目录与历元传播

状态：目标process文档；对应科学函数可已有部分实现，标准注册/生命周期/探针尚未据本文落地。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `catalogue.access` |
| 所属模块与代码归属 | `src/catalogue`；本模块负责算法及typed adapter |
| 注册operation白名单 | `build_catalogue_index,make_catalogue_provider,catalogue.query,catalogue.query_quads,catalogue.lookup`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | 离线Gaia/XP源、CatalogueBuildRequest或查询 → CatalogueIndex/Provider/CatalogueStar列表。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；仅保留批量查询接口；GPU索引和传播实现不建设。 |

## 算法与失败合同

索引锁release/calibration/hash；query传播到请求历元，lookup保持源历元；协方差由uncertainty唯一构造。

pmRA*cosDec、parallax/RV和observer缺失条件保留；XP表示误差不变为0。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

按只读sky分片查询；索引构建外存分区，结果去重按source_id固定排序；不隐式联网。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`catalogue.query`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：C01/F01；历元差、负parallax、缺RV、重复source_id与破坏索引hash。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，文档中的角色名不表示头文件现已提供新接口。

## 算法详细条款

### 盲解 catalogue/XP 公开能力与索引

CatalogueProvider.describe给release/calibration、统一XP basis、quad角尺度覆盖和索引完备状态；query/quad-query均分页，page_size取整数1..4096（0表示没有可返回的条目，按invalid_request拒绝），cursor锁dataset hash和完整请求，空cursor起始、空next结束；不同查询复用cursor为invalid_request。query按source_id返回指定SkySearchRegion中传播历元后的星，brightness_mag及XP basis随记录可用。lookup按请求ID顺序返回，缺失给逐ID issue而不挤掉位置。query_quads按source四元组排序返回descriptor各分量区间及baseline范围内的所有已登记quad；regions空表示全索引，非空为明确center+radius并集。算法不downcast provider或读隐藏路径。

本地索引构建入口build_catalogue_index使用CatalogueColumns：source_ids[N] u64，astrometry[N,8] f64，列为ra_rad,dec_rad,epoch_year,pmra_cosdec_rad_year,pmdec_rad_year,parallax_rad,RV_m_s,RV_present(0/1)；无RV数值位填0且present=0。brightness_mag[N]，xp_coefficients[N,d]与共同SpectralBasis绑定，noise按同source根保留。Gaia原生BP/RP的外定标和表示转换由GaiaXPy官方外定标模型离线解码为该统一分段多项式basis；保存release/模型/数值转换误差，不能把sampled插值当无损基函数。若用户只有sampled谱，必须输入其已知表示和协方差，物理模型状态conditional。所有帧使用provider.describe.xp_basis调用integrate_response；不从star coefficient长度猜basis。

构建步骤：按order6分片，每片选最亮64星（brightness、source_id tie），每anchor取该片及相邻片中最近12个候选，每anchor最多16个由三邻点组成的quad（按最小三角面积递减、source tuple tie，排共线）。按基线角域筛选，去重source四元组，保两种镜像规范描述符。KD树以最大spread轴、坐标中位数递归，平手用source tuple；kd_left/right u64[Nq]（无子节点UINT64_MAX），kd_axis u8[Nq]、kd_split f64[Nq]、kd_bounds f64[Nq,4,2]。quad_source_ids[Nq,4]、quad_descriptors[Nq,4]、quad_baselines[Nq]rad、quad_centers[Nq,3]。排序/树构建可外存，配额不足返回resource_exhausted且index_complete=false，production不得悄悄用未完成索引当完整搜索。index_complete只针对此生成规则，不保证任何视场可解。

quad不变式：最长基线端点A,B（长度平手按ID）；e=(B−A)/|B−A|，e_perp逆时针90°；其余C,D的坐标为 ((C−A)·e/|AB|,(C−A)·e_perp/|AB|)，对A/B交换、C/D交换和镜像枚举取字典最小四维向量并保实际置换。catalogue坐标用quad中心TAN平面，观测用detector连续中心；真实投影非仿射差由quad_descriptor_tolerance明确容纳，默认0.01（只作搜索heuristic，不作几何证书）。catalogue基线角范围等于观测最长边pixel×SolverRequest.scale_rad_pixel区间外包。regions定义搜索圆域并集，范围缺失不自行猜半径。搜索穷尽只证明该索引/阈值内未求解，不证明天空无解。query和lookup内存每页预算，source measurements可多遍按source分块。


### 构造与连接细节

- build_catalogue_index(request)/make_catalogue_provider(index)：公开列式输入构建有界离线quad索引/只读provider，不藏路径；metadata与页面/XP basis见本篇算法细则及唯一声明。
  历元语义分两条且必须一致使用：`catalogue.lookup` 返回目录历元的目录位置（按source_id取源，位置是该源自身历元，不做观测历元传播）；`catalogue.query` 返回请求历元（`CatalogueQuery.epoch_julian_year`）的预测位置（含pmRA*cosDec、视差与已知RV的空间运动）。盲解、PSF与测光构造位置假设必须用`query`的观测历元预测位置；用`lookup`的目录历元静态位置会把源的自行当成天测误差（历元差20年、pm 4″/yr时约80″）。两入口的历元差必须显式记入结果的assessment与模型身份。目录不含观测者，发布位置为重心坐标，省略的topocentric项列入assessment.omitted。


### 可复用计算模块组合

目标组合：Storage分片/页read_async → 唯一目录解码/历元模块 → source_id固定去重归并 → typed结果。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。


## 直接开发的实施合同

本process全部operation的Input/Output按[FOUNDATION_API的typed规则](../standards/FOUNDATION_API.md#当前科学操作的typed输入标准)从公开参数机械生成；跨度/provider均转强所有权，codec先register_type再登记typed hooks。bounds给最小/最大块和spill能力，plan显式接受PlanOptions，不能读取全局当前块大小。资源key/不可拆相关组/上界项见[DEVELOPMENT本process行](../standards/DEVELOPMENT.md#模块plan和工作单元表)，具体数学/停止仍由本篇拥有。

有全局拟合屏障时拆stream→ordered_accumulator→barrier region：每cohort提交R内聚合状态即可退F，不能让唯一进展cohort等待下一cohort。科学迭代以显式epoch重新准入，work不重置。取消/失败只允许内建清理继续，模块不提交新科学计算；公开输出只在所有必需任务/写入/校验结束后交付。

实施交付包含所属模块plan/kernel/adapter/codec、独立小例/负例及真实service资源/取消证据，逐项门槛见[DEVELOPMENT](../standards/DEVELOPMENT.md)。完整编排代码可参照[校准实施例](../standards/CALIBRATION_WALKTHROUGH.md)，不得复制其中像元公式到其它process。GPU只预留同一批次/内存位置/完成事件边界，不实现后端。
