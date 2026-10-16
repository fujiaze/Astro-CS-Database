# Normalize 数据合同

本合同依据 [阶段一科学模型](../../science/normalize/NORMALIZE_SCIENCE.md) 和 [阶段一架构](../architecture/NORMALIZE_PIPELINE.md)，定义阶段内类型、端口载荷和逐帧产品序列化。算法公式由 science 维护；本合同决定同一科学量怎样在代码和磁盘间保持身份。具体接口与迁移落点见 [实现规格](../../detail/normalize/contracts.md)。本文是确定的数据规格[1,2]。生产代码已具备部分参考/状态类型、球面 GLS、sidecar 读写及发布调用；完整 Runtime 载荷、模型求值和读取合同尚未全部满足。实现事实与限制集中见[科学实施差异地图](../architecture/SCIENCE_IMPLEMENTATION_DELTA.md)，不能把本合同的要求一概称为未实现或全部已实现。

## 1. 身份、版本与所有权

每个输入 occurrence 分配稳定 `frame_id`，另记录源字节的 SHA-256；重复文件的不同输入 occurrence 不因哈希相同而合并。运行拥有 `run_id`、配置哈希和冻结后的帧列表。路径用于定位，不作为身份。共享母版、响应、参考 SED、PSF、噪声和几何算子分别有内容身份，帧引用其 ID。

每个阶段内对象具有 `ObjectHeader {run_id, frame_id, schema_version, revision, producer, input_revisions}`。`revision` 在当前 run 内单调增加；`input_revisions` 精确绑定上游对象，不以目录 mtime 判断新旧。生产者发布后对象只读，消费者必须核对帧、run 和上游版本。更换 WCS、标度或像素有效性使依赖它的下游对象失效并重算。

Runtime 拥有帧对象仓和大数组块；节点只借用 const 输入并提交新的 owned 输出。最后一个消费者完成后回收，取消与异常同样释放。复用 `BlockFrame`、`RunContext`、AIO 的生命周期和预算，不增加私有全局对象仓。当前 `BlockFrame` 的数值块和 `RunContext` 的 artifact descriptor 服务尚未构成完整 typed 对象仓；`lib/include/acsd/normalize_data.h` 的类型声明不等于这些模型已由 Runtime 跨节点传输。实施时须扩展其 metadata/模型句柄、版本与消费者生命周期。

C++ 内部可用 RAII、`std::optional` 和 `shared_ptr<const T>`；跨 DLL 用现有 `acsd_head`、POD/span、host allocator 或 opaque handle，不能把 STL 类型直接放入公共 C ABI。数值大面默认 FP32，几何/稀疏/标定元数据 FP64；AIO 全局精度开关不能把稀疏值降为 FP32。

## 2. 数据对象的字段与责任

