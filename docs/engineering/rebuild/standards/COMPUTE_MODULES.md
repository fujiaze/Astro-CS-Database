# 可复用计算模块与强类型组合标准

本篇拥有计算积木的公共行为标准，基建骨架只在[SYSTEM](../SYSTEM.md)，具体科学算法仍在各 process。Task/Pipe/Block 等已有 Runtime 公开声明与部分实现；下述组合必须复用这些真实接口，不再基于历史设计阶段文字新建同名机制。

基建行为遵循[FOUNDATION_API](FOUNDATION_API.md)；以下模块函数与资源公式是接入合同，部分模块已实现，另一些仍待接入或完整验收，见[状态正本](../../../audit/IMPLEMENTATION-STATUS.md)。编译装配/调度按[SCHEDULER](SCHEDULER.md)，完整校准模块见[实施例](CALIBRATION_WALKTHROUGH.md)。

## 计算积木的分层

| 模块族 | 提供什么 | 复用者与边界 |
|---|---|---|
| arrays / pipes | 分块读写、slice/view、dtype/索引/单位、封存、只读lease与有界通道 | 全模块；科学数据不经裸指针跨异步边界 |
| elementwise / reduction | 校准/标度所需强类型逐元素算术、DQ传播、固定key map/reduce | observation、photometry、coadd；signed和unknown语义不丢 |
| operators | diagonal/CSR/compose/apply/真transpose、依赖块查询 | geometry、PSF transfer、sky、covariance、Export；一份核心实现 |
| numerics | 区间/精确原语、积分、QR/SVD/稀疏解、PSD/QP、数值证书与停止 | 科学process；规则由NUMERICS唯一拥有，不用另一个GPU求解改变目标 |
| coordinates / geometry | 规范WCS、真实footprint、曲边交叠、支持Boolean与版本反馈 | drizzle、astrometry、Export；不把严格几何简化为四角采样 |
| PSF / spectrum | 唯一像元积分/transfer、响应基积分及实际误差 | fit/measure/information/sky；不各写模板积分 |
| uncertainty | 共享根/influence图、C action、泛函方差与绝对因子界 | 全链；不凭分块或复制制造新噪声根 |
| services | 异步read/append、spill/cache、事件/诊断、取消与事务提交 | 通过pool统一资源；不属于科学算法副本 |

这里是库能力分组，不要求每组再有一个可变manager或独立线程池。普通科学库函数可同步调用小型原语；有可等待I/O/并行块时返回Task或由统一组合器调度，不在算法中调用阻塞future.get等候同一池。

## 一个计算模块的标准

模块公共声明给出强类型Input/Output、domain/codomain单位/index/shape、前置能力、数值与随机误差合同、资源上界函数、输出所有权、重入/取消/异步完成语义。实施时可把公开声明拆为共享基础头与模块公共头，现有contracts.hpp作为聚合入口；每个类型/签名只定义一次，不要求永远把所有算法塞进一个巨型头。当前可编译声明以 `model.hpp`、`runtime/*.hpp`、`modules/*.hpp` 为准，`contracts.hpp` 只聚合。

资源需求是依赖实际shape/nnz/位长的保守上界，至少分input新加载、output、scratch、队列/halo/元数据、spill与I/O在途；共享既有lease不重复收费。实际需要扩大时先增量准入，失败按安全边界返回，不能先分配后补账。不可控第三方workspace需实测余量并纳计划。

组合器包括then（上一步成功后消费typed输出）、when_all（并行独立依赖及错误汇合）、map_blocks（按固定key拆分）、reduce（固定科学顺序归约）和bounded_pipe（异步块流）；无需任意字符串表达式/反射图编辑器。非ok partial不可被then默认当完整成功，只有显式处理Assessment的分支才消费有效域。失败在所有在途任务安全结束后回收，保持第一故障及局部失败范围。

then/when_all只安排有界continuation；await后释放CPU token，回ready再准入，不在I/O完成线程运行重计算。finalize/seal/cleanup也走该完成依赖，不能在worker上get等待同一pool。有序pipe的发布信用按SYSTEM的next_required_key保留，单slot也不能被高key先占住。异步参数只能强持Ref/Block/lease或按值元数据，裸span仅限同步借用。

算法调用模块函数时共享本invocation的预算/lease/取消，不嵌套创建Host或重置work。可直接复用库函数而不重复顶层validate/plan；对外注册适配器负责边界科学校验。优化不得把单位、context、版本、共同噪声根或误差证书在函数间剥离。

