# 统一对象、身份与持久化

本合同实现[观测科学接口 §7](../../science/foundation/OBSERVATION_COORDINATES.md)及[统一统计](../../science/foundation/STATISTICS_SKY_COADD.md)，保持其公共类型名和数学语义。E02/E03/E04/E07的新增存储与设备角色是迁移目标，本轮未修改当前声明；既有产品不因此自动采用新布局。API 见[标准接口](API.md)，大数组寿命见[计算与资源](EXECUTION.md)。旧统一对象或 JSON schema 不构成新核心的兼容要求。

## 对象信封

所有可引用对象有以下信封，数组不塞入信封 JSON。小坐标为按值类型，不强加 Runtime 登记。

| 字段 | 必需含义 |
|---|---|
| `schema_id, schema_major, schema_minor` | 具名类型及形状版本；major 不兼容拒绝，未知可选字段保留 |
| `object_id, revision, content_hash` | 不可变版本及规范载荷身份；不能用可变路径代表同一模型 |
| `run_id, frame_id?, source_namespace` | 运行来源与观测归属；共享误差身份独立于 run/frame |
| `estimator` | 目标量、linear unit L 的定义、积分/面亮度/参考通量/控制角色、condition |
| `domain` | detector 连续域或 ICRS/NESTED/order、时间/历元、有效及预测支持 |
| `dependencies[]` | 具名输入对象版本/hash、角色、操作及算法/数值 policy 身份 |
| `assessment` | 下述正交状态、included/omitted、模型假设及误差来源 |
| `payload_refs[]` | 带路径/hash/shape/dtype/单位/布局的块引用或具名模型依赖 |

`object_id` 是逻辑实体，`content_hash` 绑定某次不可变载荷；所有调用锁具体 revision/hash。`SourceIdentity=(dataset namespace, exposure identity, detector/channel, native sample id)` 贯穿 G/R/Q，避免跨帧碰巧同整数、重复 tile 读取或同叶分片被误当独立观测。曝光身份由输入内容及真实曝光元数据建立，不能只用文件名。

`shared_id=(external namespace, latent entity, model revision, component)` 表达母版、同 Gaia 星谱、响应、读出公共模等同一个随机变量。root block 具有有序坐标、单位、协方差模型/hash；合并时同 shared_id 若定义不一致是错误，不能通过改 frame_id 消除相关。同图 photometry/sky 参数是观测根的函数，其影响边记录一次，不另创建“独立参数根”。

## 状态与误差分账

| 维度 | 值或必要信息 |
|---|---|
| `execution` | complete / cancelled / failed，具名错误与失败作用域 |
| `capability` | supported / unsupported / unavailable，缺项及影响对象 |
| `termination` | tolerance_met / work / depth / precision / unsupported |
| `bound_validity` | certified（相对于所声明原语/数学模型）/ approximate / invalid；原语及其前提必须在记录中 |
| `support` | absent / positive / bounded_undecided；归一行另记 no_support、support_unresolved、weak_support 阈值及正下界 |
| `science` | bounded / unconverged / conditional / selection_unresolved / geometry_solve_unresolved，逐请求泛函记录 |
| `noise` | known / conditional / approximate / unknown，独立于信号是否可用 |
| `observational_support` | observed / predicted / extrapolated / absent，与纯几何支持分开 |

`certified` 指数值包含合同，不是对 C++、库、物理模型或产品的全域形式化认证。确定数值界、随机 covariance、有限确定模型偏差、无有限界的未知模型项分别保存。随机或模型项没有信息时用显式 null+reason；JSON 禁止 NaN/Inf，图像内部/磁盘 NaN 仅表达其 validity 规定的无值。精确随机零方差合法，并且必须由 PSD 精确约束解释，不等同 unknown。

资源耗尽仍可保留有效面积界，但 science=unconverged。只有下界证明支持才构造认证归一行；下界为零的单独 sliver 不发认证 B。已有可靠正支持时可保留 bounded_undecided 小片并满足最终科学容差。任何 invalid bound 不能因为 termination=work 而被放行。支持及状态都按叶/泛函记录，不能拿单叶成功覆盖全 drop。

## 公共对象登记

