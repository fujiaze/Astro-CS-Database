# 唯一实施任务书：重构main，而不是继续研发

## 工作目标与不可更改的范围

以 main 的真实生产三阶段和科学核为基准，把 `rebuild/acsd-v2` 整理成**能正常处理原始M42、采用真正内存流水线、低内存/高性能、易维护的模块化工程**。不增加BLUE/QP等统计优化、不开发新Flat场拟合/联合超分、不再维护学术研发计划；只有用户既有功能损失、已证实科学错误、性能/资源/内存调度缺陷才纳入任务。

旧main源码/数据只读；新V2沿现有代码结构做**逐项最小改造**，不能另开一个完全重写的工程。公开头仅`include/acsd/`有一份，模块只维护一套生产kernel。每项修复应在该模块实际生产链被消费者使用，不能只把文件编进库。已有order18自动NSIDE ROI代码留用，其科学有效性与全帧范围仍由真实WCS/成品解释。

## 实施顺序：科学/工程一起迁移，真正打通数据流

### T-1｜把完整科学文档和V2实现一起收敛（与所有T0–T7同步）

当前 [完整科学文档](../../science/README.md) 已恢复main主要科学分册和V2五篇基础数学，不能再删成单页摘要。每个算法迁移必须按[MODULES科学对应表](MODULES.md)检查：main正式调用使用哪些模型/公式/单位、已有V2是否更正过已证实的错误、输入数据/样本资格/PSF/WCS/response/noise实际是什么；保留科学推导、参数与适用域，**删除或改写旧旧架构/退役算法/测试轮次的段落，不误删数学**。

将科学内容归一为本目录的统一科学术语和真实产品语义，逐处修正import过来的旧路径/断链到当前V2工程文件、代码和源码所有权；无需保留过时审计/Research Pack/门禁文档。科学文档里的候选数值模型不得自动升级成生产强制算法；新功能不开发。API、对象及执行细节见[API](API.md)、[OBJECTS](OBJECTS.md)、[EXECUTION](EXECUTION.md)；这些详细合同同样不能被简略概述替代。**先检查真实公式与源实现再改文本，不靠文档字数或自动检查表宣称一致**。



### T0｜按生产链找出真实消费者和可复用代码（只读，直接为实施服务）

阅读main `run_phase1_block,run_phase2_block,run_phase3_block`及`module_adapters.cpp`所连接的算法库；找到每个旧计算kernel的输入/输出/真正使用者、单位、缓存/线程策略。以当前V2的 `normalize.cpp,mosaic.cpp,export_workflow.cpp` 对应实际调用进行对照，确认**已有V2可用代码优先保留**。不制作庞大对照矩阵或历史文档；只在当前修改源码注释/提交描述说明迁入缘由和真正调用路径。

### T1｜把V2已存在的Pool/Pipe/Memory/Storage真正接到计算路径（最高优先级）

涉及 `src/runtime/{host,pool,pipe,memory,storage}.cpp`、`src/workflows/{normalize,mosaic,frames_provider,product}.cpp`、`include/acsd/model.hpp`和真实 consumer。
- 实际从一个原始FITS块读取，到校准→后续源测量/Drizzle→LeafFragment→Mosaic地域消费→FITS分块写出。图中每一大数据边都应有真实不可变Buffer/ArrayView及Lease，给出谁申请、使用、释放，Pipe backpressure需真的用于生产。
- 重点修改`MemoryFramesProvider::read_leaf_block`：解释`request.leaves,halo,fields`，只交付该region请求叶和对应DQ/support，禁止整帧返回；ProductProvider同样在存储层可按需读取。所有读取者改用这个真实块协议，确保没有第二套隐式整幅`read_f64`路径。
- Mosaic只保留当前region有效帧与有序leaf索引；消除三次全域map/set合并、`Nleaf×Nframe`全图Selection/Weights矩阵和整个leaf_count单块限制。必须让已存在的Sky apply/Reject/Weight/Integrate **同region顺序消费而不是四次整幅重读**。
- Runtime统一内存/IO/线程预算，当前`groups×65536`代理计划不能用来藏真实分配。真实被申请bytes应在Lease存在期间计入内存账本，含缓存和异步悬挂块。被downstream持有时不可回收，取消能唤醒所有正在等待的Pipe。
- 若因科学原因要读第二遍同frame（比如先星测拟合小模型、再Drizzle），从同一只读原始源块缓存或重读原始文件，不形成层层复制、整幅临时产品的“文件流水线”。独立三命令的跨进程产物可以正式存储。

