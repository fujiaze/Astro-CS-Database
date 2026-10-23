# sources.psf：PSF拟合与像元积分

状态：目标process文档；对应科学函数可已有部分实现，标准注册/生命周期/探针尚未据本文落地。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `sources.psf` |
| 所属模块与代码归属 | `src/sources`；本模块负责算法及typed adapter |
| 注册operation白名单 | `fit_psf,evaluate_psf,bind_observation_model`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | Native/WCS/科学源/PsfRequest或EffectivePSF/PsfEvaluationRequest → EffectivePSF/PsfEvaluation。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；未来模板批次可独立后端；参数优化和认证包围无能力时CPU，不降低误差标准。 |

## 算法与失败合同

同一evaluate_psf计算真实收光域积分与尾界；fit/measure/information复用，禁止中心采样替代。

模板值、lower/upper、PSF参数盒、尾损失和unknown物理项分列；transfer含G/α/R/P/Q真实链。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

按源和output_rows分块；共同窗口保混叠耦合，拒绝全图×全源常驻；GN scratch按窗口准入。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`normalize.psf.model`、`normalize.psf.fit`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：F01；解析Gaussian积分、部分窗口、signed transfer、output_rows子索引及秩亏。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，文档中的角色名不表示头文件现已提供新接口。

## 算法详细条款

### 数值与模型细则

科学源从 WCS 后 Gaia 预测独立选取；solver candidates 不转作测光源。以饱和/邻源/窗口掩码选择身份冻结后拟合。Gaussian 参数为 logσx、logσy、atanhρ；Moffat 为 logαx、logαy、atanhρ、log(β−1)；Airy 为 log first_zero_radius_pixel，圆对称。每源同时拟合连续位置、总 flux、常量/平面背景；flux 允许负，宽度/β 正参数用上述变换。空间 PSF 参数按归一 detector 的 degree≤2 多项式拟合，缺秩返回不可估，用户可重新请求常量，不暗中降阶。

### 数值与模型细则

Gaussian 无相关时 erf 可分离积分，相关时二维自适应积分；Moffat/Airy 对真实 footprint 分块求积。用解析尾质量界选择窗口（Gaussian erfc；Moffat (1+r²/α²)^(1−β)；Airy encircled-energy Bessel 恒等式），不把窗内归一为1。PRF首期为单波段分段常量photosite响应；只支持下文明确的achromatic模型，色相关项记录unknown/conditional。integral+tail 数值预算不满足则拒绝该测量能力，不以采样 PSF 替代。源间混叠以共同窗口联合线性 flux+background 求解；窗口巨大超预算可拒绝源，不裁掉邻源却声称总 flux。

### 数值与模型细则

PSF 非线性形参用 GN+Armijo 回溯（初始步1，减半，最多32步），每步 QR/SVD；迭代次数、参数梯度和步容差显式。拟合 influence 为 -(∂score/∂θ)^−1∂score/∂data，在可估空间计算，保留像元根、同星与 PSF 共用训练依赖。仅 GN 近似 Hessian 时标 conditional；不收敛返回 nonconverged。

### 首期PSF唯一数学参数化

首期只achromatic（wavelength_nm记录有效适用域，无谱插值PSF）；请求色相关分段核unsupported并记录未建色相关物理项。PSF先在原始detector平面给归一密度p_det[1/pixel²]，世界密度为p_det/g，在有效域条件下实现观测分册的积分；畸变不靠中心尺度代替换元。源位置偏移Δ以PixelCenter0为准。

Gaussian：Σ=[[σx²,ρσxσy],[ρσxσy,σy²]]，m²=ΔᵀΣ⁻¹Δ，p_det=exp(−m²/2)/(2πsqrt(detΣ))。参数行依次logσx、logσy、atanhρ，σ单位pixel。Moffat同Σ但σ换α，p_det=(β−1)(1+m²)^−β/(πsqrt(detΣ))；参数行logαx、logαy、atanhρ、log(β−1)。Airy圆对称，参数行log r0，r0为第一暗环半径pixel；k=j_1,1/r0，j_1,1是J1第一正根（用有界根求解，不以截断常数当精确），p_det=k²/(4π)*[2J1(kr)/(kr)]²，r=0取极限。Gaussian/Moffat椭圆m>R尾分别exp(−R²/2)、(1+R²)^(1−β)；Airy圆r>R尾=J0(kR)²+J1(kR)²。