| 类型 | 最小载荷及不变量 | 生产者 → 消费者 |
|---|---|---|
| `PixelCenter0 / PixelEdge0 / FitsPixel1` | FP64 x,y；内部中心0、连续边界同坐标轴，外部一基唯一转换；数组 data[y,x] | coordinates；全链路 |
| `SkyICRS / AstrometricContext` | 单位向量、历元/时间尺度、proper motion 约定、观测者及忽略项误差 | catalogue/coordinates → 匹配、几何、Export |
| `WcsModel` | 规范 CD(rad/pixel)、CRPIX(FITS1)、SIP、原始解析数值、原头hash、LONPOLE 转换、域、单射与规范化误差 | coordinates/astrometry → 全部位置与 footprint |
| `AstrometricSolution` | WcsModel引用、拟合参数/随机根、参数covariance或同观测influence、catalogue共享身份、模型偏差/支持域、included/omitted | astrometry → uncertainty、geometry条件、联合MC；外供WCS也须声明误差身份或unknown |
| `DetectorFootprint / FootprintProvider` | 原生收集域有向 rings/holes、PRF、源身份；provider 有点/区间/导数/测度能力等级 | observation/coordinates → geometry、PSF |
| `NativeObservation` | 具名积分 x、DQ、真实收集核、G 重组与 native source index、已应用标度、noise/模型影响 | observation/photometry → geometry、uncertainty |
| `Response / ReferenceSED / SpectralPrediction` | photon/energy、归一、谱域、折点或解析曲线、实测/假设、v、积分误差及 shared spectral factors | response → photometry、混合核 |
| `StarFlux / PhotoSolution` | 总 flux 定义/单位/位置/窗/孔径与 source_id；θ/basis/domain、rank、covariance 与同数据 influence | sources/photometry → apply_scale、noise |
| `EffectivePSF / ObservationKernel` | 归一/尾损失、像元积分、首期achromatic的空间依赖、PRF、G与标度作用；一般 signed G 不称概率 | sources/operators → information、sky、coadd |
| `SphericalAreaInterval / OverlapContribution` | 单位 sr，0≤a≤A≤Ω，原始 primitive/共同分母/每源 Σa=A 的约束身份，未决位置及上界 | geometry → R、预算、Export |
| `GeometryBudget / GeometryStatus` | B/F/Cov/孔径请求及单位/容差、支持需求、逐叶绝对限额、工作/内存/深度/算术；成功作用域 | 工作流/geometry → 数值执行与报告 |
| `GeometryRevision` | base/new snapshot、不可变primitive/R版本、受影响source/leaf/domain及共同约束、替换映射、资源账本和未决状态 | refine_geometry → 预算控制器及下游闭包事务重建 |
| `LinearAction / DrizzleOperator` | 域/陪域索引及单位、apply/transpose、块依赖、R 名义值/允许集合、N、source index、几何约束 | operators → signal/PSF/noise/basis |
| `SignalEstimate` | signal、单位/估计量、numerical_bounds、支持几何、response/error/noise 引用 | operators/coadd/projection → 产品/后续阶段 |
| `NoiseModel / CovarianceAction` | 根块、局部/共享因子、influence 链、condition、未知项、C action；可选绝对因子 action/谱界 | uncertainty → V_F/V_B/V_ctrl、GLS、孔径 |
| `PrecisionControls` | 类型 reference_flux / actual_scene / control_statistic、值/单位、estimator/condition/support、δ/M 来源或预测误差 | information → 同类稀疏重建，不能换名充权 |
| `SkyFit` | T/H/A/basis/gauge、β、K/E/P action、可估子空间、共同模式及额外零空间、参数 covariance、训练/预测域 | sky → corrected frame provider、coadd |
| `ScientificFrameView` | 帧/网格/signal/DQ/support/response/PSF/noise、native_to_leaf、可选 sky 与邻域能力 | products/provider → coadd 与独立驱动 |
| `Selection / WeightSolution` | 原始资格、接受 source 集、原因/阈值、实际 t、Γ身份、策略、数据依赖性、数值/选择状态 | coadd → 合成、有效核、全过程验证 |
| `SpatialResponse` | 实际 Σt c K 的组合 action 或可恢复模型，空间/光谱耦合、可能 signed、条件选择身份 | coadd/projection → 产品解释 |

类型里 `NoiseModel` 的数学信息和 `CovarianceAction` 的执行引用指向同一模型图；不建两个可分别更新的 covariance 正本。`LinearAction` 允许无显式矩阵，但必须可列出输入源身份和依赖块。任何核截断、低秩截断、FP32 量化都记录误差及其适用泛函；无界近似不冒称 certified。

WCS规范化/求积数值界只条件于固定解析后的模型，不覆盖真实天测拟合偏差。AstrometricSolution的参数根/influence由uncertainty一次并入共同随机变量图；可微且分支稳定时使用有身份的参数Jacobian，边界拓扑改变时走公开API的联合扰动/forward MC。首期不承诺一般拓扑解析导数；没有相关传播能力或缺WCS误差输入时，完整物理误差标conditional/unknown及omitted terms，不能把数值面积认证解释为真实天空总误差。

## 阶段产品与可重开模型图

新格式命名 `acsd.product` major=2；旧 v1 产品不直接视为满足新合同。一个已发布产品根包含 `manifest.json`、唯一 `frame_science.json` 或 `mosaic_science.json`、signal HiPS、模型 JSON 和分块数值载荷。模型 sidecar 是唯一 canonical 对象图，浏览 catalogue/摘要是派生视图。完成 manifest 最后原子发布；读者先检查 manifest，再取所有依赖，不靠目录存在判断完成。

每个块使用路径相对产品根、little-endian、C contiguous/明确 CSR 布局、dtype/shape/stride=约定连续、压缩 codec 及原始/存储字节数、原始内容 SHA-256。首期固定 raw 或 zstd 的可解码变体，禁止隐式 host long/size_t。CSR 的 offsets/indices 为 uint64；order uint32、ipix uint64；FP64 模型可用精确 binary64 位模式的载荷表示，JSON 中科学数值转换必须 round-trip 一致。高精度区间使用带 binary exponent 和位精度的整数端点表示，不能先降成 double 后写“高精度”。

canonical hash 基于规范元数据（UTF-8、排序键、无重复键、整数和浮点类型明确）与所引用原始块 hash；无关路径和压缩级别不改变科学内容身份。reader 锁解析后的数值模型，FITS 头写入前后发生的舍入是显式序列化误差，不能拿原头的未舍入 CD 与读回模型作同输入严格界比较。

| 产品 | 必需保存 | 可选能力及缺失行为 |
|---|---|---|
| Normalize 帧 | signal、单位/响应/参考、WCS/真实 footprint/DQ/support、标度 application、数值质量、源身份、能力/来源；全部可用科学模型的依赖闭包 | full covariance/几何重细化需要下述额外闭包；缺失明确失去能力，不能伪造 |
| Mosaic | 唯一 signal HiPS、实际混合核/权/选择身份、sky 分量规范、支持表示/近似及质量、输入产品/hash | 内部 covariance/action 可作为非图像模型持久化；缺失时 Export 只传播已有 signal |
| Export | 单 signal 二维 FITS、目标 WCS、BUNIT、支持均值/积分定义、响应/来源与数值质量 | 可附模型/支持表与 manifest；不能由单图补方差 |

支持可持久化为完整 leaf、可复现 primitive 的并集 action、分层认证 inside/undecided 子域，或明确 full-leaf/effective-area 近似。重叠面积之和与 N 都不是支持并集。相邻 tile 引用同 primitive/leaf，计算 Export 时合并相同 leaf 随机变量后再传播 covariance。

HiPS sky index与图像数组分开：tile宽W=2^t，leaf_order=tile_order+t；NESTED x占偶位、y占奇位，采用标准朝向 `row=W−1−x,col=y`。HPX头由所属face/tile offset构造，face6经度中断以合法原生投影中心/分片处理，不能用数组原点补缝。每个标准HiPS层仍是同一个signal的表示；内部辅助块不使用第二个科学图像通道发布。

