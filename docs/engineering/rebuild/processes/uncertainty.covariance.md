# uncertainty.covariance：共享根协方差

状态：本篇规定目标算法及实施合同；已有部分Runtime和科学模块实现，具体完成/验收只看[统一状态](../../../audit/IMPLEMENTATION-STATUS.md)，不能由本文标题推断全部未实现或已通过。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `uncertainty.covariance` |
| 所属模块与代码归属 | `src/uncertainty`；本模块负责算法及typed adapter |
| 注册operation白名单 | `make_covariance,covariance_apply,quadratic_form,geometry_covariance_bound,control_covariance`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | NoiseModel/实际action/泛函 → CovarianceAction、Variance或BoundOrUnavailable。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；复用operators因子action后端，噪声图/根身份不因设备改变。 |

## 算法与失败合同

同shared根先合并总influence，再L C0 Lᵀ；unknown沿非零action传递，PSD零模不是epsilon。

随机variance、数值界和模型未知分开；C(v)不冒充白化，带sr泛函单位正确乘积。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

按触及的稀疏根/因子流式；不得物化全天空N²，根图只读共享、求解scratch独占。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`uncertainty.quadratic_form`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：U01/S01/W01；共享噪声交叉项、signed孔径、PSD约束与unknown传播。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，当前接口及能力以公开头/registry核对，规格与代码偏差必须明确记录。

## 算法详细条款

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| make_covariance | NoiseModel内roots+同一shared根先合并总influence，PSD因子合法才构造C action。显著负谱invalid_covariance；unknown保留；不会把满秩假设加入noise |

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| geometry_covariance_bound | nominal R、非负绝对误差E、signed functional t，计算统计§2.1式3b的方差变化半径；没有absolute-factor或谱界则BoundOrUnavailable.bound=null、reason，名义C仍可用。结果不是随机variance增量 |



### 协方差操作语义

| 操作 | 本模块合同 |
|---|---|
| covariance_apply/quadratic_form | 检查C域与输入的同随机量index；full未知时covariance_apply返回unavailable无假Cv，quadratic_form可返回Variance{value=null,state=unknown}并列未知项；完全已知返回Cv/非负variance，微小负仅在证明数值区间含0时截为0并记录 |


### 控制协方差操作语义

| 操作 | 本模块合同 |
|---|---|
| control_covariance | 输入C及T，返回T C Tᵀ；跨frame Γ由相同根拼接frame域后调用同一C API，不能把reference改名为scene |


### 可复用计算模块组合

目标组合：根图/功能系数读取 → 同shared根合并 → operators因子apply/transpose → numerics二次型/界 → 具名Variance。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。


## 直接开发的实施合同

本process的算法复现按[独立实施标准](../standards/ALGORITHM_CONTRACTS.md)验收。全部operation的Input/Output按[FOUNDATION_API的typed规则](../standards/FOUNDATION_API.md#当前科学操作的typed输入标准)从公开参数机械生成；跨度/provider均转强所有权，codec先register_type再登记typed hooks。bounds给最小/最大块和spill能力，plan显式接受PlanOptions，不能读取全局当前块大小。资源key/不可拆相关组/上界项见[DEVELOPMENT本process行](../standards/DEVELOPMENT.md#模块plan和工作单元表)，具体数学/停止仍由本篇拥有。

有全局拟合屏障时拆stream→ordered_accumulator→barrier region：每cohort提交R内聚合状态即可退F，不能让唯一进展cohort等待下一cohort。科学迭代以显式epoch重新准入，work不重置。取消/失败只允许内建清理继续，模块不提交新科学计算；公开输出只在所有必需任务/写入/校验结束后交付。

实施交付包含所属模块plan/kernel/adapter/codec、独立小例/负例及真实service资源/取消证据，逐项门槛见[DEVELOPMENT](../standards/DEVELOPMENT.md)。完整编排代码可参照[校准实施例](../standards/CALIBRATION_WALKTHROUGH.md)，不得复制其中像元公式到其它process。GPU只预留同一批次/内存位置/完成事件边界，不实现后端。

## 可独立实施的算法合同

### 根、action、unknown的执行分支

make_covariance先按(SharedId,coordinates,factor hash)合并同一随机根的全部influence L，再构`C=Σ(LF)(LF)ᵀ`，额外CovarianceAction.influence在合并后统一作用。同ID异factor/单位/坐标为identity_conflict；两个有符号影响可相消，必须先合并再平方。已知独立innovation不凭hash相同变共享，身份来自模型合同。

factor已给时只验证其shape/单位及有限数值，PSD由F Fᵀ构造；输入一般协方差需NUMERICS向外eigen验证、保精确结构零及未决谱。covariance_apply按factor转置→latent→正向作用，不造N²；quadratic_form按`Σ||FᵀLᵀt||²`及实际随机根单位计算，值/区间必须一致。unknown term只要对请求泛函可能非零就保持unknown；只有证明结构性annihilation才能对该泛函消除，名义浮点接近0不够。known subset可作具名下界/条件分解，不能改名完整C。

control_covariance生成T C Tᵀ惰性action，cross-frame Γ从共同根联合域取得；geometry_covariance_bound先把signed t回溯到同源，再用已具备的绝对factor/谱上界求统计基础式3b。没有绝对能力给BoundOrUnavailable无bound，不将几何确定半径加成随机variance。每项的精度/停止和unknown原因随Result/error与依赖闭包保留。

### 资源与独立反例

F计触及根/因子nnz、输入泛函、latent向量与实际action workspace；R只持不可变图，按共享根预算去重。协方差未知不需尝试分配稠密矩阵“求一个近似”。C-ROOT：同标量根variance=4、两个influence=1和−1，合并输出variance=0，独立相加会成8；两帧同根各1的Γ=4·11ᵀ。C-UNKNOWN的未知共同offset在t=[1,-1]下有结构性零，可给有限已知泛函；单帧t则unknown。C-PSD含精确零、轻微负谱和未决跨0；C-GEOM含signed t检验绝对项不能抵消。沿公开入口/产品重开验证同根身份，不能只检查矩阵数字。
