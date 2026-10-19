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
| `PhotometricSolution` | 响应来源 parametric_narrowband(top_hat_approx/v1,c nm,FWHM nm) 或 measured_curve(有序曲线hash)、QE 独立identity/内容hash、响应适用域、XP覆盖与误差状态；response/reference IDs、fit coefficient convention、scalar/spatial scale、fit support、parameter covariance state、input/output units、application ID、fit status | photometry | applied plane、noise、writer |
| `PhotoAppliedPlane` | cleaned pixels × actual scale、同标度 validity、`application_id`、unit、corresponding photometric revision | photometry | noise、Drizzle |
| `ConditionalNoiseModel` | actual-scene/source-reference 的明确条件、已知分量位图、未知/遗漏项、共享校准因子组与跨域相关；domain、background expectation/statistic、随机分量及来源、gain/RN provenance、source-Poisson conditioning、master/cosmetic correlation、共同参数单独建模状态 | noise library（校准提供其输入） | Drizzle、reference precision |
| `DrizzleObservation` | signal 累加器或其 AIO 句柄、有效域、WCS revision、叶阶/ordering/pixfrac、精确面积口径、实际 R 算子或可求值模型、球面 PSF/协方差 refs | Drizzle | spherical precision、writer |
| `SparseReferencePrecision` | 唯一 reference、sphere domain、支持域、FP64 control records、F_ref/V_F/SNR、PSF/noise/background/geometry refs、精度和缺项 | noise library 的 sphere completion | writer；跨阶段 reader |
| `FrameScientificProduct` | 发布 signal、frame_science.json、模型文件及 hashes、帧状态、产品完整性 | writer | Mosaic |

标量字段缺失用 `optional`，不能用 0 表示未知 gain、方差或通量。对象可用性与执行错误分开：缺 PSF 可能使 reference precision unavailable，但不凭此删除已有合法信号；损坏模型、非法数值按正常数据错误处理。

## 3. 坐标与单位的精确合同

内部数组中心 `(x,y)=(column,row)` 从 0 开始，首像元边界为 `[-0.5,0.5]`。FITS WCS 的 CRPIX 始终存 1-based 数值；公共内部 `pixel_to_icrs(x0,y0)` 在调用 FITS/SIP 变换时加 1，`icrs_to_pixel` 返回时减 1。已经为 FITS 1-based 的 IPV 输出先在 IPV adapter 归一，不能重复加 1。CRPIX 可为任意合法有限值；`w/2+0.5` 是 IPV 当前生成规则，不是外部 explicit_config WCS 的通用要求。

外部天球坐标 ICRS degrees，RA `[0,360)`、Dec `[-90,90]`；内部向量/弧度依函数签名。Gaia 参考历元和实际使用的 epoch propagation 必须登记，ICRS 不等于把所有星的位置冻结在 J2000。SIP 的 A/B 与 AP/BP 同属于像元偏移域；无 AP/BP 时可迭代反演 A/B，不能忽略畸变。天区支持以球面多边形/MOC/model ref 表示，不能仅靠 RA 最小最大值。

`linear_unit` 给出积分信号的数值尺度；sphere signal 单位是该尺度/sr。冻结线性基单位词表（#9）只有两个成员：`ADU`（未施加带单位标度）与 `XPMODEL`（已施加 Gaia XP 合成通带标度后的模型通带通量单位，由该帧实际 response 归一，相对模型量、非 SI）；canonical 串为 `L/sr`、`L^2/sr^2`、`sr^2/L^2`，由冻结单位表唯一实现导出（`lib/algorithms/resample/p3_rsmp_units.cpp`）。每个控制点的 `reference_flux` 使用与其模板 h 配对的积分通量尺度，`flux_variance` 用该尺度平方，`reference_snr` 无量纲。相对标度必须记录归一参考 ID，不能无依据写 SI 或沿用未经标度的 ADU。BUNIT 和物理状态由实际应用结果生成，不按拟合是否存在猜测。


