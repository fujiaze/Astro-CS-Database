# 标准科学 API 与驱动协议

本合同定义现有公开接口与本次新增工程目标；远端已有部分科学函数与驱动实现，实际覆盖见阶段记录。本次仅改文档，新增目标尚未接入。科学类型遵循[统一对象](OBJECTS.md)，所有 action 的域、单位、原点及随机变量身份必须匹配。独立 Agent 只需本合同和五份科学规范即可构造输入，不需要理解旧 CLI、adapter、scheduler 或 session。

## 本次工程目标与实施清单

本次按负责人要求**只修改文档**；当前头文件、schema、CMake和实现保持原样。本文“现有”部分以实际contracts.hpp为准；新增基建接口字段/签名已在[FOUNDATION_API](standards/FOUNDATION_API.md)定稿，本轮尚未写入源码；它们是下一轮必须落实的目标合同，不是当前已经可调用的声明或wire字段。实施者必须先在唯一头文件落实签名/字段，再同步生成schema、集中decoder和消费者，不能靠私有helper假装公开合同已完成。

| 实施项 | 当前公开能力 | 必须落实的目标与顺序 |
|---|---|---|
| E00 虚拟计算池与组合 | 当前只有分离的服务接口，未按新骨架统一 | 先声明pool/Task/Pipe/typed模块合同与Host装配；唯一CPU/内存账本/I/O lane；算法用模块函数组合，不新增第二调度器 |
| E01 资源归因 | Resources.reserve(bytes,threads)/consume_work/usage，Reservation只有lease/bytes/threads | 先引入独立调用子域及快照，再引入host/pinned/device/spill联合准入和可复用容量arena；Usage补充分域高水位/可空CPU时间，进程RSS另列 |
| E02 数组构建 | Storage.copy/read/open | 增加begin、顺序append、seal、abort及read_reserved；预留转成实际租约，循环释放回原arena；不以整数组copy实现有界输出 |
| E03 设备承接 | ArrayView是host span，无设备承接接口 | 冻结ArrayBacking职责与Storage.adopt/backing：设备规范块/hash登记成ArrayRef、强保设备/事件、下一核复用、公开host读取按预算物化 |
| E04 诊断 | 无DEV_MODE、诊断策略/sink/静态probe合同 | 先构建主开关与OFF零求值，再加静态登记/只读观测入口和独立策略，最后有界copy/统一I/O/manifest；不修改科学目标 |
| E05 执行后端 | Context只借用Resources/Cancellation/Storage | 增加可选执行与诊断设施的装配边界；空执行设施走唯一CPU实现；不把默认科学配置藏进Context |
| E06 配置/协议 | 现有config/contract不接受本轮新增字段 | 实施时新增独立execution选择及diagnostics策略路径、严格schema和能力报告；此前不应把本文JSON目标例直接交给现有CLI |
| E07 统一封存 | 当前Storage尚未满足完整规范封存 | 只实现一套canonical_array_v1当前格式；不匹配旧产品拒绝并重建，不写双reader或转换器 |

先落实[SYSTEM](SYSTEM.md)的Host/registry标准边界，再按E01→E02→E03落实资源/存储承接；E04可在CPU边界实现，E05/E06随后接线。每项都须完成声明→schema/decoder→实现→公开API/独立driver→持久化或诊断协议→负例；缺任一环节记未完成。CPU/GPU不同执行实现共享数学定义和误差合同，不另起一份科学算法。以下章节明确目标行为；未实施的旧实现按其实际能力报告，不以文档状态替代运行证据。

## 唯一签名与调用约定

[contracts.hpp](../../../include/acsd/contracts.hpp) 唯一拥有公共字段、函数签名、参数名和枚举值；[science-input.schema.json](../../../schemas/science-input.schema.json)仅由声明生成。本篇拥有操作语义、错误、所有权和进程协议，不另定义伪代码签名或 Request 类型。现有可调用操作仅以声明为准；本次实施清单列明的新增工程能力是后续实施要求，必须先同步唯一声明/schema再调用，不可当作已经可用的操作。

