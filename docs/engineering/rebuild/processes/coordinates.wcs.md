# coordinates.wcs：规范WCS与坐标

状态：本篇规定目标算法及实施合同；已有部分Runtime和科学模块实现，具体完成/验收只看[统一状态](../../../audit/IMPLEMENTATION-STATUS.md)，不能由本文标题推断全部未实现或已通过。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `coordinates.wcs` |
| 所属模块与代码归属 | `src/coordinates`；本模块负责算法及typed adapter |
| 注册operation白名单 | `make_wcs,forward.center0,forward.edge0,inverse,derivative,solid_angle_jacobian,bind_wcs`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | HeaderFields/完整detector域/AstrometricContext或WcsModel+坐标 → WcsModel、方向、逆根盒、导数/雅可比区间。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；点批次是未来候选；严格超越区间与不稳定分支首期CPU。 |

## 算法与失败合同

统一ICRS与center0/edge0原点；准入包含整域单射/投影条件；点值成功不代替整域证书。

detector_frame绑定；bind_wcs保solution及参数误差来源，缺一般传播能力明确fixed_wcs/omitted。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

点批次可并行；整域证明按有界Box工作单元；WCS只读，逆求解scratch独占。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`coordinates.wcs.solution`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：C01/G01；非整数CRPIX、镜像CD、极点、SIP、域外逆与残差证书。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，当前接口及能力以公开头/registry核对，规格与代码偏差必须明确记录。

## 算法详细条款

### 无WCS初态、模型绑定和G的维度

Raw/校准Native始终拥有detector-only FootprintSet，但其wcs允许null；它们只定义真实收光域、身份和PRF。calibrate无需WCS/PSF/response，physical_collection=true，native_parent=null，regrouping=I，kernel/response可缺并记录能力。solve_astrometry消费该detector数据；解后bind_wcs创建新Native revision，绑定同Wcs到FootprintSet，校验整收光域，share数组不复制N个对象。fit_psf→measure源表后bind_observation_model把kernel与NormalizeFrame.response绑定新revision，保所有influence/输入依赖；apply_scale组合其transfer action，build_drizzle只能消费此后版本。绑相同模型可幂等返回同revision，绑定不同模型必须新revision，已scale状态不可换response而假装仍同单位。

### 生产者的跨模块合同

bind_wcs另接solution：solver成功传AstrometricSolution，supplied WCS可显式null；非null时solution.wcs必须同输入wcs，保存到FootprintSet.astrometry，GeometryRecipe及full闭包因此保留参数covariance、influence和included/omitted。它不是另一套WCS。首期几何/模板数值包含默认条件于该固定WCS；若非零天测参数误差尚无有效joint Jacobian/同根传播能力，输出ErrorRecord必须列astrometric_parameter_propagation_omitted，noise不能称无条件known，V_F/V_B及合成条件也保留fixed_wcs。不给外部solution时真实天测误差unknown；纯数值synthetic固定WCS请求可显式condition=fixed_numeric_model。保存误差来源不等于已完成一般拓扑参数传播，不能静默丢已提供模型后称物理误差完整。

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| make_wcs | HeaderFields typed list而非不可信字符串拼接；Box覆盖完整footprint，context/budget；结果Ref<WcsModel>包括完整准入证书，缺必需参考系/畸变unsupported。符号未证明返回domain，不返回可用于严格几何的Wcs |

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| forward/inverse | 点输入必须在domain，SkyICRS单位化误差纳rad预算；inverse返回点+root_box+分列残差，不能只返回近似点。域/切平面/存在性未决在issues.reason精确列projection_unresolved等科学原因 |

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| derivative/solid_angle_jacobian | 点J为3×2 row-major，g(Box)为sr/pixel²区间；只支持同Wcs数值模型。区间无有限界返回domain或precision_exhausted，不能靠有限差分补正式界 |


### 构造与连接细节

- bind_wcs(observation,wcs,solution)：校验完整域并创建immutable revision，原生数组共享；无WCS校准合法。


