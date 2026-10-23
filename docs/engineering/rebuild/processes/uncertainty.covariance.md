# uncertainty.covariance：共享根协方差

状态：目标process文档；对应科学函数可已有部分实现，标准注册/生命周期/探针尚未据本文落地。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

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

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，文档中的角色名不表示头文件现已提供新接口。

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
