# coadd.select：资格与单步排异

状态：本篇规定目标算法及实施合同；已有部分Runtime和科学模块实现，具体完成/验收只看[统一状态](../../../audit/IMPLEMENTATION-STATUS.md)，不能由本文标题推断全部未实现或已通过。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `coadd.select` |
| 所属模块与代码归属 | `src/coadd`；本模块负责算法及typed adapter |
| 注册operation白名单 | `select_samples`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | FramesProvider/CoaddRequest → Selection及逐位置原因和完整样本分母。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；仅明确稳定的批次可扩展；不把不同浮点分支视为无关差异。 |

## 算法与失败合同

DQ先行；single_max_loo仅共同均值/外部固定可信Γ且N≥4，一轮只拒最大者。

选择身份传weight/effective_response；mixed均值不偷送共同均值LOO。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

sky冻结后按leaf独立并行；阈值临界需细化/conditional，不按任务完成顺序决定分支。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`mosaic.selection.samples`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：W01；小N、单污染、并列值、阈值包围跨界及错误共同均值假设。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，当前接口及能力以公开头/registry核对，规格与代码偏差必须明确记录。

## 算法详细条款

### 公开操作语义

### 冻结sky输入与输出交接

输入FramesProvider必须由已冻结sky/P的只读模型视图构造，身份记录SkyFit、实际signal/noise/response版本，不在后台依赖“当前β”。调用方显式传递该provider；本process不再拟合sky。Selection与WeightSolution输出记录同一provider/sky/request/Γ/selection依赖身份，供后续integrate按typed输入验证，不能仅靠shape相同复用。


### 可复用计算模块组合

目标组合：冻结sky provider读块 → DQ/共同均值与Γ能力验证 → 单轮LOO模块 → Selection；稳定科学顺序封存。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。


## 直接开发的实施合同

本process的算法复现按[独立实施标准](../standards/ALGORITHM_CONTRACTS.md)验收。全部operation的Input/Output按[FOUNDATION_API的typed规则](../standards/FOUNDATION_API.md#当前科学操作的typed输入标准)从公开参数机械生成；跨度/provider均转强所有权，codec先register_type再登记typed hooks。bounds给最小/最大块和spill能力，plan显式接受PlanOptions，不能读取全局当前块大小。资源key/不可拆相关组/上界项见[DEVELOPMENT本process行](../standards/DEVELOPMENT.md#模块plan和工作单元表)，具体数学/停止仍由本篇拥有。

有全局拟合屏障时拆stream→ordered_accumulator→barrier region：每cohort提交R内聚合状态即可退F，不能让唯一进展cohort等待下一cohort。科学迭代以显式epoch重新准入，work不重置。取消/失败只允许内建清理继续，模块不提交新科学计算；公开输出只在所有必需任务/写入/校验结束后交付。

实施交付包含所属模块plan/kernel/adapter/codec、独立小例/负例及真实service资源/取消证据，逐项门槛见[DEVELOPMENT](../standards/DEVELOPMENT.md)。完整编排代码可参照[校准实施例](../standards/CALIBRATION_WALKTHROUGH.md)，不得复制其中像元公式到其它process。GPU只预留同一批次/内存位置/完成事件边界，不实现后端。

## 可独立实施的算法合同

### 资格、跳过和单轮LOO的固定程序

先核provider已绑定给定sky/P和实际响应版本，按leaf、frame ID稳定顺序读取完整候选。eligibility由support正下界、有限signal、DQ和请求Comparability组成；bad/saturated/nonlinear/missing不合格，repaired只有合法influence才合格。负signal或真实0本身不排除，噪声unknown不直接删除合法signal。

```text
accepted := eligibility；冻结候选数N（只计合格帧）
若policy=dq_only：封存，无统计排异
若N<4，或无共同均值证明，或无外部固定可信完整Γ：skip，保持全部合格帧
否则对每k用同Γ的删行子块GLS求t_-k，按统计基础式10求μ/v_res/q
  v下界>0才标准化；精确v=0且精确残差0无需拒
  精确约束与非零残差冲突invalid_model；跨0则selection_unresolved
计算所有q后只比较一次，最多拒绝一个最大|q|超过τ的候选
τ=Φ^-1(1−α/(2N))，并列按frame ID；不在拒绝后重算或再clip
封存原N、mask、原因/条件与Γ/sky/request身份
```

| 位置状态/原因 | accepted/后续处理 |
|---|---|
| 无合格候选 | 全0，no_support，后续无权行 |
| dq_only或统计前置不足/skipped | accepted=eligibility，记录small_n/unknown_covariance/not_comparable等原因；不冒称已检出无异常 |
| 全部q严格低于阈值 | 全保留，LOO_checked_no_rejection |
| 唯一最大者严格超过τ且排序稳定 | 只该帧0，其余保留；保数据选择条件 |
| q/阈值/最大者的区间跨决策界 | 先细化/升级；仍未决返回selection_unresolved，不以中点认证；只有显式允许的固定分支才conditional |

这些是既有reasons/Condition的语义标签，编码映射在唯一公共声明/codec落实，不新增第二份位值表。α来自RejectionPolicy且0<α<1，dq_only不需Γ。已选择后variance仍是冻结mask公式，不能沿用选择前Gaussian精确coverage。

### 资源与独立反例

F计完整K signal/DQ、Γ的K²、每次LOO solve的workspace和有界分数；逐k复用解工作区，不同时存K个K²矩阵。S-SKIP的N=1..3/unknown Γ必须保合法样本；S-LOO令Γ=I、N=5、z=[0,0,0,0,20]、α=.001，τ约3.719，坏帧|q|约17.889，其余约4.472；正确只拒第5帧，错误并行拒全部超阈者会拒5帧，故例子有判别力。整体z+10，mask必须相同；相等候选按ID且只拒一个；阈值包围跨界不得报告certified。runtime顺序/块大小不改变mask。