| 类型 | 必需字段或载荷 | 唯一生产者 | 消费者 |
|---|---|---|---|
| `FrameIdentity` | source path/hash、input occurrence、dimensions、曝光及来源、母版绑定、设备元数据、原始饱和标识、响应选择及来源 | ingest adapter | 所有节点 |
| `CalibratedPlane` | pixels、`linear_unit`、`scale_state=calibrated`、validity、master IDs、dark convention/exposure factor、flat normalization/floor、master uncertainty refs | calibration | cosmetic、IPV |
| `CleanedPlane` | pixels、validity、repair class、可用修复算子/未建模声明；继承校准身份 | cosmetic | guided sources、photometry、noise |
| `AstrometrySolution` | status/reason、source、hint/attempt records、ICRS/TAN(+SIP)、CRVAL/CD/CRPIX、A/B 与可选 AP/BP、catalogue/epoch、可用域、残差 | WCS/IPV | guided sources、photometry、Drizzle、writer |
| `ScienceSourceCatalogue` | 稳定 source/Gaia IDs、predicted/measured x/y、flux estimator、flux unit/variance status、PSF binding、saturation/blend/edge flags | guided source/PSF adapter | photometry、noise |
| `EffectivePsfModel` | domain、position→pixel-integrated normalized response、fit family/parameters、support/truncation convention、validity、模型误差状态 | PSF library | photometry、noise、Drizzle |
| `PhotometricSolution` | response/reference IDs、fit coefficient convention、scalar/spatial scale、fit support、parameter covariance state、input/output units、application ID、fit status | photometry | applied plane、noise、writer |
| `PhotoAppliedPlane` | cleaned pixels × actual scale、同标度 validity、`application_id`、unit、corresponding photometric revision | photometry | noise、Drizzle |
| `ConditionalNoiseModel` | domain、background expectation/statistic、随机分量及来源、gain/RN provenance、source-Poisson conditioning、master/cosmetic correlation、共同参数单独建模状态 | noise library（校准提供其输入） | Drizzle、reference precision |
| `DrizzleObservation` | signal 累加器或其 AIO 句柄、有效域、WCS revision、叶阶/ordering/pixfrac、精确面积口径、实际 R 算子或可求值模型、球面 PSF/协方差 refs | Drizzle | spherical precision、writer |
| `SparseReferencePrecision` | 唯一 reference、sphere domain、支持域、FP64 control records、F_ref/V_F/SNR、PSF/noise/background/geometry refs、精度和缺项 | noise library 的 sphere completion | writer；跨阶段 reader |
| `FrameScientificProduct` | 发布 signal、frame_science.json、模型文件及 hashes、帧状态、产品完整性 | writer | Mosaic |

标量字段缺失用 `optional`，不能用 0 表示未知 gain、方差或通量。对象可用性与执行错误分开：缺 PSF 可能使 reference precision unavailable，但不凭此删除已有合法信号；损坏模型、非法数值按正常数据错误处理。

## 3. 坐标与单位的精确合同

内部数组中心 `(x,y)=(column,row)` 从 0 开始，首像元边界为 `[-0.5,0.5]`。FITS WCS 的 CRPIX 始终存 1-based 数值；公共内部 `pixel_to_icrs(x0,y0)` 在调用 FITS/SIP 变换时加 1，`icrs_to_pixel` 返回时减 1。已经为 FITS 1-based 的 IPV 输出先在 IPV adapter 归一，不能重复加 1。CRPIX 可为任意合法有限值；`w/2+0.5` 是 IPV 当前生成规则，不是外部 explicit_config WCS 的通用要求。

外部天球坐标 ICRS degrees，RA `[0,360)`、Dec `[-90,90]`；内部向量/弧度依函数签名。Gaia 参考历元和实际使用的 epoch propagation 必须登记，ICRS 不等于把所有星的位置冻结在 J2000。SIP 的 A/B 与 AP/BP 同属于像元偏移域；无 AP/BP 时可迭代反演 A/B，不能忽略畸变。天区支持以球面多边形/MOC/model ref 表示，不能仅靠 RA 最小最大值。

`linear_unit` 给出积分信号的数值尺度；sphere signal 单位是该尺度/sr。每个控制点的 `reference_flux` 使用与其模板 h 配对的积分通量尺度，`flux_variance` 用该尺度平方，`reference_snr` 无量纲。相对标度必须记录归一参考 ID，不能无依据写 SI 或沿用未经标度的 ADU。BUNIT 和物理状态由实际应用结果生成，不按拟合是否存在猜测。

## 4. 端口与因果图

端口是数据合同，载体可在迁移期为已注册的正式 cache，目标为 Runtime 管理的 typed payload。`data_schema_id` 表示载荷，不能只是安排节点先后、数据仍靠猜文件路径取得。

