# operators.action：线性action

状态：本篇规定目标算法及实施合同；已有部分Runtime和科学模块实现，具体完成/验收只看[统一状态](../../../audit/IMPLEMENTATION-STATUS.md)，不能由本文标题推断全部未实现或已通过。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `operators.action` |
| 所属模块与代码归属 | `src/operators`；本模块负责算法及typed adapter |
| 注册operation白名单 | `make_action,apply,transpose_apply,compose,action.dependency_blocks,action.serialize_recipe,action.row_enclosure`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | 白名单recipe、children/arrays与匹配索引单位 → immutable LinearAction或结果Array。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；CSR/diagonal/fixed_reduce是主要未来后端候选；保驻留ArrayBacking链。 |

## 算法与失败合同

CSR/diagonal/compose/solve只有一份apply和真transpose；按相同源合并，再传播数值界。

domain/codomain及单位严格匹配；recipe、索引、算法误差与依赖可重开。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

目标行apply、源分区transpose；固定归约树，禁止全局FP无序atomic；输出走builder/spill。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`operators.apply`、`operators.transpose`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：U01/R01；显式矩阵、内积转置恒等式、signed系数、单位/index冲突。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，当前接口及能力以公开头/registry核对，规格与代码偏差必须明确记录。

## 算法详细条款

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| make_action | OBJECTS白名单recipe+有序children/arrays；检索对象不靠全局注册。维度/单位/索引/版本不一致invalid_model/unsupported；成功给不可变action并retain依赖 |

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| LinearAction全部方法、apply/transpose_apply/compose | DomainUnits.domain/codomain和单位严格相合；dependency_blocks返回实际源block IDs，transpose=true返回反向依赖；row_enclosure可unsupported；compose outer(inner(x))，所有误差和索引在新action保留 |

### 公开操作语义

### Action能力的登记边界

dependency_blocks、serialize_recipe、row_enclosure通过本process中具名typed binding访问（action.dependency_blocks/action.serialize_recipe/action.row_enclosure），输入action身份不可变；row_enclosure不支持时返回unsupported。action对象内部helper调用同一实现，不把metadata读取反复提交到池。实际公开方法清单以后由唯一声明/模块登记机检，不能根据“全部方法”文字启用未登记成员。


### 可复用计算模块组合

目标组合：dependency_blocks → 异步读触及块 → KernelExecutor CSR/diagonal/固定归约 → builder输出；transpose用源分区归并。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。


## 直接开发的实施合同

本process的算法复现按[独立实施标准](../standards/ALGORITHM_CONTRACTS.md)验收。全部operation的Input/Output按[FOUNDATION_API的typed规则](../standards/FOUNDATION_API.md#当前科学操作的typed输入标准)从公开参数机械生成；跨度/provider均转强所有权，codec先register_type再登记typed hooks。bounds给最小/最大块和spill能力，plan显式接受PlanOptions，不能读取全局当前块大小。资源key/不可拆相关组/上界项见[DEVELOPMENT本process行](../standards/DEVELOPMENT.md#模块plan和工作单元表)，具体数学/停止仍由本篇拥有。

有全局拟合屏障时拆stream→ordered_accumulator→barrier region：每cohort提交R内聚合状态即可退F，不能让唯一进展cohort等待下一cohort。科学迭代以显式epoch重新准入，work不重置。取消/失败只允许内建清理继续，模块不提交新科学计算；公开输出只在所有必需任务/写入/校验结束后交付。

实施交付包含所属模块plan/kernel/adapter/codec、独立小例/负例及真实service资源/取消证据，逐项门槛见[DEVELOPMENT](../standards/DEVELOPMENT.md)。完整编排代码可参照[校准实施例](../standards/CALIBRATION_WALKTHROUGH.md)，不得复制其中像元公式到其它process。GPU只预留同一批次/内存位置/完成事件边界，不实现后端。

## 可独立实施的算法合同

### 七种operation的具体程序

| operation | 执行与能力 | 资源/误差边界 |
|---|---|---|
| make_action | 解析OBJECTS白名单recipe，检查有序children/arrays和DAG无环；CSR offsets单调、末项=nnz、列域合法，同列重复按稳定顺序合并 | 校验O(rows+nnz+children)，大型清单分块；未知recipe unsupported |
| apply | identity/diagonal直接映射，CSR按行和列index固定归约，compose先inner再outer，solve按持久化系统/容差 | CSR O(nnz)，名义/输入/系数区间同传播；solve成本如实登记，不能叫O(nnz) |
| transpose_apply | CSR按原列归并散射、固定source key；compose按innerᵀouterᵀ，solve调用实际转置系统 | 需要外存时排序spool，不能只交换shape/复用正向解 |
| compose | 校验inner.codomain=outer.domain的单位/index，生成惰性recipe并保依赖 | 不强制物化矩阵；串行复用峰值需计中间向量与两个workspace |
| action.dependency_blocks | 从请求输出行反查真实输入块，compose递归去重，transpose反向；返回稳定有序集合 | solve通常全耦合，不能宣称只触及同号行；index/版本锁定 |
| action.serialize_recipe | 输出当前白名单recipe/有序依赖/hash，无地址/线程/stream/隐藏路径 | 所有children可新进程重开；缺闭包missing_dependency |
| action.row_enclosure | 在真正具备逐行系数/绝对能力时返回名义及严格包含，signed保持符号 | solve等无显式行能力可unsupported；不能用全0行替代 |

一般误差传递`|Δ(Ax)|≤|A0|e_x+E_A(|x0|+e_x)+归约舍入`，绝对action不是把signed结果取绝对值。求解action的界由NUMERICS扰动/残差给，未验证原语或秩不稳定时不认证。unknown误差不改成0；同源块/recipe共享只计一次物理lease。

### 固定反例、停止与复用

A-OPS用A=[[1,2],[0,3]]、x=[4,5]，Ax=[14,15]，对y=[7,11]有Aᵀy=[7,47]，两侧内积263；七个入口需各自typed/wire可达。duplicate COO同列须求和；signed误差用[1,-1]不能互消；compose以非对易2×2矩阵验证顺序；低H transpose必须外存完成或在开始前具名拒绝。取消在读取/归约/迭代段检查，不能无限排队；row_enclosure能力缺失要真正失败，不能只测metadata能序列化。
