# coadd.weights：合成权求解

状态：本篇规定目标算法及实施合同；已有部分Runtime和科学模块实现，具体完成/验收只看[统一状态](../../../audit/IMPLEMENTATION-STATUS.md)，不能由本文标题推断全部未实现或已通过。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

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

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，当前接口及能力以公开头/registry核对，规格与代码偏差必须明确记录。

## 算法详细条款

本篇上述合同与科学基础定义该过程；共享原语调用不复制到本模块。

### 冻结sky输入与输出交接

输入FramesProvider必须由已冻结sky/P的只读模型视图构造，身份记录SkyFit、实际signal/noise/response版本，不在后台依赖“当前β”。调用方显式传递该provider；本process不再拟合sky。Selection与WeightSolution输出记录同一provider/sky/request/Γ/selection依赖身份，供后续integrate按typed输入验证，不能仅靠shape相同复用。


### 可复用计算模块组合

目标组合：Selection/provider块读取 → uncertainty Γ → numerics BLUE/PSD/QP → WeightSolution及数值/选择条件。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。


## 直接开发的实施合同

本process的算法复现按[独立实施标准](../standards/ALGORITHM_CONTRACTS.md)验收。全部operation的Input/Output按[FOUNDATION_API的typed规则](../standards/FOUNDATION_API.md#当前科学操作的typed输入标准)从公开参数机械生成；跨度/provider均转强所有权，codec先register_type再登记typed hooks。bounds给最小/最大块和spill能力，plan显式接受PlanOptions，不能读取全局当前块大小。资源key/不可拆相关组/上界项见[DEVELOPMENT本process行](../standards/DEVELOPMENT.md#模块plan和工作单元表)，具体数学/停止仍由本篇拥有。

有全局拟合屏障时拆stream→ordered_accumulator→barrier region：每cohort提交R内聚合状态即可退F，不能让唯一进展cohort等待下一cohort。科学迭代以显式epoch重新准入，work不重置。取消/失败只允许内建清理继续，模块不提交新科学计算；公开输出只在所有必需任务/写入/校验结束后交付。

实施交付包含所属模块plan/kernel/adapter/codec、独立小例/负例及真实service资源/取消证据，逐项门槛见[DEVELOPMENT](../standards/DEVELOPMENT.md)。完整编排代码可参照[校准实施例](../standards/CALIBRATION_WALKTHROUGH.md)，不得复制其中像元公式到其它process。GPU只预留同一批次/内存位置/完成事件边界，不实现后端。

## 可独立实施的算法合同

### 策略入口与误差状态

先validate Selection与provider/sky/request/Γ身份相合，取accepted子域，inactive权严格0。K=0无权行，K=1权1但variance仍可unknown。equal直接1/K不要求C；automatic仅在共同均值证据及完整已知Γ能力存在时选择BLUE，否则equal并保存原因。显式blue缺条件为unavailable/unsupported，不降级；mixed_observation不能声称共同量BLUE。显式nonnegative_qp可优化声明的混合样本方差，保其实际effective kernel。

BLUE在SPD解Γq=1并认证正分母；PSD按NUMERICS先找归一零方差解，再在range求解，不能对Γ加地板。QP沿NUMERICS既定active-set/Bland和最小范数多解规则，认证sum=1、非负、stationarity/complementarity及目标界。等权/BLUE/QP各有独立公开policy入口和typed/wire正负例，不能把实现的一个分支改名覆盖其余。

Γ、求解和归一数值区间都传播到权及最终泛函；active set/PSD秩/分母未决时升级或geometry_solve_unresolved/conditional，不能只用名义权报告严格方差。WeightSolution保原Selection强引用、实际policy、covariance和选择条件；数据来源影响一次，unknown不填0。

### 资源与固定独立例

F计实际K² Γ及端点、PSD/QR/QP因子、K权和有界active-set记录；超过dense_max_bytes需已有合法结构action路线或resource_exhausted。W-BLUE：Γ=diag(1,4)，t=[4/5,1/5]、V=4/5；equal为[1/2,1/2]、V=5/4。W-NEG：Γ=[[1,2],[2,5]]，BLUE=[3/2,−1/2]、V=1/2，非负QP=[1,0]、V=1；W-PSD：diag(0,1)给[1,0]且V=0。W-UNKNOWN的automatic等权并保unknown，显式blue不可成功；W-KKT故意扰权使sum或reduced gradient失败，检测器必须拒绝。所有真值由独立有理数算，不复用生产solver。