当前Context仅借用Resources、Cancellation、Storage；E05目标允许另借用诊断/执行设施，但不承载科学默认。目标中空执行设施使用同一CPU实现、空诊断设施不采集，当前头文件尚无这两个扩展。现有科学函数同步；目标顶层invoke同步、内部Task异步完成，span/Context借用至少覆盖真实任务完成，Ref<const T>拥有共享依赖；输出数组由Storage租约保字节，独占scratch按调用申请。异步GUI在子进程层集成，不增加异步科学API或C ABI。只读模型可并发，每次调用的scratch/迭代状态独占。取消在有界工作单元边界检查，最大工作及位长仍计入统一预算。

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

unknown噪声不是零方差。首期不提供自定义经验权或启发式排异策略，未来若扩展必须先修唯一声明/schema及科学语义；不能把文中数学讨论当另一个枚举入口。rank阈值是带尺度与误差解释的请求，默认及停止分支见[NUMERICS](standards/NUMERICS.md)，不用任意epsilon删除真实模态。

## 线性 action 的能力合同

LinearAction的实际方法仅apply、transpose_apply、dependency_blocks、serialize_recipe、row_enclosure；域和单位直接读spaces，误差从recipe.error_record强引用取得。row_enclosure缺能力返回unsupported。CovarianceAction为声明中的数据模型，absolute_factor_bound、operator_norm_upper、omitted_action_norm是可空能力字段，不是另设的成员函数。普通Cv不能当白化或绝对系数action；solve/白化由SolveSpec、已给因子及numerics中的唯一求解实现完成，失败按已声明Code返回。

FramesProvider的实际方法、参数及RefinableGeometryProvider/GeometryRevision字段只由声明拥有，特定合同见下表与OBJECTS。几何revision使同源全部叶、sky分量、权/选择和Export依赖失效，workflows按依赖重新计算；新闭包完成前旧闭包保持可读，不混R/K/权版本。细化共享本进程剩余预算，不能每次API重置配额。缺refine能力返回unsupported及科学未达标状态，不偷读未锁定源。

## `FootprintProvider` 是抽象接口，公开声明必须给出唯一的构造入口，否则
`footprint` / `measure` / `compute_overlap` 对外部消费者不可达（合同驱动、CLI 与独立验证
都无法自行构造 provider）。唯一构造入口是

```text
make_footprint_provider(context, wcs) -> Ref<FootprintProvider>
```

它由 WCS 规范模型建立检测域的包含/测度能力，返回的 provider 持有 wcs 的身份与误差来源；
不同 WCS 不得共用同一 provider 实例（provider 身份绑定 wcs 的 content hash）。
provider 本身不新建面积实现：认证面积账本由 geometry 模块的单一实现给出。

实际公共接口、入口和结果规则

[contracts.hpp](../../../include/acsd/contracts.hpp) 是所有字段/签名的唯一可编译声明。本篇不授权实现者另造 Request 类型。C++20静态库是首期进程内 API，CLI与contract驱动均直接调用它。首期没有 C ABI 或异步科学 ABI；未来如有真实进程内外语消费者另作版本设计，不提前建设 opaque-handle/函数表包装。GUI及独立语言 oracle 使用下面JSON文件/子进程边界，已满足同步核心与异步集成。

目标process调用边界、类型映射与状态机遵SYSTEM；底层science函数同步；Context在整个进程中可共享同一Resources+Storage（commands各有取消域），线程安全只读Ref和provider可跨调用共享。运行清单/提交状态只属于本次command。输出Ref可超调用活着，最后引用释放数组/模型；Context外部所有者必须保持Resources和Storage直到全部Ref/Lease释放。函数不保存span，provider请求完成后其输出Lease保字节。Context本身为轻量借用，无可变科学会话注册。

Result.code=ok 表示调用被完整处理且必要目标达标；partial value在nonconverged/resource_exhausted/precision_exhausted时允许，只能消费标为有效的子域/条件对象。invalid_request/invalid_model/identity_conflict/corrupt_product/io_error/internal/cancelled不提供可发布的value；unavailable/not_identifiable/unsolved可提供仅诊断对象，必须无正式signal。无支持行code=no_support可作为批量value的行状态；空向量不等于失败。单体数学函数遇非有限输入invalid_model；to_fits/from_fits_center/from_fits_edge是唯一无Result的纯值运算，前置条件有限且加减1可表示，否则caller边界先拒绝。这四项不创建对象/状态。

