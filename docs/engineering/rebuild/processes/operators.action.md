# operators.action：线性action

状态：目标process文档；对应科学函数可已有部分实现，标准注册/生命周期/探针尚未据本文落地。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

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

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，文档中的角色名不表示头文件现已提供新接口。

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
