# 标准科学 API 与驱动协议

本合同定义目标接口；已有可编译声明与schema，科学函数及驱动程序尚未实现。科学类型遵循[统一对象](OBJECTS.md)，所有 action 的域、单位、原点及随机变量身份必须匹配。独立 Agent 只需本合同和五份科学规范即可构造输入，不需要理解旧 CLI、adapter、scheduler 或 session。

## 统一调用约定

所有接口形如 `operation(Context, immutable inputs, request) -> Result<T>`。Context 只含 caller 提供的 allocator、scratch、取消 token、work/bytes/线程租约及可选只读 provider；科学配置全部在具名 request 内。纯数学调用不需要磁盘、CLI、全局配置或 Runtime。拥有内存的 in-memory provider 足以运行每项原语；production 使用同一算法 API 的 file-backed provider。

`Result<T>={value?,assessment,issues[],consumed_resources,dependencies}`。输入错误在读取值之前拒绝；取消、资源耗尽、能力缺失与模型未决分别返回。值存在不表示所有泛函成功：area 可以有有效界且 science_unconverged。failure 不返回可误用的任意 sky/权/方差。每个 issue 含 code、scope（run/frame/block/leaf/functional）、origin、reason、影响和可重试条件。异常不得跨 C 边界，算法不得 exit。

同步输入 span 仅借用到调用返回；异步请求先取得显式 retain 租约，completion 之后才释放。返回对象由 caller allocator 或 opaque handle 持有，唯一 release 方明确；模块不能保存栈 span。只读模型可并发，scratch/迭代游标/输出缓冲每次调用独占。取消在有界工作单元边界检查；精确裁剪/求解一次不可中断区的最大工作/位长也计入预算。

## 科学操作表

下表为规范签名；集合输入均带 ordered identity 和单位，`Request` 包含该行目标容差与模型选择。`Owned` 为拥有输出，provider 返回有租约的只读块。