独立重开 `CovarianceAction` 所需闭包包括原始随机根的数值块/模型、共享注册表、G/α/R/fit influence/sky/权等实际组合 action、输入索引和条件；不依赖旧会话地址或安装位置。几何重细化还需 x/模板/控制泛函所需源块、WCS/footprint、完整 Ω/A/a primitives 与预算/算术、可再生成 action 的参数。原 FITS 可以作为只读外部内容寻址依赖，但必须声明 `requires_external_inputs` 并验证 hash；缺文件时不能声称产品可独立重细化。

首期默认 Normalize 保存 Mosaic 所需的闭包，源 x/噪声与影响按块保存，WCS/footprint 可参数化再生；不要求同时保存全部 CSR R。保存 action recipe 时必须锁算法版本、数值 policy 与实用 R 身份，重放验证名义系数/界一致；无法保证重放时保存实际系数块。空间复杂度按总依赖闭包计费，不能把依赖转到外部路径后宣称节省整个系统内存/磁盘。

存储不足不静默删除模型或改科学等级。用户可明确选择 signal-only profile；其 manifest 标 noise/response-detail/refinement 等具体缺失能力，后续按相应分支处理。任何 unknown 都沿非零 action 传递；已知部分可单独报告，不顶替完整方差。

## 兼容、发布与恢复

model graph 只允许白名单 action recipe（diagonal、CSR、local PSD factor、shared factor、compose、sky solve、selection、leaf merge）；禁止从产品加载任意代码/动态路径。未知 recipe/version 返回 unsupported，合法 signal 仍可独立读取。闭包检查包括循环引用、单位/索引、shared 定义一致、hash、未决域和状态；模型影响 DAG 与执行调度 DAG 分别验证，不能把循环依赖用临时文件隐藏。

staging 在同文件系统内写块、等待异步写完成、验证闭包/hash、刷盘并原子发布完成 manifest；覆盖已有产品先写新 revision，再原子更新入口。取消不发布完成清单。spill 与正式产品分 namespace，不作为跨阶段产品；恢复只能使用匹配输入/config/algorithm/arithmetic 的完整检查点，其它保持不可消费。

## 字段、布局及构造的唯一正本

实际 C++20 类型/签名以 [contracts.hpp](../../../include/acsd/contracts.hpp) 为正本，所有类型均已定义，Ref<T> 是 shared_ptr<const T>，不拥有裸指针借用。Model 是 Identity+Assessment；所有继承 Model 的对象在 JSON 中平铺这些字段。上述概念登记表及科学分册的伪代码不额外产生同名字段。NoiseModel 统一以 innovations/shared_factors+对应 influence 表示，统计分册 §6 的 action 字段由 make_covariance 构造，不另存第二份 covariance。

索引以 Index.content_hash 绑定完整排序，数组 shape 不等于 index 身份。native index按声明的partition和sample序；leaf 按 (order,ipix)，parameter 按 (模型、basis项)、latent 按 SharedId 和分量序。所有 flatten 为 row-major。uint64 乘加先查溢出，空数组合法、零输入 action 输出严格零；不支持/unknown 与空域不同。原生二维像元 flatten=y*width+x。Index.ids 对 leaf 存 [N,2] u64 的 order,ipix（Index.order 再校验统一阶）；native身份用下文partition/规则编码或小例sources，不强制全样本字符串。小索引可内联，大索引可块化。