## 用组合表达一个科学步骤

以下为目标结构示意，不是现有可编译代码或新公共签名：

```text
read_async(当前输入块)
  → then(校准/标度模块，产同单位与区间/DQ的只读块)
  → then(同一CSR/action模块，产signal及对应模型链)
  → bounded_pipe(显式items/bytes信用)
  → append_async(私有builder)
  → when_all(全部必要写入完成)
  → validate_and_seal(元素/误差/闭包)
```

独立source/窗口可用map_blocks并行；全局photometry/sky系统必须先汇合装配和求解，再开放后续阶段，不能因为管道方便就拆掉科学相关性。选权后coadd.integrate直接消费Selection/WeightSolution，不重新执行这些模块。共享数值函数只一份，CPU后端当前实现，未来GPU可替换支持的批次执行，模块上层数学组合不改。

## 可复制与接入完成条件

模板包必须有本地README/科学文档链接、公开typed接口与依赖列表、一个descriptor/adapter、CPU实现、合法小例与负例、必要许可证/来源。接入后registry/schema/能力目录自动生成，Host无算法switch，默认phase/usage观测自动出现。独立使用时能在最小公共基建/已列依赖下执行相同例子；依赖未提供应构建失败，不能偷偷查全局产品目录或原项目session。

高并行验收同时比较串行、单模块/组合、低H与spill、I/O延迟/失败/取消、满pipe、不同任务完成顺序。性能报告给CPU利用、有效吞吐、I/O等待、队列水位、峰值驻留、复制/传输字节和端到端完成比例；IO overlap本身不是科学正确或性能成功证明。

## 公共函数清单与请求映射

命名空间固定acsd::modules。表内f(Input)统一展开为`rt::Task<Output> f(rt::InvocationContext, Ref<Input>, Ref<rt::InvocationPlan>)`；同步小原语只在模块内部直接调用。Input按FOUNDATION_API规则从唯一科学声明机械生成，provider/span改强所有权；不能手写另一组参数名。输出指Result<T>中的T。模块内部直接复用实现，不递归Host.invoke。

| 函数/公共头 | Input → Output及唯一科学实现 | 关键失败/分块 |
|---|---|---|
| calibrate / observation.hpp | CalibrateInput(raw,request) → Ref<NativeObservation>；calibrate_observation | 校准顺序/母版同根；按样本块，完整例另列 |
| regroup / observation.hpp | RegroupInput(observation,request) → Ref<NativeObservation>；regroup_observation | G域/陪域与footprint，物理bin/linear_view分开 |
| wcs / coordinates.hpp | 依operation的MakeWcs/ForwardCenter/ForwardEdge/Inverse/Derivative/JacobianInput → 对应公开输出 | 整域证书/原点/逆根盒；按点/Box，域失败不点采样补界 |
| catalogue / catalogue.hpp | CatalogueBuild/Query/QuadQuery/LookupInput → 对应公开index/page/stars | 只读分片/历元来源/去重；外存建索引 |
| astrometry / sources.hpp | SolveAstrometryInput → Ref<AstrometricSolution> | 有界候选/全域WCS准入，unsolved保原因 |
| psf / sources.hpp | FitPsf/EvaluatePsfInput → Ref<EffectivePSF>/Ref<PsfEvaluation> | 唯一像元积分与尾界；按源/窗口/行子集 |
| measure_sources / sources.hpp | MeasureScienceSourcesInput → Ref<SourceMeasurements> | 混叠窗口联合、确定数值区间与joint_noise |
| response / response.hpp | IntegrateResponseInput → Ref<SpectralPrediction> | 支持折点/尾界，按谱/源块积分 |
| photometry / photometry.hpp | FitPhotometryInput → Ref<PhotoSolution> | 共享latent，先局部消元再全局屏障 |
| apply_scale / photometry.hpp | ApplyScaleInput → Ref<NativeObservation> | 原生Dα再G，application_id重复拒绝 |
| overlap / geometry.hpp | ComputeOverlap/RefineGeometryInput → Ref<Overlaps>/Ref<GeometryRevision> | 有界Box/primitive，precision/work停止保合法coarse |
| drizzle / geometry.hpp | BuildDrizzleInput → DrizzleResult | 同R传signal/noise/PSF，弱支持不伪归一 |
| action / operators.hpp | Apply/TransposeApply/ComposeInput → 对应ArrayRef/action | 单位/index匹配；稀疏依赖、固定归约，真transpose |
| covariance / uncertainty.hpp | MakeCovariance/CovarianceApply/QuadraticForm/GeometryBoundInput → 对应C/Array/Variance/Bound | 同根先合并，unknown/PSD/绝对因子能力分开 |
| information / information.hpp | EstimateFluxInformationInput → Information | h/Z/C与template列明确，参考条件不混actual |
| scene_variance / information.hpp | EvaluateSceneVariance/BuildTriangulation/ReconstructInput → 对应公开输出 | 支持内网/曲率界，未知外域不填值 |
| sky_controls / sky.hpp | BuildSkyControlsInput → Ref<SkyControls> | 公共支持/共同响应、patch T/H/A/E |
| sky_fit / sky.hpp | FitRelativeSky/ApplySkyInput → Ref<SkyFit>/Ref<SignalEstimate> | 分量联合、gauge/nullspace、同P噪声 |
| select / coadd.hpp | SelectSamplesInput → Ref<Selection> | DQ/单轮LOO条件，N<4不能猜 |
| weights / coadd.hpp | SolveWeightsInput → Ref<WeightSolution> | Γ共享项，BLUE/PSD/QP，缺C具名失败 |
| integrate / coadd.hpp | CoaddInput(frames,sky,selection,weights,request) → CoaddResult | 只消费已封存选择/权，不重算 |
| project / projection.hpp | BuildExportInput → ExportResult | 同叶合并b/真实支持Q，无二次pixfrac |