| 操作 | 输入 → 输出 | 特定前置/失败 |
|---|---|---|
| `make_wcs` | HeaderFields、DetectorDomain、AstrometricContext → WcsModel | 显式 ICRS、二维 TAN[-SIP]、degree 输入、CD 或 PC+CDELT、0或2…9阶、cond₂≤10⁸、≤60°域和全局单射证据；完整准入服从科学规范 |
| `to_fits / from_fits` | Center0 或 Edge0 ↔ FitsPixel1 | 精确一次 ±1；角色显式，禁止隐式 double pair 重载 |
| `forward / inverse` | WcsModel、center/edge 或 SkyICRS、数值 budget → sky 或 center/root_box/residual | 背半球、horizon、边界根盒未决、不收敛具名返回；AP/BP 仅初值 |
| `derivative / solid_angle_jacobian` | WcsModel、点或 box → J/J_error 或 g interval | 来自同一解析连续模型；不拿中心 g 乘区域当面积 |
| `footprint / measure / compute_overlap` | provider、DetectorFootprint、pixfrac、leaf/order、GeometryBudget → 曲边、Ω/A/a/约束/未决域 | source 收缩在 detector；输出有向边界/有效域，不能只四角；候选超限保位置与质量 |
| `calibrate_observation / regroup_observation` | RawObservation、masters/readout/flat、G/真实 footprint → NativeObservation | dark 含 bias 与曝光规则显式；gain 未知不伪造 shot noise；mean-bin 积分身份显式恢复 |
| `solve_astrometry` | 校准像元、blind-detection request、本地 catalogue/context → AstrometricSolution 或 unsolved | 内含WcsModel、参数误差/随机根及同观测influence或显式unknown、适用域/模型偏差；solver候选单独输出；无输入WCS仍实际尝试 |
| `measure_science_sources / fit_psf` | 校准观测、WCS、Gaia positions、PSF/PRF/窗口请求 → StarFlux、EffectivePSF、influence | 像元积分而非整数 PSF 采样；不把有限窗重归一成总通量 |
| `integrate_response` | Response、SpectralBasis、integration budget → v/error/domain | 分段折点、不双乘 photon conversion；缺谱域/XP 分辨率偏差单列 |
| `fit_photometry / apply_scale` | StarObservations/共享 XP latent、responses/basis/gauge/noise → PhotoSolution；solution+NativePatch → CalibratedPatch | flux 域联合白化 QR/SVD/分块消元，rank不足不 ridge；application_id 去重、空间 α 逐像元 |
| `build_drizzle` | OverlapProvider、SourceSignals、Request → DrizzleOperator、SignalEstimate | F=Σwx,N=ΣwΩ,B=F/N,R=w/N；平均强度入口另用 ΣwΩb；signed 分式界与共同约束保留 |
| `refine_geometry` | RefinableGeometryProvider、base snapshot、primitive绝对限额、arithmetic/resources → GeometryRevision | 返回新不可变Ω/A/a、R或其重建recipe、受影响source/leaf/constraint集合和完整未决状态；不能原地修改旧action |
| `apply / transpose_apply / compose` | LinearAction、有域单位的 vector → OwnedValues 或 action | 检查输入/输出索引版本；transpose是真转置，不是重新采样 |
| `covariance_apply / quadratic_form` | CovarianceAction、v 或 signed aperture t → Cv 或 variance_or_unknown | 未知项非零影响则完整结果 unknown；PSD 零空间保精确约束 |
| `geometry_covariance_bound` | CovarianceAction、operator error、functional → BoundOrUnavailable | 需要绝对因子 action 或可信谱界；仅 Cv 不足以给 |C| |
| `estimate_flux_information` | h,Z,C、reference/condition → V_F、rank/nullspace | 白化投影或 PSD 约束解；不可辨识不取伪逆 FF 元 |
| `evaluate_scene_variance / control_covariance` | 帧模型、query rows 或 T → V_B/Γ 或 V_ctrl action | 使用真实场景和控制统计；参考 SNR 不能换名输入 |
| `reconstruct_scene_variance` | 同类 Controls、Chart、支持三角网、Query、可选外部 δ/M → value/bound/support/identity | 域外/洞不可用；无M仅conditional预测，不造跨叶covariance |
| `build_sky_controls / fit_relative_sky` | frame provider、comparability、T/H/basis/noise/gauge → controls、SkyFit | PSF/response 可比、实际rank、全部共同模态；图连通不等于平面可辨识 |
| `apply_sky` | y、SkyFit → z、P action | z=y−Eβ，P=I−EKT；同 basis，不独立加 sky 参数方差 |
| `select_samples / solve_weights / coadd` | CoaddRequest、FramesProvider → Selection、WeightSolution、CoaddResult | 资格→可选单步LOO→权；LOO需共同可比较均值、N≥4、外部固定可信Γ；不满足跳过，不能对合法混合色差直接排异；权依目标/Γ/响应约束，unknown降级具名 |
| `build_export / export_signal` | 产品provider、OutputWcs、support policy、quantity request → Q、SignalEstimate | b真实交叠，不收缩；均值/支持积分/外推整像元积分不同身份 |
| `open_product / publish_product` | manifest/root 或完整产品对象图 → provider 或 PublishedProductRef | 统一I/O；必须闭包/hash/版本/单位检查，失败不得发布完整身份 |

`FramesProvider` 提供 `list_frames`、`describe_frame`、`read_leaf_block(frame,range,halo,fields)`、`read_native_dependencies(source_set)`、`open_action(id)`、`query_support(region)`。能力先查询，再读；fields 包括 signal/DQ/support/PSF/response/noise，不强迫只读预累加值。halo 由各算法实际核与作用域请求，资源不足拆分或 spill，不能静默裁掉邻域。provider 不根据请求顺序改变返回科学对象。

可重细化产品另暴露 `open_refinable_geometry(snapshot)` 和 `refine_geometry`。GeometryRevision包含base/new snapshot hash、primitive/算子版本、受影响source/leaf/domain、旧对象到新对象的映射和资源账本；按source细化共同A/Ω可能影响该源所有目标叶，失效范围不得只列本次查询叶。重复同一base请求不覆盖任何现有版本。

工作流预算控制器消费revision，依依赖图重新计算受影响signal/PSF/noise/basis、controls/T/H、sky K/P、Γ、selection/weights及Export，直到所有请求达标或资源耗尽。即使T/H数值不变也必须绑定新输入snapshot；sky分量全局拟合受影响时整分量失效。新下游闭包校验通过后原子发布一个新snapshot；在此之前读者继续持旧闭包，绝不把新R与旧K/权混接。多轮refinement共享剩余work/bytes总预算与取消域，不在每次API调用重置配额。不可恢复或不具refine能力返回unsupported并标具体geometry_solve_unresolved/science_unconverged；不偷偷重读未锁定外部源或冻结中心解冒充重算界。

请求类型的必需字段及首期默认如下。模板可以补默认，但进入 API 的有效请求必须完整展开并记录hash；科学必需项缺失返回 invalid_request，不从信号数值或历史目录猜测。