| 类型/字段 | 约束、形状和单位 |
|---|---|
| Unit | multiplier>0；scale 给 L 身份（例如 adu:detector/revision、photon_rate_m2:response/hash），sr/pixel/nm 指数为整数。unit 不只显示字符串；平方 covariance 的 scale 是对应 L²，multiplier 平方。不同 scale 只有显式有物理依据的 action 可转换 |
| Identity | object_id 为逻辑 UUID；revision uint64 单调；content_hash 为规范内容 hash，schema_id=acsd.<类型名>、major=2、minor=0。run_id 不进科学hash |
| Interval / ExactInterval | 有限有序端点；后者 signed_integer 为无多余零的十进制 GMP 整数，端点=integer*2^exponent2，bits记录原语精度。E07新canonical_array_v1封存中的Dyadic先作等值归一：零唯一为(0,0)，非零integer移除全部2因子使之为奇数，指数相应增加；指数溢出int32拒绝，不截断。新模式封存/hash使用归一结果；bits仍保声明的算术精度。旧产品重开按其已声明版本验证原编码/hash，不隐式归一。科学容差按外包解析，允许ExactInterval超binary64范围 |
| Array | [shape]、dtype、unit、index_id、按 flatten 范围排序的 block_ids；分块不跨元素，无 stride/隐式转置，little endian；dyadic元素用有符号整数和exponent编码，不按固定字节假定 |
| ArrayView/Lease | 每 read 保留所有输入、解压缓冲及计费直到 lease 最后一份释放；span 与 shape 不可修改；read 可重入，不保留游标状态。跨API传 Ref<Array>，不传临时 ArrayView |
| WcsModel | cd[2,2] rad/pixel，c/e/north 是右手切基；sip 的项唯一且按 (总阶,x_power) 排序；Box 闭域；normalization_error_rad 对原解析数值模型转换的数值误差，不是真实天测误差 |
| DetectorFootprint | rings 归属 wcs detector 域，source 为真实收光样本；contraction_center 必需，pixfrac 只由调用生成 drop；PRF 响应缺失意味着 uniform_PRF 假设必须在 condition 明写 |
| Calibration | dark 数组是 dark 曝光中的积分 ADU；t=exposure_seconds/dark_exposure_seconds，dark_exposure_seconds>0。flat 数值除 flat_normalization 后进入公式；缺 bias/dark/flat 分别为显式 absent，不猜磁盘母版 |
| NativeObservation | integrated_values[N] f64、dq[N] u16、native_index，FootprintSet.index与N同序，unit 必须积分L；原始 G!=I 时 footprints 和 kernel 的 constituent 根保持原生身份，G 保存变换，不能既把值变为 Gx 又重复施加 G |
| SpectralBasis | knots_nm[K+1] 严增，basis_coefficients[d,K,degree+1]，局部 t=(λ−knot_k)/(knot_{k+1}−knot_k)，升幂系数，单位 W m−2 nm−1 / coefficient_unit；XP 外定标先解码成此表示并把表示截断误差单列 |
| Response | wavelength/throughput 同长度，严格递增、分段线性，支持域外不是默认已知零，只有显式 provenance 指定零尾。top_hat 与 sampled 二选一；top_hat_center_width_nm=[center,width]，width>0；throughput 不负，includes_photon_conversion 时数值单位含 λ/(hc)；shape_noise 缺失即模型项未知 |
| Spectrum/ReferenceSED | wavelength nm、flux W/m²/nm，interpolation 首期 linear 或 piecewise_polynomial，外域策略须明确；reference target_response 决定参考转换 c，无谱不猜星云 SED |
| CatalogueStar | pmRA*cosDec，epoch Julian year，parallax rad；负 measured parallax 保留随机测量，物理距离推断须先验，不直接倒数。RV m/s，可 null；XP coefficient 索引与 basis 一致；同一 source_id 绑定 release+calibration shared namespace。`epoch_year` 是该星当前发布位置的历元：`catalogue.query` 按请求历元传播后发布（epoch_year=请求历元），`catalogue.lookup` 按源自身历元发布（epoch_year=目录历元），两者的ra/dec因此不是同一物理量，不得混用 |
| StarFlux/SourceMeasurements | 总积分 flux L，window 是真实拟合矩形，不是归一域；variance 可 null且说明；joint_noise 包含源间/背景/PSF共用项；training_pixel_ids 保留与原生根的连接 |
| Basis | 项次为总次数递增、同次 x指数递减：1,x,y,x²,xy,y²…；u=(x−center.x)/scale.x，v 同理，scale>0；plane 在同一个 gnomonic sky chart，Box 是该chart范围。weighted_mean 给空间项训练加权均值，常量项不减 |
| PhotoSolution | theta 按 frames×basis项；θ0=log a，其余为均值零空间项；alpha=exp(−Σθ_l b_l)，单位由 ADU 到具名 L；support 是训练支持的包围，真实可预测支持还须训练点 hull，不以满 Box 保证；influence 根不能重复参数noise |
| NoiseRoot/NoiseModel | root.coordinates 为随机根坐标，factor 为从独立标准 latent 到有单位根的F；精确奇异/零方差合法。NoiseModel.influence 长度=innovation+shared长度，每个是根坐标→输出的 L_k；CovarianceAction.influence 是此输出上的附加 action，初始I；Σ L_k F_k F_kᵀ L_kᵀ |
| UnknownTerm | 影响不为零则完整 covariance未知；有 norm upper 可给模型上界但不虚构已知 covariance；空影响必须有结构性零证明。根共享判同依据 SharedId+factor/coordinate hash，同id异定义报 identity_conflict |
| Overlaps | contributions 小例内联，contribution_blocks 大例有序列编码；Ω/A每source只一份，a每source/leaf一份。共同约束数据包含 a≥0、a≤A≤Ω、Σ全叶a=A，coarse候选计入总量。SphericalAreaInterval.double 只外包显示，exact 是真正运算端点 |
| DrizzleOperator | rows 输出 leaf、输入 native 积分L，系数 sr−1；normalizer_sr 与叶同序，no_support行以支持位表示，不存任意R；refine 后旧引用继续有效 |
| SignalEstimate | f64 signal、numerical_lower/upper 同shape与单位；内部无支持 signal=NaN且validity独立；未知数值界用缺数组+issue，不用 ±Inf JSON；noise 指向同 signal index；error强引用ErrorRecord |
| FrameDescription/LeafBlock | Index 是该帧实际有效leaf全集，read 返回请求顺序且不重复的子集加无支持状态；不隐式按文件顺序。halo 先去重，中心与halo共享同leaf随机身份。PSF/response/noise/native_to_leaf 缺失按capabilities给原因 |
| SkyControls/SkyFit | c[M]，T:M×N，H:M×S，A:M×P，E:N×P，K:P×M，P:N×N但只action；β[P]，component_ids[M]，输出的component另在support。gauge=reference/weighted_sum，所有共享basis公共模和额外nullspace必须持久化 |
| Selection/WeightSolution | eligibility/accepted/reasons shape [leaf,frame]，u8/u8/u16；按 leaf,frame 索引。coefficients f64同shape；inactive=0，active sum=1；无人可用无权行，signal NaN。Γ不是一张仅对角图片 |
| SpatialResponse | spatial_actions[k] 解释原输入→当前输出核，weights[k,p]、reference_factors[k] 同 frames 顺序；已吸收进 spatial_actions 的 t/c 不再乘一次，以 recipe children角色验证；signed_response反映真实负系数 |
| ExportRequest/Result | Q:M×N，支持均值系数无量纲，积分系数sr；target尺寸与pixel index一致；width,height>0；target.wcs覆盖完整输出域。extrapolated_full_flux 必须 assume_uniform_uncovered=true |
| GeometryRevision | base/next 锁hash；affected含共享Ω/A影响的所有source/leaf，约束重估使全sky分量失效。replacements 是旧hash→新hash的完整传递闭包。资源不足不发布next，允许返回只含新有效面积的 partial revision，status未达标 |

PhotoSolution.theta、RankReport 和 ExactInterval 的数组/向量必须随闭包保存；字段 null 的可选性来自 Ref 可空规则：除下文明确的pre-WCS/linear_view/可选模型、noise/PSF/response/refinement、可选Calibration map、SkyRequest T/H外，Ref 字段必需非空，违反 invalid_model。所有给能力名的 Ref 空缺均必须有对应 unavailable issue。

### E02/E07目标：数组封存、规范块与科学内容身份

本节为待实施的新封存规则，不表示当前Storage已按此封存。实施时须在manifest的算法/capability身份记录canonical_array_v1；早期v2产品由既有reader读取并明确原布局，迁移生成新revision与新身份，不原地覆盖旧产品/旧hash或假称已经满足新合同。工作单元的计算块、I/O chunk、builder.append片段和spill分区可以随H/线程调整；已封存Array的规范块边界不能随之改变。固定宽dtype按flatten从0开始每2^20原始字节切规范块，即每块floor(2^20/itemsize)个元素，最后可短；空Array无块。dyadic按每1024个完整元素切块，元素先等值归一再编码规范Dyadic JSON，无空白、键按UTF8序；可以逐字符流式编码/散列，不要求一整个大整数或规范块常驻。相同解析值、shape、dtype、unit、index必有相同raw_sha256、begin/count和block_ids。浮点保-0位，无值NaN在FP64统一quiet位0x7ff8000000000000、FP32统一0x7fc00000且必须有validity；不能把非有限数当有效数。