parameters shape=[family参数数,多项式项数] f64，Basis按OBJECTS项序；空间degree≤2，center为detector域中心、scale为半宽，Basis实际值随对象持久化。Gaussian/Moffat形状矩阵保证SPD，非有限或极端无法积分给失败。PSF有限窗口内先按真实PRF分段，所有源/背景共窗拟合，窗口外质量不重归一。PixelResponse为独立空间PRF对象，不和谱Response混用；cells矩形在local photosite坐标，gains[波长bin,cell]非负，relative_mean_one时每波段在完整收光域面积平均=1；绝对概率模式各gain≤1。空cells是显式uniform假设，未知PRF误差仍unknown。

evaluate_psf是唯一像元积分实现，fit/measure/information/sky均复用。EffectivePSF.detector_samples锁原生FootprintSet；regroup_scale_project从原生样本到当前输出（初始I，随后G、α、R、P、weights、Q按实际链组合），参数密度不再另含这些action。evaluate_psf返回PsfEvaluation中的[当前输出样本,请求源位置]积分模板，有限模板bank不是对continuum隐式稠密矩阵。误差与尾界随PsfEvaluation的实际字段和ErrorRecord强引用；请求可分源位置块，禁止消费者各写一份PSF积分。局部模型外插只conditional，不以满矩形归一域冒称实测支持。

### 生产者的跨模块合同

evaluate_psf返回PsfEvaluation而非裸Array。values/numerical_lower/numerical_upper shape=[输出样本数,source_positions数]，同一template笛卡尔索引；output_index/source_index明确各轴，索引命名为cartesian:<output_hash>:<source_hash>:row_major。native_tail_upper[source数]只报告原生正PSF模型在窗口/域外的质量，不能把signed transfer后的结果称概率。区间先在每个原生收光域积分，再经同一transfer：若原生误差e、transfer名义T0及逐元界ET，则输出确定半径≤|T0|e+ET(|h0|+e)+归约舍入；signed P等必须用绝对系数界，不用T0e抵消。缺absolute能力只可conditional/unavailable，不伪造有界。ErrorRecord强引用区分求积误差、模型尾损失和unknown物理项；partial结果保实际区间，work耗尽不只保请求tolerance。

### 生产者的跨模块合同

PSF模板幅值与来源也固定：fit_psf返回的density为校准前置已完成、最原生physical samples中的单位总ADU源模板，source_scale=null；detector_samples为该parent，regroup_scale_project初始为到当前样本的G（原生I）。FootprintSet.sample_response_gain[N]（null表示1）保已知样本乘性响应，校准更新为raw gain/flat；PRF积分乘此系数，物理bin仍在parent积分后用G，不能以bin中心flat代替成员。

### 生产者的跨模块合同

EffectivePSF.parameter_lower/upper/error记录PSF拟合的数值证书，验证同[NUMERICS](../standards/NUMERICS.md)的score根方法；evaluate_psf将这些参数盒与求积误差一起包围。参数证书缺失时只可条件于已解析固定PSF数值模型，不能伪称涵盖理想重新拟合的确定误差；unknown真实PSF/PRF偏差仍另列，不由训练Hessian自证。template_evaluation、PhotoSolution和Native的ErrorRecord均是实际强引用，生产实现不得另建误差正本。

### 生产者的跨模块合同

PsfEvaluationRequest.output_rows 空表示整个 transfer 输出；非空为该输出的严格递增、无重复零基行号，越界 invalid_request。返回 output_index 为精确子索引，values shape=[所选行数,源数]，source_index 不变。积分仅请求 transfer.dependency_blocks 所需原生样本，按工作预算分批；源拟合使用实际窗口行，information 按其查询子索引调用，禁止为了小窗口强制物化全帧×所有源矩阵。省略的输出行不重归一 PSF；native_tail_upper 仍只报告原生积分/截断域的质量，不拿未查询输出代替尾界。

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| fit_psf | fit_psf输出实际像元积分核，measure用该核和Gaia预测位置输出StarFlux及完整joint_noise/influence。max_outer取budget.max_iterations；源窗口无支持unavailable，非收敛源逐项记；可用源不足整体unavailable，不退回solver_detections |


### 构造与连接细节

- bind_observation_model(observation,kernel,response)：将fit结果及响应绑定同观测新revision，校验frame/source index和实际G；不应用α、不改数值，已scale改变response拒绝identity_conflict。
- evaluate_psf(psf,request)：单一PSF积分及实际transfer链，输出PsfEvaluation，含[output sample,source position]模板和实际数值包围。预算失败保有效包围和原因，不能另以整数采样替代。


### 可复用计算模块组合

目标组合：窗口read_async → 唯一PSF/PRF积分 → numerics联合拟合/参数盒 → immutable PSF；查询复用同积分与operators transfer。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。