| 请求类型 | 必需字段与策略 |
|---|---|
| `QuantityRequest` | `id,kind,unit,domain,functional,absolute_tolerance,conditioning`；kind为B/assigned_flux/supported_flux/aperture/covariance。functional为具名叶选择、带单位signed稀疏t或成对t/u；要求相对容差时必须另给正的absolute floor，零附近不能只算相对误差 |
| `GeometryRequest` | source集合、统一order、pixfrac（Normalize默认1，Export固定不收缩）、scope、QuantityRequests或area-only绝对预算、Nmin、算术及max_work/max_bytes/max_depth。Nmin默认0，仅用NL>0判断有保证归一；用户正阈值产生weak_support，不删除合法精确信号 |
| `NoiseRequest` | estimator/condition、根/共享模型、included/omitted、linearization及请求functional；condition区分actual_scene、blank_background、named_reference、fixed_selection；无模型不默认独立/零方差 |
| `PhotometryRequest` | Response/ReferenceSED、空间basis/归一域、gauge、source选择、counts/Gaia noise、flux likelihood/鲁棒policy、收敛与rank阈值、application_id。默认scalar+flux-domain joint Gaussian仅用于声明适用域，非Gaussian必须明确原始计数似然或条件近似 |
| `SkyRequest` | comparability证据、T/H和共享basis、分量gauge、noise、训练/预测支持、solver及rank/残差/扰动预算；默认每可估分量reference-frame系数0，公开reference身份；孤立/无比较组 correction0仅为规范 |
| `CoaddRequest` | FramesProvider、target estimator或显式mixed_observation、权policy、selection、support、science requests。默认selection=DQ_only，noise完整且共同量可比时auto权为BLUE；否则auto为equal并写原因。nonnegative_QP、经验权和任何启发式排异均显式选择；数据依赖Γ标condition |
| `RejectionPolicy` | none/DQ_only/single_max_LOO/explicit_heuristic；LOO含alpha（基线默认.001）、外部固定Γ身份、共同均值可比性与Gaussian条件，最多拒1、只1轮；不满足前置则逐位置skip+reason，不能自动换启发式 |
| `ExportRequest` | 目标WcsModel或中心/尺寸/尺度/方向、quantity（默认supported_mean）、support policy（exact/full_leaf_approx/effective_area_approx）、science budget、输出dtype。全像元外推必须显式 `assume_uniform_uncovered`；不支持的投影/缺必要subleaf形状不静默伪装exact |

rank/迭代阈值是带列尺度及误差解释的数值请求，不是以任意epsilon删除真实模态；固定默认与精度分支见EXECUTION，实施时须独立数值验证。本架构定义可估性及失败语义，不为不同矩阵统一发明一个科学误差阈值。

## 线性 action 的可执行合同

每个 action 暴露 `describe`、`apply`、`transpose_apply`、`dependency_blocks`、`serialize_recipe` 和 error/conditioning。可选 `row_enclosure`、`absolute_factor_apply`、`operator_norm_upper`、`solve/whiten` 逐项声明；没有的能力返回 unsupported，不能由普通 apply 推断。

对于原生 C=BDBᵀ+C_loc+UΛUᵀ，covariance action 为 L C Lᵀ；不同 frame view 共同 shared 根形成 cross action。GLS 要 inverse/whitening 或带约束的增广系统，不能将 C(v) 当白化。转置、共享 latent 坐标、signed aperture 和空域须在 driver 可见。非线性拟合/选择的 influence 是指定条件下线性化，记录线性化点；全过程 MC 才评价数据依赖后的 coverage。

## 实际公共接口、入口和结果规则

[contracts.hpp](../../../include/acsd/contracts.hpp) 是所有字段/签名的唯一可编译声明。前文概念表不再授权实现者另造 Request 类型。C++20静态库是首期进程内 API，CLI与contract驱动均直接调用它。首期没有 C ABI 或异步科学 ABI；未来如有真实进程内外语消费者另作版本设计，不提前建设 opaque-handle/函数表包装。GUI及独立语言 oracle 使用下面JSON文件/子进程边界，已满足同步核心与异步集成。

所有 science函数同步；Context在整个进程中可共享同一Resources+Storage（commands各有取消域），线程安全只读Ref和provider可跨调用共享。运行清单/提交状态只属于本次command。输出Ref可超调用活着，最后引用释放数组/模型；Context外部所有者必须保持Resources和Storage直到全部Ref/Lease释放。函数不保存span，provider请求完成后其输出Lease保字节。Context本身为轻量借用，无旧会话注册。

