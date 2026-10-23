# sky.fit：相对背景拟合与应用

状态：目标process文档；对应科学函数可已有部分实现，标准注册/生命周期/探针尚未据本文落地。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `sky.fit` |
| 所属模块与代码归属 | `src/sky`；本模块负责算法及typed adapter |
| 注册operation白名单 | `fit_relative_sky,apply_sky`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | SkyControls/SkyRequest或SignalEstimate/SkyFit → SkyFit/新SignalEstimate。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；稀疏action/矩阵装配未来后端，gauge/秩/全局屏障不变。 |

## 算法与失败合同

按真实分量联合[H,A G]求解，显式gauge与nullspace；P=I-EKT保同数据交叉项。

孤立/不可估方向默认零修正且unavailable；预测域外需显式许可及predicted状态。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

不同分量并行，同分量联合；全解冻结后屏障开放coadd，不能边改β边写最终tile。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`mosaic.sky.fit`、`mosaic.sky.fit_component`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：S01；KH=0、同数据交叉项、孤立帧、额外nullspace与域外预测。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，文档中的角色名不表示头文件现已提供新接口。

## 算法详细条款

### 算法细则

先构实际控制连通分量；gauge reference_frame 是按 UTF-8 frame ID 最小者或用户显式者，reference 全部 basis 系数置0；weighted_sum 用给定正权。求解联合 [H,A G]，小分量 SVD，大分量 sparse QR 或 LSMR，按[NUMERICS](../standards/NUMERICS.md)残差/秩准则。PSD 噪声先处理精确约束；仅有 C action 用增广 KKT [C,D;Dᵀ,0]，MINRES+可验证预条件器，不能把 C(v) 当 whitening。非 gauge 的额外 nullspace 保留，β 取最小欧氏范数代表，但不可估 E 方向不给“测得 sky”；有不唯一影响时该位置默认 correction=0、unavailable，不用代表值伪装修正。孤立帧同样0并记录无相对测量。用户 allow_prediction 才允许训练域外按固定模型预测，带 predicted/extrapolated 状态，完整噪声仍 P C Pᵀ。


### 拟合与应用操作语义

| 操作 | 本模块合同 |
|---|---|
| fit_relative_sky/apply_sky | request指定gauge、noise与solve；fit产β,K,P/nullspace；apply返回z及同P噪声。不可比不造边，孤立0不冒称已测背景；nullspace影响查询处修正不可用。构造输入见[sky.controls](sky.controls.md) |


### 可复用计算模块组合

目标组合：控制块读取 → numerics PSD/QR/KKT联合解 → 全分量屏障 → operators P及同数据noise → SkyFit。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。
