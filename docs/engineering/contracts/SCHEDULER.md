# 阶段调度

每个 normalize、mosaic、export 命令建立独立Runtime、调度与内存管线，跨阶段只交换已发布产品。资源来源和并行轴见[数据流](../architecture/DATA_FLOW.md)，对象交接见[命名块](PIPELINE_BLOCK.md)，线程/数组寿命见[Runtime](RUNTIME.md)及[所有权](OWNERSHIP_LIFETIME.md)。

## 1. 实际接口与计划

`lib/include/acsd/core/scheduler.h` 声明 `Scheduler`、`NodeSpec`、`NodeStatus` 与 `NodeFn`。NodeFn为`(node_id,RunContext&)→Result<void>`；节点声明依赖、operation对应执行器、resource_class、内存估计、最小/最大worker及估计来源。`add_node/build/run`负责建图与执行，不能把设计中的泛称`run(stage,config)→manifest`误写成现有C++签名。

计划校验唯一生产者、合法依赖、无环和资源/生命周期。估计值与实际观测分开：不可估计时给原因，不把未知内存写成真实0。节点状态为PLANNED、QUEUED、RUNNING、COMPLETED、FAILED、CANCELLED、SKIPPED，状态快照含node_id。

## 2. 阶段执行形态

| 阶段 | 目标形态与当前核对点 |
|---|---|
| Normalize | WCS→科学源/PSF→光度/噪声→球面完成的真实依赖；帧级并发和帧内工作共享预算，预取和查询复用不得跳过科学条件 |
| Mosaic | 全局覆盖/背景模型和局部tile消费具有不同范围，不能将全局拟合简单描述为每窗口独立；内部统计权按同估计量构造 |
| Export | 有界子块读、目标WCS重采样、写出；真实流式程度、内存上限和参考结果按实际路径核对 |

这是设计与实现阅读路径，不声明三种形态都已完整接入。真实八/七/五节点和局部session区别见[模块地图](../architecture/MODULE_MAP.md)，缺口见[实施差异](../architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 3. 租约、内存与取消

Scheduler持有ThreadBudget并注入RunContext。节点根据计划申请真实租约，帧内和帧间不能重复占有同一授权；模块不另设全局预算或私建长期池。`executor_runtime.h::shared_work_executor`按预算实例复用池，只有预算源确实唯一时才能得到该范围的统一执行资源。

内存估计和压力治理控制派发/在飞对象，不能在像元计算中改变科学精度。队列同时限制条目与在飞字节，具体见[异步I/O](ASYNC_IO.md)。CPU/内存利用率只测量，不设产品审批或性能退出门。

cancel置调度器及当前RunContext的token，节点和内核按明确粒度响应。停止派发后等待在飞借用结束再回收，I/O收尾保留真实发布状态；已可见但未确认持久化的对象不能一律删除。进程退出码在CLI映射，详见[错误合同](LOG_AND_ERROR.md)。

## ARS 事件驱动调度的目标准入顺序

现有 `Scheduler::run`、`heavy_gate`、`MemoryPressureGovernor::choose_victim` 仅提供节点派发与压力治理，`evicted` 标记不等于内存已回收。目标先确认科学依赖/typed revisions，再为 `new_output+scratch+new_AIO_buffer+staging` 申请原子增量 credit/IO slots，最后才按关键路径、预计耗时、资源驻留、I/O 背压、公平 aging 排序。无法满足时放 scheduler waiting queue，不让 compute worker 阻塞于 MemoryCredit；高压只牺牲可回滚且未提交的 Frame，join CPU、drain AIO、释放实际 bytes 后从相同 occurrence 完整重算。具体条件、未知估算处理和伪代码参见 [ARS 设计 §4–7](../architecture/PROCESS_RUNTIME_DETAILED_DESIGN.md)。

## 4. 数值复现

归约按声明的帧/块/像素顺序与算法进行，整数身份、资格和计数精确比较。跨worker/ISA的浮点结果按模块数值域与[测试标准](../testing/TEST.md)容差比较，固定归约结构与误差限分别检查。不能由“无跨星归约”或“任务按下标回填”推出整条科学链逐位一致。

优化与工作窃取只改变合法执行安排，不能改变输入有效集合、模型和发布物理定义。确定性参数、随机种子、配置、二进制、输入和实际worker随实验保存。

## 5. 调度探针与运行事件

`eng/contracts/schemas/scheduler_probe_event.schema.json`描述独立探针行：必填`ts/stage/kind/value/unit`；kind为node_wall、queue_wait、block_birth、block_death、rss、io、worker_busy、cache_hit，可带node/block/bytes/frame_id/window_id/worker。ts为进程内单调秒，单位为s/B/1。

这些探针行不满足CLI运行事件的必填字段与kind枚举，不能直接称与`jsonl_event_v1`同格式兼容。写stdout时必须显式适配到[CLI协议](CLI_PROTOCOL.md)，开发细分事件另受[探针规范](../standards/DIAGNOSTIC_PROBES.md)控制。schema允许某字段不代表生产已有对应采样，计划值不填观测值。

核对DAG失败、预算超卖、内存回压、取消/异常、数组寿命、跨worker数值和真实trace来源。旧参数与过程叙述保存在[来源档案](../../../实验/documentation-refactor/docs/SCHEDULING_SOURCE.txt)；本轮没有执行调度或科学运行。