Result.code=ok 表示调用被完整处理且必要目标达标；partial value在nonconverged/resource_exhausted/precision_exhausted时允许，只能消费标为有效的子域/条件对象。invalid_request/invalid_model/identity_conflict/corrupt_product/io_error/internal/cancelled不提供可发布的value；unavailable/not_identifiable/unsolved可提供仅诊断对象，必须无正式signal。无支持行code=no_support可作为批量value的行状态；空向量不等于失败。单体数学函数遇非有限输入invalid_model；to_fits/from_fits是唯一无Result的纯值运算，前置条件有限且加减1可表示，否则caller边界先拒绝。这四项不创建对象/状态。

### 独立构造与全部操作特定合同

所有未另述内存、线程、误差遵循统一调用合同。请求必填字段全部在声明中，无秘密全局默认；CLI模板只负责展开下述固定默认。每项操作都把依赖ID和消耗计入Result，不能靠结果值非null判断科学成功。

| 操作 | 具体请求、返回和检查 / 对下游的影响 |
|---|---|
| Storage.copy/read | dtype+little-endian bytes+shape+Unit+index创建不可变Array；校验bytes/元素数，计费后复制；read取flatten BlockRequest、保Lease；越界invalid_request、配额不足resource_exhausted。copy是独立驱动构造矩阵/噪声的公开入口 |
| Resources.reserve/consume_work/usage | 同时预留bytes和线程，Reservation.lease释放归还容量；work消耗不可归还，零units可查询剩余；失败不改变配额。usage为进程累计；调用开始/结束差记Result |
| make_action | OBJECTS白名单recipe+有序children/arrays；检索对象不靠全局注册。维度/单位/索引/版本不一致invalid_model/unsupported；成功给不可变action并retain依赖 |
| make_covariance | roots+同一shared根先合并总influence，PSD因子合法才构造C action。显著负谱invalid_covariance；unknown保留；不会把满秩假设加入noise |
| make_frames_provider | descriptions/signals/dq等长逐帧、同index、唯一frame/source；内存provider完整实现FramesProvider，缺某能力unavailable；返回对象保全部数组，完全不需产品目录 |
| make_wcs | HeaderFields typed list而非不可信字符串拼接；Box覆盖完整footprint，context/budget；结果Ref<WcsModel>包括完整准入证书，缺必需参考系/畸变unsupported。符号未证明返回domain，不返回可用于严格几何的Wcs |
| forward/inverse | 点输入必须在domain，SkyICRS单位化误差纳rad预算；inverse返回点+root_box+分列残差，不能只返回近似点。域/切平面/存在性未决在issues.reason精确列projection_unresolved等科学原因 |
| derivative/solid_angle_jacobian | 点J为3×2 row-major，g(Box)为sr/pixel²区间；只支持同Wcs数值模型。区间无有限界返回domain或precision_exhausted，不能靠有限差分补正式界 |
| FootprintProvider全部方法、footprint/measure | provider.model锁同Wcs hash，forward/boundary/derivative enclosure包含全请求域；boundary_curve为Bernstein表示，t必须在参数域；measure原生域Ω，footprint显式f收缩后返回曲边；measure与compute_overlap不擅自重复收缩 |
| compute_overlap | source单个footprint、order、f、预算；返回所有候选及粗未决域。max_bytes由子预算与Context最小者限制；资源失败仍可返回有效面积。未知provider方法unsupported而非自动点采样 |
| calibrate_observation | RawObservation+Calibration，按SYSTEM N1得到integrated native数组及noise根，校准均值和噪声分开；flat无效逐样本DQ，整个输入维度错invalid_model |
| regroup_observation | G域与native_index一致，输出footprint数匹配陪域，M/b可逆；mean成员必须与实际成员一致，缺hole不猜。成功保G与C变换，非法非线性G unsupported |
| solve_astrometry | SolverRequest含搜索域/尺度/候选和假设限额、detect阈值、min_matches、SIP和budget；返回AstrometricSolution，未解返回unsolved及detector诊断，产品无Wcs；至少min_matches>参数秩，输入Wcs旁路由normalize显式选择且记录来源 |
| fit_psf / measure_science_sources | fit_psf输出实际像元积分核，measure用该核和Gaia预测位置输出StarFlux及完整joint_noise/influence。max_outer取budget.max_iterations；源窗口无支持unavailable，非收敛源逐项记；可用源不足整体unavailable，不退回solver_detections |
| integrate_response | Response/SpectralBasis/budget，输出v[d]、absolute_errors[d]及支持域；积分界不掩盖谱分辨率未知bias，缺尾界complete=false。相同响应和basis hash可按算术预算缓存 |
| fit_photometry | measurements按PhotometryRequest.frames显式顺序，CatalogueStar按source_id与测量join，prediction按response一一对应；未观测latent不造数据。rank缺失返回not_identifiable带nullspace；GN失败nonconverged，不应用任意scale |
| apply_scale | solution显式含frame，application_id不重复，原单位与模型匹配；输出新NativeObservation，α空间逐像元；下游PSF/noise/signal只取新revision |
| build_drizzle | overlaps source全匹配NativeObservation、order/f同overlap身份；构造同R的signal/noise/support/response。no_support或未证明正支持不构造归一行。DrizzleRequest.requested_functionals必须反映最终量，area-only只能证明area |
| RefinableGeometryProvider.open/refine、refine_geometry | open锁hash与闭包，refine依GeometryBudget生成immutable revision；base不存在missing_dependency；不能refine unsupported。GeometryRevision.next成功后事务替换依赖，前文失效协议适用 |
| LinearAction全部方法、apply/transpose_apply/compose | DomainUnits.index和单位严格相合；dependency_blocks返回实际源block IDs，transpose=true返回反向依赖；row_enclosure可unsupported；compose outer(inner(x))，所有误差和索引在新action保留 |
| covariance_apply/quadratic_form | 检查C域与输入的同随机量index；full未知时covariance_apply返回unavailable无假Cv，quadratic_form可返回Variance{value=null,state=unknown}并列未知项；完全已知返回Cv/非负variance，微小负仅在证明数值区间含0时截为0并记录 |
| geometry_covariance_bound | nominal R、非负绝对误差E、signed functional t，计算统计§2.1式3b的方差变化半径；没有absolute-factor或谱界则BoundOrUnavailable.bound=null、reason，名义C仍可用。结果不是随机variance增量 |
| estimate_flux_information | h[N]单位1/sr或归一flux模板、background:N×P、C、具名reference；输出flux_variance+rank，h被背景吸收not_identifiable，无有限V；PSD精确约束可给真实V=0 |
| evaluate_scene_variance/control_covariance | 前者实际leaf query、后者C及T；结果分别各leaf Variance和TCTᵀ，不将reference变scene。跨frame Γ由相同根拼接frame域后调用同一C API |
| reconstruct_scene_variance | Controls.kind必须actual_scene，chart/三角/holes和Query，δ/M来自controls显式外部输入；输出查询顺序values/intervals/states。无M条件预测，外域NaN且absent，禁止制造C offdiag |
| FramesProvider全部方法 | list按frame ID排序；describe给能力；read明确中心leaves、halo、fields，只支持声明字段，不能静默漏halo；native_dependencies按请求SourceIdentity顺序；open_action锁hash；query_support只返回请求index范围；refinable缺失unsupported。并发/请求顺序不改变科学结果 |
| build_sky_controls/fit_relative_sky/apply_sky | request指定groups、T/H或默认patch、共同Basis、gauge、noise及solve；build产T,H,A,E；fit产β,K,P/nullspace；apply返回z及同P噪声。不可比不造边、孤立0仅gauge；nullspace影响查询处修正不可用 |
| select_samples/solve_weights/coadd | 共享CoaddRequest，前两者分别产Selection与WeightSolution；coadd按同过程调用，禁止另实现一次。coadd输入provider必须已应用给定sky或由函数构造P，不能重复apply_sky。request mixed=false需common_mean证据；mixed=true保实际核。BLUE缺C明确unavailable，auto才equal |
| build_export/export_signal | 前者prodProvider+目标Q/quantity/support/dtype/budget产Q和estimate，后者仅序列化ExportResult；不重新投影。无noise保unknown，exact support缺模型unavailable；extrapolation没有显式假设invalid_request |
| ProductProvider.graph/read_array、open_product/publish_product | graph不可变闭包，read_array同Storage租约；open锁manifest_hash并验证，publish一次新目录，闭包缺失missing_dependency，写盘失败io_error，无完成身份。不得执行产品携带的任意代码 |
| normalize | NormalizeRequest.frames与outputs等长，frame ID唯一，联合photometry跨frames；supplied_wcs非null走已知头路径且记录supplied，不再盲解。不能标度的帧仅allow_relative_uncalibrated=true才出具名相对L并noise条件。逐帧失败不取消其它可用帧；返回partial及完整分母，失败帧不产HiPS |
| mosaic | 先open每个input，检查同order、Source去重、单位可解释→构建sky→coadd→publish；任一输入损坏默认整体失败，不静默少帧。sky/选择未达请求可留诊断，不把其发布为完整成功 |
| export_image | open→build_export→export_signal；输入Normalize或Mosaic都可，首期只科学真交叠，支持政策显式；输出含sidecar闭包与FITS，单图不能恢复未知信息 |

