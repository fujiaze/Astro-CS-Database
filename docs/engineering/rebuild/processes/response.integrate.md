# response.integrate：响应与谱基积分

状态：本篇规定目标算法及实施合同；已有部分Runtime和科学模块实现，具体完成/验收只看[统一状态](../../../audit/IMPLEMENTATION-STATUS.md)，不能由本文标题推断全部未实现或已通过。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `response.integrate` |
| 所属模块与代码归属 | `src/response`；本模块负责算法及typed adapter |
| 注册operation白名单 | `integrate_response`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | Response、SpectralBasis、NumericBudget → SpectralPrediction及逐维absolute_errors。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；可留批量求积边界，认证超越运算首期CPU。 |

## 算法与失败合同

按谱/滤镜/QE折点分段积分；GK差值仅估计，认证靠包含或明确余项；XP外尾无界不称完整。

波长nm、谱单位及photon_conversion显式；误差进入photometry score。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

按response/basis/源块并行；缓存锁输入hash/算术，scratch和真实精度位长计费。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`normalize.response.integral`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：F01；分段多项式解析积分、断点、XP外支持及尾界缺失。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，当前接口及能力以公开头/registry核对，规格与代码偏差必须明确记录。

## 算法详细条款

### 数值与模型细则

response 基函数按谱/滤镜/QE 折点分段，GK15/31只作名义求积/细化排序。认证路径按下文固定的“可解析多项式→八阶Taylor余项→纯包含积分”能力顺序执行，普通GK差值不能成为认证半径。响应超XP支持域：有外部谱上界可补尾界，否则unavailable/conditional截带预测，不能宣称完整q。

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| integrate_response | Response/SpectralBasis/budget，输出v[d]、absolute_errors[d]及支持域；积分界不掩盖谱分辨率未知bias，缺尾界complete=false。相同响应和basis hash可按算术预算缓存 |


### 可复用计算模块组合

目标组合：响应/谱基只读块 → map_blocks分段积分 → 固定归约/尾界 → SpectralPrediction。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。


## 直接开发的实施合同

本process的算法复现按[独立实施标准](../standards/ALGORITHM_CONTRACTS.md)验收。全部operation的Input/Output按[FOUNDATION_API的typed规则](../standards/FOUNDATION_API.md#当前科学操作的typed输入标准)从公开参数机械生成；跨度/provider均转强所有权，codec先register_type再登记typed hooks。bounds给最小/最大块和spill能力，plan显式接受PlanOptions，不能读取全局当前块大小。资源key/不可拆相关组/上界项见[DEVELOPMENT本process行](../standards/DEVELOPMENT.md#模块plan和工作单元表)，具体数学/停止仍由本篇拥有。

有全局拟合屏障时拆stream→ordered_accumulator→barrier region：每cohort提交R内聚合状态即可退F，不能让唯一进展cohort等待下一cohort。科学迭代以显式epoch重新准入，work不重置。取消/失败只允许内建清理继续，模块不提交新科学计算；公开输出只在所有必需任务/写入/校验结束后交付。

实施交付包含所属模块plan/kernel/adapter/codec、独立小例/负例及真实service资源/取消证据，逐项门槛见[DEVELOPMENT](../standards/DEVELOPMENT.md)。完整编排代码可参照[校准实施例](../standards/CALIBRATION_WALKTHROUGH.md)，不得复制其中像元公式到其它process。GPU只预留同一批次/内存位置/完成事件边界，不实现后端。

## 可独立实施的算法合同

### 被积对象和固定分段

本process计算[Gaia响应基础](../../../science/foundation/GAIA_RESPONSE.md)式(1)/(3)的每一basis投影v_l，不逐星重新积分。波长以nm保存，energy谱转photon时显式乘`λ_nm·10^-9/(h c)`且dλ仍为nm；response已含conversion则不再乘。T、QE、其他效率的规范插值就是输入模型，不能私自换插值器；响应/谱基支持外不推断零。先取所有knots、top-hat边界、插值分段和合法谱支持端点的有序并集，阶跃两侧分别建面板。相同位置重复节点按模型定义合并，不让求积跨不连续点。

### 认证型积分的唯一决策程序

```text
for 每basis l、每初始光滑分段 [a,b]，按(l,a,b)稳定排序:
  若输入给的是有限次多项式乘积：有向卷积系数并解析积分
  否则若能给整段八阶导数界：用中心0..7阶Taylor积分和显式余项
  否则若有整段函数范围扩展F([a,b])：用(b-a)·F([a,b])
  以上均无能力：unsupported；不得用GK差值冒充证书
  同时可算GK名义值/细化优先级；证书为上述区间
按最大认证宽度优先二分面板，平手(l,a,b)；每次从头核算该面板
固定key有向合并各面板与已知尾界；全部分量满足预算才tolerance_met
```

Taylor路径令m=(a+b)/2、L=b−a、`c_k=f^(k)(m)/k!`，逐项有向积分`Σ(k=0..7)c_k∫(λ−m)^k dλ`；若`sup|f^(8)|≤M8`，安全余项半径取`M8·L·(L/2)^8/8!`。断点处无C8能力则先分段或走纯包含，不数值差分“估计”M8。纯包含路径即使很宽也真实有效，细分到预算耗尽保实际区间；不因慢而切换无证书策略。名义GK若不落在已证明包围内须检查/升级并报告数值矛盾，不能掩盖问题。

各分量给名义v_l及真实absolute_errors_l=max(v_l−L_l,U_l−v_l)，认证使用全部区间和上下游请求的泛函权；q=cᵀv的误差用Σ|c_l|e_l，若c也有确定区间同做signed乘法。停止判断使用明确的abs/rel tolerance，rel只能配已证明尺度，不能相对于任意接近0的名义值。无尾上界时支持外缺失单列unknown/complete=false；缺尾不被面板精度升级消除。

### 资源、缓存与错误

每个basis分段面板记录端点、算术位长、区间/导数状态和work；最多保有界面板队列，其余按有限游标/外存。`R≈O(d)`输出与basis/response引用，`F`包含当前面板多项式次数、Taylor8导数工作集及MPFR真实limb；所有乘积尺寸checked。每次节点/原语/精度升级按NUMERICS及调用work计费。算术达max_bits给precision_exhausted；work/内存耗尽给resource_exhausted和已计算范围的真实界，未处理段必须有已初始化粗界，否则不返回完整v。

缓存键含response、basis、插值/支持、photon_conversion、算术版本和已达到误差，不只含“波长范围”。粗结果不能满足更严请求时细化或重算；同一Gaia源跨response的随机系数仍共根，本process不复制其noise。

### 固定独立反例

I-POLY：f(λ)=1+2λ、response=1、[0,1]且conversion=false，精确积分2；跨λ=1/2的阶跃响应分别解析积分，不能一段GK差值为0就停止。I-UNIT：平坦`1 W m^-2 nm^-1`、450..650nm、无量纲photon response，真值`10^-9(650²−450²)/(2hc)`，核对单位和常数而不是凭数量级判错。I-TAIL：谱支持[500,600]而response[450,650]，无外界必须不完整；I-OSC：含内部振荡但端点/常用节点同值的函数，只有导数/范围界才能认证；I-CANCEL包含正负basis积分接近0，rel精度不能除零。不同块/线程有相同模型身份及包含真值；本轮只冻结这些验收。