### 资源与Storage目标：资源子域与数组构建

下述child/ResourceRequest/ReservationState/ArrayBuilder/ArrayBuildRequest等均为E01/E02目标角色，当前头文件尚未声明，精确目标签名见FOUNDATION_API，实施时同步唯一公共头。调用者先由进程Resources创建command子域，再为每次顶层API创建唯一operation子域，得到Context；嵌套科学函数复用该域，Result.usage是该域快照，父结果含子域但统计汇总不得重复相加。拒绝重复scope ID、超父限额或试图重置work；输出租约保计费域活着。Usage峰值逐次占用变化更新，CPU时间不可得为null，非零工作不能由空值填0。进程RSS只能由process_snapshot报告，不按调用/模块猜归因。

| 操作 | 完整契约 |
|---|---|
| Resources.child | scope为父域内唯一ID，Limits逐项不超过父域；这是额度上限而非提前占有，祖先实际消耗仍受同一原子账本约束；返回共享可变服务，不能作为科学对象持久化 |
| Storage.begin | ArrayBuildRequest明确dtype/shape/unit/index、placement、buffer_bytes；先检查乘积溢出、索引和预留来源，再把所需容量从capacity.state->available()原子转到builder；失败不消费capacity。memory需完整输出容量，spill需有界缓冲与磁盘额度，automatic按资源选择二者，选择不改数值/规范块 |
| ArrayBuilder.append | 单writer、顺序追加count个完整元素；bytes长度与dtype一致，dyadic为count个规范元素；成功返回累计元素数。过量/坏格式在消费前invalid_request；I/O/资源失败使builder进入failed，不能重试append以猜已写偏移；只能abort。调用bytes借用至返回，不保存临时span |
| ArrayBuilder.seal | 元素数恰等shape乘积，所有规范块/hash/覆盖验证完成后一次发布ArrayRef；失败不返回可消费Array。sealed后append/seal均invalid_request，abort无副作用；未封存析构必须撤销私有暂存 |
| Storage.read_reserved | 与read同样的只读视图，用既有capacity覆盖缺失加载/解压缓冲；缓存命中不重复计费物理字节。租约持有实际拆分容量；不足立即失败，不再隐式申请另一份 |

ReservationState.claim在容量不足或已close时无副作用失败；已claim租约释放时，若arena仍open则回到该arena的available，可用于下一块read_reserved，不重复向进程申请。Reservation析构/移动赋值关闭旧arena并立即归还未claim额度；已claim输出继续计费，最后释放才归还祖先。state.close幂等，close后不能再claim；state自身的共享引用不允许重新开域。begin/read_reserved只消费其实际需要的claim；scratch可直接claim后使用，线程claim只由worker持有。

例如H=32MiB输出256MiB：worker先预留约6MiB host（输入、scratch、1MiB输出缓冲及元数据分别核算）和足够spill；begin取走builder缓冲/磁盘claim，每轮read_reserved得到输入view，view销毁把容量退回同一arena，append复用输出缓冲，seal产生256个规范1MiB块。Reservation关闭后只留正式输出仍需的存储/元数据租约；不需要256MiB连续RAM，也不需要每轮另reserve输入。

copy/read是自行预留的便利入口，已有完整工作单元预留的生产路径使用begin/read_reserved；禁止先占全部H再由copy/read重复申请。Storage.adopt/backing、ArrayBuilder、Reservation和后台设备句柄不进入JSON operation白名单，独立驱动仍用copy/open，二者进入同一后端。流式写入、规范块和计费细节分别归OBJECTS/EXECUTION。

### 注册调用与科学操作归属

对外执行统一以process_id + operation_id选择已编译typed binding，类型/单位/index按注册合同校验。科学操作语义逐项见[process目录](processes/README.md)，不再在共享API复制算法表。当前free函数名只用于定位既有实现，后续允许破坏式修改签名及wire；不要求保留旧operation别名。底层helper不绕开统一所有权/错误合同。

以下是共享服务或固定组合入口，非新增科学process；仍须走统一服务/工作流边界：

