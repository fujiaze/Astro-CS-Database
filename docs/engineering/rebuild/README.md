# ACSD 新核心架构规范

本规范以五份科学地基为输入，定义全新核心的模块、对象、接口、计算和独立验证边界。阅读顺序为[系统与算法关系](SYSTEM.md)、[统一对象与产品](OBJECTS.md)、[标准 API](API.md)、[计算与资源](EXECUTION.md)、[独立真值协议](VERIFICATION.md)。这些是目标设计，不是现有代码接口或已经完成的实现。

科学依据分别为[观测与坐标](../../science/foundation/OBSERVATION_COORDINATES.md)、[Gaia 与响应](../../science/foundation/GAIA_RESPONSE.md)、[真实球面 Drizzle](../../science/foundation/SPHERICAL_DRIZZLE.md)、[统计、sky 与合成](../../science/foundation/STATISTICS_SKY_COADD.md)、[独立证据](../../science/foundation/INDEPENDENT_ORACLES.md)。数学定义与适用域由它们拥有。本规范只确定其工程表达。

本次设计按负责人已验收这五份科学规范的指示开展；科学文件页首及历史证据中的“候选、待审核”是前轮交付状态，不作为重新推导或更改科学定义的授权，也不被本次设计改写。科学地基验收不等于新架构、生产代码或真实数据产品验收。

新核心遵循本目录；旧 architecture、UNIFIED_OBJECTS、PUBLIC_API、ARS、adapter、schema 和 detail 只解释旧系统，不约束新核心的节点数、容器、调度或 ABI。新核心尚未接替旧生产路径。产品目标保留三个独立命令、CPU 双平台、Windows GUI 外挂和单 signal 发布边界；冲突的旧科学/工程说明不能反向修改五份地基。

| 决策范围 | 定义位置 |
|---|---|
| 模块责任、调用顺序、算法复用边界 | SYSTEM |
| 统一语义、共享身份、状态、磁盘闭包 | OBJECTS |
| 独立调用、语言/进程边界、错误与并发 | API |
| 算子流式计算、预算、精度、内存与并行 | EXECUTION |
| 新真值生成、独立性、负例、审核条件 | VERIFICATION |

当前阶段交付架构规范与独立审核记录。架构接受后才建立独立重构分支、直接按本规范重写；本次不创建该分支、不改生产算法、不以文档提交触发实现。独立 Agent 的设计审核用于发现合同缺口，不代替负责人对架构的接受，也不伪称新增科学实验已经执行。
