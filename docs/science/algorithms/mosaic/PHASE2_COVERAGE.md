# 球面产品覆盖并集与层级聚合

本页从[统一科学模型](../../unified/UNIFIED_SCIENCE_MODEL.md)、[背景模型](../../sky/UPM.md)及[Mosaic管线](../../../detail/mosaic/pipeline.md)定义输入产品的几何并集和离散父聚合。完整六个C函数、五种结构、实际字段/容量/错误见[Coverage详细设计](../../../detail/mosaic/modules.md#coverage)。coverage是几何登记，support是样本支撑，validity/accepted是数据资格，三者独立；它们都不凭名称取得统计逆方差意义。

## 集合模型、层级与分辨率

每帧已发布HiPS在本地产品层级提供一组NESTED tile单元T_f。对order o的像元单元C(o,p)，p∈[0,12·4^o)，NESTED每级将父单元分为四子单元，子号可写p_child=4p_parent+q、q∈{0,1,2,3}。连续退s级即parent(p,s)=floor(p/4^s)=p>>(2s)，这是整数精确包含关系，不依赖天空小角近似。

当前ACSD的properties.hips_order与AIO枚举Npix用于tile层级；512×512 tile的像元叶阶是tile_order+9。coverage的max_leaf_order沿旧名保存hips_order，target_order也是该tile层级；下游sampler以target_order+9求像元中心。文档、产品schema和标准互操作时须检查各自定义，不能从变量名把两个order混为一谈。

现策略选o_target=min_f(o_f)，构造

\[
 U=\bigcup_f\{p\mathbin{\gg}2(o_f-o_{\rm target}):p\in T_f\},
 \qquad\Omega_{\rm tile}=\bigcup_{p\in U}C(o_{\rm target},p).
\]

输出对U升序去重，所有cell.order=o_target。min是当前分辨率/执行政策，不是由集合论必然推出的唯一目标阶；不把更细插值说成新增观测信息。

当输入细tile仅覆盖某个粗父单元的一部分时，父聚合输出覆盖整个父单元，因此Ω_tile相对精细足迹可能是**外包络**。不同细tile映到同父并不能证明两帧有真实像元重叠。已有tile也可能含NaN/内孔/support不足，几何并集不能充当背景可用重叠图或有效像元并集；这些由sampler逐位置确认。

该接口输出单一order的cell集合，并非完整多阶MOC规范化器。一般MOC可用多阶紧凑表达同一天区；本函数没有把四兄弟继续合成任意多阶最小表示，也没有输出intersection/depth/missing-tiles层。若要定义覆盖深度n(p)=Σ1[p∈Ω_f]或交集∩Ω_f，应保留逐帧集合另算，不能从去重U独自恢复。

## 产品解释与输入有效域

inspect_frame经统一AIO打开signal产品，读取properties与tile列表，不读信号值去重新检测星点、拟合WCS或Drizzle。当前入口要求：

| 属性 | 当前行为及科学解释 |
|---|---|
| hips_order | atoi转换后非负，AIO另有0–29范围检查；严格词法和跨消费者安全上限另核 |
| hips_tile_width | 必须512，是当前产品实现约定，不能表述为IVOA禁止其它2次幂宽度 |
| hips_version | 非空；不是严格版本协商，更不能拿属性字符串冒充标准编号 |
| hips_frame | 单帧接受equatorial/icrs，跨帧按字符串全等；不执行坐标系转换，自行/历元不由ICRS名称解决 |
| hips_ordering | 显式声明必须NESTED；缺键当前登记空串，AIO已有NESTED解释，不能假装测得ordering |
| obs_filter | 键必须存在，空串表示未声明；值不同允许混合，原文“跨帧filter全等拒绝”已过时 |

每帧通带身份保留给最终响应说明；星光通带差/未知星云SED属于模型解释限制，不是覆盖集合无法求并集。不能以首帧filter替全马赛克物理响应。当前frame字符串全等是软件检查，equatorial/icrs两名的兼容归一要在正式adapter决定，不能由本页悄悄放宽实现。

parse_props按换行拆、首个等号分key/value，首字符#跳过；两端只去空格和尾CR，重复键后值覆盖前值，非通用完整properties语法。8192字节缓冲、atoi宽松解析、字符串截断及AIO错项跳过均需对损坏输入独立检验。返回0只证明当前检查通过，不证明所有物理元数据充分。

## 完整离散步骤与两遍协议

```text
build(paths,N,out):
  if out null: return 1
  save out.inputs and out.union_cells
  memset(out); restore both pointers
  if paths null or N==0: error="no inputs"; return 1  # status currently stays 0
  allocate frame_tiles[N] and infos[N]; target=-1; frame_ref unset
  for f=0..N-1:
    reject null/empty path with status1
    open signal via AIO; reject failed open/properties/required fields
    collect each successful tile_ipix; current failed per-tile reads are skipped
    save path/basename/filter/frame/ordering and tile count
    if frame_ref unset: frame_ref=frame
    else require frame==frame_ref
    target = minimum valid input hips_order seen
  require target>=0
  append each p >> (2*(o_f-target)) from every frame
  sort all appended integers, unique -> U
  set out.n_inputs=N, target_order=target, n_union_cells=|U|
  if out.union_cells not null: write all |U| cells (target,p)
  if out.inputs not null: write all N frame infos
  status=0; return 0
```

先以空指针probe得N/K，再分配后fill；每次全量重扫且没有快照缓存。**build不保存或校验调用者给的容量**，若两遍间输入增长或缓冲不够会直接越界写，不能把协议称容量安全。调用者必须冻结产品快照并提供足量缓冲；改接口/容量保护属生产实现后续工作，记录E71。结构基本错误返回与status不同，消费者不能仅看status。

独立缓冲无所有权转移，free仅清POD；完整尺寸、字符串容量、资源及其余五个函数分支由detail拥有。主入口并无内部取消或通用异常屏障；所有std容器分配都不能由C链接自动获得不抛异常保证。

## 辅助分类和边界归属

coverage_support_classify先看coverage_known/support_known；任一未知就是UNAVAILABLE，否则coverage0为UNCOVERED，之后按有效帧支撑计数和min_support_frames分SUPPORTED/UNSUPPORTED。unknown不能零填成“已知无覆盖/支撑”，支撑阈只决定资格，不产权重。

boundary_owner用同一父移位取唯一owner；leaf_shift须0–31，非法UINT64_MAX只是错误哨兵，不验证leaf_ipix物理有效。deterministic_reduction_order对输入集合升序去重；它有真正capacity参数，空间不足返回错误且回真实需求，与build的未检查行为分开。

weight_source_token_reject只作18个禁用来源token的ASCII忽略大小写全等拒绝，不合成权重，未命中也不证明新来源科学正确。点源I_F、背景control_ivar与面亮度精度的定义由[噪声分册](../../noise_snr/NOISE_SNR.md)拥有，不能把旧头注释w=SNR²/F_ref²直接当任意面亮度权重；support/帧覆盖数只提供几何/资格。

## 复杂度、精度与边界

令T=Σ_f|T_f|为去重前总tile数、K=|U|，N为输入帧数。一次读盘/收集O(T+N)，排序O(T log T)，不是O(K log K)；frame_tiles与待排union都在内存中，峰值O(T+N)，不是O(max_f|T_f|+K)。两遍总工作约两次扫描/排序；AIO目录/MOC的内部成本另计，不能用固定8192字节解释全部I/O内存。

在合法order和索引范围内父移位/排序没有浮点误差，集合值可精确比较；外包络分辨率误差、MOC/目录不完整、缺tile、未知ordering和产品读取错误是不同误差。整数精确不证明足迹物理正确，等长数组也不证明同run/天区。

NESTED合法o,p要求0≤p<12·4^o且位移<64；下游像元阶又加9，不能只借AIO的tile_order≤29就保证后续位移安全。主build未逐项重核tile编号，sampler已有首项/逐cell检查不能反证上游所有路径安全。输入和所有中间对象计数须可表示，所有失败明确记录。

模块当前串行，独立对象可重入；同样输入快照下集合/排序与枚举顺序无关，不据此声称未知文件改动、读取错误或跨平台原始POD填充字节也逐位相同。保序/跨worker比较应按语义字段，非对未初始化padding做memcmp。

## 独立验证和既有证据

参考实现直接用独立FITS/MOC reader枚举构造的产品与天空方向，检验NESTED父包含和集合，而不是复制生产读盘/解析路径。两侧都照抄同一移位式只验证实现一致，另需astropy-healpix或HEALPix原库按sky检查父子方向与手性。

解析例：同阶相邻两个单元给K2；重复同单元双帧给K1；混阶7/8只在细cell的父号等于粗cell时K1且target7，否则K2。不同细cell同父但精细不相交的反例检验“粗同父不证明真实重叠”。空合法tile表给K0，输入空列表是错误，二者分开。

原F1–F6设计仍有价值：固定seed2–8帧/混阶3–8/断连与跨面；probe/fill及重复；缺属性/非512/非法frame/显式RING；单输入与空MOC；所有错误的rc/status；同快照不同枚举/worker及I/O成本。当前不同filter/空filter合法，不延用旧filter-mismatch负例；新增两遍变化/小容量、越界order、tile读取失败和异常资源负例。只有已存在且实际执行的测试才报告通过。

旧RealHipsUnion和FilterMismatchRejected、target7、Fatduck本机路径/GTEST_SKIP与源码行数都属于当时登记；当前所查树没有lib/algorithms/coverage/tests目录，不能把它们当现成可运行测试。原始任务/测试登记全文保留[既有来源档案](../../../../实验/documentation-refactor/docs/MOSAIC_REDUCERS_SOURCE.txt)，本轮未构建或执行Coverage/真实HiPS；科学几何与接口静态核对分别报告。

## 标准、原始论文和参考代码

1. Górski K. M. et al. (2005), *HEALPix: A Framework for High-Resolution Discretization and Fast Analysis of Data Distributed on the Sphere*, ApJ622,759–771，[论文](https://arxiv.org/abs/astro-ph/0409513)。对应层次等面积像元与NESTED；源码见[HEALPix项目](https://healpix.sourceforge.io/)，独立使用时冻结版本及许可。
2. Fernique P. et al., [IVOA HiPS 1.0，2017-05-19](https://www.ivoa.net/documents/HiPS/)：层级产品和存储标准；本库tile_width512/坐标实现域只是当前实现政策。
3. Fernique P. et al., [IVOA MOC 2.0，2022-07-27](https://www.ivoa.net/documents/MOC/)：多阶覆盖表达。基线引用MOC1.0的历史仍保留，当前固定单阶tile集合不自动取得全部MOC2.0的时空/规范化能力。
4. [astropy-healpix](https://astropy-healpix.readthedocs.io/en/latest/)与[Astropy FITS](https://docs.astropy.org/en/stable/io/fits/)为独立读取/sky对照，冻结版本、ordering和单位，采用时核对BSD类许可证。集合结果与AIO使用同库的自读自写不是同等级证据。
5. Tarjan (1972), SIAM J. Comput.1,146（DOI10.1137/0201010）及Hopcroft & Tarjan (1973), CACM16,372属于图连通算法参考；本coverage模块不执行背景重叠图分量求解，引用不能当它已完成连通性/可辨识性证明。
