# information.reference：参考通量信息量

状态：目标process文档；对应科学函数可已有部分实现，标准注册/生命周期/探针尚未据本文落地。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `information.reference` |
| 所属模块与代码归属 | `src/information`；本模块负责算法及typed adapter |
| 注册operation白名单 | `estimate_flux_information`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | InformationRequest的h/Z/C或Normalize展开的ReferenceInformationRequest → Information/V_F。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；模板/因子action复用后端；求解仍须合法包围，不能用kernel耗时宣称整体加速。 |

## 算法与失败合同

同R投影模板与背景，按声明条件求可估通量信息；actual_scene/reference/blank不互换。

精确显式h分支与template_evaluation列绑定分支区别；V_F/V_B/V_ctrl按名称和条件保存。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

按查询位置/源列分块复用evaluate_psf；依赖触及行，不为小查询生成完整模板库。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`normalize.information.reference_flux`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：F01/V01；解析设计矩阵、背景退化、缺模板界、unknown C与参考模型错配。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，文档中的角色名不表示头文件现已提供新接口。

## 算法详细条款

### 生产者的跨模块合同

fit_psf、measure、photometry和information必须消费PsfEvaluation实际包围。InformationRequest.template_evaluation可空仅表示caller直接指定精确矩阵h（以所解析数字为定义）；非空时template_column指对应列，h必须为该列按output_index重新索引的名义值，lower/upper及error用于求解扰动/秩稳定性。Normalize内部构造这种绑定，不把PSF积分误差遗忘在临时结果。区间不足使信息量误差未达标时继续求积/升级，或unconverged/conditional，不仅传播随机covariance。

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| estimate_flux_information | h[N]单位1/sr或归一flux模板、background:N×P、C、具名reference；输出flux_variance+rank，h被背景吸收not_identifiable，无有限V；PSD精确约束可给真实V=0 |


### Normalize参考请求展开

Normalize.information使用ReferenceInformationRequest，不要求用户预先知道尚未生成的R/h/C。positions非空按指定ICRS位置，空时流式枚举control_order叶中心且在该帧足迹支持内；control_order≤drizzle.order。工作流将位置逆到detector，用evaluate_psf生成原生模板、实际同R投影h，background Basis在同原生样本积分后经同R得到Z，再调用estimate_flux_information。reference.condition=actual_scene且native_noise为空时使用该帧真实noise图；named_reference/blank_background必须提供同native index的native_noise和明确条件（可用已知物理均值/电子gain/读出等构造），无模型则precision unavailable，不把actual photon variance改名为参考variance。提供的NoiseModel完整经G/α/R影响一次，C仍unknown时V_F unavailable，signal不受影响。用户若只需直接矩阵科学调用，InformationRequest仍接受显式h/Z/C，两个请求角色不混淆。


### 可复用计算模块组合

目标组合：请求位置/模板列 → 唯一PSF+R/背景action → uncertainty C能力 → numerics可估泛函解及证书。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。


## 直接开发的实施合同

本process全部operation的Input/Output按[FOUNDATION_API的typed规则](../standards/FOUNDATION_API.md#当前科学操作的typed输入标准)从公开参数机械生成；跨度/provider均转强所有权，codec先register_type再登记typed hooks。bounds给最小/最大块和spill能力，plan显式接受PlanOptions，不能读取全局当前块大小。资源key/不可拆相关组/上界项见[DEVELOPMENT本process行](../standards/DEVELOPMENT.md#模块plan和工作单元表)，具体数学/停止仍由本篇拥有。

有全局拟合屏障时拆stream→ordered_accumulator→barrier region：每cohort提交R内聚合状态即可退F，不能让唯一进展cohort等待下一cohort。科学迭代以显式epoch重新准入，work不重置。取消/失败只允许内建清理继续，模块不提交新科学计算；公开输出只在所有必需任务/写入/校验结束后交付。

实施交付包含所属模块plan/kernel/adapter/codec、独立小例/负例及真实service资源/取消证据，逐项门槛见[DEVELOPMENT](../standards/DEVELOPMENT.md)。完整编排代码可参照[校准实施例](../standards/CALIBRATION_WALKTHROUGH.md)，不得复制其中像元公式到其它process。GPU只预留同一批次/内存位置/完成事件边界，不实现后端。