### 3.1 给 Mosaic 的科学数值与相关身份（R1–R5 综合）

本合同现有 FrameScientificProduct 保持不变；其可求值内容必须满足下列最小因果义务，而非引入另一整套对象仓：

1. 由 Gaia fit 发布 response 有序 hash、reference SED/XP band support、实际 application_id、alpha(u) 与参数 Cov/Jacobian 的 availability；未知 gain、口径或窄线目标 SED 的系统项按 unknown/omitted 表达。若未施加 Gaia，单位仍为 ADU；不得将两个不同 linear_base 在 P2 内悄然合并。
2. 由严格几何发布 \(R_{pj}=(a_{jp}/A_j)/N_p\) 的源像元索引与 normalized coefficients（或不可变可重建引用）、\(\Omega_j,A_j,a_{jp},N_p\) 精度预算、valid/signal/support。R 与叶 index 使用同一 NESTED 阶数和 WCS/SIP 身份。仅有角点、closure 或中心定位的记录不能充当物理曲边验证。
3. 由 noise completion 发布 **不同估计量** 的 reference V_F/SNR 与 scene V_B/noise factors；Mosaic 首选 actual-scene (V_B=r_pC_{x,actual}r_p^T)，或先重建可观测场景噪声/PSF 参数再经 R 正向求值，reference Fisher 仅在 same-condition T 有效时桥接；控制点的 estimator_kind、observed/predicted、support_domain 和 model error 不可混用；对参与 R 的未知 variance 位必须传播 full_variance_unavailable，partial variance 只可作诊断。
4. 跨共享 Gaia 响应/母版或多帧天空拟合的 Cov group_id、参数协方差与模型缺项供下游低秩消费；不能要求写全局巨大 Cov 矩阵。target leaf p 不因自己的 reference precision unknown 就丢失已可用的合法 signal。
5. 第三阶段只从 Mosaic 正式 signal/response/provenance 导出 FITS；不能凭一个单 signal 恢复原始 R、full covariance 或真实绝对通带。

默认目标是一个明确的 target_order（所有设备的原始像元在 P1 直接投到这一阶）；P2 混 order 返回显式不支持且无完成产物。不同原始像元尺度、有效 PSF、filter 与设备型号不因此受限。


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

空间标度按[噪声分册](../../science/noise_snr/NOISE_SNR.md)的位置参考约定配对：原生解析总通量F_native、单位总通量P、实际位置标度α_*=α(u)，每控制点保存`F_ref(u)=α_* F_native`及`h_planar,j=(α_j/α_*)P_j`。F_ref是位置参考幅度参数，单位与applied积分量一致；**不是一般空间场下实际applied积分总通量**。h_planar完整积分不要求1，P不按有限窗重归一，signal与covariance分别按一/二次幂变换。

`reference.normalization=native_calibrated`表示所给flux为F_native，逐点由实际α_*转换；`photo_applied`表示所给flux已是位置参考参数数值，不再乘标度，不因帧级给常数而声称全域同一个实际物理源。两者保存normalization与参考ID。实际总通量另求`F_applied,total=F_native Σ_all α_jP_j`，完整域与窗口诊断`alpha_window_scale`分名；没有完整响应域时不声明已求得总量。确定性参数换算与方差/SNR关系由科学正本定义。

控制点由 noise model 有效 patch 中心的确定性网格产生，网格尺寸来自现有 patch 配置，边缘/空洞用 validity 表示；没有合格 patch 的区域不虚构点。星表用于掩膜和 PSF 拟合，控制点不必位于实际恒星处。PSF 无空间支持则该点 unavailable，不用 frame median 填充。

控制状态为 `available` 或 `unavailable`；available 有 `accuracy=conditional_complete|approximate|upper_bound` 和 `omitted_terms`。只有确定被省略项是 PSD 随机噪声、模板/有效域固定时，缺 source Poisson 的条件 SNR 才可称 upper_bound；有未定标增益、母版或模板误差而无此保证时用 approximate。这些状态不宣称系统精度完整。