| 节点 | 输入端口 | 输出端口 |
|---|---|---|
| calibration | identity、raw、master bindings | calibrated |
| cosmetic | calibrated | cleaned |
| WCS | identity、calibrated、astrometry config/catalogue | wcs |
| guided source/PSF | identity、cleaned、wcs、Gaia | sources、psf |
| photometry | identity、cleaned、wcs、sources、psf、per-frame response/reference | photprov、photoapplied |
| noise | identity、photoapplied、photprov、psf、sources、calibration uncertainty | noise_model、planar_reference（仅阶段内） |
| Drizzle | identity、photoapplied、wcs、psf、noise_model | stacked、drizzle_model、sparse_reference、frame_reference |
| writer | identity、wcs、photprov、noise_model、stacked、drizzle_model、sparse_reference、frame_reference | frame_product |

保留八个生产调度节点。Drizzle 得到球面观测后调用 **noise 库**完成 `sparse_reference`/`frame_reference`，不复制 SNR 数学，不回写 noise 节点，故没有循环依赖。若增加独立 sphere-completion 调度节点，必须保持上述依赖和算法所有权；现行调度合同保持八节点方案。

`planar_reference` 的量属于 `photo_applied_planar`，不能在 writer 改标签后当作 `drizzled_sphere`。生成最终控制点必须用同一次 Drizzle 的 R、`h_sphere=R h_planar`、`C_sphere=R C_planar Rᵀ` 和相应背景模板 `Z_sphere`。模型评估应使用算子/局部窗口，不要求生成全帧密集协方差矩阵。

## 5. 参考定义、状态与摘要

支持两种明确来源：

- `sed_response`：配置选择有身份的参考 SED 和归一（谱单位/参考星等、系统响应积分约定），逐帧利用真实 response 和已拟合 scale 得到 F_ref。缺曲线/零点则不可执行此转换，记录 unavailable。
- `explicit_linear_flux`：用户给正有限 F_ref、积分单位和独立 reference ID；只在该单位/身份内可比。可以计算条件数值 SNR，但不可冒称物理绝对标准或跨未知通带同一化。

未选参考不从实际恒星中位通量发明 canonical reference；信号可正常产出，reference precision unavailable。保留已有 F0/source-median 路径为具名 legacy diagnostic，不能接 canonical 端口。配置扩展与迁移键见实现规格。

空间标度非恒定时，参考响应和 F_ref 可随位置变化；每个控制点分别保存 F_ref。若源平面模板 P 按总积分量归一，施加 D_alpha 后，取 `a_ref(u)=sum_i alpha_i P_i`，`P'_i=alpha_i P_i/a_ref`，`F'_ref=a_ref F_ref`，则模板仍归一且 signal/covariance 各缩放一次。局部常量近似须具名并记录适用域，不能仅用中心 alpha 声称全足迹精确。

控制点由 noise model 有效 patch 中心的确定性网格产生，网格尺寸来自现有 patch 配置，边缘/空洞用 validity 表示；没有合格 patch 的区域不虚构点。星表用于掩膜和 PSF 拟合，控制点不必位于实际恒星处。PSF 无空间支持则该点 unavailable，不用 frame median 填充。

控制状态为 `available` 或 `unavailable`；available 有 `accuracy=conditional_complete|approximate|upper_bound` 和 `omitted_terms`。只有确定被省略项是 PSD 随机噪声、模板/有效域固定时，缺 source Poisson 的条件 SNR 才可称 upper_bound；有未定标增益、母版或模板误差而无此保证时用 approximate。这些状态不宣称系统精度完整。

帧级 reference 摘要固定为 **area-weighted median of log reference_snr** 后取 exp：各有效 patch 的真实球面支持面积为权重（patch 分区不重复计面积）；同时记录支持面积、有效点数和参考定义。摘要不替代局部 controls，不用它重缩放 controls。现有 `ACSD_FRAME_SNR` 保持其 legacy identity；球面 completion 已有独立摘要 producer `summarize_frame_reference`，不能据它存在就把兼容头字段改认作此摘要。其实际支持域仍须与本节合同核对。

## 6. 精度传递和秩亏

