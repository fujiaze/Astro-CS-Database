# observation.calibrate：观测校准

状态：本篇规定目标算法及实施合同；已有部分Runtime和科学模块实现，具体完成/验收只看[统一状态](../../../audit/IMPLEMENTATION-STATUS.md)，不能由本文标题推断全部未实现或已通过。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `observation.calibrate` |
| 所属模块与代码归属 | `src/observation`；本模块负责算法及typed adapter |
| 注册operation白名单 | `calibrate_observation`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | RawObservation、Calibration → NativeObservation；原始ADU、曝光时间和flat规范显式，输出积分计数及DQ。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；数组校准可作为未来逐元素kernel，FP64/区间认证策略不变。 |

## 算法与失败合同

按bias/dark曝光归一/flat顺序，保context/frame、母版共享根和确定数值包围；flat无效不伪修复。

G、α、R尚未应用；输出交给regroup、astrometry或PSF，未知shot期望保持unknown。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

按原始样本块并行；母版只读共享，块写入互斥输出范围；单元输入+输出+DQ+区间+scratch一并预留。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`normalize.calibration.signal`、`normalize.calibration.frame`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：U01/F01/X01；常量flat、带bias的dark、共享母版、flat≤0及非有限输入。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，当前接口及能力以公开头/registry核对，规格与代码偏差必须明确记录。

## 算法详细条款

### 算法细则

1. 以 u16 DQ 保存 bad=1、saturated=2、nonlinear=4、missing=8、repaired=16；前四位默认不合格，repaired 必须附 G/influence 才可使用。NaN 的值与有效位冲突为 invalid_model。
2. 原始二维 ADU 读入 FP64。dark 若含 bias，用 dark-bias 后再按曝光比缩放；不含 bias 则直接缩放。x=(d-b-t d_rate)/flat。flat 已按明确 normalization 标准化；flat≤0、溢出或饱和不做数值修复，保留 DQ。
3. 输入 raw_noise 可指定完整 electron expectation 与读出因子。没有非负 electron expectation 时，gain/read 元数据本身不足以精确恢复 shot variance；默认 noise unknown；用户显式 Poisson plug-in 可用 max(d-b,0) 的电子期望并标 approximate。母版的噪声根按 shared_id 并入 Jacobian，而非每帧重建。

### 外部输入和FITS边界

首期Normalize输入仅二维FITS主图或显式指定image HDU，BITPIX=8/16/32/64/−32/−64，按BSCALE/BZERO转FP64；BLANK/非有限标missing并保NaN。BUNIT须ADU或显式单位映射，electron图不能当ADU混入。DQ扩展若有须整数同shape，位按本篇校准规则；明确SATURATE头或配置给出的阈值可标饱和，否则输入携带linear_unsaturated_assumed条件，不虚构线性/饱和边界已测。header冲突或第三轴不猜slice。time/exposure需明确单位和time scale，UTC→TDB用ERFA及观测者条件，缺输入误差单列。除显式supplied_wcs外，缺WCS头仍实际盲解；输入WCS参考系/投影按foundation准入。products/Raw构造由storage+CLI共享reader完成，driver可直接用公开RawObservation/Array而不需FITS。

### 生产者的跨模块合同

calibrate_observation将RawObservation.context和frame逐字段保留到NativeObservation；regroup、bind_wcs、bind_observation_model、apply_scale不得丢失或改写。NormalizeFrame.frame必须等raw.frame，measure_science_sources输出frame=observation.frame。solve_astrometry从observation.context读取历元/TDB/observer，Gaia query按该历元传播；已知WCS与科学源也须与同context相容。相同像元的2016/2026两帧不会因值相同变同一观测，SourceIdentity.exposure和context均参与依赖。

### 生产者的跨模块合同

NativeObservation.numerical_lower/upper随integrated_values同index；calibrate为(d−b−t dark)/flat用向外区间传播实际浮点/模型数值误差，regroup按signed G传，apply_scale用PhotoSolution.parameter_lower/upper和exp区间传，再建G Dα链。真实母版/fit随机误差仍在NoiseModel，不加到数值半径。原始输入已解析的数组数字定义数值模型，有限外部确定误差若给出必须进区间，未知物理项进ErrorRecord。build_drizzle使用这些x区间，绝不把scaled FP64值默认为零数值误差。

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| calibrate_observation | RawObservation+Calibration，按本process校准细则得到integrated native数组及noise根，校准均值和噪声分开；flat无效逐样本DQ，整个输入维度错invalid_model |


### 可复用计算模块组合

目标组合：arrays.read_async → elementwise校准/区间/DQ → arrays.append_async → seal；母版缓存复用只读lease。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。


## 直接开发的实施合同

本process的算法复现按[独立实施标准](../standards/ALGORITHM_CONTRACTS.md)验收。全部operation的Input/Output按[FOUNDATION_API的typed规则](../standards/FOUNDATION_API.md#当前科学操作的typed输入标准)从公开参数机械生成；跨度/provider均转强所有权，codec先register_type再登记typed hooks。bounds给最小/最大块和spill能力，plan显式接受PlanOptions，不能读取全局当前块大小。资源key/不可拆相关组/上界项见[DEVELOPMENT本process行](../standards/DEVELOPMENT.md#模块plan和工作单元表)，具体数学/停止仍由本篇拥有。

有全局拟合屏障时拆stream→ordered_accumulator→barrier region：每cohort提交R内聚合状态即可退F，不能让唯一进展cohort等待下一cohort。科学迭代以显式epoch重新准入，work不重置。取消/失败只允许内建清理继续，模块不提交新科学计算；公开输出只在所有必需任务/写入/校验结束后交付。

实施交付包含所属模块plan/kernel/adapter/codec、独立小例/负例及真实service资源/取消证据，逐项门槛见[DEVELOPMENT](../standards/DEVELOPMENT.md)。完整编排代码可参照[校准实施例](../standards/CALIBRATION_WALKTHROUGH.md)，不得复制其中像元公式到其它process。GPU只预留同一批次/内存位置/完成事件边界，不实现后端。

## 可独立实施的算法合同

### 执行顺序与认证依赖

本篇的公式、四writer编排、R/F/E及全部固定C-CAL输入唯一见[完整实施例](../standards/CALIBRATION_WALKTHROUGH.md)，不复制第二套校准器。validate只核元数据；load读并验证实际输入后，kernel按同一个公式同时产生value/lower/upper/DQ，sink排空四writer。缺省master为标量0/0/1，不分配全图；dark含bias的同bias系数为β−1。

参数唯一来自Calibration、Raw.context/单位和NumericBudget；flat≤0/NaN/DQ冲突按实施例处理，负raw/x合法。数值包围须依赖已验证四则，随机shot/master根另算；未验证interval实现不能把正常输出称certified。像元之间可并行，母版和noise根共享；所有输入/输出/codec/区间临时峰值必须在60n+S及持久R中实测核对，不能只证明预留数值合适。

验收在C-CAL上增加A-CAL-PEAK：真实allocator日志逐块证明输入34n、输出26n与S的上界，失败allocate不增allocated，第四writer seal失败后前三个私有产物回收。A-CAL-CERT让N-ADD失败后，上游不得仍标严格数值证书。取消、work耗尽、DEV_MODE磁盘故障按既有完整实施例；这些是待实跑门槛。