帧级 reference 摘要固定为 **area-weighted median of log reference_snr** 后取 exp：各有效 patch 的真实球面支持面积为权重（patch 分区不重复计面积）；同时记录支持面积、有效点数和参考定义。摘要不替代局部 controls，不用它重缩放 controls。现有 `ACSD_FRAME_SNR` 保持其 legacy identity；球面 completion 已有独立摘要 producer `summarize_frame_reference`，不能据它存在就把兼容头字段改认作此摘要。其实际支持域仍须与本节合同核对。

## 6. 精度传递和秩亏

每点以冻结参考源条件 `C_ref` 求 GLS 的 V_F，使用对应 h、Z。C_ref 包含的是参考源的泊松期望，不是实际天区所有像元的真实源亮度方差。存储 `covariance_condition=reference_source` 和真实背景/噪声模型；阶段二需要实际面亮度方差时须按其条件重新评估，不能把参考源中心的 V_B 当作任意天区的噪声。

局部 R 可能秩亏；不得给 C_sphere 盲加 epsilon 后报告更高信息量。在同一保留子空间内处理 h、Z 与 C：使用显式秩判定/分解，报告秩与条件数；若参数不可辨识则 precision unavailable。h 位于协方差零空间可能是精确约束，须单独建模而非默默用伪逆丢弃。基础实现只接受可验证的可估计子空间，不宣称一般精确约束解已完成。

Mosaic 优先从实际场景噪声与球面 operator 求 (V_B)；未知场景时可构建有支持域/模型误差身份的噪声-PSF 预测。`surface_transfer` **不是首选求解路径**，独立于 SNR：available 时有目标 leaf、`surface_variance`、`transfer_factor=V_B/V_F`、单位和 covariance conditioning；unavailable 时只给原因。两种方差必须同源、同位置、同条件，`transfer_factor` 有量纲；对任一非零 Drizzle 系数对应方差或跨系数 covariance 缺失时，完整 scene variance 状态为 unavailable；仅可单独发布 known partial 和 omitted_terms，不生成 full IVAR。有合法参考 SNR 但不能求 T 的点仍可发布；阶段二不得偷偷设 T=1。阶段二生产权重的切换不由本合同自动授权。

## 7. 唯一正式序列化

每帧产品根目录发布 **一个 `frame_science.json`**，机器形状为 [normalize_frame_science.schema.json](../../../eng/contracts/schemas/normalize_frame_science.schema.json)。它包含 frame/source/run、信号定位、WCS、photometry、reference、model refs、支持域、稀疏 controls 和帧摘要；模型较大时由相对路径+hash 引用同一产品内的 AIO 模型文件。Catalogue HiPS 可作可选浏览投影，但不作为第二份 canonical precision 真源。

JSON UTF-8，FP64 round-trip、禁止 NaN/Inf；未知数用 null，状态与 reason 必填。sky/flux/variance/PSF/covariance 每点引用不能悬空。文件路径不得逃出产品根；signal 指向真实 HiPS 子树；模型引用须在产品 manifest 中登记并覆盖哈希。共享外部母版可仅存身份/未建模状态，但下游所需可求值模型必须在产品内完整可读，不能依赖计算节点 `/testdata`。

writer 是唯一最终发布者。Drizzle 可以借用统一 HiPS writer 在 run-private staging 流式写 tile，节点工作完成不等于产品发布；writer 等所有 AIO 任务结束，验证 signal、sidecar、model refs 同帧同版本，最后原子发布完成 manifest。信号合法而 precision unavailable 是完整且能力受限的产品；完成 manifest 表示磁盘完整，不表示科学全误差已验证。

无 WCS/无合法信号的帧只进运行清单，不伪造 frame product。运行清单列出全部预期帧及 succeeded/failed/cancelled、各科学状态/原因与实际产品，允许保存其他合法帧，不能把部分成功报告为 N/N。