一个表项有多个operation时生成具名函数重载，输入类型与TypeId明确；例如`psf(ctx,Ref<EvaluatePsfInput>,plan)`与fit输入不同。这里不授权字符串operation去调用任意函数。make_action/模型bind/区间provider能力仍在所属process登记，按同规则生成typed函数；纯Storage/Frames读块走service。所有57个登记operation应由生成器逐项验证Input/Output/codec存在，缺项构建失败。

## 可组合的块函数签名

以下代码块与FOUNDATION_API的声明一起使用，返回Task的函数自行submit_cpu或异步Storage，不由调用者再套一个不计费线程。

```cpp
namespace acsd::modules {
struct CalibrateInput { Ref<RawObservation> raw; Calibration request; };
struct CoaddInput {
  Ref<FramesProvider> frames;
  Ref<SkyFit> sky;
  Ref<Selection> selection;
  Ref<WeightSolution> weights;
  CoaddRequest request;
};
struct BlockContext {
  rt::InvocationContext invocation;
  rt::ArenaRef arena;
  rt::Key key;
  BlockRequest range;
};
struct SignalBlock {
  rt::Block<double> value, lower, upper;
  rt::Block<std::uint16_t> dq;
};
struct CalibrationBlock {
  rt::Block<double> raw;
  rt::Block<std::uint16_t> dq;
  std::optional<rt::Block<double>> bias, dark, flat;
  Calibration policy;
};
struct SignalWriter {
  rt::WriterRef value, lower, upper, dq;
};
struct SignalArrays { ArrayRef value, lower, upper, dq; };
template<class In,class Out>
rt::Task<rt::Done> stream_map_ordered(
    rt::InvocationContext, Ref<rt::InvocationPlan>, U32 region_id,
    std::function<rt::Task<In>(BlockContext)> load,
    std::function<rt::Task<Out>(BlockContext,In)> transform,
    std::function<rt::Task<rt::Done>(BlockContext,Out)> sink);
rt::Task<CalibrationBlock> read_calibration_block(
    BlockContext, Ref<CalibrateInput>);
rt::Task<SignalBlock> calibrate_block(BlockContext, CalibrationBlock);
rt::Task<SignalWriter> begin_signal(
    rt::InvocationContext, Ref<rt::InvocationPlan>, Ref<CalibrateInput>);
rt::Task<rt::Done> append_signal(BlockContext, SignalWriter, SignalBlock);
rt::Task<SignalArrays> seal_signal(rt::InvocationContext, SignalWriter);
rt::Task<rt::Done> abort_signal(rt::InvocationContext, SignalWriter);
rt::Task<Ref<NativeObservation>> finish_native(
    rt::InvocationContext, Ref<CalibrateInput>, SignalArrays);
Result<rt::InvocationPlan> plan_calibration(
    rt::InvocationContext, Ref<CalibrateInput>, rt::PlanOptions);
Result<rt::PlanBounds> bounds_calibration(
    rt::InvocationContext, Ref<CalibrateInput>);
Result<rt::Done> validate_calibration(
    rt::InvocationContext, Ref<CalibrateInput>);
rt::Task<Ref<NativeObservation>> validate_native(
    rt::InvocationContext, Ref<CalibrateInput>, Ref<NativeObservation>);
template<class T> Result<T> failure_like(const Result<rt::Done>&);
} // namespace acsd::modules
```

