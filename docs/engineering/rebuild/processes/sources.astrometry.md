# sources.astrometry：有界盲解

状态：本篇规定目标算法及实施合同；已有部分Runtime和科学模块实现，具体完成/验收只看[统一状态](../../../audit/IMPLEMENTATION-STATUS.md)，不能由本文标题推断全部未实现或已通过。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `sources.astrometry` |
| 所属模块与代码归属 | `src/sources`；本模块负责算法及typed adapter |
| 注册operation白名单 | `solve_astrometry`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | NativeObservation、CatalogueProvider、SolverRequest → AstrometricSolution；无解为unsolved。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；匹配/模板候选可未来批处理；接受/模型选择仍由同一CPU控制逻辑决定。 |

## 算法与失败合同

固定检测/quad候选→一对一匹配→TAN/SIP拟合→整域WCS准入；候选星不直接当科学测光源。

观测context、detector坐标、catalogue历元一致；不发布未验证WCS。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

候选验证可批量并行但接受顺序固定；全局max_hypotheses/work共享，不能每候选重置。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`normalize.astrometry.solution`、`normalize.astrometry.search`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：C01/X01；有/无WCS、候选耗尽、盲解失败、重复匹配和整域不单射。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，当前接口及能力以公开头/registry核对，规格与代码偏差必须明确记录。

## 算法详细条款

### 算法细则

本地 Gaia 预处理按 HEALPix order 6 sky 分片；另建立四星 quad 不变式索引。索引记录 release/epoch、源 ID、ICRS 单位向量、亮度、XP 表示及其校准 hash。quad 以最长边为基线，将另外两点投影到该基线坐标，按镜像/交换两种方向枚举规范有序四维比值；KD-tree 保存四维坐标与源四元组、角尺度。生成时限制尺度范围和每片前 max_candidates 亮星，不声称覆盖全部天体。

观测端先用固定格块的中位数/MAD 作候选检测背景（仅检测，不作 science sky），5×5 局部最大、detect_snr 阈值、按 SNR 与 (y,x) 确定排序、非极大抑制半径2像元。每格64×64最多8候选，再截到 max_candidates。候选位置用局部正值加权质心初始化并用像元积分 Gaussian 优化；零/负背景像元不被变成正 photon 均值。

按候选确定顺序枚举 quads，按规范不变式在索引容差盒中查询，镜像均尝试；粗仿射 TAN 候选以所有检出星和历元传播后的 Gaia 互近邻验证。SolverRequest.regions非空只搜指定圆域并集，空时对完整离线索引搜索。scale_rad_pixel 是明确搜索范围，不从缺头猜唯一尺度。max_hypotheses 是已验证候选数量硬上限，超过返回 unsolved（search_exhausted），不伪称不存在天空解。

匹配先以match_radius_pixel gating，按下文最大匹配数/最小总平方距离规则一对一匹配，固定匹配后以平面+SIP 非线性最小二乘拟合，解析坐标 Jacobian，QR/SVD 线搜索。SIP 从0升至请求阶数，增加项必须满可估、留出残差改善；按u64 source_id直接mod5、余数0留出其余训练（不调用平台hash），至少 min_matches 且每参数至少3个训练约束。列尺度、rank和收敛见[NUMERICS](../standards/NUMERICS.md)。输出 WCS 仍须整 detector 域通过单射/60°准入，星点拟合成功不代替面积域准入。失败可尝试下个候选；全部失败返回 unsolved，附各原因与最优候选残差，不发布球面帧。此路径只保证有界搜索与正确失败，不保证任意视场盲解成功。

天测参数和像元同数据 influence 通过最优条件微分求导；缺 PSF/大气模型等误差记 omitted。catalogue 位置 ERFA 历元传播；RV/observer 缺失不得产生已知 topocentric 方向。DCR 默认 omitted，其物理误差没有输入界时完整物理定位误差 conditional。

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| solve_astrometry | SolverRequest含搜索域/尺度/候选和假设限额、detect阈值、min_matches、SIP和budget；返回AstrometricSolution，未解返回unsolved及detector诊断，产品无Wcs；至少min_matches>参数秩，输入Wcs旁路由normalize显式选择且记录来源 |


### 可复用计算模块组合

