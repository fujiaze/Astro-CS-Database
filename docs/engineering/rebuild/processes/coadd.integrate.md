# coadd.integrate：最终信号合成

状态：目标process文档；对应科学函数可已有部分实现，标准注册/生命周期/探针尚未据本文落地。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

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

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，文档中的角色名不表示头文件现已提供新接口。

## 算法详细条款

本篇上述合同与科学基础定义该过程；共享原语调用不复制到本模块。


### 冻结背景后的整合条件

先冻结全分量 sky 解，再逐 leaf 资格→可选单步 LOO→权→归约。Γ 从共同 root 联合查询，不各帧独立读对角替代。BLUE/PSD与非负QP见[NUMERICS](../standards/NUMERICS.md)；不同 K 的 mixed_observation 只能 equal 或显式 nonnegative_qp，后者是该混合样本方差目标，不称共同量 BLUE。LOO 前置失败逐位置 skip，N<4不猜坏帧。effective_response 保存实际 t,c,核组合及条件，不强行分离滤镜。


### 整合调用语义

| 操作 | 本模块合同 |
|---|---|
| coadd | 只消费已封存Selection与WeightSolution，不调用select/weights；provider已绑定给定sky/P，不再次apply_sky。request mixed=false需common_mean证据；mixed=true保实际核。BLUE缺C由weights阶段明确unavailable，automatic选择equal的原因随WeightSolution保留 |

### 强类型数据边与现有入口改造

Selection/WeightSolution是目标typed输入的必需强引用。现有coadd free函数尚未接受这些输入，后续应修唯一声明及核心实现，不能以保留旧签名为由隐藏会话或重复求解。固定mosaic工作流按select→weights→integrate各执行一次；只有用户单独请求新的选择/权版本才重算。没有对应权或身份不匹配在validate拒绝，不悄悄补算；独立driver必须能只给已验证的三个阶段对象完成整合。

新增反例：总work只足一轮选择/求权/归约，预先运行前两阶段后integrate仍应完成；传另一SkyFit/selection/Γ的权必须失败，不能重算绕过身份冲突。


### 可复用计算模块组合

目标组合：已封存Selection/WeightSolution读取 → operators/固定归约 → signal与模型pipe → 封存；不重新选择或求权。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。
