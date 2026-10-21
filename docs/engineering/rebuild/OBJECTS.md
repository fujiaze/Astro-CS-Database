# 统一对象、身份与持久化

本合同实现[观测科学接口 §7](../../science/foundation/OBSERVATION_COORDINATES.md)及[统一统计](../../science/foundation/STATISTICS_SKY_COADD.md)，保持其公共类型名和数学语义。API 见[标准接口](API.md)，大数组寿命见[计算与资源](EXECUTION.md)。旧统一对象或 JSON schema 不构成新核心的兼容要求。

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
| `StarFlux / PhotoSolution` | 总 flux 定义/单位/位置/窗/孔径与 source_id；θ/basis/domain、rank、covariance 与同数据 influence | sources_psf/photometry → apply_scale、noise |
| `EffectivePSF / ObservationKernel` | 归一/尾损失、像元积分、空间/波长依赖、PRF、G 与标度作用；一般 signed G 不称概率 | sources_psf/operators → information、sky、coadd |
| `SphericalAreaInterval / OverlapContribution` | 单位 sr，0≤a≤A≤Ω，原始 primitive/共同分母/每源 Σa=A 的约束身份，未决位置及上界 | geometry → R、预算、Export |
| `GeometryBudget / GeometryStatus` | B/F/Cov/孔径请求及单位/容差、支持需求、逐叶绝对限额、工作/内存/深度/算术；成功作用域 | 工作流/geometry → 数值执行与报告 |
| `GeometryRevision` | base/new snapshot、不可变primitive/R版本、受影响source/leaf/domain及共同约束、替换映射、资源账本和未决状态 | refine_geometry → 预算控制器及下游闭包事务重建 |
| `LinearAction / DrizzleOperator` | 域/陪域索引及单位、apply/transpose、块依赖、R 名义值/允许集合、N、source index、几何约束 | operators → signal/PSF/noise/basis |
| `SignalEstimate` | signal、单位/估计量、numerical_bounds、支持几何、response/error/noise 引用 | operators/coadd/projection → 产品/后续阶段 |
| `NoiseModel / CovarianceAction` | 根块、局部/共享因子、influence 链、condition、未知项、C action；可选绝对因子 action/谱界 | uncertainty → V_F/V_B/V_ctrl、GLS、孔径 |
| `PrecisionControls` | 类型 reference_flux / actual_scene / control_statistic、值/单位、estimator/condition/support、δ/M 来源或预测误差 | information → 同类稀疏重建，不能换名充权 |
| `SkyFit` | T/H/A/basis/gauge、β、K/E/P action、可估子空间、共同模式及额外零空间、参数 covariance、训练/预测域 | sky → corrected frame provider、coadd |
| `ScientificFrameView` | 帧/网格/signal/DQ/support/response/PSF/noise、native_to_leaf、可选 sky 与邻域能力 | products/provider → coadd 与独立驱动 |
| `Selection / WeightSolution` | 原始资格、接受 source 集、原因/阈值、实际 t、Γ身份、策略、数据依赖性、数值/选择状态 | selection_coadd → 合成、有效核、全过程验证 |
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