错误作用域由最小受影响对象决定；坏帧不能把所有帧统计分母抹掉，primitive坏界却必须使相关全部下游失效。错误码枚举为稳定小写字符串，CLI数值退出码另映射；不以异常文本当协议。

## JSON 对象编码与完整合同驱动

[contract.schema.json](../../../schemas/contract.schema.json)定义envelope，类型字段由声明合同逐字段解码：struct对象字段名与声明一致，enum用枚举名小写，optional=null，vector为数组，std::array精确长度，pair为二元素数组，variant用 {"alternative":"类型名","value":...}。Ref<T>可 {"$ref":"局部object名或content hash"} 或 {"type":"T","data":{...}}；读取后生成有identity的immutable对象。用户输入可省Model.identity/assessment，由decoder生成（默认不假定任何科学保证，assessment按操作校验）；产品序列化不允许省。非Model struct内联。

Array输入特例 {"type":"Array","dtype":"f64","shape":[...],"unit":{...},"index_id":"...","values":[...]} 仅合同驱动小例≤65536元素，驱动用Storage.copy，数值必须有限；较大输入用OBJECTS block descriptors和hash。抽象LinearAction wire为 {"type":"LinearAction","recipe":...,"children":[refs],"arrays":[refs]}，CovarianceAction由NoiseModel decode+make_covariance；FramesProvider wire为descriptions/signals/dq后调用make_frames_provider；FootprintProvider的typed data为{wcs:WcsRef}，只构造canonical_tan_sip实现；Refinable/Product provider必须open公开产品。无需独立Agent继承内部生产类。