每点以冻结参考源条件 `C_ref` 求 GLS 的 V_F，使用对应 h、Z。C_ref 包含的是参考源的泊松期望，不是实际天区所有像元的真实源亮度方差。存储 `covariance_condition=reference_source` 和真实背景/噪声模型；阶段二需要实际面亮度方差时须按其条件重新评估，不能把参考源中心的 V_B 当作任意天区的噪声。

局部 R 可能秩亏；不得给 C_sphere 盲加 epsilon 后报告更高信息量。在同一保留子空间内处理 h、Z 与 C：使用显式秩判定/分解，报告秩与条件数；若参数不可辨识则 precision unavailable。h 位于协方差零空间可能是精确约束，须单独建模而非默默用伪逆丢弃。基础实现只接受可验证的可估计子空间，不宣称一般精确约束解已完成。

`surface_transfer` 独立于 SNR：available 时有目标 leaf、`surface_variance`、`transfer_factor=V_B/V_F`、单位和 covariance conditioning；unavailable 时只给原因。两种方差必须同源、同位置、同条件，`transfer_factor` 有量纲。有合法参考 SNR 但不能求 T 的点仍可发布；阶段二不得偷偷设 T=1。阶段二生产权重的切换不由本合同自动授权。

## 7. 唯一正式序列化

每帧产品根目录发布 **一个 `frame_science.json`**，机器形状为 [normalize_frame_science.schema.json](../../../eng/contracts/schemas/normalize_frame_science.schema.json)。它包含 frame/source/run、信号定位、WCS、photometry、reference、model refs、支持域、稀疏 controls 和帧摘要；模型较大时由相对路径+hash 引用同一产品内的 AIO 模型文件。Catalogue HiPS 可作可选浏览投影，但不作为第二份 canonical precision 真源。

JSON UTF-8，FP64 round-trip、禁止 NaN/Inf；未知数用 null，状态与 reason 必填。sky/flux/variance/PSF/covariance 每点引用不能悬空。文件路径不得逃出产品根；signal 指向真实 HiPS 子树；模型引用须在产品 manifest 中登记并覆盖哈希。共享外部母版可仅存身份/未建模状态，但下游所需可求值模型必须在产品内完整可读，不能依赖计算节点 `/testdata`。

writer 是唯一最终发布者。Drizzle 可以借用统一 HiPS writer 在 run-private staging 流式写 tile，节点工作完成不等于产品发布；writer 等所有 AIO 任务结束，验证 signal、sidecar、model refs 同帧同版本，最后原子发布完成 manifest。信号合法而 precision unavailable 是完整且能力受限的产品；完成 manifest 表示磁盘完整，不表示科学全误差已验证。

无 WCS/无合法信号的帧只进运行清单，不伪造 frame product。运行清单列出全部预期帧及 succeeded/failed/cancelled、各科学状态/原因与实际产品，允许保存其他合法帧，不能把部分成功报告为 N/N。

### 7.1 局部模型的基础序列化表示

基础模型保存每控制点局部数值窗口，不引入另一套任意核拟合算法。精确因子必须能按声明恢复同一 C，不能在显式矩阵标签下存短向量。`acsd.normalize.local-factor/v2` 以不同字段表达源方差、算子和逻辑协方差；显式模型用 `acsd.normalize.local-model/v1`。模型文件使用 UTF-8 JSON，包含 `model_schema`、`model_id`、`kind`、`domain`、`dtype=float64`、`unit`、`control_id`；数组元素按声明顺序排列，矩阵采用 row-major。内容 hash 来自实际文件，未知量用 null。模型较大时可由 AIO 以同字段/单位的版本化二进制载荷承载，不能序列化裸 C++ struct 或机器指针。实际 reader 的覆盖见实施差异。

