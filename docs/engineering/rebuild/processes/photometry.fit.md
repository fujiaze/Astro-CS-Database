# photometry.fit：联合光度标度拟合

状态：本篇规定目标算法及实施合同；已有部分Runtime和科学模块实现，具体完成/验收只看[统一状态](../../../audit/IMPLEMENTATION-STATUS.md)，不能由本文标题推断全部未实现或已通过。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `photometry.fit` |
| 所属模块与代码归属 | `src/photometry`；本模块负责算法及typed adapter |
| 注册operation白名单 | `fit_photometry`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | SourceMeasurements、CatalogueStar、SpectralPrediction、PhotometryRequest → PhotoSolution。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；矩阵装配/批量局部消元可未来加速；rank/停止/精度升级共用CPU控制器。 |

## 算法与失败合同

joint_flux_gaussian共享latent；按source消元再全局Schur/QR，空间基训练均值零；不另加错误gauge。

按frames/source_id/response显式join；通量/谱/模板区间进入score与参数盒，同数据influence保留。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

每星局部计算可并行，全局θ冻结为屏障；大系统流式/稀疏，不拆成独立帧忽略共享根。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`normalize.photometry.solution`、`normalize.photometry.fit`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：F01；共享Gaia latent、负通量、秩亏、驻点零步与缺确定界。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，当前接口及能力以公开头/registry核对，规格与代码偏差必须明确记录。

## 算法详细条款

### 数值与模型细则

photometry 采用科学分册 flux-domain joint Gaussian latent。θ=[log a,空间项]，空间基按训练权均值零；每星 latent 与 Gaia 系数测量共用一个根。初始化 log a 为正通量比的中位数（仅初值，无正比则0），latent 为 Gaia 观测。GN 每步按 source 消元谱 nuisance、全局 Schur/QR 解 θ，再回代；固定联合 covariance，不能每步偷换为未包含 logdet 的 marginal likelihood。Armijo 与迭代停止同 PSF；Huber 默认关，显式开启 δ=1.345 时仅给冻结权 influence 与聚类 bootstrap 验证。低计数原始 Poisson likelihood 首期 unsupported。星覆盖不足或谱/位置混淆时给 nullspace，不 ridge。α=exp(−hθ) 应用每个原生像元及模板，application_id 已存在立即 identity_conflict；训练像元→θ→x 的链式 Jacobian 与直接像元根合并一次。

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| fit_photometry | measurements按PhotometryRequest.frames显式顺序，CatalogueStar按source_id与测量join，prediction按response一一对应；未观测latent不造数据。rank缺失返回not_identifiable带nullspace；GN失败nonconverged，不应用任意scale |


### 请求顺序与共享latent

PhotometryRequest.frames给唯一明确顺序K，与responses/predictions及measurements长度相同；measurements[k].frame=frames[k]。每帧同Gaia source_id通过CatalogueStar release/calibration共享latent；star数据是各测量源ID的并集且唯一。prediction[k].basis必须是这些星的共同xp_basis，responses[k]同prediction response。PhotoSolution.frames/theta/response顺序沿用请求；theta shape[K,P]。Gaia绝对latent测量固定光度尺度，不额外固定任一帧a=1；空间项均值零是唯一标量/空间分离约束。旧reference_frames字段已删除，不授权编码Agent自创参考帧光度gauge。allow_relative_uncalibrated只在完整拟合不可用时保原生具名相对信号，不伪造绝对尺度。


### 可复用计算模块组合

目标组合：按星读取/验证源表与谱预测 → 并行局部消元 → 全局Schur/QR屏障 → score/参数盒验证 → PhotoSolution。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。


## 直接开发的实施合同

