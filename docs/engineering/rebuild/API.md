# 标准科学 API 与驱动协议

本合同定义目标接口；已有可编译声明与schema，科学函数及驱动程序尚未实现。科学类型遵循[统一对象](OBJECTS.md)，所有 action 的域、单位、原点及随机变量身份必须匹配。独立 Agent 只需本合同和五份科学规范即可构造输入，不需要理解旧 CLI、adapter、scheduler 或 session。

## 唯一签名与调用约定

[contracts.hpp](../../../include/acsd/contracts.hpp) 唯一拥有公共字段、函数签名、参数名和枚举值；[science-input.schema.json](../../../schemas/science-input.schema.json)仅由声明生成。本篇拥有操作语义、错误、所有权和进程协议，不另定义伪代码签名或 Request 类型。所有未在声明出现的操作均不构成实现要求。

Context 实际字段仅 Resources&、const Cancellation&、Storage&。所有科学函数同步，span/Context 借用至返回，Ref<const T>拥有共享依赖；输出数组由Storage租约保字节，独占scratch按调用申请。异步GUI在子进程层集成，不增加异步科学API或C ABI。只读模型可并发，每次调用的scratch/迭代状态独占。取消在有界工作单元边界检查，最大工作及位长仍计入统一预算。

Result<T> 实际字段为 code、value、assessment、usage、dependencies；issues在assessment.issues，不在Result顶层。值存在不表示所有泛函成功，面积有效而科学量未收敛可以同时成立；failure不返回可误用的任意sky/权/方差。Issue的作用域、原因和重试条件遵声明，算法不exit，不以异常文本作协议。

## 策略与请求字段

下表只解释声明中现有字段，大小写与wire枚举完全一致。完整字段由声明及生成schema提供；模板先展开完整请求再计算hash，缺科学必需项返回invalid_request。

| 正本字段 | 唯一取值/语义 |
|---|---|
| QuantityRequest.kind | brightness、assigned_flux、supported_flux、aperture、covariance、area；index及left/right明确泛函，tolerance和condition随对象传递 |
| GeometryBudget | requested_functionals、per_leaf_absolute_area、area_absolute_sr、normalizer_min_sr、max_work/max_bytes/max_depth、arithmetic；DrizzleRequest通过budget嵌入它 |
| PhotometryRequest | responses/reference/basis/frames/application_id/solve/gradient_tolerance/step_tolerance/max_outer_iterations/likelihood；声明模型为joint_flux_gaussian。Gaia latent锚定尺度，空间基均值零；不另加参考帧gauge。SolvePolicy.huber控制显式Huber IRLS |
| RejectionPolicy.kind | dq_only、single_max_loo；dq_only仍执行DQ资格，表示无统计排异。single_max_loo使用alpha、external_fixed_covariance、common_mean_proof，最多拒一个且仅一轮；前置不满足逐位置skip+reason |
| WeightPolicy | automatic、equal、blue、nonnegative_qp；automatic在共同量可比且C完整时选blue，否则equal并记原因；显式blue缺能力则unavailable。mixed_observation只允许equal或显式nonnegative_qp，automatic选equal |
| SupportMap::Kind / ExportRequest.support_policy | primitives、dyadic_cells提供实际支持形状的严格交叠；full_leaf_approx、effective_area_approx是具名近似。严格请求缺所选支持能力则unavailable，不把近似冒充精确 |
| ExportRequest.quantity | supported_mean、supported_flux、extrapolated_full_flux；最后一项必须assume_uniform_uncovered=true。target是OutputWcs，dtype和budget均显式 |

unknown噪声不是零方差。首期不提供自定义经验权或启发式排异策略，未来若扩展必须先修唯一声明/schema及科学语义；不能把文中数学讨论当另一个枚举入口。rank阈值是带尺度与误差解释的请求，默认及停止分支见EXECUTION，不用任意epsilon删除真实模态。

## 线性 action 的能力合同

LinearAction的实际方法仅apply、transpose_apply、dependency_blocks、serialize_recipe、row_enclosure；域和单位直接读spaces，误差从recipe.error_record强引用取得。row_enclosure缺能力返回unsupported。CovarianceAction为声明中的数据模型，absolute_factor_bound、operator_norm_upper、omitted_action_norm是可空能力字段，不是另设的成员函数。普通Cv不能当白化或绝对系数action；solve/白化由SolveSpec、已给因子及numerics中的唯一求解实现完成，失败按已声明Code返回。