### 7.1 局部模型的基础序列化表示

基础模型保存每控制点局部数值窗口，不引入另一套任意核拟合算法。精确因子必须能按声明恢复同一 C，不能在显式矩阵标签下存短向量。`acsd.normalize.local-factor/v2` 以不同字段表达源方差、算子和逻辑协方差；显式模型用 `acsd.normalize.local-model/v1`。模型文件使用 UTF-8 JSON，包含 `model_schema`、`model_id`、`kind`、`domain`、`dtype=float64`、`unit`、`control_id`；数组元素按声明顺序排列，矩阵采用 row-major。内容 hash 来自实际文件，未知量用 null。模型较大时可由 AIO 以同字段/单位的版本化二进制载荷承载，不能序列化裸 C++ struct 或机器指针。实际 reader 的覆盖见实施差异。

| kind | 必需数值/索引载荷 | 计算意义 |
|---|---|---|
| psf | source pixel indices、**原生单位总通量像元响应** P、planar_template=h_ref、source_pixel_alpha、reference_alpha_star、flux_estimator、积分/相位/包络、支持、sphere leaf indices、h_sphere | h_ref=(α/α_*)P，h_sphere=R h_ref；analytic_total限定P/F_native，不强令h_ref窗内或完整积分为1 |
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

同一个完成模型以 **C=R diag(v)Rᵀ** 精确重建、能与小窗显式 C 逐项对拍，才可写 `exact_factorization_of_model=true`。如果未来有平面共同母版/标度低秩项，必须使用声明了额外因子的更一般模型/新版本，不能把源噪声近似成 diagonal 后仍称全协方差已建模。研发期错误的 frame-science/v1 与有错误科学声明的旧局部模型均不属于正式产品输入域；正式 FrameScience 顶层唯一为 v2。已知 v1 产品拒绝并从原始 FITS 重算，不做兼容、模型内容改标签、版本猜测或 +9 修补。局部模型可继续采用语义正确且可独立验证的 local-factor/v2；其版本号独立于 FrameScience 顶层版本。

同一局部窗口的索引、h、C、Z 和 R 必须维数相容；CSR 数组非负几何系数及归一按实际有效样本验证。计算 `V_F` 不能先逆全帧矩阵，采用局部 SPD 解或一致的可估计子空间分解；留出/截断/窗口模型误差单独标记。每个控制点可引用自己的模型 ID，多个点确实共享同一模型时才共用 ID。跨控制点不凭同名 PSF 强加同一数值矩阵。

基础局部表示足以重算控制点条件 GLS 和对应位置的 transfer。连续位置与 actual-scene 条件需真实可求值的平面 PSF/noise/geometry 模型或经过标定的重建；局部窗口不冒称全帧任意位置已精确建模。阶段二 reader 据此识别能力，保留当前 per-sample-ivar 基线。

## 8. 正式 v2 读取与工作缓存

| 输入 | 读取结果 | 禁止行为 |
|---|---|---|
| frame_science.json 支持的 schema | 验证形状、身份、hash、单位与模型引用后读取 | 未验证引用直接算 canonical 权重 |
| 已发布 v2 帧产品缺失 sidecar | DATA_INCOMPLETE，拒绝整份 v2 产品 | 从旧 HiPS 或 catalogue 推造 precision |
| frame-science/v1 或旧 sparse_snr_layer | UNSUPPORTED_SCHEMA，拒绝作为正式产品输入；从原始 FITS 重算 | +9 修补、迁移、兼容 adapter、静默降级 |
| 未知 schema 或科学字段冲突 | UNSUPPORTED_SCHEMA 或 DATA_CONFLICT，拒绝产品 | 猜字段、视为合法 unavailable、无声 T=1 |
| JSON/模型文件损坏 | 报具体数据错误，不能冒充正常 unavailable | 吞错改零或继续引用坏 hash |