本process的算法复现按[独立实施标准](../standards/ALGORITHM_CONTRACTS.md)验收。全部operation的Input/Output按[FOUNDATION_API的typed规则](../standards/FOUNDATION_API.md#当前科学操作的typed输入标准)从公开参数机械生成；跨度/provider均转强所有权，codec先register_type再登记typed hooks。bounds给最小/最大块和spill能力，plan显式接受PlanOptions，不能读取全局当前块大小。资源key/不可拆相关组/上界项见[DEVELOPMENT本process行](../standards/DEVELOPMENT.md#模块plan和工作单元表)，具体数学/停止仍由本篇拥有。

有全局拟合屏障时拆stream→ordered_accumulator→barrier region：每cohort提交R内聚合状态即可退F，不能让唯一进展cohort等待下一cohort。科学迭代以显式epoch重新准入，work不重置。取消/失败只允许内建清理继续，模块不提交新科学计算；公开输出只在所有必需任务/写入/校验结束后交付。

实施交付包含所属模块plan/kernel/adapter/codec、独立小例/负例及真实service资源/取消证据，逐项门槛见[DEVELOPMENT](../standards/DEVELOPMENT.md)。完整编排代码可参照[校准实施例](../standards/CALIBRATION_WALKTHROUGH.md)，不得复制其中像元公式到其它process。GPU只预留同一批次/内存位置/完成事件边界，不实现后端。

## 可独立实施的算法合同

### 固定联合目标、score与分块解

估计量唯一采用[Gaia基础](../../../science/foundation/GAIA_RESPONSE.md)的joint_flux_gaussian：数据d=[各帧counts,每source一次Gaia系数]，m_counts=exp(hᵀθ_k)v_kᵀc_s，m_Gaia=c_s；固定联合协方差C与PSD约束。每source按release/calibration/source_id只一个latent，未观测帧不造残差；Gaia测量只计一次。空间项使用已定训练加权均值零basis，θ0为log a，不另设参考帧a=1。

SPD或已正确消去精确约束的坐标中，令r=d−m、W为合法白化后的精度作用，则`Φ=1/2 rᵀWr`、`g=−JᵀWr`、真实Hessian=`JᵀWJ−Σ_i(Wr)_i ∇²m_i`。counts行对θ_j的导数=h_j m，对c的导数=exp(hᵀθ)v；二阶θθ为h hᵀm，θc为h exp(hᵀθ)v。这些公式用于真实score根证书，不以冻结IRLS/GN矩阵冒充精确Hessian。

PSD存在非线性无噪声方程时，令w=(θ,c)、a(w)=U0ᵀ(m(w)−d)=0，正谱目标为Φ+。若不能经已证明的可行参数化消去a，统一解KKT根`[∇Φ+ + Aᵀλ, a]=0`，A=∂a/∂w；Jacobian为`[[H_L,Aᵀ],[A,0]]`，H_L含Σλ_i∇²a_i。按NUMERICS的约束根路线验证可行性和约束切空间上的局部极小；不能简单以C+替代W后丢掉a。根证书和influence对此完整KKT系统求导，再选θ分量。

按source顺序流式组局部c块和全局θ贡献，QR/Schur消元后提交R内有序累加器，释放该cohort F；全部source EOF后barrier解全局θ再回代。只有联合C确有对应块结构才可独立消元；跨source共享校准根须作为全局latent/稀疏耦合保留，不能删去交叉块。SPD/无约束分支消去最优线性c时profile score为gθ，Hprofile用真实H的Schur补。PSD约束profile则使用Lagrangian梯度（含λᵀaθ）及完整KKT的Schur消元；固定θ可能没有可行c时须保联合约束KKT，不强行单独求c。本目标没有marginal logdet，也不能把参数依赖残差covariance更新当同一目标。

### 根证书、influence与失败

初始化/Armijo/零步收敛按既定标准，外层次数与梯度/步来自PhotometryRequest。候选解在固定可估子空间以NUMERICS Krawczyk及真实Hessian验证局部驻点/极小，再投影θ包围；约束分支验证完整KKT根及切空间H_L。不能给非凸全局最优承诺。counts、谱v、PSF/测量的区间均进入score；未知物理bias仍unknown。无约束influence按`−H^-1 ∂g/∂d`，KKT分支按完整根Jacobian求导，与共同noise根合并；fit使用的像元与后续apply_scale的直接像元项必须合并一次。

H可估性不足返回not_identifiable带nullspace，不给任意scale；未达梯度或盒验证失败分别nonconverged/precision_exhausted及真实条件状态。Huber只在显式请求时使用既定δ/条件权influence和聚类bootstrap，不能沿用Gaussian覆盖定理；原始Poisson本入口unsupported。

### 资源与独立例

每星谱维d、局部观测k、全局参数p：F计(k+d)×(d+p)设计、d²消元和区间/解算workspace；R计p²或实际稀疏全局系统/共享根，外存贡献不能漏全局工作集。P-LATENT：两帧同星counts=[2,3]、Gaia c=1、v=1，精确名义a=[2,3]；设Gaia方差σ²且counts无噪声，log a的交叉协方差=σ²（在c=1的线性化），不能因帧不同归零。P-NUMERIC用counts=100的[100,100]与[99,101]区间，在同固定谱q=100下分别包a=1和a∈[.99,1.01]；P-RANK空间列共线不给满秩，P-SCORE非零残差时真实H与GN不相等，需独立导数对照。P-PSD-PROFILE令Φ=(c−1)²/2、精确约束e^θ c=d，θ=0、d=2时profile梯度=−2，普通偏导gθ=0不能代替它；KKT乘子项必须给出正确梯度。