FramesProvider的实际方法、参数及RefinableGeometryProvider/GeometryRevision字段只由声明拥有，特定合同见下表与OBJECTS。几何revision使同源全部叶、sky分量、权/选择和Export依赖失效，workflows按依赖重新计算；新闭包完成前旧闭包保持可读，不混R/K/权版本。细化共享本进程剩余预算，不能每次API重置配额。缺refine能力返回unsupported及科学未达标状态，不偷读未锁定源。

## 实际公共接口、入口和结果规则

[contracts.hpp](../../../include/acsd/contracts.hpp) 是所有字段/签名的唯一可编译声明。本篇不授权实现者另造 Request 类型。C++20静态库是首期进程内 API，CLI与contract驱动均直接调用它。首期没有 C ABI 或异步科学 ABI；未来如有真实进程内外语消费者另作版本设计，不提前建设 opaque-handle/函数表包装。GUI及独立语言 oracle 使用下面JSON文件/子进程边界，已满足同步核心与异步集成。

所有 science函数同步；Context在整个进程中可共享同一Resources+Storage（commands各有取消域），线程安全只读Ref和provider可跨调用共享。运行清单/提交状态只属于本次command。输出Ref可超调用活着，最后引用释放数组/模型；Context外部所有者必须保持Resources和Storage直到全部Ref/Lease释放。函数不保存span，provider请求完成后其输出Lease保字节。Context本身为轻量借用，无旧会话注册。

Result.code=ok 表示调用被完整处理且必要目标达标；partial value在nonconverged/resource_exhausted/precision_exhausted时允许，只能消费标为有效的子域/条件对象。invalid_request/invalid_model/identity_conflict/corrupt_product/io_error/internal/cancelled不提供可发布的value；unavailable/not_identifiable/unsolved可提供仅诊断对象，必须无正式signal。无支持行code=no_support可作为批量value的行状态；空向量不等于失败。单体数学函数遇非有限输入invalid_model；to_fits/from_fits_center/from_fits_edge是唯一无Result的纯值运算，前置条件有限且加减1可表示，否则caller边界先拒绝。这四项不创建对象/状态。

### 独立构造与全部操作特定合同

所有未另述内存、线程、误差遵循统一调用合同。请求必填字段全部在声明中，无秘密全局默认；CLI模板只负责展开下述固定默认。每项操作都把依赖ID和消耗计入Result，不能靠结果值非null判断科学成功。

| 操作 | 具体请求、返回和检查 / 对下游的影响 |
|---|---|
| Storage.copy/read | dtype+little-endian bytes+shape+Unit+index创建不可变Array；校验bytes/元素数，计费后复制；read取flatten BlockRequest、保Lease；越界invalid_request、配额不足resource_exhausted。copy是独立驱动构造矩阵/噪声的公开入口 |
| Resources.reserve/consume_work/usage | 同时预留bytes和线程，Reservation.lease释放归还容量；work消耗不可归还，零units可查询剩余；失败不改变配额。usage为进程累计；调用开始/结束差记Result |
| make_action | OBJECTS白名单recipe+有序children/arrays；检索对象不靠全局注册。维度/单位/索引/版本不一致invalid_model/unsupported；成功给不可变action并retain依赖 |
| make_covariance | NoiseModel内roots+同一shared根先合并总influence，PSD因子合法才构造C action。显著负谱invalid_covariance；unknown保留；不会把满秩假设加入noise |
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
| build_drizzle | overlaps source全匹配NativeObservation、order/f同overlap身份；构造同R的signal/noise/support/response。no_support或未证明正支持不构造归一行。DrizzleRequest.budget.requested_functionals必须反映最终量，area-only只能证明area |
| RefinableGeometryProvider.open/refine、refine_geometry | open锁hash与闭包，refine依GeometryBudget生成immutable revision；base不存在missing_dependency；不能refine unsupported。GeometryRevision.next成功后事务替换依赖，前文失效协议适用 |
| LinearAction全部方法、apply/transpose_apply/compose | DomainUnits.domain/codomain和单位严格相合；dependency_blocks返回实际源block IDs，transpose=true返回反向依赖；row_enclosure可unsupported；compose outer(inner(x))，所有误差和索引在新action保留 |
| covariance_apply/quadratic_form | 检查C域与输入的同随机量index；full未知时covariance_apply返回unavailable无假Cv，quadratic_form可返回Variance{value=null,state=unknown}并列未知项；完全已知返回Cv/非负variance，微小负仅在证明数值区间含0时截为0并记录 |
| geometry_covariance_bound | nominal R、非负绝对误差E、signed functional t，计算统计§2.1式3b的方差变化半径；没有absolute-factor或谱界则BoundOrUnavailable.bound=null、reason，名义C仍可用。结果不是随机variance增量 |
| estimate_flux_information | h[N]单位1/sr或归一flux模板、background:N×P、C、具名reference；输出flux_variance+rank，h被背景吸收not_identifiable，无有限V；PSD精确约束可给真实V=0 |
| evaluate_scene_variance/control_covariance | 前者实际leaf query、后者C及T；结果分别各leaf Variance和TCTᵀ，不将reference变scene。跨frame Γ由相同根拼接frame域后调用同一C API |
| reconstruct_scene_variance | Controls.kind必须actual_scene，chart/三角/holes和Query，δ/M来自controls显式外部输入；输出查询顺序values/intervals/states。无M条件预测，外域NaN且absent，禁止制造C offdiag |
| FramesProvider全部方法 | list_frames按frame ID排序；describe_frame给能力；read_leaf_block的FrameReadRequest明确中心leaves、halo、fields，只支持声明字段，不能静默漏halo；read_native_dependencies按请求SourceIdentity顺序；open_action锁hash；query_support只返回请求index范围；refinable缺失unsupported。并发/请求顺序不改变科学结果 |
| build_sky_controls/fit_relative_sky/apply_sky | request指定groups、T/H或默认patch、共同Basis、gauge、noise及solve；build产T,H,A,E；fit产β,K,P/nullspace；apply返回z及同P噪声。不可比不造边、孤立0仅gauge；nullspace影响查询处修正不可用 |
| select_samples/solve_weights/coadd | 共享CoaddRequest，前两者分别产Selection与WeightSolution；coadd按同过程调用，禁止另实现一次。coadd输入provider必须已应用给定sky或由函数构造P，不能重复apply_sky。request mixed=false需common_mean证据；mixed=true保实际核。BLUE缺C明确unavailable，auto才equal |
| build_export/export_signal | 前者prodProvider+目标Q/quantity/support/dtype/budget产Q和estimate，后者仅序列化ExportResult；不重新投影。无noise保unknown，primitives/dyadic_cells严格支持缺模型unavailable；extrapolation没有显式假设invalid_request |
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