| 操作 | 共用合同 |
|---|---|
| Storage.copy/read | dtype+little-endian bytes+shape+Unit+index创建不可变Array；校验bytes/元素数，计费后复制；read取flatten BlockRequest、保Lease；越界invalid_request、配额不足resource_exhausted。copy是独立驱动构造矩阵/噪声的公开入口 |
| Resources资源扩展（E01目标；当前仅reserve/consume_work/usage） | ResourceRequest同时预留host/pinned/device/spill和线程，pinned是host子集；Reservation为move-only可拆分容量，销毁归还未转交部分；work不可归还，零units查询剩余。child创建唯一归因域且不重置祖先额度；失败不改变配额。usage为当前资源子域的归因累计，Result取本调用独立子域的快照；process_snapshot另报全进程，禁止累计高水位差分或并发累计差分 |
| make_frames_provider | descriptions/signals/dq等长逐帧、同index、唯一frame/source；内存provider完整实现FramesProvider，缺某能力unavailable；返回对象保全部数组，完全不需产品目录 |
| FramesProvider全部方法 | list_frames按frame ID排序；describe_frame给能力；read_leaf_block的FrameReadRequest明确中心leaves、halo、fields，只支持声明字段，不能静默漏halo；read_native_dependencies按请求SourceIdentity顺序；open_action锁hash；query_support只返回请求index范围；refinable缺失unsupported。并发/请求顺序不改变科学结果 |
| ProductProvider.graph/read_array、open_product/publish_product | graph不可变闭包，read_array同Storage租约；open锁manifest_hash并验证，publish一次新目录，闭包缺失missing_dependency，写盘失败io_error，无完成身份。不得执行产品携带的任意代码 |
| export_signal | Storage产品writer消费已验证ExportResult，仅编码FITS/闭包和事务发布；不重算Q，export.fits.signal归service.storage观测 |
| normalize | NormalizeRequest.frames与outputs等长，frame ID唯一，联合photometry跨frames；supplied_wcs非null走已知头路径且记录supplied，不再盲解。不能标度的帧仅allow_relative_uncalibrated=true才出具名相对L并noise条件。逐帧失败不取消其它可用帧；返回partial及完整分母，失败帧不产HiPS |
| mosaic | 先open每个input，检查同order、Source去重、单位可解释→构建sky→coadd→publish；任一输入损坏默认整体失败，不静默少帧。sky/选择未达请求可留诊断，不把其发布为完整成功 |
| export_image | open→build_export→export_signal；输入Normalize或Mosaic都可，首期只科学真交叠，支持政策显式；输出含sidecar闭包与FITS，单图不能恢复未知信息 |

## JSON 对象编码与完整合同驱动

目标ProcessInvocation信封必需process_id、operation_id、invocation_id、typed input及显式science request；execution/diagnostics在基础设施字段。result统一返回这些身份、resolved_request/input hash、build/实际backend、Result及输出对象/资源快照。registry根据Request/Response声明生成逐操作schema；现有下面operation/arguments/request信封只记录当前驱动形状，后续可直接替换，不要求双协议或旧别名。

registration key是(process_id,operation_id)，operation_id只在本process内唯一；type_id必须和登记一致。调用C++与JSON解码后进入同一typed binding，不能C++绕过Host而JSON单独做科学预计算。

[contract.schema.json](../../../schemas/contract.schema.json)定义envelope，类型字段由声明合同逐字段解码：struct对象字段名与声明一致，enum用枚举名小写，optional=null，vector为数组，std::array精确长度，pair为二元素数组，variant用 {"alternative":"类型名","value":...}。Ref<T>可 {"$ref":"局部object名或content hash"} 或 {"type":"T","data":{...}}；读取后生成有identity的immutable对象。用户输入可省Model.identity/assessment，由decoder生成（默认不假定任何科学保证，assessment按操作校验）；产品序列化不允许省。非Model struct内联。

Array输入特例 {"type":"Array","dtype":"f64","shape":[...],"unit":{...},"index_id":"...","values":[...]} 仅合同驱动小例≤65536元素，驱动用Storage.copy，数值必须有限；较大输入用OBJECTS block descriptors和hash。抽象LinearAction wire为 {"type":"LinearAction","recipe":...,"children":[refs],"arrays":[refs]}，CovarianceAction由NoiseModel decode+make_covariance；FramesProvider wire为descriptions/signals/dq后调用make_frames_provider；FootprintProvider的typed data为{wcs:WcsRef}，只构造canonical_tan_sip实现；Refinable/Product provider必须open公开产品。无需独立Agent继承内部生产类。

