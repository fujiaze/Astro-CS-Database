# C1｜Runtime、内存管道、资源账本、Storage与故障恢复彻底收敛

## 核心目标

以已经存在的 `VirtualComputePool/Task/Pipe/Storage/ProcessHost` 为**唯一固定基建**，实现受预算约束、跨多个科学模块可靠运行的 CPU 科学计算，不创建第二线程池、第二缓存/分配系统、隐式全局上下文或没有所有权的异步指针。此文档讨论工程状态与资源而不改变任何 scientific estimand。优先审阅 `src/runtime/host.cpp`、`pool.cpp`、`task.cpp`、`pipe.cpp`、`memory.cpp`、`storage.cpp`、`wire.cpp`、`src/storage/local_storage.*`、`src/resources/cpu_resources.*` 以及 GUI/CLI 的真实使用路径。

## R0：正确性模型、状态机与所有权

1. 从工程需求正向推导 `ProcessHost` 的状态转移：validate→plan→reserve→run→finalize/seal→publish→release；异常、用户取消和资源不足从各阶段进入安全终止。写出每条状态边允许持有什么 lease、task、scratch、published object；定义不变量“未seal不发布、借用未完成不释放、有足够credits才开工”。
2. 将 `Task<T>` 的 pending/ready/running/suspended/canceled/failed/completed 明确与CPU占用区分：等待I/O不持CPU token，不在同一个满线程池内`future.get()`造成死锁；on-complete callback不得偷偷执行昂贵科学计算。
3. 共享所有权必须能解释数组slice/view、跨异步边界、发布产品后的对象寿命；取消future不等于底层I/O已停。每个尚在运行的读写、buffer/Storage backing、completion都要等待真实结束再释放。
4. Region与replanning处理F/R/E（工作、驻留、在途）；额外递归科学反馈显式重新准入，不把一轮任务的work上限清零以实现无穷迭代。
5. 禁止忽略退出时的writer错误、吞掉子进程异常、错误状态从low level被转换成空合法产品。

**工程证明方法**：给出可读状态图、关键函数在真实源码的转移证据、临界区/原子操作及一次性发布推理；小型故障重现作为反例。测试线程同时完成不是“无并发错误”的形式证明。

## R1：真实内存账本与有界执行

- 全面核对分配成本：原生byte buffer、`std::vector` 容量、字符串与编码、变长source列表、CSR nnz、工作队列、解压/压缩/hash暂存、FFTW/CFITSIO（如有）、MPFR/GMP精确数、QR/FFT稠密/稀疏workspace、异步I/O在途以及全局控制节点。不得只用 `sizeof(array element)` 估算。
- 一次分配的所有者唯一；共享lease只计一次；命名scope树允许审查命令、invocation和task各自高水位，但不能把每个子scope高水位求和当成进程真实峰值。
- 任意request先按size_t/uint64有界乘加检查溢出，`shape×dtype`、`begin×itemsize`、tile/order/nside及拆分坐标尤其要验证；应区分合法资源拒绝和整数溢出导致的越界。
- 真实 `32 MiB/256 MiB` 等低内存配置不能改变科学容差、rank、mask、PSF、面亮度估计量；允许spill、重规划、减少并发或明确resource_exhausted。不是只跑一个“低内存模式返回success”的smoke。
- 并发任务需要公平前进：P=1、4、8、I/O慢/快、E=0、最低驻留工作单元R、fork/join/barrier、嵌套大对象、取消同时发生、多个科学Process组成链，必须有可以解释的调度不变量和失败上界。
- 数值可重复性：浮点规约次序/分块/线程数可以造成差异；严格证书必须依预定模型外包差异，普通点值必须在明确条件误差内。固定归约键是工程行为，不等于无需评估舍入影响。

## R2：内存管道与I/O

1. 证明 `Pipe` 的生产、阻塞、关闭、反压、消费和abort规则：`upstream capacity` 及任意多个消费者不允许无界排队；消费者异常时生产者必须观察错误并停止，否则存在无限等待或发布半成品的风险。
2. 至少以真实三段跨模块流水线复核 `reader→校准→图像块处理→writer`，及 `fork/join/barrier`、工作区复用、稳定排序、跨GeometryRevision重算；单个Pipe的测试不足以证明生产工作流不会死锁。
3. I/O提交应有请求数和byte额度；块流复制必须计CPU/内存时间；可选择原生异步或有限I/O worker，但不能在I/O callback里偷偷使用未计费计算。
4. 大数据资源执行要用真实FITS图像、多帧+多级索引、临时缓存/有效读取范围；不能先把全图载入内存再声称“后面是streaming”。给出运行中最多驻留哪些数据、哪些spill、哪些能复用。
5. 取消/异常后追踪队列、lease、磁盘文件、未完成任务直到完成清理；再次启动应没有冒充complete的旧状态。