Normalize每项input含frame_id、FITS path、可选calibration与response对象路径，catalogue路径另有配置；Mosaic/Export为product manifest路径+必需expected_hash。scientific文件中非I/O字段严格同头合同，无未知键。所有有效配置展开后按schema/类型验证并存闭包。模板默认：f=1、select=dq_only、weights=automatic、support_policy=dyadic_cells、publication full、threads=可用CPU的min(8,quota)、memory=1GiB、work=10000000、spill=8GiB、max_depth=64、initial_bits=64、max_bits=256、solver max_iter=200、rank_relative=1e−12（只是数值分类起点，必须同时记录误差界）。科学 tolerance、response、参考SED/单位、尺度搜索域和历元无通用默认，缺失invalid_request。

退出码：0所有必需目标达标并发布；2配置/输入无效；3能力不支持/必需模型 unavailable；4计算/I/O/完整性失败；5取消；6部分帧失败或数值未达标。优先级取消>完整性/I/O>输入>能力>partial>成功。无论退出码，run_summary列全部输入数量、已发布/unsolved/failed/cancelled数量与逐帧状态，不把仅完成部分当exit0。

JSONL每行UTF8单对象：protocol=acsd.events/2，run_id，seq（从0严格递增），event（run_started/frame_started/block_progress/frame_finished/product_published/issue/run_finished），frame_id/block可null，code，assessment可null，completed,total（同kind单位），message。单写者归并事件序列，stderr仅常规人类日志，输出背压队列有限、合并进度事件但不能丢issue/terminal。每run恰一个run_finished，含exit_code及summary；意外崩溃可能缺terminal，GUI必须标失败，不能推断成功。取消：GUI向stdin写 {"protocol":"acsd.control/2","command":"cancel","run_id":...} 换行，CLI控制线程发token；Ctrl-C/SIGTERM同效果。取消先等安全边界清理，无自动kill超时后冒称完整结束。GUI必要时用户主动终止标aborted。

GUI管理配置、输入分组、运行/取消、事件显示与产品浏览；不解析内部C++对象、不做science运算、不依赖旧ABI。Qt6独立进程启动可与Windows UCRT科学CLI组合；GUI版本探测config/event major，不兼容拒绝启动并给原因。

## 首轮审核修订后的新增操作与排序

声明中的全部free函数参数已有名字。驱动的arguments只含非context、非request参数，最后名为request的参数放envelope.request；没有该参数则request必须{}。原始wire名单以已命名声明为准，禁止按未命名类型猜key。to_fits/forward重载仍由point.type判别。

