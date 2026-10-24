# sources.measure：科学源通量测量

状态：本篇规定目标算法及实施合同；已有部分Runtime和科学模块实现，具体完成/验收只看[统一状态](../../../audit/IMPLEMENTATION-STATUS.md)，不能由本文标题推断全部未实现或已通过。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `sources.measure` |
| 所属模块与代码归属 | `src/sources`；本模块负责算法及typed adapter |
| 注册operation白名单 | `measure_science_sources`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | Native/WCS、Gaia科学源、PSF → SourceMeasurements；按stars顺序的通量与joint_noise。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；未来窗口矩阵批次；统一numerics求解与CPU回退，不复制PSF积分。 |

## 算法与失败合同

WCS后独立选源；窗口联合求flux/背景，位置和PSF形参来自冻结模型；位置/形状拟合归fit_psf，不在本入口暗中重新拟合。实际模板区间与Native区间进入验证求解。

frame保observation.frame；flux_numerical_lower/upper及error/observation强引用完整。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

独立窗口组并行，混叠组联合；支持和联立rank需全组判断；通量界不能拿随机variance替代。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`normalize.photometry.measurements`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：F01；已知总通量、混叠、背景相关、缺模板界和不可估通量。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，当前接口及能力以公开头/registry核对，规格与代码偏差必须明确记录。

## 算法详细条款

### 生产者的跨模块合同

measure_science_sources 必须实际构造 SourceMeasurements.flux_numerical_lower/upper：固定模板的线性幅值/背景拟合，将 Native 输入区间与 PsfEvaluation 模板区间送入同一设计矩阵的验证求解，使用 [NUMERICS](../standards/NUMERICS.md) 的矩阵逆扰动界及残差包围；自由位置/混叠参数按该处 score 根盒验证再投影到通量坐标。PSF 拟合数值参数盒也参与模板区间。支持不足、秩未证实或包围无法认证时保留名义值但明示 conditional/unconverged 与缺失区间，不能将随机方差当确定误差。实际模板证书归并到 SourceMeasurements.error 并保可重开依赖。fit_photometry 对每个源直接消费这些通量区间来包围 score，与 prediction 区间共同生成 PhotoSolution 参数盒；缺失通量界绝不按零处理。


### 可复用计算模块组合

目标组合：窗口读取 → PSF模板及包围 → numerics通量/背景/位置联立验证 → SourceMeasurements同根与误差载体。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。


## 直接开发的实施合同