T1完成时，工程人员能够从**具体一条真实frame的每个block**跟到下游consumer和release，而不是展示一个独立demo Pipe。

### T2｜Normalize：优先迁main的成熟kernel、保留高阶真实分辨率

具体文件与科学关系见[MODULES M01–M05](MODULES.md)。分批把main校准/坏点、IPV/Gaia、WCS/SIP、源/PSF、真实filter响应/测光搬到V2已有模块；逐块流动且不重写已有成熟核。对已能跑的order18 Drizzle补全全参与帧共同order预规划/实际PSF、并落实main的stripe/cache/touched Tile性能。

一帧4096² Normalize处理期间追看实际原像元是否参与Drizzle、是否把星点错误变成一块HEALPix均值/丢支持、是否产生空Tile和大块未归还缓存。不足的WCS由旧IPV生产路径补回，不能因某张原FITS头缺WCS直接走伪默认。

### T3｜Mosaic：恢复传统排异与加权叠加，不再研究coadd统计模型

按[MODULES M06–M10](MODULES.md)：旧main Coverage/Sampler/Sky Plane/Rejection/Integrate的正确计算核进入V2一个正常消费者链。拒绝方法沿main真实可达选项和适用N，旧明确错误用相同物理推理作最小更正；main `p2_integrate_pixel`的六态、`Σwx/Σw`和support语义保留。

V2 `src/coadd/`的BLUE、QP、GLS/完全Γ模型、single_max_loo新路径不应成为传统叠加前置，也不需为本任务实现这些缺口。若它们仅属于旧V2研究规格而无本次生产消费者，代码尽量删除/隔离其生产入口，不留需要维护的两套默认实现。函数或文件名可维持兼容，但明确真正运行的是传统kernel。

Sky从main的联合共同面与相对加性差迁入，控制数据只产生需要的小型全局模型，块处理阶段按冻结同一Sky系数校正。别为“去缝”研发全新flat屏优化；有真实不可辨认乘性差时指出来自哪组校准/光度输入和实际剩余，不假装最终背景一致。

先在现有M2/M5真正不同面板的512²/1024²产品上修通**完整Mosaic→Export**，随后扩大4096²。不要重跑已合法的Normalize块，除非算法/身份依赖变化。

### T4｜Export：恢复标准HiPS、高性能Tile缓存与真正流式FITS

按[MODULES M11](MODULES.md)。标准512 Tile层级、leaf order和像元角尺度分清；保留main `p3_resample.cpp`共享LRU/缺失负缓存与 `P3FitsStream`。Mosaic输出真实分区，Export以目标WCS分块请求球面数据，writer以固定顺序写FITS。只有最终完整文件/跨进程可恢复点才由Storage发布；避免同时常驻完整4096²输出+mask+coverage。

真正用Astropy/CFITSIO打开输出，检查星位/PSF/背景/NaN/单位/头/空间覆盖；不是单看WCS roundtrip或二进制hash。

### T5｜统一资源调度和性能收口（与T1–T4同步进行）

