# 标准科学 API 与驱动协议

本合同定义目标接口，尚无实现头文件、schema 或驱动程序。科学类型遵循[统一对象](OBJECTS.md)，所有 action 的域、单位、原点及随机变量身份必须匹配。独立 Agent 只需本合同和五份科学规范即可构造输入，不需要理解旧 CLI、adapter、scheduler 或 session。

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

rank/迭代阈值是带列尺度及误差解释的数值请求，不是以任意epsilon删除真实模态；实现模板须在后续独立数值验证中固定具体配置。本架构定义可估性及失败语义，不为不同矩阵统一发明一个科学误差阈值。

## 线性 action 的可执行合同

每个 action 暴露 `describe`、`apply`、`transpose_apply`、`dependency_blocks`、`serialize_recipe` 和 error/conditioning。可选 `row_enclosure`、`absolute_factor_apply`、`operator_norm_upper`、`solve/whiten` 逐项声明；没有的能力返回 unsupported，不能由普通 apply 推断。

对于原生 C=BDBᵀ+C_loc+UΛUᵀ，covariance action 为 L C Lᵀ；不同 frame view 共同 shared 根形成 cross action。GLS 要 inverse/whitening 或带约束的增广系统，不能将 C(v) 当白化。转置、共享 latent 坐标、signed aperture 和空域须在 driver 可见。非线性拟合/选择的 influence 是指定条件下线性化，记录线性化点；全过程 MC 才评价数据依赖后的 coverage。

## 版本化 C ABI

首期 ABI major=1，不沿用旧 process ABI。所有表/结构有 `struct_size,abi_major,abi_minor`；固定宽度整数、显式 enum、字节串长度和 span 长度，禁止 STL、bool 布局、异常、C++ 类或 allocator 跨边界。查询 `acsd_science_get_api` 得函数表和 operation/type capability；目标符号前缀 `acsd_science_*`，具体编译头在后续实现分支生成。

大对象 `uint64 opaque handle` 由创建它的 host/context 管理，retain/release 成对；context 不得在 outstanding handles/events 前销毁。数组 map 返回只读 view+lease，unmap 后不可用；mutable builder 仅在 publish 前独占。async submit/cancel/wait 的 event 有 terminal 状态和结果句柄，不用线程局部 error 指针。C 驱动与 CLI 只是标准 API 的客户，不建第二套估计器。

## 独立进程合同驱动器

目标工具 `acsd-contract --request request.json --response result.json` 只用于公开合同调用/验证；正式用户仍用 `acsd normalize|mosaic|export --json config.json`。驱动器只接受公开操作白名单，不暴露模块内部 helper；请求通过文件使大模型不依赖 shell 转义。

请求 envelope：`protocol="acsd.contract"`、major=1、request_id、operation、input_objects、request、resources、output_root。数组是带 hash/dtype/shape 的外部块；小型坐标/标量可内联。响应含 request/input hashes、解析后的模型身份、api/algorithm/build/arithmetic、结果对象/hash、所有状态、实际work/bytes/time和 issues。退出0只表示驱动调用完整处理，科学成功必须检查逐泛函状态；语法/协议/读取失败返回非0和结构化错误。

设计形状示例（占位 identity 仅说明字段，不是真实科学产品）：

```json
{
  "protocol": "acsd.contract", "major": 1,
  "request_id": "review-new-tan-01", "operation": "compute_overlap",
  "input_objects": {"wcs": "input/wcs.json", "footprints": "input/footprints.json"},
  "request": {"order": 18, "pixfrac": 0.63, "area_absolute_sr": "1e-20",
    "scope": "all_contributions", "arithmetic": "outward_mpfr_192"},
  "resources": {"max_work": 40001, "max_bytes": 268435456, "threads": 1},
  "output_root": "result"
}
```

真实 science request 可调用 `build_drizzle` 传带单位 B/F/Cov/孔径容差，不能以本例 area 请求代替最终科学停止。area-only 响应禁止伪造 B达标。decimal 请求容差先有向解析，精度选择不是“位数越高即已成功”。

正式 CLI 配置分 scientific request、product profile、resource limits；响应/SED/gauge/排异均显式或由模板给出并在 manifest 展开默认值。GUI 只管理配置、子进程与 JSONL 事件。运行事件统一含 run/frame/block、事件序号、状态/错误，stdout 只 JSONL，常规日志另流；取消写明已完成及未完成输入，退出码由CLI映射到输入错误/不支持/运行失败/取消/部分未达标，不把任意“运行结束”映射成功。