ArrayBuilder状态仅open→sealed或open→failed→aborted，任一未sealed析构等价abort。begin锁shape/index/unit；append按flatten顺序消费完整元素，累积到规范边界可将字节增量写私有raw spool并计算SHA256；seal验证总元素数、块覆盖及hash，形成不可变Array并保所有存储依赖。memory placement只能在完整输出已预留时使用；spill可用小于规范块的固定缓冲，raw写与zstd流式解码/压缩均受实际缓冲额度约束。压缩工作集不适合预算时使用raw，raw_sha256及科学身份不变。索引/块清单也可外存构建，seal的最终元数据仍须准入；无法容纳必要元数据正确返回resource_exhausted。

copy是这个builder流程的小数组便利包装；open接受外部合法任意分块描述；已规范块可由descriptor重算身份并在访问时逐块验证bytes，非规范块在发布新Array身份前必须通过同一全流式校验与封存规范化，使科学对象只引用规范块。输入别名、外部物理分块与codec/path不进入科学hash。需要规范化的外部块raw/hash先校验再参加规范化，不能把未核验的数据宣布为同一个Array。规范块内容可惰性加载，封存验证身份后，reader每次读取仍核验实际物理块，不能因缓存命中跳过版本/来源绑定。

对象hash仍包含实际科学数值、数值包围、支持/条件/算法与依赖身份；语义相容但数值或区间不同不要求同hash。同build/后端/算术的R01要求相同规范运算与停止顺序，从而给相同包围；跨后端按同科学容差及合法包含比较。Usage、未来资源子域ID/诊断性能记录、执行块边界及GeometryRevision.consumed是运行来源，不纳科学hash；manifest锁住存储布局与完整文件清单，不能拿科学root hash替代manifest_hash。规范化只改变存储分块，不允许重算数值、降低dtype或调整界。

### action recipe 的精确定义

所有 action 必须暴露 apply 和真 transpose，拒绝非同index/单位输入；返回数组通过 Context.storage 分配。make_action 接受配方和已解析有序 children/arrays，长度及content_hash须与配方一致。不存在隐式科学全局注册。

| kind | children / arrays 顺序；计算 |
|---|---|
| identity | 0/0，codomain=domain，同单位 |
| diagonal | 0/1：[N]对角d，y=d*x |
| csr | 0/3：offsets u64[M+1]、indices u64[nnz]、values f64[nnz]；每行列升序且已合并重复 |
| compose | 2/0：outer,inner；outer(inner(x))；transpose顺序反转 |
| sum | J/1：coeff f64[J]，Σcoeff[j]*child[j](x)，所有域相同 |
| transpose | 1/0：childᵀ，交换域 |
| factor_covariance | J/0：各child为独立latent→输出的总L_kF_k；Σ child childᵀ，共享latent先合并同根列，不独立求和 |
| solve | 1/0：child=M，具名SolveSpec载荷及数学映射见下文；参数/残差在error_record，不装载任意代码 |
| sky | 3/0：E,K,T；I−EKT，transpose=I−TᵀKᵀEᵀ |
| selection | 0/2：indices u64[M]、scale f64[M]；选取/缩放，transpose scatter确定归并 |
| leaf_merge | 0/3：与CSR相同，输入若原始片共用leaf则先以共同index聚合，不造独立root |

row_enclosure 返回完整domain列顺序的区间小行，超预算可 unsupported；大规模用dependency_blocks+稀疏CSR bound数据。absolute_factor_bound 若有必须是各根总因子的逐元绝对上界 action（codomain/latent索引明确），不能只是 |C(v)|。通过谱范数路线则用 operator_norm_upper 及 ||δ||₂，保存来源。所有 solve recipe 持久化实际系统、分解/可再生参数、列尺度、gauge/秩；只有“算法版本号”不构成闭包。

### E04目标：诊断快照的载荷和寿命

本节各Probe/Snapshot名称是待声明的基础设施角色；静态登记包含id/kind/semantic_revision/producer/boundary/semantic/unit_rule/coordinate_space/payload，实际单位取被观测对象而非猜ADU或L；ProbeTags的frame/component/model_hash、leaf、roi、attempt锁本次数值边界。数组快照必须携带原dtype/shape/endianness、index排序和单位、有效域/DQ以及所需WCS/HEALPix变换；模型快照为已有白名单模型及已完成数组的引用闭包，不调用新科学计算。signed F/N/B、V_F/V_B/V_ctrl必须使用各自probe ID，不能笼统命名weight。

SnapshotPayload.fields用于已有Information/Variance等按值结果的小型标量列，name/类型/单位明确，unknown为null，ExactInterval不转double；不要求先构造新的科学Array。SnapshotPayload的views只在snapshot同步返回前可借用；arrays/models也只能在该期间读现有有效数据。sink必须立即try-copy至D计费的私有不可变缓冲/元数据，或SKIPPED_BUDGET；返回后不继续持有科学Lease、ArrayRef或指针，不触发新Storage.read、设备download或完整闭包遍历。大数据只取已在手的所选ROI/块；未就绪、非主机可读或模型依赖不在手时INVALID_VIEW/NOT_READY并记原因。异步writer只拥有复制后的诊断租约，完成/取消前不释放；诊断副本不能写回科学对象。

二维host数组写原FP32/FP64 FITS并附metadata；不能正确生成WCS时只保存原块+完整坐标元数据，标wcs_unavailable，不编造头。精确dyadic、稀疏表和模型使用规范JSON/原块，禁止转double丢精度；摘要必须标representation=summary，不冒充原像素。禁止拉伸、clamp、abs、NaN→0。私有临时文件写完hash/size后原子改名；取消或失败保incomplete标记。状态只用WRITTEN/NOT_SELECTED/NOT_SEEN/NOT_READY/SKIPPED_BUDGET/IO_ERROR/CANCELLED/INVALID_VIEW；debug_manifest不成为产品输入或恢复点。

### 产品具体布局和版本规则