驱动operation白名单等于头文件free函数名称，重载forward/to_fits用 input字段point.type明确，Storage.copy/read和provider方法用 operation="storage.copy"/"frames.read_leaf_block"等限定名，输入字段对应参数名，Context从resources构造。input_objects是局部名→typed对象，arguments是函数参数名→ref/值，request字段是最后一个请求参数（无请求则{}）；多个Ref参数都在arguments。shape/type/未知字段是硬校验，拒绝对象循环/缺失引用；input_objects按依赖拓扑解码后调用同一API，无科学预计算。输入对象hash由响应返回；driver不得对期望结果调容差。

result.json字段：protocol,major,request_id,operation,input_hashes,resolved_request_hash,build,algorithm_set,arithmetic,result（对应Result<T>），objects（所有输出typed对象），resources。科学nonconverged也写正常JSON并exit0，读者必须看result.code/assessment及每行状态；语法/解码/文件读取失败exit2并写同envelope的result.code。没有实现的operation返回unsupported，不能伪造示例response。stdout为JSONL事件，科学数据只入result文件。

## CLI 配置、事件与 GUI

CLI只有 acsd normalize|mosaic|export --json config.json、--version --json、doctor --json、template <command>；后两项是环境诊断/静态模板，不构造科学数据。三命令配置遵循[config.schema.json](../../../schemas/config.schema.json)：schema="acsd.config/2"，command，inputs，scientific（typed request JSON路径），output，resources，product_profile。配置路径相对config所在目录、产品内部相对产品根，不相对进程cwd。inputs为真实FITS/manifest列表，scientific文件为NormalizeRequest/MosaicRequest/ExportWorkflowRequest字段，但frames.raw、outputs、inputs、input/output由CLI从配置输入/输出唯一注入，若科学文件同时提供冲突字段则invalid_request；CLI只做解码和构建公开对象，不含算法。