| kind | 必需数值/索引载荷 | 计算意义 |
|---|---|---|
| psf | source pixel indices、**单位总通量的像元积分响应** P、flux_estimator、积分方法/子像元相位、f_enclosed/f_out、支持域、sphere leaf indices、h | h=R P；analytic_total 必须连续模型归一 + 实际像元求积，中心点未归一代理不具 canonical 精度 |
| covariance | 同一 sphere leaf 与 source pixel 顺序、维数、**显式 C_ref 或同一个精确 factor+CSR 表示**、rank/conditioning、background basis Z、随机分量/缺项 | 对称 PSD 的冻结条件模型；允许精确重建 C=R diag(v) Rᵀ，不强制落 nl×nl 数组 |
| drizzle_operator | source pixel indices、source Ω/A、sphere leaf indices、CSR row_offsets/column_indices/coefficients、shape | R 对已缩放积分像元的真实线性映射，非另算近似几何 |
| support | 每 patch 的 detector 边界、实际球面边界采样/面积、有效性与孔洞、leaf_order | 网格分区及模型有效域；边界曲线近似明确记录 |
| scale | 参数基函数/系数、有效域、输入/输出单位、application ID、参数误差状态 | 一次标度与参考模板变换 |
| calibration_uncertainty | master IDs、Jacobian/已有分量表示、共享源绑定、遗漏项 | 已建模误差；未测母版不补假矩阵 |

在生产模型明确为平面独立条件随机分量且 v_j>0 的情况下，优先使用 CSR(R) + 逐源像元 v_j 的精确因子表示，不要求默认写明文 nl×nl 协方差。显式矩阵用于小规模科学对照或可选表示。模型假设成立时这种表示没有统计近似，但不补回 omitted master/photometric correlation 或未知噪声分量。因子真实单位为 [linear_integrated_signal]²，R 单位 sr⁻¹，派生 C 单位 [linear_signal]²/sr²；三者不可共用一个 BUNIT。

`acsd.normalize.local-factor/v2` 分别声明以下字段，不以 `values=v_j` 的短向量 shape 和源单位代替逻辑 C 的 shape 和单位：

- `logical_covariance_shape=[nl,nl]`、`derived_covariance_unit=linear_unit²/sr²`、`covariance_condition=reference_source`；
- `factor_model=planar_diagonal`、`factor_variance=v_j`、`factor_shape=[m]`、`factor_variance_unit=linear_unit²`、`source_pixel_indices`；
- `operator_ref=<same control R>`、`operator_shape=[nl,m]`、`operator_unit=sr^-1`、`leaf_ipix_order=HEALPix_NESTED`、真实 CSR row/column/coeff 与每叶完整 N_p；
- `background_basis_shape=[nl,n_bg]`、`background_basis_unit/basis_definition`（Z 必须与被估计的 β 单位成对）、`rank/condition/solver/relative_tol` 和独立模型/hash refs；
- `assumptions=[diagonal_planar_condition]` 及所有 `omitted_terms`，不把模型内的“精确”冒称物理世界中所有误差都已建模。

同一个完成模型以 **C=R diag(v)Rᵀ** 精确重建、能与小窗显式 C 逐项对拍，才可写 `exact_factorization_of_model=true`。如果未来有平面共同母版/标度低秩项，必须使用声明了额外因子的更一般模型/新版本，不能把源噪声近似成 diagonal 后仍称全协方差已建模。已有 v1 读侧按旧含义/真实来源做严格兼容或拒绝 canonical 使用，不允许悄悄重新标记其数值单位。FrameScience v1 可以通过已有具版本的 model refs 指向 factor v2；只有发生破坏性 top-level 形状修改时才升级 frame_science schema。

同一局部窗口的索引、h、C、Z 和 R 必须维数相容；CSR 数组非负几何系数及归一按实际有效样本验证。计算 `V_F` 不能先逆全帧矩阵，采用局部 SPD 解或一致的可估计子空间分解；留出/截断/窗口模型误差单独标记。每个控制点可引用自己的模型 ID，多个点确实共享同一模型时才共用 ID。跨控制点不凭同名 PSF 强加同一数值矩阵。