逐模块审查main既有性能策略是否被实际迁入（见[PERFORMANCE](PERFORMANCE.md)）：Drizzle stripe/touched/LRU、Sky稀疏/块解、P2动态microchunk、Rejection小stack scratch、P3共享TileCache/流式写；同一V2线程池/内存scope执行，避开N×N嵌套OMP。测真正`wall,CPU,active workers,RSS,IO,decoded bytes,cache hit,CSR nnz,touched tile`，按实际热点而非“再添加算法”修。12GiB进程RSS上限不变；资源不足缩block/并发/安全spill，不降order、少处理帧或更改科学值换速度。

### T6｜由主Agent阅读实际代码与真实天文图像，修到科学与工程收敛

按[REVIEW](REVIEW.md)做人工可推理的代码走查；真实原始像元→校准→WCS→Drizzle→Sky→Reject→权重→输出逐点检验真实消费者、单位和量纲；对 M42 star/nebula/面板交叠的实际图像计算和审视。对真正发现的问题回到对应模块修复，再分析改变的后续数据边。**没有预设的“测试全部绿=完成”门禁**；不会因某些夹具通过或某个反例失败自动判整工程胜负。需要数字时从独立算式/真实原图给出其物理意义。

### T7｜真实目标：全幅、多面板、完整四通道

在T1–T6真实管线工作后，用高阶4096²真实帧和多个M1–M6面板验证，再复用已有缓存处理完整196个候选的Hα/R/G/B，明确每帧合法参与或由于真实设备/标定问题排除。共同目标WCS导出四张FITS，交付真实文件、真实空间覆盖、面板接缝、星点形态、内存/性能及仍未知的物理项。**不能把以前三帧或同M5的旧结果当正式完成**。

## T8｜工程消费者收敛后，清理旧夹具/验证产物与无效自动入口

`validation/` 当前仍含源码构建直接引用的少量诊断driver，以及大量历史输入/日志/证据包。**不把这些既有文件当作科学正确性的评判规范**。与T1–T7同步检查 `CMakeLists.txt`、代码生成器、实际CLI和持续使用的本地调试工具，逐项确认哪些验证/夹具/报告没有生产或必要编译消费者；**删除无消费者的历史夹具和多轮结果**，不用复制归档目录，不再新增按轮次编号的永久产物。仍需要帮助理解真实代码的微小自测可以保留为开发者本地工具，但不具有自动放行资格。不要在未经依赖分析前整删 `validation/` 导致当前工程无法编译。当前历史`docs/audit`、`docs/workplans`和自动`check_design`已从工作树移除。

## 工作过程与合并安排

实施顺序按真实数据边推进，**每写一个小块消费者就把其上游接通并做必要真实值检查**。允许独立子Agent同时核对IPV/PSF、旧Mosaic方法、Tile/IO性能、二维天区空间映射，但共享`include/acsd/`和`src/workflows/`的改动由主Agent串行合并；不留两个相互竞争的Workflow。文档由实际代码改动的Agent更新本组对应段落，工程到位后再统一删掉过时说法，绝不在没改源码时宣称管线完成。

发现bug→查真实生产调用→找对应旧算法/物理依据→改代码→看真实数据与分配路径→检查受影响消费者→继续，不因缺陷自动停止另开任务。只有原始数据/真实物理信息或外部权限/机器资源实在缺失，记录最小阻断并保留可复用的工作。

## 明确不做

- 不再实施22个Process逐个“全新数学复现”的研究计划；
- 不再发展BLUE/非负QP/完整相关协方差优化、LOO新算法、乘性flat联合求解器、SACoadd或超分；
- 不再按历史Issue编号/审核轮次/门禁、夹具、测试覆盖率组织工作；
- 不为每个处理节点建立产品文件或同时持有一张整图作为“内存管道”；
- 不为快速得到exit=0降低NSIDE、删掉原有排异/光度/PSF或伪造噪声精度。

**工程完成的证据必须来自正确的实际代码及真实工作负载。** 源码重构完成后继续修改与代码相符的精简文档，无须新增重复任务书。