产品根目录：manifest.json，frame_science.json 或 mosaic_science.json 或 export_science.json，objects/<hash>.json，blocks/<sha256>.bin[.zst]，signal/Norder*/Dir*/Npix*.fits（Export 改 signal.fits）。manifest 字段：format=acsd.product，major=2，minor=0，stage，product_id，revision，root_object，profile，capabilities，inputs[]（manifest hash），objects[]（相对路径+hash），blocks[]（descriptor），configuration_hash，algorithm_set，arithmetic，assessment，complete=true。完成前只有 staging manifest，正式 manifest 绝不先写。

block descriptor 为 {id,path,dtype,shape,index_id,unit,begin,count,codec,raw_bytes,stored_bytes,raw_sha256}；begin/count为元素范围，不能与字节混用。raw或zstd单frame无dictionary，zstd解压上限=raw_bytes且经shape*itemsize验证；dyadic用Dyadic对象数组UTF8块，dtype=dyadic且raw_bytes按实际计费。FITS 为标准可读image，不复用bin块作浏览格式。sidecar根指root_object，同一图一份正本。

规范JSON数字：整数直接十进制，binary64以 {"f64":"<16hex little-endian bits>"} 规范hash，用户可读JSON有限decimal输入在解析后转换该hash表示；-0保留位，科学值禁止非有限，NaN仅图像块带validity时准入。字符串NFC，key按UTF8排序，无空白、无重复key；Id内容hash不含自身content_hash/object_id/revision/run_id、物理路径、时间/耗时，但包含依赖的内容hash、schema、数据值/单位/状态/算法误差记录。数组block hash覆盖未压缩规范bytes；E07新封存按canonical_array_v1固定边界，不按计算块大小；旧产品保持其声明算法与原块边界，重开不得重算成新身份。语义相同但舍入不同的float不会碰巧同hash。

对象图不包含raw C++指针、Lease或Context。header构造的Ref递归序列化为 {"$ref":"sha256:..."}，对象唯一存objects目录。ActionRecipe通过白名单可重开，算法升级无法重放同系数则保存CSR或依赖分解；不接受“使用当前最新算法” recipe。产品major不兼容直接拒绝；minor未知可选字段保留并纳hash，未知必需capability只禁对应模型操作，signal可在manifest基本解析成功后读取。

发布目的路径必须不存在；首期不自动覆盖目录。新revision为新目录，入口指针单独atomic rename更新。Windows FlushFileBuffers+同卷MoveFileEx(WRITE_THROUGH)，Linux fdatasync文件+fsync目录+rename；不支持原子语义的远程文件系统返回 unsupported。进程崩溃留下staging可按checkpoint验证恢复，绝不把其当product。检查点是同格式的私有对象图另加 completed_work_keys、input/config/algorithm/arithmetic hashes、剩余账本和spool分区hash；只恢复已完整校验work，资源消耗累计，不重新给满work。checkpoint/正式产品区分路径及complete标志。

默认 full 模式必须自包含 Mosaic/Export需要的 x、WCS/footprint、noise共享根、影响、PSF/response、实际R及geometry primitive闭包；默认同时可refine。signal_only 仅允许显式选，标no_noise/no_refinement/no_detailed_response；Mosaic equal与Export signal仍可用，不伪称严格全物理误差。外部原FITS引用仅显式 external_inputs profile，hash和required能力清楚；full禁依赖外路径。

磁盘配额耗尽返回resource_exhausted且保留staging诊断；只要正式manifest未发布，不产生完成身份。重开先验hash可按访问惰性验证块，但任何未经验证块不可用于科学值；manifest/object闭包先全验证索引/循环/SharedId。损坏signal块致该产品不可用，不静默转零；损坏可选模型块只失去该能力并报错。

## 大载荷、几何闭包与solve规范

本节规定大对象的实际编码与阶段状态；登记表只作导航，编码必须满足以下不变量。

### 原生身份、足迹和模型的阶段状态

Index.sources只允许≤4096样本的in-memory小例；大原生Index使用partitions，每项共享dataset/exposure/detector/channel并覆盖[index_begin,index_begin+count)，所有partition恰好无交叠覆盖index。规则样本ID为first_sample+local_row*sample_stride；有不规则ID时Index.ids[N] u64覆盖该公式。重复索引可持有同一个SourceIdentity用于读视图，但科学输入sample集合不得重复。native_sources不再在Raw里复制，Raw.source_index为唯一正本。namespace_id锁parameter/latent编号方案。leaf仍用[order,ipix]编码；以source index小对象而不是每样本4个string分配内存。

FootprintSet正规模式：index、width,height，样本中心=(row%width,row/width)，detector中心=M*center+offset，local_cell是相对此sample中心的矩形，四角同样变换。原生矩形M=I、local_cell=[−.5,.5]²；Bayer若坐标保原detector，则M=2I、local_cell=[−.25,.25]²，实际detector半宽.5。crop/bin可用相应M与真实并集例外。width*height=index.size，除例外外不每像元分配环。

FootprintExceptions为分块列：sample_rows[E] u64升序；ring_offsets[E+1] u64指向rings；vertex_offsets[R+1] u64；vertices[V,2] f64 detector坐标；hole_flags[R]u8。offsets从0、末端=对应元素数，所有环验证方向/不自交。例外完全替换该sample的默认矩形，不在原矩形上重复OR。小例可用inline_footprints（≤4096，与index等长），正规参数化与inline二选一。detector_footprint按row解析单样本并持WCS/PRF租约，外层按块逐样本调用，不materialize全图。FootprintSet.exceptions可null，PixelResponse可null但须uniform_PRF条件，不是已知物理零误差。

FootprintSet.wcs及DetectorFootprint.wcs在pre-WCS阶段允许null。只有footprint/measure/compute_overlap/fit_psf/Drizzle等需要天空的位置消费处要求非null；calibrate和solve_astrometry只用detector几何。bind_wcs创建新revision并保原数组，不能拿伪Wcs填空。Native.kernel/response在拟合前可null；bind_observation_model是拟合后明确连接点，native_parent只在regroup派生存在且为DAG。physical_collection=false的linear_view可以无footprints，但必须有native_parent和G；只有该状态允许缺收光域。G:当前m×parent n，integrated_values是当前Gx值；noise根是同parent根经G组合，不把current值再乘一次G。

