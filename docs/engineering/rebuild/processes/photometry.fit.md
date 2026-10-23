# photometry.fit：联合光度标度拟合

状态：目标process文档；对应科学函数可已有部分实现，标准注册/生命周期/探针尚未据本文落地。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

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

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，文档中的角色名不表示头文件现已提供新接口。

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
