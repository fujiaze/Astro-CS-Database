# sky.fit：相对背景拟合与应用

状态：本篇规定目标算法及实施合同；已有部分Runtime和科学模块实现，具体完成/验收只看[统一状态](../../../audit/IMPLEMENTATION-STATUS.md)，不能由本文标题推断全部未实现或已通过。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

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

C确实块对角的计算分量可并行；gauge分量之间有共享root仍须联合/耦合。全解冻结后屏障开放coadd，不能边改β边写最终tile。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`mosaic.sky.fit`、`mosaic.sky.fit_component`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：S01；KH=0、同数据交叉项、孤立帧、额外nullspace与域外预测。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，当前接口及能力以公开头/registry核对，规格与代码偏差必须明确记录。

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


## 直接开发的实施合同

本process的算法复现按[独立实施标准](../standards/ALGORITHM_CONTRACTS.md)验收。全部operation的Input/Output按[FOUNDATION_API的typed规则](../standards/FOUNDATION_API.md#当前科学操作的typed输入标准)从公开参数机械生成；跨度/provider均转强所有权，codec先register_type再登记typed hooks。bounds给最小/最大块和spill能力，plan显式接受PlanOptions，不能读取全局当前块大小。资源key/不可拆相关组/上界项见[DEVELOPMENT本process行](../standards/DEVELOPMENT.md#模块plan和工作单元表)，具体数学/停止仍由本篇拥有。

有全局拟合屏障时拆stream→ordered_accumulator→barrier region：每cohort提交R内聚合状态即可退F，不能让唯一进展cohort等待下一cohort。科学迭代以显式epoch重新准入，work不重置。取消/失败只允许内建清理继续，模块不提交新科学计算；公开输出只在所有必需任务/写入/校验结束后交付。

实施交付包含所属模块plan/kernel/adapter/codec、独立小例/负例及真实service资源/取消证据，逐项门槛见[DEVELOPMENT](../standards/DEVELOPMENT.md)。完整编排代码可参照[校准实施例](../standards/CALIBRATION_WALKTHROUGH.md)，不得复制其中像元公式到其它process。GPU只预留同一批次/内存位置/完成事件边界，不实现后端。

## 可独立实施的算法合同

### 联合解与有效修正域

每个真实控制连通分量使用已冻结c=T y、共同H、A与E以及完整control C。gauge分量按科学设计/公共天空可比关系定义，计算分块还须考虑C：只有完整C在分块间确实块对角才可独立解；跨分量共享root时需合并求解或保全局shared-latent/KKT耦合，不能因为无几何重叠就删交叉块。reference gauge或weighted_sum按请求施加为显式参数基G，不加ridge。PSD C的零空间先给精确方程；求解D=[H,A G]的白化QR，保非gauge额外nullspace。大系统首期只有满足NUMERICS证书的sparse QR/LSMR或含C的KKT路线才能报告对应能力；无可行workspace具名resource_exhausted，迭代收敛不自动证明秩。

由同一求解map提取β=Kc，P=I−E K T；apply_sky不重拟合、不重复减。某输出位置若E行对残余nullspace非零，则其correction不可估，默认0且unavailable；训练支持外还需allow_prediction，记录predicted/extrapolated，不因Box内就叫有数据。完整噪声为P C_y Pᵀ，保同数据负交叉项；参数预测variance不是完整corrected covariance。

### 屏障、资源与独立例

局部控制贡献按顺序提交R，全部EOF后才借全局求解F，禁止E=0时某cohort持唯一lane等下一cohort。R/F依据实际连通分量n_c、参数p、nnz和白化/预条件器workspace，dense_max_bytes不能被忽略；迭代残差及请求输出泛函同时达标才成功。取消不发布部分β给后续coadd。

K-SKY：两帧一位置y=[3,5]、相同公共s、offset模型且reference β1=0，真值s=3、β2=2、corrected=[3,3]；C_y=I时P=[[1,0],[1,0]]，corrected covariance=11ᵀ，不是两个独立方差或各自加skyvariance。K-CROSS：约束后D=[[1,0],[1,0],[0,1],[0,1]]，C=I且C13=C31=1/2，y=[0,0,0,1]；联合GLS为(2/15,7/15)，错误按两组独立求解为(0,1/2)。再测全部控制共线的平面、孤立帧、同一真公共梯度与训练域外查询；K/P action序列化重开必须保同根/有效修正域。