EffectivePSF完整参数/密度/模板transfer由SYSTEM规定；pixel_integral旧概念已由evaluate_psf唯一实现取代。parameters各行及spatial_basis持久化，achromatic首期必须true；PixelResponse首期单波段（wavelength_edges恰两个端点、gains[1,cell]），多波段要求显式SED与波长积分合同，当前unsupported。空间PRF与仪器谱Response是不同对象。

### 几何recipe、primitive约束与大贡献记录

GeometryRecipe包含footprints（连Wcs/PRF）、source_index、source_rows（本recipe负责的源序号）、order、pixfrac、原始budget/arithmetic及algorithm/version。一个单源compute_overlap也生成仅含该源的FootprintSet recipe；workflow批量归并可以共享完整FootprintSet和分区source_rows。实际初始预算不是refine的新配额，后者受Context剩余账本约束。GeometrySnapshot保存recipes、source_inputs（实际已scale的x/noise/kernel/response）、functionals、overlaps、operators及dependent IDs；新进程不需原FITS或旧会话就能恢复计算。

GeometryPrimitiveTable按(kind,source_row,leaf_order,leaf_ipix)排序，列：source_rows[P]u64、leaf_orders[P]u32、leaf_ipix[P]u64、kinds[P]u8、lower/upper[P]dyadic。kind=0原生Ω、1drop A、2相交a、3coarse未决质量；无leaf时order=UINT32_MAX、ipix=UINT64_MAX。dyadic元素仅Dyadic{signed_integer,exponent2}，precision_bits在表；ExactInterval由同row两列组成。所有primitive source_row指recipe.source_index，不以vector位置变更身份。共同Ω/A在表只存一次，a仅每源每叶一行。

GeometryConstraints.matrix为[约束行数,P]的稀疏action，输入是同表面积primitive；lower/upper[约束行数]dyadic。行表达lower≤M*mass≤upper：a≥0、a−A≤0、A−Ω≤0；完整partition含sum_leaf a−A=0。单侧无界不用Inf端点，先用有限sphere/source上界替代，或只登记可有限的方向。coarse未决分区作为非负剩余质量列参与列和；未穷举leaf时不能伪写“sum已枚举a=A”。overlap共享constraint Id必须指此类型的内容hash，产品闭包包含该对象。

OverlapTable列source_rows[Nc]u64、leaf_orders[Nc]u32、leaf_ipix[Nc]u64、native_primitive/drop_primitive/overlap_primitive[Nc]u64，后三者是primitive row；按(source_row,leaf_order,leaf_ipix)排序，唯一键不重复，除recipe内的source_index之外不复制SourceIdentity字符串。每列Array分块按统一Nc index，reader可按任一源/目标的外存索引读取，无自创struct字节布局。inline contributions只允许≤4096且必须与同一个primitive表/constraints匹配。order/f检查读取recipe明确字段，不从hash猜。GeometryRevision.replacements可只持Id，但base/next Snapshot的具名输入与新闭包对象都实际存在。

### 精确支持的可解码单元

SupportMap.kind=dyadic_cells使用五列：cell_face u8[S]，cell_depth u32[S]，cell_x/cell_y u64[S]，cell_state u8[S]；state=1 inside、2 undecided，outside省略。单元为该face的[x/2^depth,(x+1)/2^depth]×[y/2^depth,(y+1)/2^depth]。depth≤63，0≤x,y<2^depth；行按(face,depth,x,y)规范排序且祖先/后代不可重复覆盖，内部无交集。跨目标leaf粗未决可以depth<leaf_order存在，面积上界同时属于所有后代候选，query不能把其当每leaf独立完整质量。cell数组可分块，查询按facet区间索引；完整primitives模式的IDs必须指GeometryRecipe/PrimitiveTable，不是无载荷tag。

Σinside质量给lower、inside+undecided给upper，纯切触可以永久undecided直至其科学影响可忽略；达到有限预算并不把uncertain boundary改成exact shape。Export使用原primitive重细化，或将undecided的最坏质量传播至b，再判断自己的容差。full-leaf/effective-area近似无cell表，必须声明近似，不从面积数组猜左右半。depth>63请求须以primitive再求解，不将地址溢出。area数组是查询叶的派生bounds，cell表/primitive是形状正本。

### solve recipe的唯一载荷和真转置

ActionRecipe.kind=solve只用一个child M，无arrays；solve字段必为SolveSpec，其它kind为null。M的domain为n参数、codomain为m观测。SolveSpec.column_lift=Z(n×r)，row_lift=W(m×r)（square用），preconditioner为可选加速action，column_scales[n]正数，reduced_rank=r，rank_certified/rank_error、gauge_constraints和SolvePolicy都是具名字段，不再塞进无定义数组。Z/W本身由标准CSR/identity action持久化，无opaque内存分解块。

square定义A=Wᵀ M Z（r×r），要求A可逆；solve(b)=Z A⁻¹ Wᵀb，真transpose(t)=W A⁻ᵀZᵀt。完整square系统取Z=W=I。gauge/PSD的精确线性约束先由numerics装成有显式块的KKT矩阵M及Z/W，不能在recipe里忽略rhs约束。

least_squares定义A=MZ（m×r），要求满列秩；solve(b)=Z(AᵀA)⁻¹Aᵀb，但实际用QR/SVD/LSMR不显式正规逆；transpose(t)=A(AᵀA)⁻¹Zᵀt。row_lift必须null；已白化的M给Euclidean LS，非白化GLS必须先白化或KKT，不另有秘密权。rank缺失unsupported/not_identifiable，不用伪逆自行改变参数空间。column_scales只决定内部数值缩放，以上数学映射与其无关。

SolveSpec.policy包含显式整数/布尔/实数，JSON按实际类型；algorithm取canonical_qr_v1/canonical_svd_v1/lsmr_v1/minres_kkt_v1，与version一起锁定。error_record含实际残差和求解扰动证书；reader用同version算法恢复同数学映射与被声明数值包围，不能要求未实现版本重放。若科学闭包需完全固定名义系数（例如geometry反馈冻结权）且不能保证重现，则将该有限action保存CSR，不借当前solver近似代替。无需持久化跨平台库私有分解布局。

### 状态、单位与输入别名