## R3：Storage 的科学对象闭包

- 明确产品包含哪些**完整科学闭包**：native来源与必要变换、response与参考SED、WCS/footprint、几何primitive和R配方、当前signal、noise random roots与共享身份、PSF/背景basis及impact、GeometryRevision/sky/Selection/WeightSolution依赖、误差状态、支持/数值界。完整闭包以科学能力为准，不是任意JSON字段越多越好。
- 读写同一对象需有身份：类型/单位/坐标/shape/leaf order、deterministic content hash、物理相关性、不可变依赖清单；重复读取相同图不产生“新的独立观测”。
- 检查 `Dyadic` 精确typed read/write、正负零、NaN规范、大小端、signed/unsigned宽度、二进制端点与十进制显示不等价；所用现有数据格式只维护当前一套，不为旧V1/V2写兼容读取器。
- `ArrayBuilder` 真正begin/append/seal/abort有严格状态机；写失败、short-write、磁盘满、checksum错、意外结束、进程被kill、重复源及未知recipe应么拒绝么恢复合法上次完整产品，不能产生半成品complete。
- 文件写入与原子发布的契约不仅要检查单个数据文件，还要检查manifest与全部依赖是否一同就绪；临时文件清理应可幂等。防止锁顺序反转、缓存腾挪导致死锁、旧Lease在revision变化时被提前撤销。
- **跨进程复现**：产品发布后彻底结束进程；原始FITS卸载/隔离后，新进程从完整产品读`signal、covariance action、PSF/response、支持`并执行一轮合法geometry refine；如果声明产品只具signal能力，允许公开标记signal_only，不能伪造已知noise。
- 完整产品重开不是运行时`Host`的内存快照；要明确哪些可持久化并重建的recipe安全、哪些只可耗尽/拒绝。不允许加载产品内任意代码。

## R4：诊断与运行故障可观察性

- 默认每个invocation有时间/CPU/bytes/峰值、取消和失败原因，不需要科学过程单独实现第二套日志。DEV_MODE开关只增加有限的中间数据与探针、科学结果在OFF/ON应一致或有明确浮点扰动界。
- probe要能看见真正被消费的输入/输出和单位；例如Noise变换后根是否同一、Drizzle R字节版本、SkyFit β及selection decisions、Export 支持占比。不强制在所有正常运行中保存巨量中间数组。
- probe/OOM/磁盘满/日志写失败不能把本来正确的科学结果变成“科学算法成功但产品损坏”或改变估计模型；记录限制与丢弃情况。错误观测只可用于诊断，不是更改科学结果的隐式控制变量。
- 诊断过程不能用一份新的二次计算冒充“探针”，造成与生产路径不一致。

## R5：第三方依赖、内存安全与并发安全

明确 CFITSIO/WCSLIB/HEALPix/MPFR/GMP/BLAS/Qt等实际使用的来源与许可；各库的线程安全、全局浮点rounding环境、allocator、locale、文件路径和异常处理，与本项目唯一CPU池的关系需以源码/官方行为证明。尤其注意MPFR自身有向模式与编译器优化、`fenv`线程/优化交互。可以用独立小探针、ASan/UBSan/TSan、heap分析仪做故障观察，但不能把“跑过sanitizer”称为所有线程状态已证明安全。

## 实际实验矩阵（有限且有目的）

| 实验 | 必须有的对照/原始证据 | 判定依据 |
|---|---|---|
| 小P=1真实三Process链 | 明确工作单元与sleep/I/O延迟；完整Task/lease轨迹 | 没有死锁；完成后资源归零，输出等价 |
| 低H（32/256 MiB） | 同一冻结请求、科学阈值；1/4线程、cache on/off | 合法spill或明确拒绝；无伪科学变更 |
| 大型真实图像 | 读取中峰值RSS/heap、I/O吞吐、nnz与归约 | 不无界增长；最终产品科学身份不随块大小改变 |
| storage短写/满盘/kill | 原完整产品/失败中间文件与新进程重开 | 永不发布半成品complete；无不可解释锁死 |
| unknown/损坏/重复源 | 精确破坏到被消费字段而非无关hash | 正确因果拒绝，不泄漏旧状态 |
| DEV OFF/ON | 同一输入/输出B、R、C、manifest、计算成本 | 数学等价及诊断失败隔离 |
| 多线程取消/并行publish | 时间顺序交错、join/barrier、文件/lease | 无UAF/双释放/重复发布/永远挂起 |

**完成标准**：实际生产调用路径经逐状态/所有权推理成立；足够有判别力的失败实验没有发现违反不变量的反例；任何尚不能证明的第三方行为明确条件化，并具有保守安全边界。工程报告包含实际性能/内存/故障数据与复现环境，不以运行脚本汇总PASS替代推理。