现有驱动operation白名单对应头文件free函数；目标驱动必须按registry中process_id/operation_id白名单分派并自动导出schema，不手写第二套分派表。现有的重载forward/to_fits用 input字段point.type明确，Storage.copy/read和provider方法用 operation="storage.copy"/"frames.read_leaf_block"等限定名，输入字段对应参数名，Context从resources构造。provider科学方法必须在所属process白名单内，Storage/FramesProvider纯构造与读块方法由具名service白名单拥有，二者均不能反射私有方法。input_objects是局部名→typed对象，arguments是函数参数名→ref/值，request字段是最后一个请求参数（无请求则{}）；多个Ref参数都在arguments。shape/type/未知字段是硬校验，拒绝对象循环/缺失引用；input_objects按依赖拓扑解码后调用同一API，无科学预计算。输入对象hash由响应返回；driver不得对期望结果调容差。

result.json字段：protocol,major,request_id,operation,input_hashes,resolved_request_hash,build,algorithm_set,arithmetic,result（对应Result<T>），objects（所有输出typed对象），resources。科学nonconverged也写正常JSON并exit0，读者必须看result.code/assessment及每行状态；语法/解码/文件读取失败exit2并写同envelope的result.code。没有实现的operation返回unsupported，不能伪造示例response。stdout为JSONL事件，科学数据只入result文件。

## 执行选择与内部诊断协议：执行选择与内部诊断协议

下述为待实施协议，不是当前schema已接受的输入。目标config及contract信封增加execution和diagnostics两个基础设施字段；缺省execution={backend:cpu,device:null,allow_cpu_fallback:false}，diagnostics缺省无采集。ExecutionPolicy是待声明的基础设施角色：backend为cpu/gpu，device可空字符串，allow_cpu_fallback为布尔；CLI/driver按ExecutionPolicy装配一个已解析策略的executor；嵌入式调用者提供相同策略的实现，科学模块不读配置。首期请求gpu且不允许回退返回unsupported，显式允许时记录requested/actual/reason后走CPU。不得把CPU回退写成GPU成功。执行后端/算术身份进入算法证据与缓存key，设备序号和性能计时不进入科学内容hash。

diagnostics是独立策略文件路径，后续实现应建立独立诊断schema，相对config/request文件；不合并到科学request或configuration_hash。OFF构建可检查该外层路径字段但不打开文件、不读诊断环境变量、不创建sink；run_summary明确diagnostics_compiled=false、requested=true、status=not_compiled。ON时策略校验失败仅禁用诊断并记录config_error，不改变合法科学任务退出码。实际策略、编译开关、选择器hash单独进debug_manifest，不假充科学输入。

策略分别控制perf和snapshots；静态probe_id白名单、frame/component/leaf/ROI、attempt、采样步长和上限确定所选项，禁止按完成先后或随机抽样。未知probe ID是诊断配置错误，不容许任意表达式、文件路径或重新计算中间结果。策略的memory_bytes是显式附加诊断配额D，science resources.bytes仍为H；全进程受H+D及其它命令已声明配额的统一上限约束，启动摘要同时列science/diagnostic/total，不把附加内存藏在H内。固定总内存场景由用户在同一配置预先分配H和D。对照探针ON/OFF保持相同H与science work。probe_ids空表示所有已登记ID，但snapshots.enabled=true必须显式列出至少一个快照ID；frames/components/leaves/attempts空表示不限，roi=null表示不限。采样对规范ProbeTags的SHA256前64位大端整数取模sample_every_n_blocks，不用可变执行块编号；预算竞争可使实际WRITTEN子集不同，所有所选/跳过身份均记录。event_buffer_bytes≤memory_bytes，max_inflight_items=0或memory_bytes=0表示不采集，不是无限。磁盘、在途项与事件缓冲也有独立上限，超额只跳诊断。