阶段内 cache 按 `(run,frame,object,revision,input hashes)` 登记，写读统一 AIO/ArtifactStore，OFF/ON 都存在且相同。实施时按生产调用链一次性替换 producer、reader、schema、发布及 P2/P3 消费者；不在正式版本中保留 v1 reader 或数据迁移。不得先关上游写盘而使既有生产消费者悬空。科学 spill 是 Runtime 内存预算行为；开发 snapshot 永远不作输入或恢复来源。

reader 除 shape 还须核对：control_id/model_id 唯一；所有引用存在且 kind/domain 正确；leaf 编号在声明阶内且与 sky 坐标一致；V_F、SNR 和 F_ref 配对；available T 与 V_B/V_F 同单位同条件；frame summary 的 n_valid/面积/数值由有效 controls 得到；CRVAL/Dec、CD 非奇异与 SIP 维数合法；photometry 状态、application ID、单位和真实 signal 标度一致。JSON Schema 不表达这些物理关系或外部文件 hash，执行者在统一 reader 完成，不能把 shape 校验冒称科学验证。

### 因子单位与逐控制点诊断

local-factor/v2 量纲一致性同时包括**线性单位自身**与立体角幂次。源 v_j 的单位为 `L²`，算子 R 的单位为 `sr^-1`，派生 C 的单位必须为 `L²/sr²`，其中 L 是该 frame 真正施加 photometric scale 后的线性 signal 单位；`adu^2` 和 `electron^2/sr^2` 即使 sr 次幂满足计算规则，也**不是**一个可直接组合的模型。reader 必须核对 base linear unit 和 sr exponent，未知转换不许仅因 sr 指数正确就接纳。

每个控制点的 PSF 积分数据应根据**该控制点自己的** profile、pixel footprint 记录 f_enclosed/f_out、subpixel_phase、pixel_integration_error、PSF 参数和有效域。跨控制点的最小值/最大值只能作为帧摘要，不能冒充每一个控制点数据。representation、representation_note 与 models[] 引用应来自同一版本，写 v2 时同一模型的自由文本域不应残留 v1 的声明。

