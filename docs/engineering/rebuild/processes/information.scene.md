# information.scene：实际场景方差与稀疏预测

状态：本篇规定目标算法及实施合同；已有部分Runtime和科学模块实现，具体完成/验收只看[统一状态](../../../audit/IMPLEMENTATION-STATUS.md)，不能由本文标题推断全部未实现或已通过。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `information.scene` |
| 所属模块与代码归属 | `src/information`；本模块负责算法及typed adapter |
| 注册operation白名单 | `evaluate_scene_variance,build_supported_triangulation,reconstruct_scene_variance`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | FramesProvider/frame/leaves或PrecisionControls/Chart/SupportMap/queries → Variance列表/SupportedTriangulation/VariancePrediction。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；查询批次可未来扩展，精确谓词与支持判断CPU。 |

## 算法与失败合同

actual-scene优先直接action；稀疏路径用支持内Delaunay、δ/M界；域外/缺曲率界保conditional。

参考灵敏度不能充当场景variance；holes和响应不兼容不跨边插值。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

chart与halo分区，三角形整体支持判定；有界网格构建和查询批次；固定ID打破退化。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`normalize.information.scene_variance`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：V01；仿射精确、洞、共线/共圆、无M与C-infinity bump反例。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，当前接口及能力以公开头/registry核对，规格与代码偏差必须明确记录。

## 算法详细条款

### 稀疏actual-scene三角网构建和参考信息请求

build_supported_triangulation是information模块的唯一2D Delaunay实现：控点投影到给定gnomonic Chart，要求点在chart定义域且距中心≤maximum_radius_rad（首期≤5°）；用Bowyer–Watson增量插入，初始超三角覆盖全部点，按控点ID升序排列（ID为来源index身份及原始行号，不按任务序重编号），邻接walk定位与局部cavity更新。orientation/incircle用binary64过滤+GMP精确有理谓词；精确incircle=0不直接计入inside cavity，插入后按下文对角线规则legalize，不加随机joggle；去掉超三角关联项。triangle只有完全在support中且不跨hole/不兼容response才保留，边界无法判包含则不可用/待细化，不能用顶点都在支持内代替整三角包含。共线返回unavailable，重合点同身份同值合并，冲突值invalid_model。算法典型O(M log M)、最坏O(M²)，全部实际work计费，未完成网不作为已观测支持。SupportedTriangulation.vertices[M,2]保存chart坐标，triangles[T,3]u64索引，查询barycentric与curvature误差复用该唯一basis。

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| reconstruct_scene_variance | Controls.kind必须actual_scene，chart/三角/holes和Query，δ/M来自controls显式外部输入；输出查询顺序values/intervals/states。无M条件预测，外域NaN且absent，禁止制造C offdiag |


### 实际方差查询操作语义

| 操作 | 本模块合同 |
|---|---|
| evaluate_scene_variance | 按请求frame/leaves返回各leaf的实际Variance；不将reference变scene，跨frame相关仍由[uncertainty.covariance](uncertainty.covariance.md)同根构造 |


### 可复用计算模块组合

目标组合：实际C/action泛函查询，或控点→精确谓词三角网→支持内查询map；分别保actual/reference/conditional身份。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。


## 直接开发的实施合同

本process的算法复现按[独立实施标准](../standards/ALGORITHM_CONTRACTS.md)验收。全部operation的Input/Output按[FOUNDATION_API的typed规则](../standards/FOUNDATION_API.md#当前科学操作的typed输入标准)从公开参数机械生成；跨度/provider均转强所有权，codec先register_type再登记typed hooks。bounds给最小/最大块和spill能力，plan显式接受PlanOptions，不能读取全局当前块大小。资源key/不可拆相关组/上界项见[DEVELOPMENT本process行](../standards/DEVELOPMENT.md#模块plan和工作单元表)，具体数学/停止仍由本篇拥有。

有全局拟合屏障时拆stream→ordered_accumulator→barrier region：每cohort提交R内聚合状态即可退F，不能让唯一进展cohort等待下一cohort。科学迭代以显式epoch重新准入，work不重置。取消/失败只允许内建清理继续，模块不提交新科学计算；公开输出只在所有必需任务/写入/校验结束后交付。

实施交付包含所属模块plan/kernel/adapter/codec、独立小例/负例及真实service资源/取消证据，逐项门槛见[DEVELOPMENT](../standards/DEVELOPMENT.md)。完整编排代码可参照[校准实施例](../standards/CALIBRATION_WALKTHROUGH.md)，不得复制其中像元公式到其它process。GPU只预留同一批次/内存位置/完成事件边界，不实现后端。

## 可独立实施的算法合同

### 真实方差与支持查询的执行序列

evaluate_scene_variance对具名leaf把原生根经实际估计量action回溯，使用uncertainty计算V_B；C unknown则unknown，不拿reference I_F替代。直接查询允许known V=0。稀疏重建的Controls.kind必须actual_scene且同estimator/response/条件，只对已证明严格正V的完整输入控制集定义u=log V；δ与M是外部可信输入，不能从同一控点拟合后自证。

构网/log重建的控制集含精确V=0时返回domain（log_variance_zero），负方差证据返回invalid_covariance，unknown控制返回unavailable，名义正但数值区间跨0时先细化/升级，仍未决则precision_exhausted或显式conditional无严格区间。不得加epsilon或悄悄drop控点再宣称同一输入支持完整；调用方可显式分开零方差域，直接查询仍返回known0。上述reason沿既有Issue传递，不新造Code枚举。

网格按本篇Bowyer–Watson/精确谓词构造，再对每个triangle检查其完整闭域是否在support内、是否穿hole或不兼容response。检查方法固定为support Boolean包含：polygon/primitives用整三角边/内部包含与相交判定，dyadic支持用覆盖triangle的树cell分类；一块无法判inside则该triangle不可认证，先细化或删除。只删不完整triangle，不创建未经约束的跨洞新边。halo控点参与全局相同ID的局部三角网，边界tie必须一致。

查询先按chart domain和support定位；落在triangle边/顶点按最小triangle ID决定，外域不nearest-fill。barycentric向外求界，严格非负且sum包含1；名义值用log线性插值，δ/M按统计基础式4a求Δ再exp。没有M只给conditional名义预测及无严格区间；无support给absent/NaN的已有有效位，不在JSON伪造非有限数字。结果不生成新跨叶covariance。

共圆tie固定为凸四边形两条对角线各取排序后的端点ID对，选字典较小者。每轮按无向edge ID排序扫描，发现非法Delaunay edge，或incircle=0且当前对角线较大，就flip并重启扫描，直到无可翻边；边界/非凸四边形不翻。输出三角形保持CCW、旋转到最小顶点ID在前，再字典排序。正方形ID0=(0,0)、1=(1,0)、2=(1,1)、3=(0,1)必须取(0,2)，输出(0,1,2)/(0,2,3)；若u=[0,0,0,1]，中心名义u=0而非另一对角线得到的1/2。该固定tie是估计量的一部分，不能交给无约束库默认值。

### 资源、停止与独立例

F计chart的实际M点、三角邻接/精确谓词位长和查询块；最坏M² work受预算，超限不能发布半网当完整支持。S-TRI：三点(0,0),(1,0),(0,1)，u=[0,1,2]，query=(1/4,1/4)，uhat=3/4；δ_i=1/10、M=0时Δ=1/10。三角顶点均在支持、但中央有hole时整三角必须拒绝；共线/重复冲突/共圆tie、跨chart、无M的任意高bump负例须覆盖。直接variance查询与重建分别验收，不能将插值成功认证噪声图恢复。