选择器在类别之间取AND，同一非空列表内取OR；tag为null不匹配对应非空过滤，空列表不过滤。同一个策略roi分别按每个被选probe登记的局部Box坐标解释（detector或target pixel/chart），不代表一个可跨probe自动投影的共同天空ROI；非Box空间使用leaves过滤。roi要求与tags.roi同坐标、闭矩形有正面积交叠；无roi tag或坐标系不支持ROI时不匹配，接触零面积不算相交，不隐式变换坐标。所有过滤通过后才按规范ProbeTags（含显式null、整数attempt、规范浮点和UTF8排序键）散列采样；attempts明确过滤重试编号，不能以事件到达顺序计attempt。

性能/快照不写入stdout科学事件流。策略root相对诊断策略文件所在目录解析，标准化后不得与正式产品/staging/checkpoint根重叠或嵌套；开发工作区仅在该root/run_id下，固定debug_manifest.json、perf/events.jsonl、perf/summary.json、snapshots/<probe_id>/<encoded_tags>；文件名不接裸路径标签，sequence/attempt避免覆盖。debug manifest的schema=acsd.dev-diagnostics/2，non_scientific_product=true，build/策略hash、probe semantic_revision、实际backend、entries的源模型hash/单位/索引/坐标/范围/文件hash/状态均必需；实施时格式形状由同一诊断schema的manifest定义检验，当前尚无该schema。perf JSONL的schema=acsd.dev-perf/2，run_id/seq加待声明PerfRecord；所有记录包含process_id/operation_id/invocation_id，Host自动阶段span也遵同一字段；wall_ns用steady clock，CPU不可得为null。span事件必须有run内唯一span_id，parent_span_id引用同run中实际父span，根为null；嵌套/递归同probe_id仍各有span_id。非span的span_id/parent_span_id均可null，不以probe_id或线程号猜层级，丢失父事件时摘要标parent_missing。summary给observed/dropped/skipped/failed/flush开销，空采样不能写成零成本。

普通日志/事件必须一直存在：stdout仅acsd.events/2，stderr为有界人类日志；已知故障包含Issue的code/scope/origin/reason/impact/retry_condition，拒绝把异常字符串当错误码。必要运行日志在用户output旁run目录，路径/标识按需脱敏，不记录凭据。资源快照和run_summary分别引用进程监测与调用归因，不能用系统CPU或iowait冒充本算法CPU/I/O。详细探针的丢样不得丢科学issue/terminal。常规运行状态不可被ACSD_DEV_MODE关闭。

观测记录的owner_kind为process/service。没有科学父invocation的初始读入/最终发布等服务调用按SYSTEM创建command子服务scope，process_id为service.storage/resources、具名operation_id与独立invocation_id，parent_span_id可为真实command span或null；科学调用内则使用实际父invocation归因，不捏造缺失父科学对象。

### 诊断策略与记录的实施字段

下例是完整目标策略；各计数/字节/attempt为无符号64位，超范围拒绝，bool严格布尔，未知字段拒绝。root为UTF8路径，memory/disk/event_buffer为bytes，flush_timeout为ms；perf.mode只允许summary/events/both。selection各列表元素唯一，leaves按公开HealpixLeaf编码，roi为公开Box或null。这里不产生新的科学请求或当前可执行配置。

```json
{
  "schema": "acsd.diagnostics/2",
  "root": "./debug",
  "memory_bytes": 16777216,
  "disk_bytes": 268435456,
  "max_inflight_items": 4,
  "event_buffer_bytes": 1048576,
  "flush_timeout_ms": 1000,
  "perf": {
    "enabled": true,
    "mode": "both"
  },
  "snapshots": {
    "enabled": true
  },
  "selection": {
    "probe_ids": [
      "normalize.drizzle.geometry",
      "mosaic.sky.fit",
      "mosaic.weight.coefficients",
      "operators.apply"
    ],
    "frames": [],
    "components": [],
    "leaves": [],
    "roi": null,
    "sample_every_n_blocks": 16,
    "attempts": []
  }
}
```

目标性能记录包含probe_id、tags（frame/component/model_hash可空，leaf/roi可空，attempt必需）、start_steady_ns，以及wall_ns/cpu_ns/count/bytes/gauge各可空、backend、可空thread_id/span_id/parent_span_id。ns/count/bytes为u64、gauge为有限double；span必须有wall_ns及唯一span_id，counter/gauge分别使用count/gauge。采样比例/丢样、队列深度、迭代/nnz/精度升级等指标各有明确单位，不能将null填0。