Unit.linear_power和scale共同表达L的维数，variance=2，纯面积/功能系数=0；CSR values的unit是output/input，不是默认无量纲。NoiseModel.variance_unit须等于影响后信号单位平方。以C作用孔径系数时，返回单位按API的乘积规则。

Assessment.execution记录调用是否complete/cancelled/failed，capability为supported/unsupported/unavailable；bool complete是所有本对象必需目标已完成（可在execution=complete而science=unconverged时为false）。成功结束执行不等于科学达标。SupportMap.states逐index为0 absent、1 positive、2 bounded_undecided；normalization_states为0 no_support、1 positive、2 support_unresolved、3 weak_support；observational_states为0 absent、1 observed、2 predicted、3 extrapolated。三个数组不可互换，空能力须issue，不能用全局单值替代逐叶。

ErrorRecord.requested/achieved按同序小型泛函集合（≤4096）；对整图B请求，achieved记该domain最坏误差半径，逐叶真实数值包围保存在SignalEstimate.numerical_lower/upper分块数组，不在ErrorRecord复制全图对象。finite_model_bias按相同泛函顺序，未知项列unknown_model_terms；stochastic_noise是完整C或null。ActionRecipe.error_record和SignalEstimate.error为有寿命强引用，闭包不依赖全局ID查找误差对象。

非native、非leaf的Index.ids=null表示namespace_id内的0..size−1固定序号，不是缺失身份；native规则来自partitions；leaf必须有实际叶ID。所有输入typed对象的局部名按依赖DAG解析，Array.index_id和recipe.arrays/children中的局部名会在解码时解析到实际content_hash再校验，不将用户别名写为产品身份。未解析别名missing_dependency，循环corrupt_product/invalid_model。字符串Id普通业务值不自动全局替换，只有声明的index_id/recipe依赖字段这样解析。

## 独立块输入与专用PSF结果的实际字段

BlockDescriptor是产品manifest与独立驱动共用唯一类型：id,path,dtype,shape,index_id,unit,begin,count,codec,raw_bytes,stored_bytes,raw_sha256。shape是所属整Array的shape，begin/count为flatten元素范围。id为规范科学descriptor（dtype/shape/index_id/unit/begin/count/raw_sha256）的SHA256，不含path/压缩选择；raw_sha256校验未压缩bytes。同一raw块可在不同索引/单位descriptor中复用，不能让相同raw bytes强迫不同科学域同ID。

Storage.open(root,description,blocks)验证块覆盖、尺寸、dtype/单位/index一致及raw/hash后返回有寿命Array，元数据可先校验、bytes按访问惰性hash，未经校验的bytes不能用于计算。E07新封存/显式迁移模式对非规范输入先全流式校验、等值归一与重分块，再发布新revision；open_product按产品声明的算法/capability选择旧验证或新规范规则，旧产品重开保持原Array及父图身份，不隐式迁移；无法识别其算法版本则unsupported。当前Storage.open没有模式参数，迁移时须在唯一声明及reader装配补齐版本上下文，不能猜块大小决定模式。raw/zstd与产品相同little-endian合同，解压上限raw_bytes并先资源预留。dyadic raw payload为UTF8规范Dyadic对象数组（signed_integer,exponent2），个数=count，无NaN/Inf，不当固定itemsize；整数指数和字符串语法逐项查。copy/read/open同一Storage后台，不引入第二份数组系统。

PsfEvaluation.psf保原模型和实际transfer；source_positions≤4096每次，values及lower/upper三数组同shape/index，native_tail_upper绑定source_index；error为强引用。template数值区间和原生尾损失分别持久化。它们是跨sources/information/photometry对象，只有这一份正式定义，裸Array不被假定携带隐含ErrorRecord。普通矩阵数组仍是轻量数值数组。

PhotoSolution.parameter_lower/upper同theta[K,P]，error强引用实际数值证书；不能把covariance当参数数值求解误差。未有有限参数包围时两数组为null且error明确conditional/unconverged，apply_scale不能据此给无条件数值认证。Native数值包围同样为显式字段；上游界缺失不能用0替代。sources的模板ErrorRecord与response.absolute_errors均进入该数值账本。FrameDescription.noise/PSF/response/native_to_leaf在signal_only能力缺失时允许空且逐项issue；full则保存所有实际可用模型和来源，不把缺失能力伪造为available。

Ref可空的穷尽例外再明确：FootprintSet.astrometry、exceptions、sample_response_gain及pre-WCS.wcs；EffectivePSF.source_scale在未标度阶段、缺数值证书的parameter_lower/upper；PhotoSolution缺数值证书的parameter_lower/upper；InformationRequest.template_evaluation在精确显式h分支。这些空值均有上述专门语义。其它未列的必需Ref缺失为invalid_model，不授权任意用null逃避能力。

PsfEvaluation.values的笛卡尔索引和output_index/source_index保同序；InformationRequest.template_column必须在source_index范围，template_evaluation为空时必须0。提取h列是标准selection action并显式重新标为output_index，不能靠shape相同混入别的叶序。Native.context/frame及FootprintSet.astrometry都进full产品闭包，后续操作不得用run_id、exposure字符串或当前时钟猜它们。

## SourceMeasurements 数值载体（唯一字段语义）

flux_numerical_lower/upper 为 f64 [stars.size()]，与 stars 严格同序，index_id=joint_noise.input_output.domain 的源通量索引，单位与 StarFlux.integrated_flux 一致（标度前 ADU，即本观测计数单位；不把它误记为最终参考 L）。两数组必须同时存在，包含名义 integrated_flux，保存实际源测量求解的确定数值包围；确实无法认证时二者同为 null，error 明确 numerical_flux_uncertified，结果 conditional/unconverged，不能按零半径消费。stars 的 Variance/joint_noise 仅表示随机误差，不替代此包围。

error 为强引用，保该次实际 PSF 模板积分/参数盒、Native 数值区间、窗口线性求解或局部根验证的误差账本及 included/omitted。实际模板求值证书的影响归并进通量区间与 error；其 ErrorRecord 依赖须强引用可重开，不必保一份完整 NxS 模板矩阵。observation 强引用被测 Native（保 frame/context、数值输入及 noise），而非隐式当前会话。full 产品沿这些字段保存闭包；StarFlux 中名义值不再另有第二套区间正本。