### Detector坐标绑定

所有观测点坐标属于FootprintSet.detector_frame；make_wcs显式接detector_frame并持久化，bind必须相同。盲解在图像采样格检测后先按FootprintSet.detector_from_sample与offset转到该detector坐标再拟合，match_radius_pixel/scale_rad_pixel及PSF宽度均以该detector单位解释；不得把Bayer采样间距误当真实photosite宽度。用户显式变换Wcs到新detector坐标系时，必须同时变换FootprintSet并给新detector_frame。


### 可复用计算模块组合

目标组合：规范模型准入 → map_blocks坐标/Box求界 → numerics验证/细分 → immutable WCS结果；点值与整域证书分开。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。


## 直接开发的实施合同

本process的算法复现按[独立实施标准](../standards/ALGORITHM_CONTRACTS.md)验收。全部operation的Input/Output按[FOUNDATION_API的typed规则](../standards/FOUNDATION_API.md#当前科学操作的typed输入标准)从公开参数机械生成；跨度/provider均转强所有权，codec先register_type再登记typed hooks。bounds给最小/最大块和spill能力，plan显式接受PlanOptions，不能读取全局当前块大小。资源key/不可拆相关组/上界项见[DEVELOPMENT本process行](../standards/DEVELOPMENT.md#模块plan和工作单元表)，具体数学/停止仍由本篇拥有。

有全局拟合屏障时拆stream→ordered_accumulator→barrier region：每cohort提交R内聚合状态即可退F，不能让唯一进展cohort等待下一cohort。科学迭代以显式epoch重新准入，work不重置。取消/失败只允许内建清理继续，模块不提交新科学计算；公开输出只在所有必需任务/写入/校验结束后交付。

实施交付包含所属模块plan/kernel/adapter/codec、独立小例/负例及真实service资源/取消证据，逐项门槛见[DEVELOPMENT](../standards/DEVELOPMENT.md)。完整编排代码可参照[校准实施例](../standards/CALIBRATION_WALKTHROUGH.md)，不得复制其中像元公式到其它process。GPU只预留同一批次/内存位置/完成事件边界，不实现后端。

## 可独立实施的算法合同

### 被计算对象与操作序列

唯一坐标/TAN/SIP/整域单射定义见[观测与坐标基础](../../../science/foundation/OBSERVATION_COORDINATES.md)。make_wcs先规范header单位/轴/CRPIX/参考系，构造同一WcsModel的解析多项式和导数，再以覆盖全部FootprintSet的Box验证定义域、非奇异/单射及60°准入。不能以匹配星或网格点代替整域证明；基础中的充分判据不成立是未决，不自动证明模型一定非法。

forward.center0/edge0只在绑定detector坐标中各自解释原点；共享一次解析表达式。derivative直接解析微分，solid_angle_jacobian用同一导数/球面换元有向求界；禁止有限差分作正式包含。inverse先作天空到切平面，Newton求名义点，再用区间根盒证明域内存在/唯一及残差，root_box和天空总误差分列。跨RA切线用三维单位向量/atan2误差，不能直接减RA。

### 参数、资源与停止

所有容差/位长/迭代来自NumericBudget，模型误差来自输入头/context，不从roundtrip自造。work计每Box/解析/精度段；F含degree对应系数/区间、逆根盒队列及MPFR位长。Box队列有界，不能把全域细分节点无限驻留。原语未认证、极点分支/导数符号未决或唯一性不能证实时，按domain/precision_exhausted及具体reason返回；不以中心值WCS继续严格geometry。bind_wcs只在域/frame一致后创建新revision。

W-ORIGIN用CRPIX=(17.293,41.617)的非整数外部真值，W-SIP用Jacobian跨0而采样点均正常的多项式，W-CUT跨RA0及靠近天极，W-PRIMITIVE使用sin[1000,1002]依赖负例证明认证不能越过NUMERICS门槛。C01需独立标准库世界坐标对照；有小roundtrip仍不能省原点/全域准入负例。