debug manifest包括run_id/build(source_hash,dev_mode,actual backend)、策略hash/选择器、complete、entries和summary。entry包含probe_id/semantic_revision/tags/kind、representation(original/summary)、dtype/shape/unit/index_hash/coordinate_space/source_hashes、files(path,sha256,bytes)、status/reason。summary至少有selected/written/skipped_budget/not_seen/dropped_events/io_errors/parent_missing/flush_wall_ns/bytes_written。无文件的跳过项也记状态；metadata生成失败尽力保有界计数，不声称成功。FITS/原块与JSON的科学语义和寿命见OBJECTS；后续schema须逐字段体现这些类型、枚举和null约束。

## CLI 配置、事件与 GUI

CLI只有 acsd normalize|mosaic|export --json config.json、--version --json、doctor --json、template <command>；后两项是环境诊断/静态模板，不构造科学数据。三命令配置遵循[config.schema.json](../../../schemas/config.schema.json)：schema="acsd.config/2"，command，inputs，scientific（typed request JSON路径），output，resources，product_profile。配置路径相对config所在目录、产品内部相对产品根，不相对进程cwd。inputs为真实FITS/manifest列表，scientific文件为NormalizeRequest/MosaicRequest/ExportWorkflowRequest字段，但frames.raw、outputs、inputs、input/output由CLI从配置输入/输出唯一注入，若科学文件同时提供冲突字段则invalid_request；CLI只做解码和构建公开对象，不含算法。

Normalize每项input含frame_id、FITS path、可选calibration与response对象路径，catalogue路径另有配置；Mosaic/Export为product manifest路径+必需expected_hash。scientific文件中非I/O字段严格同头合同，无未知键。所有有效配置展开后按schema/类型验证并存闭包。模板默认：f=1、select=dq_only、weights=automatic、support_policy=dyadic_cells、publication full、threads=可用CPU的min(8,quota)、memory=1GiB、work=10000000、spill=8GiB、max_depth=64、initial_bits=64、max_bits=256、solver max_iter=200、rank_relative=1e−12（只是数值分类起点，必须同时记录误差界）。科学 tolerance、response、参考SED/单位、尺度搜索域和历元无通用默认，缺失invalid_request。

退出码：0所有必需目标达标并发布；2配置/输入无效；3能力不支持/必需模型 unavailable；4计算/I/O/完整性失败；5取消；6部分帧失败或数值未达标。优先级取消>完整性/I/O>输入>能力>partial>成功。无论退出码，run_summary列全部输入数量、已发布/unsolved/failed/cancelled数量与逐帧状态，不把仅完成部分当exit0。

诊断命令（`--version --json`、`doctor --json`）不产生输入：run_summary 的
inputs_total/published/unsolved/failed/cancelled 全为 0、frames 为空数组；`--version`
额外给出 `version` 字段。诊断命令的事件流同样是 JSONL：doctor 的环境报告是**一行一个
JSON 对象**，与事件行同级。

run_summary 的逐帧数组元素形状固定为四个字段：`frame`（帧标识；normalize 用输入的frame_id，mosaic/export 用命令名）、`status`（`published`/`failed`/`unsolved`/`cancelled`）、`code`（稳定小写错误码，成功为`ok`）与`reason`（人读原因，一句话）。元素顺序与输入顺序一致；失败的帧也必须出现，分母是全部输入数。

JSONL每行UTF8单对象：protocol=acsd.events/2，run_id，seq（从0严格递增），event（run_started/frame_started/block_progress/frame_finished/product_published/issue/run_finished），frame_id/block可null，code，assessment可null，completed,total（同kind单位），message。单写者归并事件序列，stderr仅常规人类日志，输出背压队列有限、合并进度事件但不能丢issue/terminal。每run恰一个run_finished，含exit_code及summary；意外崩溃可能缺terminal，GUI必须标失败，不能推断成功。取消：GUI向stdin写 {"protocol":"acsd.control/2","command":"cancel","run_id":...} 换行，CLI控制线程发token；Ctrl-C/SIGTERM同效果。取消先等安全边界清理，无自动kill超时后冒称完整结束。GUI必要时用户主动终止标aborted。