本process的算法复现按[独立实施标准](../standards/ALGORITHM_CONTRACTS.md)验收。全部operation的Input/Output按[FOUNDATION_API的typed规则](../standards/FOUNDATION_API.md#当前科学操作的typed输入标准)从公开参数机械生成；跨度/provider均转强所有权，codec先register_type再登记typed hooks。bounds给最小/最大块和spill能力，plan显式接受PlanOptions，不能读取全局当前块大小。资源key/不可拆相关组/上界项见[DEVELOPMENT本process行](../standards/DEVELOPMENT.md#模块plan和工作单元表)，具体数学/停止仍由本篇拥有。

有全局拟合屏障时拆stream→ordered_accumulator→barrier region：每cohort提交R内聚合状态即可退F，不能让唯一进展cohort等待下一cohort。科学迭代以显式epoch重新准入，work不重置。取消/失败只允许内建清理继续，模块不提交新科学计算；公开输出只在所有必需任务/写入/校验结束后交付。

实施交付包含所属模块plan/kernel/adapter/codec、独立小例/负例及真实service资源/取消证据，逐项门槛见[DEVELOPMENT](../standards/DEVELOPMENT.md)。完整编排代码可参照[校准实施例](../standards/CALIBRATION_WALKTHROUGH.md)，不得复制其中像元公式到其它process。GPU只预留同一批次/内存位置/完成事件边界，不实现后端。

## 可独立实施的算法合同

### 估计量、窗口和参数来源

固定已拟合PSF参数/连续源位置及真实采样链，数据模型为本篇的像元模型`d=H F+Zβ+ε`，随机量与单位遵[观测基础](../../../science/foundation/OBSERVATION_COORDINATES.md)。F为请求stars顺序的总积分通量，H每列来自唯一evaluate_psf，不以峰值或窗口和冒充F。PsfRequest.window_radius_pixel给以detector单位的初始半窗，不因低预算缩小科学窗口。tail_absolute_flux的单位与当前观测积分量相同（ADU或具名L），须以已包围max|F|乘尾质量比较；不是无量纲tail_mass_upper。首次无有限F界只能试窗/拟合再按倍增半窗验证，不能直接把该参数当概率阈值。

没有背景阶数Request字段，首期每组固定一个detector面连续常量密度β。原生列`Z0_j=∫P_j r_j(u) d²u`，r_j包含已知PRF/样本乘性响应；再经同H实际采样链G/Dα得到Z。β单位为该前置模型的积分量/detector pixel²，变换后的列不能重新归一。只有单位面积、均匀响应、未变换样本才有Z=1；一般bin/gain后不再成立。不由实现自行选择平面或剪裁背景。

先按source_id建立Gaia观测历元位置→detector位置映射，以实际photosite/footprint相交选行，mask来自DQ与有效值。初始窗口不足尾界时按PSF解析尾界扩到满足请求，否则给明确尾项/未达标；窗口外邻源不能当零。以窗口共享实际行或必需邻源的关系建无向图，取确定连通分量，组内按source_id、行按native index排序。对完整数据协方差非块对角的组，必须合并或保共享latent的全局稀疏耦合；不能只因窗口不接触便忽略噪声相关。空支持、同source重复和身份冲突在构造矩阵前拒绝。

### 唯一执行步骤

```text
冻结stars/PSF/WCS/Native与请求身份；建立窗口图及相关噪声依赖
逐组取得完整可用行；evaluate_psf(output_rows)一次取得H及区间
构造D=[H,Z]；C从同一Native噪声图限制到这些行，不重造独立根
已知SPD/PSD C：NUMERICS先处理精确约束，再白化QR/SVD
完整C未知：名义点使用显式单位工作权，标conditional，随机误差仍unknown
解可估子空间的(F,β)，检查每个请求F泛函是否消灭null(D)
固定同一个D/工作权，用输入/模板区间验证线性解和通量坐标
组装F对像元及PSF/WCS参数的完整influence；同根先合并
按原stars顺序封存StarFlux、joint_noise、数值端点和训练像元依赖
```

负F合法，不clip后重新解；对PSD C的零空间必须保无噪声约束。固定工作权的conditional名义估计与“已知最佳方差”是不同结论。若一组某请求F不可估，不能用伪逆对角给有限variance。当前输出缺逐源无效载体时，整组/本请求以not_identifiable无可消费的完整SourceMeasurements返回；只有已有明确状态/有效域载体时才允许partial，不填零星通量来维持shape。

本入口固定θ，但不把θ的不确定性丢掉：实际模板区间含PSF参数盒/WCS数值界；随机influence用全导数`dF/dd_total = ∂F/∂d + (∂F/∂θ)(dθ/dd_total)`，位置同理。PSF训练与当前像元共根时合并一次，不能将两个独立variance相加。若要在测量中再拟合位置/形状，须先新增明确请求/score并另行验收；当前不隐式开启。

### 误差、资源和停止

线性解的认证使用NUMERICS已固定的可估/gauge子空间扰动界；H、d、白化/解算残差均进入界，随机C不当作输入数值半径。缺模板/原语证书则可给固定名义模型的conditional结果，但不能声称包围理想PSF重新拟合。F区间传给photometry；SourceMeasurements.psf/observation/error/training_pixel_ids和joint_noise都随产品闭包保留。

一组m行、s星、q=s+1参数：预算至少计读取行/PSF区间、D的m×q与端点、QR的m×q、q²因子、共享根/参数影响及numerics workspace。C可用factor/action时不物化m²；不能取得合法可行C求解分支就具名失败。组内QR迭代和每次精度升级消耗同一work，取消点在读/模板块/求解段后。整组不可拆成独立逐星；全局相关分量过大只能稀疏耦合/spill或resource_exhausted。

### 固定独立反例

M-MEASURE：三像元`H=[[1/2,0],[0,1/2],[1/2,1/2]]`、Z=1、C=I、d=[5,7,10]，真值F=[6,10]、β=2，通量covariance为[[8,4],[4,8]]。逐星窗口和/忽略共同背景会失败。另加共同常量根τ²11ᵀ，其对这两个F的贡献严格为0，检验交叉项而非另加背景variance。

M-RANK：两列H相同，只F1+F2可估，必须报告not_identifiable而非各半并宣称已知。M-BACKGROUND：H=[1/2,0,1/2]、Z=[1,2,1]、F=6、β=2，d=[5,4,5]；错误将采样后背景设为1会给F=2。M-BOUND：固定D而d3区间扩大±δ，两F均须扩大±2δ；M-ROOT：PSF与像元同根不能双计；M-GROUP：不相交窗口但共享latent，独立解与联合解对照必须暴露差别。全部再走typed/wire、低H、取消和新进程重开；本轮未执行生产测试。