目标组合：检测块map → 目录异步查询 → 候选验证when_all → 固定候选顺序接受 → WCS整域准入；不按完成先后选解。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。


## 直接开发的实施合同

本process的算法复现按[独立实施标准](../standards/ALGORITHM_CONTRACTS.md)验收。全部operation的Input/Output按[FOUNDATION_API的typed规则](../standards/FOUNDATION_API.md#当前科学操作的typed输入标准)从公开参数机械生成；跨度/provider均转强所有权，codec先register_type再登记typed hooks。bounds给最小/最大块和spill能力，plan显式接受PlanOptions，不能读取全局当前块大小。资源key/不可拆相关组/上界项见[DEVELOPMENT本process行](../standards/DEVELOPMENT.md#模块plan和工作单元表)，具体数学/停止仍由本篇拥有。

有全局拟合屏障时拆stream→ordered_accumulator→barrier region：每cohort提交R内聚合状态即可退F，不能让唯一进展cohort等待下一cohort。科学迭代以显式epoch重新准入，work不重置。取消/失败只允许内建清理继续，模块不提交新科学计算；公开输出只在所有必需任务/写入/校验结束后交付。

实施交付包含所属模块plan/kernel/adapter/codec、独立小例/负例及真实service资源/取消证据，逐项门槛见[DEVELOPMENT](../standards/DEVELOPMENT.md)。完整编排代码可参照[校准实施例](../standards/CALIBRATION_WALKTHROUGH.md)，不得复制其中像元公式到其它process。GPU只预留同一批次/内存位置/完成事件边界，不实现后端。

## 可独立实施的算法合同

### 候选、匹配与接受的固定次序

检测仅定位候选，所有阈值来自SolverRequest和本篇已定64×64/5×5/NMS规则；MAD=0不造无限SNR，使用已知检测噪声或将该tile记不可检测。初始Gaussian质心只作天测，不成为science flux。先将采样格位置变换到FootprintSet.detector坐标，再构quad；按观测候选序、quad描述符距离、目录source四元组和镜像序形成稳定hypothesis序，分页/并行完成顺序不能改变它。

一对一匹配固定为半径gating图上的“先最大匹配数，再最小平方像元残差和”，同成本按(detector candidate序,source_id)字典序；使用确定性增广路径/最小费用匹配，不能逐星贪心抢最近点。粗互近邻可作候选诊断，不能替代最终一对一优化。固定匹配后按精确u64 source_id % 5 == 0留出、其余训练（不调用平台hash；不保证统计随机），训练须min_matches且每自由参数至少3条标量约束，留出至少一颗，否则本候选证据不足。平方距离的单位与match_radius_pixel同detector，不把Bayer步长当像元。

```text
按稳定序展开一个hypothesis（计max_hypotheses/work）
  由quad得到合法TAN初值，CRPIX参考点固定，不与SIP常/一次项重复拟合
  在gating图求最大数/最小cost匹配；冻结训练/留出
  从SIP0到请求阶：白化QR/GN、同NUMERICS停止与rank判定
  升阶仅当新增列可估且留出残差严格改善，边界未决不默认升阶
  要求匹配数、请求残差/精度、尺度/搜索域和完整WCS域准入全部满足
  第一合格hypothesis按序接受；并行较后候选须等前者终态
否则继续；穷尽/预算停止给unsolved及最优诊断，不发布球面帧
```

不承诺该有界heuristic找到全局最好解。sky误差先按基础转同坐标残差，模型/目录噪声不以训练RMS归零。欠定m<n、非驻点停滞、留出缺失、整域折叠都不得以低训练误差接受。WCS参数、同像元influence和目录根随解保留，缺DCR/RV/observer项具名。

### 资源与固定反例

最大候选C、假设H受Request，F计C² gating边/增广路径、训练设计m×p及p²/区间workspace；图太大先空间索引但不改变gating集合，超额度拒绝。A-MATCH用gated成本[[1,2],[1.1,不可达]]，正确为d1→s2/d2→s1两匹配，贪心只一对失败；A-UNDER含5星但自由参数更多须not_identifiable/unsolved；A-TIE倒序线程完成仍选同一第一合格候选；A-FOLD训练点好但全域Jacobian跨0不得接受；A-EMPTY留出为空不得“最大误差0通过”。
