# observation.regroup：物理重组与线性视图

状态：目标process文档；对应科学函数可已有部分实现，标准注册/生命周期/探针尚未据本文落地。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `observation.regroup` |
| 所属模块与代码归属 | `src/observation`；本模块负责算法及typed adapter |
| 注册operation白名单 | `regroup_observation`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | NativeObservation、RegroupRequest → 新NativeObservation；G的域/陪域与footprint对应。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；可复用CSR/逐元素批次；不预建第二套GPU重组模型。 |

## 算法与失败合同

physical_sum/mean保持成员和洞；linear_view显式区别物理收光域；保raw parent，不把缺像元补成完整bin。

传G C Gᵀ及signed数值区间；空间标度时沿parent先Dα再G。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

按输出行分块，依赖G触及的parent块；按固定源键合并，transpose借operators，不自建线程。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`observation.regroup.signal`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：U01；sum/mean含缺孔、翻转、转置及signed linear_view负例。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，文档中的角色名不表示头文件现已提供新接口。

## 算法详细条款

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| regroup_observation | G域与native_index一致，输出footprint数匹配陪域，M/b可逆；mean成员必须与实际成员一致，缺hole不猜。成功保G与C变换，非法非线性G unsupported |


### 重组与物理Drizzle次序

Native.linear_view不能进入Normalize物理Drizzle；若NormalizeFrame.regroup=linear_view，配置预检返回unsupported，用户可在独立API使用该视图；原生通道Normalize由不请求该regroup明确表达，不能默默忽略请求。physical_sum/mean保持raw parent，空间α必须先在最原始physical parent逐样本应用再按G归并，不能把一个bin中心α代替各成员；apply_scale负责沿parent/action链确定次序，当前值与template/noise共用G Dα链。


### G的维度与实际收光域

对G: n原生→m重组，只有physical_sum/physical_mean（非负、对应互不重复photosite的真实收光并集、系数分别1或1/成员数）产生可用物理重组样本；mean严格Drizzle前恢复sum。此时R_out是从m个重组收光域构造的leaf×m算子，总链为R_out G（leaf×n），不再写R_native G。一般signed/线性去马赛克为linear_view，可得到Gx及GCGᵀ作测量/诊断，但physical_collection=false且不伪造Ω/新物理footprint；直接build_drizzle返回unsupported。Normalize严格主路径保留native_parent，在原生CFA通道上使用R_native x，G派生不进入此signal链。未来若要signed输出重建须另定义估计量，当前不授权编码Agent自创。crop/flip/Bayer为物理选取，bin为物理并集，均支持。重复观测判重回溯original roots/source_index，不能用新channel名逃避同曝光重复。


### 重组约束

1. sum/mean/crop/Bayer/flip/transpose 用显式 G、M、b。mean 恢复积分时乘成员数，noise 平方同变；有缺孔时成员及 footprint 必须对应，不用固定 bin 面积替代。G的严格维度及signed linear_view限制见本篇G维度条款；仅物理重组使用R_out G，原生路径只用R_native。
2. 首期无自动非线性修复或去马赛克。API 收到无 influence 的数据依赖 G 返回 unsupported；合法未修复像元继续处理。


### 可复用计算模块组合

目标组合：parent块依赖读取 → operators稀疏G/同根传播 → 输出pipe → builder封存；物理footprint由本模块验证。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。