Normalize每项input含frame_id、FITS path、可选calibration与response对象路径，catalogue路径另有配置；Mosaic/Export为product manifest路径+必需expected_hash。scientific文件中非I/O字段严格同头合同，无未知键。所有有效配置展开后按schema/类型验证并存闭包。模板默认：f=1、select=dq_only、weights=automatic、topology support=exact、publication full、threads=可用CPU的min(8,quota)、memory=1GiB、work=10000000、spill=8GiB、max_depth=64、initial_bits=64、max_bits=256、solver max_iter=200、rank_relative=1e−12（只是数值分类起点，必须同时记录误差界）。科学 tolerance、response、参考SED/单位、尺度搜索域和历元无通用默认，缺失invalid_request。

退出码：0所有必需目标达标并发布；2配置/输入无效；3能力不支持/必需模型 unavailable；4计算/I/O/完整性失败；5取消；6部分帧失败或数值未达标。优先级取消>完整性/I/O>输入>能力>partial>成功。无论退出码，run_summary列全部输入数量、已发布/unsolved/failed/cancelled数量与逐帧状态，不把仅完成部分当exit0。

JSONL每行UTF8单对象：protocol=acsd.events/2，run_id，seq（从0严格递增），event（run_started/frame_started/block_progress/frame_finished/product_published/issue/run_finished），frame_id/block可null，code，assessment可null，completed,total（同kind单位），message。单写者归并事件序列，stderr仅常规人类日志，输出背压队列有限、合并进度事件但不能丢issue/terminal。每run恰一个run_finished，含exit_code及summary；意外崩溃可能缺terminal，GUI必须标失败，不能推断成功。取消：GUI向stdin写 {"protocol":"acsd.control/2","command":"cancel","run_id":...} 换行，CLI控制线程发token；Ctrl-C/SIGTERM同效果。取消先等安全边界清理，无自动kill超时后冒称完整结束。GUI必要时用户主动终止标aborted。

GUI管理配置、输入分组、运行/取消、事件显示与产品浏览；不解析内部C++对象、不做science运算、不依赖旧ABI。Qt6独立进程启动可与Windows UCRT科学CLI组合；GUI版本探测config/event major，不兼容拒绝启动并给原因。

## 首轮审核修订后的新增操作与排序

声明中的全部free函数参数已有名字。驱动的arguments只含非context、非request参数，最后名为request的参数放envelope.request；没有该参数则request必须{}。原始wire名单以已命名声明为准，禁止按未命名类型猜key。to_fits/forward重载仍由point.type判别。

新增操作：
- detector_footprint(footprints,source_row)：流式解析一条detector足迹，不需要WCS；绑定后才可几何。
- bind_wcs(observation,wcs)：校验完整域并创建immutable revision，原生数组共享；无WCS校准合法。
- bind_observation_model(observation,kernel,response)：将fit结果及响应绑定同观测新revision，校验frame/source index和实际G；不应用α、不改数值，已scale改变response拒绝identity_conflict。
- build_catalogue_index(request)/make_catalogue_provider(index)：公开列式输入构建有界离线quad索引/只读provider，不藏路径；metadata与页面/XP basis见SYSTEM及声明。
- evaluate_psf(psf,request)：单一PSF积分及实际transfer链，输出[output sample,source position]模板。预算失败保有效包围和原因，不能另以整数采样替代。
这些操作是CLI/driver与科学模块实际消费者的接口，不引入插件或注册会话。

PhotometryRequest.frames给唯一明确顺序K，与responses/predictions及measurements长度相同；measurements[k].frame=frames[k]。每帧同Gaia source_id通过CatalogueStar release/calibration共享latent；star数据是各测量源ID的并集且唯一。prediction[k].basis必须是这些星的共同xp_basis，responses[k]同prediction response。PhotoSolution.frames/theta/response顺序沿用请求；theta shape[K,P]。Gaia绝对latent测量固定光度尺度，不额外固定任一帧a=1；空间项均值零是唯一标量/空间分离约束。旧reference_frames字段已删除，不授权编码Agent自创参考帧光度gauge。allow_relative_uncalibrated只在完整拟合不可用时保原生具名相对信号，不伪造绝对尺度。

Normalize调用顺序确定为calibrate→可选物理regroup→solve/supplied WCS→bind_wcs→fit_psf/measure→bind_observation_model→integrate_response/fit_photometry→apply_scale→geometry/build_drizzle→information→publish。一般linear_view不是严格Drizzle输入，Normalize回原生parent投影须在运行配置中显式记录此路径；不得无提示以Gx代替原生。

所有provider methods在驱动用固定字段表：catalogue.query的request=CatalogueQuery，catalogue.query_quads的request=QuadQuery，catalogue.lookup的arguments.source_ids，frames.read_leaf_block的request=FrameReadRequest，footprint.forward_interval的arguments.domain、boundary_interval的arguments.curve/parameter、refinable.open的arguments.snapshot。其余methods在声明里命名；schema decoder不得允许任意私有方法。

