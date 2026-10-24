# projection.export：目标投影与层级派生

状态：本篇规定目标算法及实施合同；已有部分Runtime和科学模块实现，具体完成/验收只看[统一状态](../../../audit/IMPLEMENTATION-STATUS.md)，不能由本文标题推断全部未实现或已通过。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `projection.export` |
| 所属模块与代码归属 | `src/projection`；本模块负责算法及typed adapter |
| 注册operation白名单 | `build_export`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | ProductProvider、ExportRequest → ExportResult与同Q的signal/noise/response。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；Q action/归约可未来后端；严格交叠复用geometry，写FITS不属于GPU核。 |

## 算法与失败合同

真实b=Ω(E∩H∩U)，同leaf先合并；支持均值/积分/外推请求分别处理，不第二次收缩pixfrac。

支持形状缺失严格请求unavailable；无支持NaN、负值/零保留；层级是派生非独立观测。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

按目标WCS块并行，halo取实际相交叶；Q流式构建，同一源随机身份不因分块改变。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`export.projection.overlaps`、`export.projection.block`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：E01；部分支持、同leaf多片、WCS/RAwrap、负值/零/NaN及FITS量化。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，当前接口及能力以公开头/registry核对，规格与代码偏差必须明确记录。

## 算法详细条款

### 算法细则

build_export 对目标 WCS 整像元 footprint 计算真叶和 support 的交集 b；可用 N4 target dyadic Boolean 表示复合支持，按目标像元积分行。均值行 b/Σb，支持积分行 b，外推行 Ω(E)b/Σb。same leaf 的多个片先合并 b；所有下游模型使用同一 Q。没有支持形状且 primitives/dyadic_cells 严格请求则 unavailable；显式 full_leaf_approx 才使用完整 leaf。层级向下只对已知支持的四子叶按支持面积归约并保存独立 Q 与误差；没有完整支持用具名近似。不得把层级图作为同阶新观测供 Mosaic。

HiPS默认tile宽512，可显式64/128/256，必须leaf_order≥log2(tile_width)，完整布局映射见[OBJECTS](../OBJECTS.md)。每层唯一 signal FITS；properties 指明 hips_frame=equatorial、hips_order、hips_tile_width、hips_tile_format=fits、dataproduct_type=image。NorderK/DirD/NpixP.fits，D=floor(P/10000)*10000。Allsky.fits 按 HiPS 标准生成，PNG/JPEG 仅可选浏览派生且不成为第二科学图。

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| build_export | 前者prodProvider+目标Q/quantity/support/dtype/budget产Q和estimate，后者仅序列化ExportResult；不重新投影。无noise保unknown，primitives/dyadic_cells严格支持缺模型unavailable；extrapolation没有显式假设invalid_request |


### HiPS当前标准的索引边界

HiPS发布允许tile_log2_width=6..9（64/128/256/512，默认512）；leaf_order≥tile_log2_width，tile_order=leaf_order−tile_log2_width，低阶模型可选较小合法tile或FITS导出，不伪造索引。标准依据为[HiPS 1.0](https://www.ivoa.net/documents/HiPS/20170519/REC-HIPS-1.0-20170519.pdf)与[HiPS原始论文](https://doi.org/10.1051/0004-6361/201526075)的可变tile宽定义，未采用HiPS2.0草案新增能力。


### 可复用计算模块组合

目标组合：目标块/实际支持交叠模块 → operators Q及noise/response同链 → 有界输出pipe/封存ExportResult。本process到此返回；下游工作流显式接PublishRequest/目的路径，再调用Storage异步FITS writer/发布，模块不读取全局输出目录。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。


## 直接开发的实施合同

本process的算法复现按[独立实施标准](../standards/ALGORITHM_CONTRACTS.md)验收。全部operation的Input/Output按[FOUNDATION_API的typed规则](../standards/FOUNDATION_API.md#当前科学操作的typed输入标准)从公开参数机械生成；跨度/provider均转强所有权，codec先register_type再登记typed hooks。bounds给最小/最大块和spill能力，plan显式接受PlanOptions，不能读取全局当前块大小。资源key/不可拆相关组/上界项见[DEVELOPMENT本process行](../standards/DEVELOPMENT.md#模块plan和工作单元表)，具体数学/停止仍由本篇拥有。

有全局拟合屏障时拆stream→ordered_accumulator→barrier region：每cohort提交R内聚合状态即可退F，不能让唯一进展cohort等待下一cohort。科学迭代以显式epoch重新准入，work不重置。取消/失败只允许内建清理继续，模块不提交新科学计算；公开输出只在所有必需任务/写入/校验结束后交付。

实施交付包含所属模块plan/kernel/adapter/codec、独立小例/负例及真实service资源/取消证据，逐项门槛见[DEVELOPMENT](../standards/DEVELOPMENT.md)。完整编排代码可参照[校准实施例](../standards/CALIBRATION_WALKTHROUGH.md)，不得复制其中像元公式到其它process。GPU只预留同一批次/内存位置/完成事件边界，不实现后端。

## 可独立实施的算法合同

### 目标交叠与支持近似的精确执行

先对完整目标WCS/尺寸作坐标准入，逐目标真实像元E_q确定球面域；通过geometry同一候选/包含核求`b_qp=Ω(E_q∩H_p∩S_p)`，不是像元中心采样，也不再施加pixfrac。将一个leaf的全部support片/holes先求并再合并b，以稳定leaf ID构一行Q。support_policy=primitives/dyadic_cells时须有对应可重开闭包；full_leaf_approx仅在显式请求时令S_p=整leaf，并保近似条件，不能以性能自动切换。

supported_mean用b/Σb，supported_flux用b，extrapolated_full_flux用Ω(E_q)b/Σb且assume_uniform_uncovered=true；单位分别继承B或乘sr。Σb正下界未证明为support_unresolved，无支持为no_support。根据Q的同一数值包围作用signal/noise/PSF/response，signed输入与跨leaf共享根完整保留；同leaf分片不能当独立像元平方权。目标支持/面积的未决量也进入输出误差。

### 封存、资源与独立反例

计算块和最终FITS/HiPS序列化分离：build_export只产Q/estimate，writer只序列化一次，不在写文件时重投影。F包括完整目标行的触及leaf/halo、支持树frontier、b端点、writer/codec及几何精度；长行可spool/多遍，目标width×height/字节checked。R持Q recipe和所需support/noise闭包；新进程没有原FITS仍可打开full产品重算查询，缺闭包具名失败。

E-MERGE：同一leaf两个不重叠support片各面积1/4、B=2、Var=9，目标supported_flux=1，variance=(1/2)^2·9=9/4，错误按片独立得到9/8。supported_mean=2；目标全面积1且显式均匀外推则full_flux=2，但必须带外推假设。E-HOLE目标跨hole不得补支持；E-REOPEN跨进程验证Q/noise共享根/未知项和FITS头像元/单位；E-TILE按OBJECTS的NESTED索引检查边界，不拿浏览PNG作为第二科学signal。
