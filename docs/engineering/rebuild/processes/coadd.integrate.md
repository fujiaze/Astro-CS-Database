# coadd.integrate：最终信号合成

状态：本篇规定目标算法及实施合同；已有部分Runtime和科学模块实现，具体完成/验收只看[统一状态](../../../audit/IMPLEMENTATION-STATUS.md)，不能由本文标题推断全部未实现或已通过。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `coadd.integrate` |
| 所属模块与代码归属 | `src/coadd`；本模块负责算法及typed adapter |
| 注册operation白名单 | `coadd`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | 同一冻结sky版本的FramesProvider、SkyFit、Selection、WeightSolution及CoaddRequest → CoaddResult/唯一SignalEstimate。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；归约/因子action可未来后端，产品写入仍基建统一。 |

## 算法与失败合同

只消费已封存的Selection/WeightSolution，验证输入frame/source顺序、支持/单位/index、SkyFit/Γ/selection/request hash和版本完全匹配。不得再次执行select/weights或从可变会话取“当前权”；实际t/c/核组合保存effective_response。

保存完整共享根、支持和selection；不发布额外variance/weight作为第二科学signal。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

按目标块流式归约，唯一数据依赖链明确；scratch/Γ/输出同时准入，固定源顺序。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`mosaic.integration.signal`、`mosaic.integration.block`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：U01/W01/X01；解析混合核、共享噪声、缺帧与单/多线程结果一致。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，当前接口及能力以公开头/registry核对，规格与代码偏差必须明确记录。

## 算法详细条款

本篇上述合同与科学基础定义该过程；共享原语调用不复制到本模块。


### 冻结背景后的整合条件

工作流先冻结全分量sky解，再依次select→weights→integrate；本process只执行最后归约，消费前两阶段对象。Γ 从共同 root 联合查询，不各帧独立读对角替代。BLUE/PSD与非负QP见[NUMERICS](../standards/NUMERICS.md)；不同 K 的 mixed_observation 只能 equal 或显式 nonnegative_qp，后者是该混合样本方差目标，不称共同量 BLUE。LOO 前置失败逐位置 skip，N<4不猜坏帧。effective_response 保存实际 t,c,核组合及条件，不强行分离滤镜。


### 整合调用语义

| 操作 | 本模块合同 |
|---|---|
| coadd | 只消费已封存Selection与WeightSolution，不调用select/weights；provider已绑定给定sky/P，不再次apply_sky。request mixed=false需common_mean证据；mixed=true保实际核。BLUE缺C由weights阶段明确unavailable，automatic选择equal的原因随WeightSolution保留 |

### 强类型数据边与现有入口改造

Selection/WeightSolution是目标typed输入的必需强引用。当前公开声明/typed绑定须显式保留这些输入；历史未接入版本只作旧快照，不能以保留旧签名为由隐藏会话或重复求解。固定mosaic工作流按select→weights→integrate各执行一次；只有用户单独请求新的选择/权版本才重算。没有对应权或身份不匹配在validate拒绝，不悄悄补算；独立driver必须能只给已验证的三个阶段对象完成整合。

新增反例：总work只足一轮选择/求权/归约，预先运行前两阶段后integrate仍应完成；传另一SkyFit/selection/Γ的权必须失败，不能重算绕过身份冲突。


### 可复用计算模块组合

目标组合：已封存Selection/WeightSolution读取 → operators/固定归约 → signal与模型pipe → 封存；不重新选择或求权。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。


## 直接开发的实施合同

本process的算法复现按[独立实施标准](../standards/ALGORITHM_CONTRACTS.md)验收。全部operation的Input/Output按[FOUNDATION_API的typed规则](../standards/FOUNDATION_API.md#当前科学操作的typed输入标准)从公开参数机械生成；跨度/provider均转强所有权，codec先register_type再登记typed hooks。bounds给最小/最大块和spill能力，plan显式接受PlanOptions，不能读取全局当前块大小。资源key/不可拆相关组/上界项见[DEVELOPMENT本process行](../standards/DEVELOPMENT.md#模块plan和工作单元表)，具体数学/停止仍由本篇拥有。

有全局拟合屏障时拆stream→ordered_accumulator→barrier region：每cohort提交R内聚合状态即可退F，不能让唯一进展cohort等待下一cohort。科学迭代以显式epoch重新准入，work不重置。取消/失败只允许内建清理继续，模块不提交新科学计算；公开输出只在所有必需任务/写入/校验结束后交付。

实施交付包含所属模块plan/kernel/adapter/codec、独立小例/负例及真实service资源/取消证据，逐项门槛见[DEVELOPMENT](../standards/DEVELOPMENT.md)。完整编排代码可参照[校准实施例](../standards/CALIBRATION_WALKTHROUGH.md)，不得复制其中像元公式到其它process。GPU只预留同一批次/内存位置/完成事件边界，不实现后端。

## 可独立实施的算法合同

### 强类型输入与唯一归约程序

当前typed CoaddInput必须显式带frames、sky、selection、weights、request；coadd公开函数/头中的历史接口是否已改，以当前公开声明核对，不在本文保留旧签名契约。validate逐项锁provider/sky/selection/weights的依赖身份、leaf/frame index和actual Γ；非合格权为0、active sum=1的数值包围须含1。身份不合直接失败，不重跑选择/求权或apply_sky。

按leaf和稳定frame ID读取已校正z，B=Σt_k z_k；负权/负z合法。数值界用signed t/z区间全部乘积和归约舍入；随机图为Σt_k L_k的同根合并，不各帧variance独立相加。权来自数据/Selection时明确冻结选择条件，物理有效核包含真实t、reference因子和已用spatial action，不把已经吸收的系数再乘。no_support行不构假signal/权；unknown C时合法B可保，但noise仍unknown。

### 资源、失败与独立例

F计当前leaf完整K样本/权/端点和根块，R为最终模型/recipe，分块输出不物化全图×全部帧。integrate实际work只计读取/归约/模型构造，不重复消耗select/weights预算；输入的科学分支没有完成则不能创建complete结果。C-INTEGRATE：z=[2,5]、t=[2/3,1/3]得3，Γ=diag(1,4)得V=8/9；若两帧共享variance4的同根，t归一后共享项仍4，不能被平均缩小。C-WORK给恰够一轮的总work，前三阶段各执行一次能完成；C-ID给另一SkyFit/selection的权，必须拒绝而非重算。输出重开保response及选择条件。
