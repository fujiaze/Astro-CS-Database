# 可观测性

可观测性提供运行事件、日志、trace、资源采样和开发诊断，使调用者知道实际执行了什么。依据[最高设计](../../ACSD_DESIGN.md)、[日志错误合同](../../engineering/contracts/LOG_AND_ERROR.md)和[开发模式](../../engineering/architecture/DEV_MODE.md)。

## 通道与消费者

| 通道 | 入口/生产者 | 消费者和边界 |
|---|---|---|
| stdout 运行事件 | `lib/infrastructure/cli/jsonl.h` 与 `protocol.h` | GUI、外部运行器；`kind/sequence` |
| 结构化日志 | `observability/logging/log_event.py` 参考格式；Core logger 需适配 | 操作员和诊断；`event/seq` |
| 节点 trace | `TraceEvent`、Runtime 和调度器真实调用点 | replay、[运行图](../../engineering/resources/observability/RUN_GRAPH.md) |
| 生产资源记录 | `cli/resource_recorder.h` | `resource_timeseries.csv` 与运行摘要 |
| Python 资源伴随器 | `observability/monitoring/monitor.py` | `monitor_timeseries.csv`，独立列合同与指纹链 |
| 开发性能与快照 | `acsd/probe.h`、`acsd/dev_diagnostics.h` | 开发工作区；[探针规范](../../engineering/standards/DIAGNOSTIC_PROBES.md) |

通道之间通过 run/node/frame 身份关联。计划中的 worker、provider、哈希和入口不能填入真实观测字段。运行图有 `PLAN_ONLY` 节点不代表节点已执行。不同 CSV 不因都含 CPU/RSS 而互换。

## 调用与数据寿命

模块报告事件或只读观察 view，汇聚与运行层负责文件句柄、目录、队列和收尾。异步写出先复制或持有显式租约，保证科学对象回收后不再借用。模块不自行建日志线程池或选择任意落点。

正式日志默认 `<output_dir>/logs`，错误处理和运行策略集中在[日志系统](log_and_error_system.md)。快照位于独立开发工作区，不能成为下游科学输入。正式 cache/spill 按科学消费者生命周期保留，与开发快照分开。

## 资源测量

资源统计定义与评价归[性能模型](../../engineering/resources/PERFORMANCE_MODEL.md)，字段采集归[资源监控合同](../../engineering/resources/observability/RESOURCE_MONITORING.md)。CPU、内存和线程指标用于记录与分析，生产命令不因利用率阈值改变退出码。外部监控脚本的 `--gate-required` 是该脚本的分析模式，不成为项目研发审批或产品发布授权。

系统 iowait、进程块 I/O 等待、缺页速率、逻辑读取和物理读取分别报告。活动节点数不等于活动计算线程数；VmSize 不等于 Windows commit；无观测不能填真实零。遗留字段名保持可解释，但统计结论以实际来源和口径为准。

`ACSD_NODE_TRACE`、`ACSD_LEASE_TRACE`、`ACSD_P1CAP_TRACE`、`ACSD_UPM_PROFILE` 是已有调用路径的观察选择器，应受唯一开发编译边界控制；其各路径是否全部隔离仍需核对。`ACSD_SKY_MAX_ITER` 与 `ACSD_UPM_CG_MAX` 会改变算法迭代，属于实验数值旋钮，不能作为被动探针推荐给用户。

## 失败与验证

正式日志/事件写失败上行到 CLI；开发诊断写失败记录缺失原因。取消时有界收尾，错误保留原始归属。验证分别检查事件 schema、数据来源、实际调用计数、目录与哈希、缺失样本、并发寿命和 ON/OFF 科学结果，不能以 trace 行数代替科学验证。
