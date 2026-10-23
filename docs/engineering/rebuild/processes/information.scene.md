# information.scene：实际场景方差与稀疏预测

状态：目标process文档；对应科学函数可已有部分实现，标准注册/生命周期/探针尚未据本文落地。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

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

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，文档中的角色名不表示头文件现已提供新接口。

## 算法详细条款

### 稀疏actual-scene三角网构建和参考信息请求

build_supported_triangulation是information模块的唯一2D Delaunay实现：控点投影到给定gnomonic Chart，要求点在chart定义域且距中心≤maximum_radius_rad（首期≤5°）；用Bowyer–Watson增量插入，初始超三角覆盖全部点，按输入hash固定排列，邻接walk定位与局部cavity更新。orientation/incircle用binary64过滤+GMP精确有理谓词；cocircular tie按控点ID，不加随机joggle；去掉超三角关联项。triangle只有完全在support中且不跨hole/不兼容response才保留，边界无法判包含则不可用/待细化，不能用顶点都在支持内代替整三角包含。共线返回unavailable，重合点同身份同值合并，冲突值invalid_model。算法典型O(M log M)、最坏O(M²)，全部实际work计费，未完成网不作为已观测支持。SupportedTriangulation.vertices[M,2]保存chart坐标，triangles[T,3]u64索引，查询barycentric与curvature误差复用该唯一basis。

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
