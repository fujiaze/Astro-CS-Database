# sky.controls：共同天空控制量

状态：本篇规定目标算法及实施合同；已有部分Runtime和科学模块实现，具体完成/验收只看[统一状态](../../../audit/IMPLEMENTATION-STATUS.md)，不能由本文标题推断全部未实现或已通过。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `sky.controls` |
| 所属模块与代码归属 | `src/sky`；本模块负责算法及typed adapter |
| 注册operation白名单 | `build_sky_controls`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | FramesProvider、SkyRequest → SkyControls，明确T/H/A/E和共同支持。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；卷积/控制action可未来批次；共同量判定由科学模块固定。 |

## 算法与失败合同

固定patch/掩码面积平均，不median/clip控制量；只对可比较响应建立共同天空边。

背景基与application基同chart/Basis；不同通带不得假设相同均值。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

按真实支持patch并行装配，全局连通关系冻结；宽核halo由尾质量预算给出。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`mosaic.sky.controls`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：S01；公共支持、异滤镜/PSF、空控制和跨patch基一致性。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，当前接口及能力以公开头/registry核对，规格与代码偏差必须明确记录。

## 算法详细条款

本篇上述合同与科学基础定义该过程；共享原语调用不复制到本模块。


### 控制patch和共同basis构造

无显式 T/H 时，按共享 order 的 face chart 固定32×32 leaf patch 建控制中心；只在可比组公共支持内，以固定 mask 的面积平均建 T，每帧每 patch 一行。mask 来自 DQ/support，可选排星圆域必须在请求固定；不在控制像元上做 median/clip。H 每 patch 一个独立共同 s 列，同 patch 各帧共享；若有给定空间传递核则 H 用实际响应后的共同 basis。A 与 E 均按同一个 Basis(center,scale,weighted_mean) 求值，不允许不同插值。PSF 匹配只支持可实现的 Gaussian 额外卷积（σ_target²−σ_frame² PSD），其他核用户须提供验证过的匹配 action；不能自动反卷积。宽核 halo 用尾质量预算推导。


### 控制构造操作语义

| 操作 | 本模块合同 |
|---|---|
| build_sky_controls | request指定groups、T/H或默认patch、共同Basis与noise；产T,H,A,E，具体patch/匹配条件见本篇，不在fit中重写 |


### 可复用计算模块组合

目标组合：共同支持/响应验证 → operators T/H/A/E装配 → uncertainty控制协方差 → 连通分量输入封存。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。


## 直接开发的实施合同

本process的算法复现按[独立实施标准](../standards/ALGORITHM_CONTRACTS.md)验收。全部operation的Input/Output按[FOUNDATION_API的typed规则](../standards/FOUNDATION_API.md#当前科学操作的typed输入标准)从公开参数机械生成；跨度/provider均转强所有权，codec先register_type再登记typed hooks。bounds给最小/最大块和spill能力，plan显式接受PlanOptions，不能读取全局当前块大小。资源key/不可拆相关组/上界项见[DEVELOPMENT本process行](../standards/DEVELOPMENT.md#模块plan和工作单元表)，具体数学/停止仍由本篇拥有。

有全局拟合屏障时拆stream→ordered_accumulator→barrier region：每cohort提交R内聚合状态即可退F，不能让唯一进展cohort等待下一cohort。科学迭代以显式epoch重新准入，work不重置。取消/失败只允许内建清理继续，模块不提交新科学计算；公开输出只在所有必需任务/写入/校验结束后交付。

实施交付包含所属模块plan/kernel/adapter/codec、独立小例/负例及真实service资源/取消证据，逐项门槛见[DEVELOPMENT](../standards/DEVELOPMENT.md)。完整编排代码可参照[校准实施例](../standards/CALIBRATION_WALKTHROUGH.md)，不得复制其中像元公式到其它process。GPU只预留同一批次/内存位置/完成事件边界，不实现后端。

## 可独立实施的算法合同

### 公共支持、匹配和矩阵构造

先按Comparability/响应条件建组，同一sky目标不成立不造公共s。默认32×32 patch按固定face/chart和共享order定位；每帧先取真实S∩有效DQ，再求组内公共mask。Gaussian匹配只在target−frame协方差PSD且已有验证卷积action时允许；有限halo按尾界，输入halo不足的边界从公共mask排除或具名未决，不用零padding假装已匹配。非Gaussian匹配需用户显式合法action。

每个(frame,patch)固定一行T：以公共mask的实际支持面积归一、同key有向累积，分母下界须正。先将所用匹配action合成到T，再c=T y，control covariance直接T C Tᵀ，重叠patch/halo/共同root均保相关。H为同patch共同天空列经实际匹配/响应后的作用；A=T E，其中E用同一持久化Basis在输出样本的实际积分/求值，不能fit用格点均值而apply用另一插值器。显式T/H请求走相同维度/单位/支持验证。

### 资源、失败与独立例

固定mask不能在控制值上median/clip后仍把T叫线性；可选排星范围只来自明确请求。空公共支持no_support，不可比组unsupported/unavailable，未达匹配误差须conditional或拒绝。F包括patch完整halo、support Boolean、T/H/A稀疏行、卷积/根workspace，R保矩阵recipe和公共mask；管道先把组控制贡献提交R再释放F。

K-SUPPORT：两帧支持[0,2]与[1,3]，公共区[1,2]，两行T只积分该区，独立各自归一会不同；K-CORR同一像元进入两个控制时其cov非零；K-HALO在边界缺输入不得补0；K-BASIS给平面真值并检查A=T E精确一致。PSF不同但假定未匹配的共同s须被前置拒绝，不等sky拟合失败才发现。