GUI管理配置、输入分组、运行/取消、事件显示与产品浏览；不解析内部C++对象、不做science运算、不依赖旧ABI。Qt6独立进程启动可与Windows UCRT科学CLI组合；GUI/CLI须使用当前匹配的config/event协议；不匹配明确拒绝，内部开发不保留旧协议分支。

## 构造操作、顺序和单位

声明中的全部free函数参数已有名字。驱动的arguments只含非context、非request参数，最后名为request的参数放envelope.request；没有该参数则request必须{}。原始wire名单以已命名声明为准，禁止按未命名类型猜key。to_fits/forward重载仍由point.type判别。

科学构造与连接操作各自属于[process目录](processes/README.md)中的typed binding；provider/Storage接口是公开基础设施白名单，不能借operation文本反射任意私有方法。

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

CLI初始化一个CpuResources实现、一个Storage实现和取消源，按Limits创建Context；独立C++调用者可实现这三份公开接口或复用src/resources与src/storage实现。实现中Resources原子准入、Storage内存/磁盘backend及immutable arrays均遵循EXECUTION，无可变科学会话注册、无科学全局配置。现有小数组可用Storage.copy、已有块用open；E02实施后算法的大输出必须用begin→append→seal，begin/read_reserved消费现有Reservation容量避免二次reserve；模型小对象可由caller独占组装，consumer集中校验，publish后Ref<const>不可写。

校验顺序固定：协议/字段/类型→尺寸与溢出、单位/index/identity→科学域/能力与资源准入→算法→逐泛函误差/状态→序列化量化→发布闭包。无效模型在消费边界一次拒绝，内部已验证块不逐元素重复防御或记录日志。未知能力只限制对应操作，不能把合法负值/零删掉。

## 独立驱动块入口与返回

contract信封增加可选input_root（默认request文件所在目录）和blocks[]（默认空），blocks逐项为BlockDescriptor。descriptor.path相对input_root解析，显式root之外不相对进程cwd猜文件。输入可用id局部别名，decoder解析index_id后计算真正descriptor hash，再将Array.block_ids别名替换；已有sha256身份必须匹配。Array的typed data形式含block_ids，decoder调用Storage.open，不要求先建立正式product。小型Array.values也支持dtype=dyadic的Dyadic对象，不能用整数float替代高精度端点。

storage.copy参数为dtype,bytes（u8序列）,shape,unit,index，按公开C++参数名；storage.read为array,range；storage.open为root,description,blocks；product.read_array为provider,array,range。Context/Storage注入，不从输入装载对象代码。对应所有操作已进入唯一生成schema；其request必须{}。其它资源对象方法只供同步C++调用，不作为用户可扩配脚本。

Result<T>在response.result中按字段序列化：by-value struct为普通字段对象，vector为数组，optional为空则null，Ref<T>为{$ref:hash}；response.objects[hash]给type/data（实际闭包），response.blocks[]给所有输出descriptor，output_root给其路径根。to-fits.expected-model的data对应result.value；make_wcs等Ref返回则须从objects取typed对象。新请求必须将所需typed对象及其依赖加入input_objects，将blocks带入并input_root指前次输出根；单独抄一个hash不能跨进程查隐藏会话。response文件路径由--response显式指定，模型块在output_root，二者不混作path根。

U01已补四个辅助Index（CSR offsets/nnz各一个），所有Array.index_id均可在公开输入对象图解析。examples/external-array.request.json使用真实24byte little-endian块、SHA256和相同R，期望[3,7]；不需旧产品目录。形状检查之外的别名/块覆盖/hash校验先于任何科学算法。

measure_science_sources 的结果除名义 StarFlux 与随机 joint_noise，必须含 OBJECTS 定义的通量数值界、强引用 error 和 observation。fit_photometry 的公开 measurements 参数即为此账本入口，独立进程无需另查隐含 Native；缺证书传播 conditional 而不补零。evaluate_psf 的 output_rows 提供窗口/分块查询，行号、返回子索引与错误规则见 SYSTEM；同步调用和预算/借用规则同其余科学 API。