新增操作：
- detector_footprint(footprints,source_row)：流式解析一条detector足迹，不需要WCS；绑定后才可几何。
- bind_wcs(observation,wcs,solution)：校验完整域并创建immutable revision，原生数组共享；无WCS校准合法。
- bind_observation_model(observation,kernel,response)：将fit结果及响应绑定同观测新revision，校验frame/source index和实际G；不应用α、不改数值，已scale改变response拒绝identity_conflict。
- build_catalogue_index(request)/make_catalogue_provider(index)：公开列式输入构建有界离线quad索引/只读provider，不藏路径；metadata与页面/XP basis见SYSTEM及声明。
- evaluate_psf(psf,request)：单一PSF积分及实际transfer链，输出PsfEvaluation，含[output sample,source position]模板和实际数值包围。预算失败保有效包围和原因，不能另以整数采样替代。
这些操作是CLI/driver与科学模块实际消费者的接口，不引入插件或注册会话。

PhotometryRequest.frames给唯一明确顺序K，与responses/predictions及measurements长度相同；measurements[k].frame=frames[k]。每帧同Gaia source_id通过CatalogueStar release/calibration共享latent；star数据是各测量源ID的并集且唯一。prediction[k].basis必须是这些星的共同xp_basis，responses[k]同prediction response。PhotoSolution.frames/theta/response顺序沿用请求；theta shape[K,P]。Gaia绝对latent测量固定光度尺度，不额外固定任一帧a=1；空间项均值零是唯一标量/空间分离约束。旧reference_frames字段已删除，不授权编码Agent自创参考帧光度gauge。allow_relative_uncalibrated只在完整拟合不可用时保原生具名相对信号，不伪造绝对尺度。

Normalize调用顺序确定为calibrate→可选物理regroup→solve/supplied WCS→bind_wcs→fit_psf/measure→bind_observation_model→integrate_response/fit_photometry→apply_scale→geometry/build_drizzle→information→publish。一般linear_view不是严格Drizzle输入，Normalize回原生parent投影须在运行配置中显式记录此路径；不得无提示以Gx代替原生。

所有provider methods在驱动用固定字段表：catalogue.query的request=CatalogueQuery，catalogue.query_quads的request=QuadQuery，catalogue.lookup的arguments.source_ids，frames.read_leaf_block的request=FrameReadRequest，footprint.forward_interval的arguments.domain、boundary_interval的arguments.curve/parameter、refinable.open的arguments.snapshot。其余methods在声明里命名；schema decoder不得允许任意私有方法。

Unit增加linear_power：信号L为1，variance为2，纯面积/功能系数为0；scale锁同一个L身份，sr/pixel/nm指数按代数传播。CovarianceAction作为数值C映射：输入functional单位U，输出单位(signal_unit²)*U；quadratic_form单位(signal_unit²)*U²。C的index恒为signal index；输入系数可为无量纲或带sr的孔径，不能误要求functional具有signal单位。默认DomainUnits记录C对无量纲输入时的variance输出；带单位functional由调用显式乘积校验。

### 可机检的科学输入与CLI科学文件

[science-input.schema.json](../../../schemas/science-input.schema.json)由唯一声明导出每种struct实际字段、数组维数类型、enum、Ref及全部operation参数；contract.schema直接引用它。本机验证器注册本地schema，不访问占位域名acsd.invalid。运行tools/generate_contract_schema.py，再运行check_design.py同步检查；生成物不能反向修改科学。Schema只检形状；单位/index/支持/PSD/原语能力等语义仍须在集中边界校验，合法JSON不等于科学有效。

CLI的scientific文件按[cli-science.schema.json](../../../schemas/cli-science.schema.json)，形状为{command,request,frame_options?}，command必须等config.command。request是对应workflow类型去掉I/O注入字段：Normalize去frames/outputs，Mosaic去inputs/output，Export去input/output。Normalize.frame_options是{frame,supplied_wcs,regroup}列表（可省为空），按frame ID join config.inputs，不得有重复/未知frame；缺项默认null/null。frames的raw/Calibration/Response由input文件构建，calibration缺省是显式无母版/噪声unknown，不从目录搜索。Mosaic/Export input从manifest锁hash构造。Normalize.outputs为config.output下按frame ID的SHA256十六进制目录，避免路径注入；publication默认FP64/tile512/durable，与展开请求一起记录。若用户要求不同dtype/tilesize/durability，在科学文件之外用config.publication显式覆盖并展开，Export与projection.output_dtype必须一致，否则invalid_request。

