# geometry.drizzle：同R重采样

状态：本篇规定目标算法及实施合同；已有部分Runtime和科学模块实现，具体完成/验收只看[统一状态](../../../audit/IMPLEMENTATION-STATUS.md)，不能由本文标题推断全部未实现或已通过。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `geometry.drizzle` |
| 所属模块与代码归属 | `src/geometry`；本模块负责算法及typed adapter |
| 注册operation白名单 | `build_drizzle`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | Overlaps、NativeObservation、DrizzleRequest → DrizzleResult/SignalEstimate和同R模型。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；矩阵apply/归约可未来后端；几何与科学停止仍同一控制逻辑。 |

## 算法与失败合同

按target/source/primitive归并F/N/R；N正下界才归一，signal/noise/PSF/support同一R。

Native确定区间进入signed numerator；支持并集不由面积和替代；feedback交工作流控制。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

source→有界spool→目标行外存归并；分块不改source身份和固定归约；无整图nnz常驻要求。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`normalize.drizzle.signal`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：G01/U01；常量场、共享源拆分、弱支持、signed值及先平均后归一负例。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，当前接口及能力以公开头/registry核对，规格与代码偏差必须明确记录。

## 算法详细条款

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| build_drizzle | overlaps source全匹配NativeObservation、order/f同overlap身份；构造同R的signal/noise/support/response。no_support或未证明正支持不构造归一行。DrizzleRequest.budget.requested_functionals必须反映最终量，area-only只能证明area |


### 可复用计算模块组合

目标组合：overlap spool读取 → 固定target/source归并 → operators构R及signal/noise/template同链 → 流式封存。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。


## 直接开发的实施合同

本process的算法复现按[独立实施标准](../standards/ALGORITHM_CONTRACTS.md)验收。全部operation的Input/Output按[FOUNDATION_API的typed规则](../standards/FOUNDATION_API.md#当前科学操作的typed输入标准)从公开参数机械生成；跨度/provider均转强所有权，codec先register_type再登记typed hooks。bounds给最小/最大块和spill能力，plan显式接受PlanOptions，不能读取全局当前块大小。资源key/不可拆相关组/上界项见[DEVELOPMENT本process行](../standards/DEVELOPMENT.md#模块plan和工作单元表)，具体数学/停止仍由本篇拥有。

有全局拟合屏障时拆stream→ordered_accumulator→barrier region：每cohort提交R内聚合状态即可退F，不能让唯一进展cohort等待下一cohort。科学迭代以显式epoch重新准入，work不重置。取消/失败只允许内建清理继续，模块不提交新科学计算；公开输出只在所有必需任务/写入/校验结束后交付。

实施交付包含所属模块plan/kernel/adapter/codec、独立小例/负例及真实service资源/取消证据，逐项门槛见[DEVELOPMENT](../standards/DEVELOPMENT.md)。完整编排代码可参照[校准实施例](../standards/CALIBRATION_WALKTHROUGH.md)，不得复制其中像元公式到其它process。GPU只预留同一批次/内存位置/完成事件边界，不实现后端。

## 可独立实施的算法合同

### 数学量和输入闭包

只实现[球面Drizzle基础](../../../science/foundation/SPHERICAL_DRIZZLE.md)式(1)/(2)的已定义估计量；输入x是积分量，Ω是未收缩面积，A是实际drop面积，a是drop/真叶交叠。共享Ω/A及primitive必须按source身份只存一份；所有a均指同revision，不从独立double显示端点另造ledger。source单位/index、WCS、order、pixfrac及G physical语义在validate锁定。linear_view不进入物理Drizzle，physical_mean先按真实成员恢复sum。

### 外存CSR和同一R的执行程序

```text
读取overlap有序块，核对source域完整性与共同ledger约束
原生key转成(target_leaf,source_id,primitive_revision)记录
在有限run内稳定排序，spill为私有run；k路归并，每路至多一计费buffer
同(source,leaf)片先求并/归并，不能重复计算同一面积primitive
for 每target行p，先流式计算N_p及其完整区间，判支持
  N_U=0: no_support；N_L≤0<N_U: support_unresolved，不构归一行
  N_L>0: 再读该行spool，按基础公式产生R_pj=w_jp/N_p及区间
  按source_id固定归约B与输入x区间；构同一R的noise/PSF/response action
  逐行写offsets/columns/coefficients及误差recipe，封存后交付
```

第二遍可用单行spool，长行不能强迫全部nnz常驻；transpose归约也按source key外存归并。CSR offset/nnz乘法先checked，单行过大时多遍计算，仍无法容纳最小输入/输出/scratch则resource_exhausted。只对非零/未决候选存条目，未决质量保留在ledger，不能在排序过滤时丢掉。R与signal只生产一次，误差action及必要依赖随最终模型强持。

### 误差闭包、支持与局部细化

归一N并非覆盖并集S；SupportMap用独立union包含表示。N正下界/weak_support与B的容差分别判断。signed x乘全部端点，不能以非负光子假设缩B范围；输入数值区间、Ω/A/a共同约束、归约舍入均进入最终B与R界。noise为R C Rᵀ，同source被多个leaf分享时保同根；请求signed aperture或PSF时沿同R/绝对误差action，不取仅对角近似替代。

局部细化请求先追踪输出泛函→R行→所有共享a/A/Ω primitive，形成affected闭包；即使只一个a细化，共享A/Ω影响的其它行也要重算。生成新immutable GeometryRevision，旧R继续供原引用使用；只有新R、signal/noise/PSF/response及下游sky/weights/Q一致完成才事务替换。预算不足只给有明确有效域的partial面积/旧模型，不发布混revision的完整产品。执行不重新调用overlap之外的第二套几何核。

### 资源与固定独立反例

plan输入至少包含源数、实际nnz上界、最大活跃归并路数、索引/offset、R端点/误差recipe、MPFR/ledger位长、行spool和writer/codec工作集；并行窗口另计E。R metadata/recipe计持久R，排序/归并/单行两遍计F，spill累计写量也受配额；不把外存当无界存储。每个读块/归并段/有向归约按实际work，取消后私有run安全清理。

D-ROW：两个source的Ω=A=1，当前叶a=[1,1/2]，第二叶a=[0,1/2]，x=[2,5]。当前行N=3/2、R=[2/3,1/3]、B=3；C=diag(1,4)则Var(B)=8/9。两叶F总和=7，但当前覆盖并集可为1，不能把N=3/2当union。D-SHARED：同一source拆两个leaf，独立像元variance必须含交叉项；D-SIGNED用负x；D-ZERO使N区间跨0；D-REFINE细化共享A而只重算一行应被身份检查拒绝。小H spill与内存路径需同科学结果/误差和hash，失败seal不遗留正式产品。本轮未运行这些生产验收。