Unit增加linear_power：信号L为1，variance为2，纯面积/功能系数为0；scale锁同一个L身份，sr/pixel/nm指数按代数传播。CovarianceAction作为数值C映射：输入functional单位U，输出单位(signal_unit²)*U；quadratic_form单位(signal_unit²)*U²。C的index恒为signal index；输入系数可为无量纲或带sr的孔径，不能误要求functional具有signal单位。默认DomainUnits记录C对无量纲输入时的variance输出；带单位functional由调用显式乘积校验。

### 可机检的科学输入与CLI科学文件

[science-input.schema.json](../../../schemas/science-input.schema.json)由唯一声明导出每种struct实际字段、数组维数类型、enum、Ref及全部operation参数；contract.schema直接引用它。本机验证器注册本地schema，不访问占位域名acsd.invalid。运行tools/generate_contract_schema.py再check_design --同步检查；生成物不能反向修改科学。Schema只检形状；单位/index/支持/PSD/原语能力等语义仍须在集中边界校验，合法JSON不等于科学有效。

CLI的scientific文件按[cli-science.schema.json](../../../schemas/cli-science.schema.json)，形状为{command,request,frame_options?}，command必须等config.command。request是对应workflow类型去掉I/O注入字段：Normalize去frames/outputs，Mosaic去inputs/output，Export去input/output。Normalize.frame_options是{frame,supplied_wcs,regroup}列表（可省为空），按frame ID join config.inputs，不得有重复/未知frame；缺项默认null/null。frames的raw/Calibration/Response由input文件构建，calibration缺省是显式无母版/噪声unknown，不从目录搜索。Mosaic/Export input从manifest锁hash构造。Normalize.outputs为config.output下按frame ID的SHA256十六进制目录，避免路径注入；publication默认FP64/tile512/durable，与展开请求一起记录。若用户要求不同dtype/tilesize/durability，在科学文件之外用config.publication显式覆盖并展开，Export与projection.output_dtype必须一致，否则invalid_request。

ReferenceInformationRequest的noise可空只有actual_scene分支；named_reference/blank_background缺noise返回precision unavailable，不能填零，也不取消合法signal。build_supported_triangulation(controls,chart,support)输出可恢复的有限观测三角网，失败/资源未完成不提供可用网；全部插值只在其支持内。输入物理组件没有给出时，接口不授权Agent推导未知shot variance。

C++参数名是唯一wire key；variant标签严格用声明的std::string/double/std::int64_t。JSON数字可为有限decimal；规范持久化f64位编码另由产品decoder读取，调用输入也允许{f64:16hex}保精度，并先拒绝其NaN/Inf位模式。所有Array.values按dtype范围与shape检查；Array小例index_id别名必须解析后再校验。输入对象DAG不按JSON key顺序绑定。

同一schema可以实际校验[U01矩阵请求](../../../examples/u01.request.json)：包含完整索引、CSR R、整数F、共享根及C对象、signed aperture和运行资源，无伪造identity/hash。期望方差4.328125L²/sr²；输入、调用及输出真值模型在examples说明，未宣称驱动已运行。

### JSON整数、初始化与验证顺序

u64必须由保留64位整数的parser读取；C++/Python可直接用十进制JSON整数，JavaScript客户端必须采用无损整数解析，不能先JSON.parse成double再声称相同输入。大型index/ids在u64二进制块中传递，GUI事件与产品浏览不要求它自行重算NESTED。外部f64位模式先检查有限，decimal容差取不宽于请求的保守解析值并保原lexeme身份。

CLI初始化一个CpuResources实现、一个Storage实现和取消源，按Limits创建Context；独立C++调用者可实现这三份公开接口或复用services实现。实现中Resources原子准入、Storage内存/磁盘backend及immutable arrays均遵循EXECUTION，无注册会话、无科学全局配置。所有大数组以Storage.copy/读取构造；发布前对象可由caller独占builder组装，consumer集中校验，publish后Ref<const>不可写。

校验顺序固定：协议/字段/类型→尺寸与溢出、单位/index/identity→科学域/能力与资源准入→算法→逐泛函误差/状态→序列化量化→发布闭包。无效模型在消费边界一次拒绝，内部已验证块不逐元素重复防御或记录日志。未知能力只限制对应操作，不能把合法负值/零删掉。