ReferenceInformationRequest的native_noise可空只有actual_scene分支；named_reference/blank_background缺noise返回precision unavailable，不能填零，也不取消合法signal。build_supported_triangulation(controls,chart,support)输出可恢复的有限观测三角网，失败/资源未完成不提供可用网；全部插值只在其支持内。输入物理组件没有给出时，接口不授权Agent推导未知shot variance。

C++参数名是唯一wire key；variant标签严格用声明的std::string/double/std::int64_t。JSON数字可为有限decimal；规范持久化f64位编码另由产品decoder读取，调用输入也允许{f64:16hex}保精度，并先拒绝其NaN/Inf位模式。所有Array.values按dtype范围与shape检查；Array小例index_id别名必须解析后再校验。输入对象DAG不按JSON key顺序绑定。

同一schema可以实际校验[U01矩阵请求](../../../examples/u01.request.json)：包含完整索引、CSR R、整数F、共享根及C对象、signed aperture和运行资源，无伪造identity/hash。期望方差4.328125L²/sr²；输入、调用及输出真值模型在examples说明，未宣称驱动已运行。

### JSON整数、初始化与验证顺序

u64必须由保留64位整数的parser读取；C++/Python可直接用十进制JSON整数，JavaScript客户端必须采用无损整数解析，不能先JSON.parse成double再声称相同输入。大型index/ids在u64二进制块中传递，GUI事件与产品浏览不要求它自行重算NESTED。外部f64位模式先检查有限，decimal容差取不宽于请求的保守解析值并保原lexeme身份。

CLI初始化一个CpuResources实现、一个Storage实现和取消源，按Limits创建Context；独立C++调用者可实现这三份公开接口或复用src/resources与src/storage实现。实现中Resources原子准入、Storage内存/磁盘backend及immutable arrays均遵循EXECUTION，无注册会话、无科学全局配置。所有大数组以Storage.copy/读取构造；发布前对象可由caller独占builder组装，consumer集中校验，publish后Ref<const>不可写。

校验顺序固定：协议/字段/类型→尺寸与溢出、单位/index/identity→科学域/能力与资源准入→算法→逐泛函误差/状态→序列化量化→发布闭包。无效模型在消费边界一次拒绝，内部已验证块不逐元素重复防御或记录日志。未知能力只限制对应操作，不能把合法负值/零删掉。

## 独立驱动块入口、返回与存储操作

contract信封增加可选input_root（默认request文件所在目录）和blocks[]（默认空），blocks逐项为BlockDescriptor。descriptor.path相对input_root解析，显式root之外不相对进程cwd猜文件。输入可用id局部别名，decoder解析index_id后计算真正descriptor hash，再将Array.block_ids别名替换；已有sha256身份必须匹配。Array的typed data形式含block_ids，decoder调用Storage.open，不要求先建立正式product。小型Array.values也支持dtype=dyadic的Dyadic对象，不能用整数float替代高精度端点。

storage.copy参数为dtype,bytes（u8序列）,shape,unit,index，按公开C++参数名；storage.read为array,range；storage.open为root,description,blocks；product.read_array为provider,array,range。Context/Storage注入，不从输入装载对象代码。对应所有操作已进入唯一生成schema；其request必须{}。其它资源对象方法只供同步C++调用，不作为用户可扩配脚本。

Result<T>在response.result中按字段序列化：by-value struct为普通字段对象，vector为数组，optional为空则null，Ref<T>为{$ref:hash}；response.objects[hash]给type/data（实际闭包），response.blocks[]给所有输出descriptor，output_root给其路径根。to-fits.expected-model的data对应result.value；make_wcs等Ref返回则须从objects取typed对象。新请求必须将所需typed对象及其依赖加入input_objects，将blocks带入并input_root指前次输出根；单独抄一个hash不能跨进程查隐藏会话。response文件路径由--response显式指定，模型块在output_root，二者不混作path根。

U01已补四个辅助Index（CSR offsets/nnz各一个），所有Array.index_id均可在公开输入对象图解析。examples/external-array.request.json使用真实24byte little-endian块、SHA256和相同R，期望[3,7]；不需旧产品目录。形状检查之外的别名/块覆盖/hash校验先于任何科学算法。

measure_science_sources 的结果除名义 StarFlux 与随机 joint_noise，必须含 OBJECTS 定义的通量数值界、强引用 error 和 observation。fit_photometry 的公开 measurements 参数即为此账本入口，独立进程无需另查隐含 Native；缺证书传播 conditional 而不补零。evaluate_psf 的 output_rows 提供窗口/分块查询，行号、返回子索引与错误规则见 SYSTEM；同步调用和预算/借用规则同其余科学 API。