基础局部表示足以重算控制点条件 GLS 和对应位置的 transfer。连续位置与 actual-scene 条件需真实可求值的平面 PSF/noise/geometry 模型或经过标定的重建；局部窗口不冒称全帧任意位置已精确建模。阶段二 reader 据此识别能力，保留当前 per-sample-ivar 基线。

## 8. 读取兼容与工作缓存

| 输入 | 读取结果 | 禁止行为 |
|---|---|---|
| frame_science.json 支持的 schema | 验证形状、身份、hash、单位与模型引用后读取 | 未验证引用直接算 canonical 权重 |
| 无 sidecar 的现有 HiPS | signal/有来源的 variance 基线照常读；canonical precision unavailable | 从 catalogue `snr_psf` 或 ACSD_FRAME_SNR 自动补 canonical |
| v1 sparse_snr_layer 像素网格 | legacy view；只有完整 reference/units/domain/models 能经显式 adapter 验证时转为新对象 | 单靠 schema 可加载就宣称球面 C/h 完整 |
| 不支持的新 schema | signal 可按其独立合同读；precision unavailable 并记 unsupported_schema | 猜字段/无声 T=1 |
| JSON/模型文件损坏 | 报具体数据错误，不能冒充正常 unavailable | 吞错改零或继续引用坏 hash |

阶段内 cache 按 `(run,frame,object,revision,input hashes)` 登记，写读统一 AIO/ArtifactStore，OFF/ON 都存在且相同。迁移必须按边一次替换 producer 和 consumer，保留读取兼容直到调用点迁完；不能先关写盘而消费者仍读目录。科学 spill 是 Runtime 内存预算行为；开发 snapshot 永远不作输入或恢复来源。

reader 除 shape 还须核对：control_id/model_id 唯一；所有引用存在且 kind/domain 正确；leaf 编号在声明阶内且与 sky 坐标一致；V_F、SNR 和 F_ref 配对；available T 与 V_B/V_F 同单位同条件；frame summary 的 n_valid/面积/数值由有效 controls 得到；CRVAL/Dec、CD 非奇异与 SIP 维数合法；photometry 状态、application ID、单位和真实 signal 标度一致。JSON Schema 不表达这些物理关系或外部文件 hash，执行者在统一 reader 完成，不能把 shape 校验冒称科学验证。

### 因子单位与逐控制点诊断

local-factor/v2 量纲一致性同时包括**线性单位自身**与立体角幂次。源 v_j 的单位为 `L²`，算子 R 的单位为 `sr^-1`，派生 C 的单位必须为 `L²/sr²`，其中 L 是该 frame 真正施加 photometric scale 后的线性 signal 单位；`adu^2` 和 `electron^2/sr^2` 即使 sr 次幂满足计算规则，也**不是**一个可直接组合的模型。reader 必须核对 base linear unit 和 sr exponent，未知转换不许仅因 sr 指数正确就接纳。

每个控制点的 PSF 积分数据应根据**该控制点自己的** profile、pixel footprint 记录 f_enclosed/f_out、subpixel_phase、pixel_integration_error、PSF 参数和有效域。跨控制点的最小值/最大值只能作为帧摘要，不能冒充每一个控制点数据。representation、representation_note 与 models[] 引用应来自同一版本，写 v2 时同一模型的自由文本域不应残留 v1 的声明。

生产逐点诊断、表示文本和单位校验的具体差异集中见[实施地图](../architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。读写检查的独立证据见[因子与 PSF 复核](../../../实验/normalize-precision-p5/ROUND2_INDEPENDENT_REVIEW_633FFC31.md)，不能据部分结构检查通过宣布全部模型可独立求值。
## 参考文献

[1] [Normalize 科学正本](../../science/normalize/NORMALIZE_SCIENCE.md)。

[2] [Normalize 生产链与模块责任](../architecture/NORMALIZE_PIPELINE.md)。