stream_map_ordered须在对应run_region的region_context调用，先由plan取得每cohort的publication permit与arena，再load→transform→按key串行sink；load/transform可在已准入的E窗口重叠，sink只接下一个key。最多展开plan窗口的Task，不为全图预建vector<future>。首个必需失败停止展开新cohort并排空已展开任务，外层非ok无虚假完成值；科学允许的partial由该process另设显式有效域处理，不靠此组合器自动补像素。失败/取消仍调用外层settle清理。

read_calibration_block并行read raw/DQ及实际存在的master maps，缺省bias/dark/flat不分配整块常量数组；缺省由policy明确，非法index/单位在run前拒绝。calibrate_block只做同一校准内核及数值区间，不在此构造重复NoiseModel；读取和输出块均带真实Lease与相同range/index。CPU callable使用runtime传入的task_context记实际work。

begin_signal创建四个当前格式的私有writer，使用plan.persistent/progress已有额度。某个begin失败必须排空已成功创建的writer并abort，不能只返回首错。append_signal等四个append实际完成才结束，核对累计元素一致；任一失败进入整个SignalWriter failed。seal_signal用settle_all排空四个seal，成功才给SignalArrays；失败释放已sealed但未交付的临时ArrayRef并清理其私有cache/块，不能泄漏三个成功数组。abort_signal涵盖未sealed与孤立sealed私有产物，排空后幂等清理；已公开交付的数组不能被它删除。

finish_native使用同一校准模型构造器绑定SignalArrays、Raw.context/frame/native_index/footprints、I regrouping与实际NoiseModel/ErrorRecord；不再做像素校准。原始母版共享根按真实Jacobian合并；没有模型保持unknown。validate_native集中检查同index/shape、数值包围、DQ、依赖闭包和误差条件，成功后不改数组。failure_like只复制原code/assessment/issues/usage/dependencies且清空value，不把失败变成另一算法结果。

## 每模块的资源模型必须怎样填写

资源报告由三部分组成：R=存活模型/最终metadata/控制节点；F=单cohort按SCHEDULER生存期模拟的峰值；E=额外完整cohort窗口。表中n、nnz、K、p、bits均取本次实际上界；每一项通过公共allocator/workspace_upper计费，不能把Eigen/GMP、descriptor字符串和排序buffer漏掉。

| 模块族 | 每cohort必要项/计划输入 |
|---|---|
| 校准/标度 | 同索引输入及masters、DQ，输出值/双端点/DQ，区间内核workspace；母版缺省不造常量全图 |
| G/CSR/action/Drizzle/Q | 被触及输入、offset/indices/values、输出与界、源键归并/转置spool、真实nnz；按依赖块，不按全图大小 |
| WCS/几何 | 有界候选/Box/顶点/primitive、精确有理位长、ledger/排序/spool；候选爆炸先缩域，动态扩展先重算F |
| PSF/measure | 窗口样本数×模板列、参数/设计矩阵、QR/SVD/score盒workspace、共同混叠组；不能逐源拆掉组相关 |
| response | 谱/响应节点、分段面板队列、维数d与算术bits，积分结果和尾界 |
| photometry/sky | 每星/每控制局部块＋全局参数/Schur/QR/KKT workspace；全局部分入R/barrier，不能按一个叶块估算 |
| covariance/information | 触及根/因子nnz、泛函/模板列、可估子空间解workspace；不物化输出N² |
| selection/weights/coadd | 当前接受样本数K、Γ的K²、PSD/QP workspace、mask/权/支持，源顺序归约；大K若无合法结构路线明确拒绝 |
| 三角网 | 当前chart控点/triangle/精确谓词、halo/holes/查询批次；典型O(MlogM)不是内存上限证明 |

每个process的plan需给min_block_elements、可分块轴与不可拆barrier、以上项的来源/上界及spill路线。实施表见[DEVELOPMENT](DEVELOPMENT.md)；缺任何必需workspace上界不得宣称该模块已完成接入。