生产逐点诊断、表示文本和单位校验的具体差异集中见[实施地图](../architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。读写检查的独立证据见[因子与 PSF 复核](../../../实验/normalize-precision-p5/ROUND2_INDEPENDENT_REVIEW_633FFC31.md)，不能据部分结构检查通过宣布全部模型可独立求值。
## 参考文献

[1] [Normalize 科学正本](../../science/normalize/NORMALIZE_SCIENCE.md)。

[2] [Normalize 生产链与模块责任](../architecture/NORMALIZE_PIPELINE.md)。


## 9. frame-science/v2 唯一冻结目标合同（Issue #10；2026-10-17）

**性质：TARGET / NOT_RUN。** 截至源码基线 `d09371c7f40e`，`write_frame_science_document` 与 `read_frame_science_document` 仍硬编码 `acsd.normalize.frame-science/v1`，仓库中 `eng/contracts/schemas/normalize_frame_science.schema.json` 及两个 examples 也是 v1。此节冻结后续**同步切换**的唯一正式语义；文档已冻结**不是**已有产物 v2 已发布。机器 schema / 实例在代码 Agent 的同一原子实现提交中升级，不提前让旧 writer 与新 schema 在 main 上不匹配。

### 9.1 版本、字段与单位（唯一单表）

顶层必须为 `schema_id="acsd.normalize.frame-science/v2"`、`schema_version=2`、`product_role="normalize_frame"`。顶层字段集合仍以现有 JSON schema 所列 required 为基础：`run_id,frame_id,input_occurrence,source_sha256,config_sha256,software_sha,signal,wcs,photometry,reference,models,calibration,drizzle,precision,source_dimensions,exposure_seconds,exposure_source`；严禁 `additionalProperties` 冒充未指定的科学字段。v2 的 **software_sha 只能表示有来源的构建时源码 HEAD**；在 #75 身份合同落地前记录 `unverified` 而不能将 configure 期陈旧串当真实性证明。另于产品 manifest 绑定真实已加载 CLI/DLL SHA-256、build source digest、配置、输入及响应内容哈希；SHA 字段若声明为 sha256 必须是 64 位小写十六进制字符串。

- `input_occurrence`：0-based、非负整数，按冻结输入序列中的每次出现分别计数；`frame_id` 唯一绑定该 occurrence，不能只取 basename；`source_sha256` 是原始 FITS 内容身份，`run_id/config_sha256` 绑定此次执行。
- `source_dimensions=[width,height]` 为两个正整数，`exposure_seconds` 为已知时正有限数，否则在上游给出具名不可用状态，禁止 0 充当合法曝光；WCS solved 时 `crval_deg/crpix` 各二元素，`cd_deg_per_pixel` 四元素非奇异、SIP 全系数有限且索引域有效。
- `signal`：`hips_path` 为产品根内安全相对路径，`ordering="nested"`，`tile_width=512`，`scale_state` 限 `native_calibrated|relative_scaled|response_calibrated`，`invalid_policy` 显式给出。声明数值 `linear_unit=L` 时 `bunit=L/sr`（`L` ∈ {`ADU`,`XPMODEL`}，由实际施加状态派生：`native_calibrated` ⇒ `ADU`，逐帧实际施加 `k_photo` ⇒ `XPMODEL`）；`photometry.scale_applied` 和 `application_id` 必须与真实像素运算、variance/ivar 单位及相应响应模型一一匹配，不能无条件写 `ADU/sr`。
- 核心几何不变量：`Nside=2^K`（正整数 2 的幂），`signal.leaf_order=K=log2(Nside)`；`tile_order=K-log2(tile_width)=K-9` 且 **tile_order 仅用于 HiPS 目录** `Norder{tile_order}`。本 512 tile 帧产品可接受的基础域为 `9<=K<=29`（还须满足生产几何、资源和 id 溢出检查）；`signal.leaf_order`、所有可用的 `precision.controls[].leaf_order`、模型 support/operator leaf order **严格相等**，不做容差或自动 +9。
- 叶下标是 NESTED 的无符号 64-bit 整数，JSON `leaf_ipix` 必须以不带指数/前导 + 的十进制**字符串**存储，解析后满足 `0<=ipix<12*4^K`；包括 ipix=0。由同一 HEALPix oracle 求 `ang2pix_nest(K,ra,dec)`，不得拿 tile `Npix` 或局部 x/y 替代球面位置。叶立体角 `Omega_leaf=4*pi/(12*4^K)` sr；K=11 时 Nside=2048、tile_order=2，误用 2 的叶面积会相差 `4^9=262144`。
- `precision` 在 `drizzled_sphere` 域用 float64 保存控制点；`available` 控制点的 `F_ref>0,V_F>0,SNR=F_ref/sqrt(V_F)` 有限且单位配对，模型 ID、PSF h、noise C、nuisance Z、R、支持域和 reference 定义完整；`unavailable` 给具名 reason，不能伪造 0、NaN、Inf 或空模型为可用。成功且 `precision.status=unavailable` 的 **v2 sidecar 仍必须存在**；`frame_summary` 的有效数目、非重复支持面积和加权中位数从 controls 可复算。
- `models[]` 的 `model_id` 全局唯一，`kind/domain/representation` 与实际文件吻合；相对路径不得含上溯、绝对路径、符号链接逃逸，模型文件的实际 SHA-256 与 manifest 一致。局部 C 模型的 `v_j` 单位 `L^2`、CSR(R) 的单位 `sr^-1`、逻辑 C 单位 `L^2/sr^2`，验证 `h_s=R h_p`、`C_s=R diag(v)R^T`，并记录条件随机项/相关性缺项；支持小窗精确分解后核对 GLS。不得拿 schema 形状验收替代数值科学验收。

### 9.2 发布、阶段消费与错误域

`p1_op_drizzle` 在 run-private `<work>/<frame_key>/frame_science` 写入暂存文件、model refs；`p1_op_writer` 等 AIO 完成后，按 `frame_id/run_id/occurrence/source/config`、几何/单位、模型 hashes、signal tiles 和科学状态校验；**先形成不可见完整 revision，再以唯一完成指针发布**。当前 `publish_staged_frame_science` 将各文件复制、原子写 sidecar、回读；其成功并不自动证明跨多个文件的统一 revision pointer 已实现。只写出 HiPS tiles 没有 sidecar 与完成指针 = 未提交产品，不是 precision unavailable。

返回语义分离：`OK_AVAILABLE`（完整发布、可用 precision）、`OK_PRECISION_UNAVAILABLE`（完整 v2 sidecar、合法 signal、有具名不可用原因）、`NOT_APPLICABLE`（原始 FITS / 尚未发布的私有暂存目录，无 sidecar 是正常工作流）、`DATA_INCOMPLETE`（声称已发布但缺 sidecar/tiles/manifest）、`UNSUPPORTED_SCHEMA`（v1、未知 schema）、`DATA_CORRUPT`（JSON、哈希、坏数值、损坏模型）、`DATA_CONFLICT`（同一帧身份、leaf 几何、模型条件、单位或状态互相矛盾）。前两个是成功能力状态；其余不得被包装成 precision unavailable。v1/旧错误产品直接拒绝；**以原始 FITS 重算，没有识别旧 K-9、无迁移/兼容/自动 +9/回退**。

这份 v2 是目标科学合同；response_mode、QE hash 与 SceneNoise 等新增建议须与 schema/serializer/reader/实际 CLI 一起实施，**不可在未更新 schema 时直接写入自造字段并宣称生产已消费**。Stage 1 只负责正确生成与发布；Stage 2 按完成指针 → v2 reader → 验证球面模型 → 解析有效控制域，之后才可在**相同被估计量及噪声条件**下消费精度。现行 `p2_sparse_layer_probe` 仍只读取 `ACSD_SPARSE_SNR_LAYER`，不是 v2 消费；切换时删除将旧 pixel sparse view 当 canonical 的入口。Mosaic 的逐样本**actual-scene** variance/ivar 在误差完整及同估计量前提下成为优先面亮度权重；其次通过真实噪声/PSF/标度模型和 R 预测 scene variance；点源 `I_F=1/V_F` 不直接等于 `1/V_B`，转换须有同条件 `T=V_B/V_F`（#8），actual scene 不满足时 precision 仅供诊断、权重保持具名来源。Stage 3 读取 Stage 2/Stage 1 真正完成的正式产品及保留下来的线性单位、响应/相关误差身份；不得靠产品名猜单位或解读 v1。

### 9.3 并行代码任务的确定性负例

(1) `K=9,11,13` 对应 Nside 512/2048/8192、Norder 0/2/4；任何 control 的 K-9、tiles 错目录、角面积错、极区/face 边界叶映射不符 → DATA_CONFLICT。(2) 同源 FITS 出现两次，`source_sha256` 相同但 occurrence 0/1，frame_id 不得碰撞。(3) v1（包括 `leaf_order=K` 但仍标 v1）/缺 sidecar 的已完成目录 → 明确拒绝。(4) `precision.status=unavailable` 且完整 v2 sidecar → OK_PRECISION_UNAVAILABLE。(5) 修改 model 字节、交换不同帧 sidecar、把 photapplied 的 `bunit` 改成 `ADU/sr`、把 `adu^2` 与 `electron^2/sr^2` 拼接、`leaf_ipix=2^53+1` 用 JSON 浮点存 → 分别明确 DATA_CORRUPT/CONFLICT。(6) 独立真实 reader + 外部 HiPS/FITS 读回，报告 `TARGET/NOT_RUN/PASS` 各测试的实际证据，不把文档裁决写为生产通过。
