# coadd.weights：合成权求解

状态：目标process文档；对应科学函数可已有部分实现，标准注册/生命周期/探针尚未据本文落地。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `coadd.weights` |
| 所属模块与代码归属 | `src/coadd`；本模块负责算法及typed adapter |
| 注册operation白名单 | `solve_weights`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | FramesProvider/Selection/CoaddRequest → WeightSolution。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；小矩阵批量可未来后端；统一numerics实现rank/停止，疑难项按批CPU。 |

## 算法与失败合同

automatic在共同量且完整C时BLUE，否则具名equal；显式非负QP按PSD约束求解，不加ridge。

Γ含shared与sky交叉项；权的数值界/选择条件随输出，不称mixed为共同量BLUE。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

按leaf/相同mask批次；局部K²和分解计预算，大K无适用结构路线则明确资源拒绝。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`mosaic.weight.coefficients`、`mosaic.weights.solve`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：W01；SPD/PSD/零方差归一解、负权/非负QP、unknown Γ和KKT负例。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，文档中的角色名不表示头文件现已提供新接口。

## 算法详细条款

本篇上述合同与科学基础定义该过程；共享原语调用不复制到本模块。

### 冻结sky输入与输出交接

输入FramesProvider必须由已冻结sky/P的只读模型视图构造，身份记录SkyFit、实际signal/noise/response版本，不在后台依赖“当前β”。调用方显式传递该provider；本process不再拟合sky。Selection与WeightSolution输出记录同一provider/sky/request/Γ/selection依赖身份，供后续integrate按typed输入验证，不能仅靠shape相同复用。


### 可复用计算模块组合

目标组合：Selection/provider块读取 → uncertainty Γ → numerics BLUE/PSD/QP → WeightSolution及数值/选择条件。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。
