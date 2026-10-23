# geometry.overlap：曲边交叠与支持

状态：目标process文档；对应科学函数可已有部分实现，标准注册/生命周期/探针尚未据本文落地。本轮仅修文档。公共process标准见[基建骨架](../SYSTEM.md)，字段/输入协议见[API](../API.md)，共享原语见[数值标准](../standards/NUMERICS.md)。

## 注册与边界

| 项 | 该process的固定声明 |
|---|---|
| process_id | `geometry.overlap` |
| 所属模块与代码归属 | `src/geometry`；本模块负责算法及typed adapter |
| 注册operation白名单 | `make_footprint_provider,detector_footprint,footprint,measure,compute_overlap,refine_geometry,footprint.forward_interval,footprint.boundary_interval,footprint.derivative_interval,refinable.open,refinable.refine`；一个操作映射一个本模块typed adapter，不能由字符串调用任意成员 |
| 输入与输出 | 规范WCS/FootprintProvider/真实footprint、order/pixfrac/GeometryBudget → Overlaps或GeometryRevision。 |
| 数学依据 | 五份[科学基础](../README.md#科学基础)与本篇算法条款；不继承v1内部接口 |
| CPU与未来后端 | 当前CPU；仅筛选/规则批次可扩展；严格裁剪/超越包含/不稳定分支CPU，不实现GPU几何。 |

## 算法与失败合同

affine-strip cut-cell主核、target-domain A fallback、严格支持Boolean；所有候选/粗未决质量完整。

Ω/A共同源约束；pixfrac只收缩一次；revision替换整个依赖闭包，不混旧R与新权。

输入在基建通用形状检查后由本模块校验科学语义；失败沿Result/Assessment/Issue返回，不exit。缺能力与未达精度/资源上限分别具名，不拿空数组或零variance伪装成功。数学模型改变需先修科学依据，再改本模块及受影响消费者。

## 执行、寿命与观测

按source/Box有界分区并行，候选爆炸spill；MPFR/GMP位长计费，复用总work，不全天空常驻。

validate/plan/run/finalize遵统一状态机；输入为不可变Ref/有界借用，plan只给保守资源模型与可行分块，正式数值计算只在run。输出用Storage封存后一次提交；partial仅按科学状态消费，工作流不把partial当完整发布。取消检查、容量arena、跨调用输出寿命与不可控库分配余量按[EXECUTION](../EXECUTION.md)。

默认span及资源快照由框架自动包围每个invocation，带process_id/operation/invocation_id。本process只登记：`normalize.drizzle.geometry`、`normalize.drizzle.overlap`、`geometry.precision_upgrade`。注册表中的pipeline前缀是稳定语义标签，不限制该process只能从对应命令调用；需要观察其它已有边界时在本篇登记新id并由构建校验唯一性。I/O、资源及FITS writer探针由基建service登记，带当前invocation归因，不在本模块重复登记。数组/模型快照仅复制已在手的选定块，不触发新科学计算/设备读回。

## 独立验收与维护

最小反例：G01/R01；曲边、孔洞、接触、极点、预算耗尽及原语共同相关。还需通过公共注册/取消/资源/单线程与多线程矩阵；计时必须包含本process的读取、执行、封存及明确分摊的共享成本，不能用失败更快冒充优化。

改算法时先改本篇的数学/输入输出/停止与反例，再改模块typed adapter与实现。仅新增本模块已声明operation无需改框架调度分支；改变共享类型/资源规则才联动公共标准。具体函数签名只在唯一公开声明落实，文档中的角色名不表示头文件现已提供新接口。

## 算法详细条款

### 算法细则

采用科学分册 §6 的 affine-strip 为唯一生产交叠主核，A 的 target-domain 包含积分是同入口 fallback；B 是独立验证，不在主路径重复算全部面积。源环首期支持简单 polygon 和孔洞，边为连续 detector 直线；收缩以 contraction_center 作同一 affine f 作用于全部环，hole 随之收缩。ring 必须不自交，外环逆时针、洞顺时针且严格内含；多外环须内部分离。一般 composite G 核保留 constituent 原生 footprint。

每次计一个 interval box/候选裁剪/求积面板为1 work，精确有理运算每4096 limb 操作再记1，MPFR 每次超越运算按 ceil(bits/64) 记 work；全部累计受 Context 与子预算双约束。最多4096候选格/节点，超过先细分；资源不足保 coarse_unresolved，不枚举全天空常驻。累计采用有向 dyadic ledger，精度升级是128→192→256→…每次×2至 max_bits。相邻两级细化的界宽下降比>0.9连续3次且舍入宽≥总宽一半时触发升级；非此情形继续空间细化。到 max_depth 或极点 chart 无定义触发 target A；fallback 与主核共用剩余配额，不能重置。

支持 S=union_j(D_j∩H_p) 需要单独 Boolean：target face dyadic 树节点对各 drop 判 inside/outside/undecided；任一 inside 即 union inside，全部 outside 即 outside，其余 undecided 并细分。子域 OR 后计一次面积，洞照包含谓词处理。精确模式可持久化 primitives+树，有限边界质量在 budget 内才 bounded；不以 Σa 或 N 代替 union。

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| FootprintProvider全部方法、footprint/measure | provider.model锁同Wcs hash，forward/boundary/derivative enclosure包含全请求域；boundary_curve为Bernstein表示，t必须在参数域；measure原生域Ω，footprint显式f收缩后返回曲边；measure与compute_overlap不擅自重复收缩 |

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| compute_overlap | source单个footprint、order、f、预算；返回所有候选及粗未决域。max_bytes由子预算与Context最小者限制；资源失败仍可返回有效面积。未知provider方法unsupported而非自动点采样 |

### 公开操作语义

| 操作 | 本模块合同 |
|---|---|
| RefinableGeometryProvider.open/refine、refine_geometry | open锁hash与闭包，refine依GeometryBudget生成immutable revision；base不存在missing_dependency；不能refine unsupported。GeometryRevision.next成功后事务替换依赖，按[API](../API.md)与[EXECUTION](../EXECUTION.md)的闭包失效/重建合同执行 |


### 构造与连接细节

- detector_footprint(footprints,source_row)：流式解析一条detector足迹，不需要WCS；绑定后才可几何。

### Provider科学能力的登记边界

公开保留的FootprintProvider区间方法绑定在本process内：forward_interval(domain)、boundary_interval(curve,parameter)、derivative_interval(domain)逐项typed request/response，不能绕过统一校验或构造另一provider。refinable.open/refine同属本process的几何模型版本能力，静态登记并按真实支持报告。若实际声明新增其它可对外执行的科学方法，先补本process binding/文档/独立例，再生成schema；不承诺旧方法名长期兼容。


### 可复用计算模块组合

目标组合：footprint/WCS验证 → 有界Box队列 → numerics精确裁剪/ledger → coarse或certified块 → 流式spool封存。

这些是[COMPUTE_MODULES](../standards/COMPUTE_MODULES.md)的强类型库调用，沿同一虚拟计算池、预算和取消域执行。I/O与独立块可重叠，pipe/在途上限和最小进展容量在plan给出；不在本process新建池/磁盘worker或重写公共原语。小型同步helper可直接调用，大块与异步等待交Task组合器，顶层invoke在真正完成后返回。
