"""Archive reviewed scheduler/queue contracts and explain their real interfaces."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DOCUMENTS = {
 'docs/engineering/contracts/SCHEDULER.md': r'''# 阶段调度

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

## 4. 数值复现

归约按声明的帧/块/像素顺序与算法进行，整数身份、资格和计数精确比较。跨worker/ISA的浮点结果按模块数值域与[测试标准](../testing/TEST.md)容差比较，固定归约结构与误差限分别检查。不能由“无跨星归约”或“任务按下标回填”推出整条科学链逐位一致。

优化与工作窃取只改变合法执行安排，不能改变输入有效集合、模型和发布物理定义。确定性参数、随机种子、配置、二进制、输入和实际worker随实验保存。

## 5. 调度探针与运行事件

`eng/contracts/schemas/scheduler_probe_event.schema.json`描述独立探针行：必填`ts/stage/kind/value/unit`；kind为node_wall、queue_wait、block_birth、block_death、rss、io、worker_busy、cache_hit，可带node/block/bytes/frame_id/window_id/worker。ts为进程内单调秒，单位为s/B/1。

这些探针行不满足CLI运行事件的必填字段与kind枚举，不能直接称与`jsonl_event_v1`同格式兼容。写stdout时必须显式适配到[CLI协议](CLI_PROTOCOL.md)，开发细分事件另受[探针规范](../standards/DIAGNOSTIC_PROBES.md)控制。schema允许某字段不代表生产已有对应采样，计划值不填观测值。

核对DAG失败、预算超卖、内存回压、取消/异常、数组寿命、跨worker数值和真实trace来源。旧参数与过程叙述保存在[来源档案](../../../实验/documentation-refactor/docs/SCHEDULING_SOURCE.txt)；本轮没有执行调度或科学运行。
''',
 'docs/engineering/contracts/ASYNC_IO.md': r'''# 有界异步 I/O

本合同说明producer/consumer队列及reader寿命，依据[并发标准](../standards/CONCURRENCY.md)与[调度合同](SCHEDULER.md)。参考实现为`lib/algorithms/coverage/include/astro/phase2/async_io.h`，通用模板存在不代表科学生产路径已采用它。

## 1. 类型与容量

`BoundedAsyncQueue<T>`持deque、每实例mutex及两个condition_variable。T按值传入push后移动入队，成功出队时所有权移给消费者；失败push也可能销毁已传入的值，调用方不能继续借用原buffer。条目自身负责数组/句柄的RAII，不能把裸悬空指针作为有所有权条目。

`bounded_queue_capacity(budget,item_bytes)`当前返回至少1，item_bytes=0也返回1。该函数不验证总预算足以容纳一项，不能把结果称严格内存上限。调用方先检查条目尺寸并把buffer、控制块、在飞producer/consumer及scratch纳入预算；内存不足时调整分块或明确失败，不凭最低容量1宣称预算满足。

## 2. 正常与取消语义

| 操作 | 当前行为 |
|---|---|
| push | 满时等待；closed或有错误时返回false，否则移入并唤醒consumer |
| pop | 空时等待；有非空error立即nullopt；closed且空时nullopt；其余移出首项 |
| close | 不再接收新项，唤醒双方，已有项可排空 |
| cancel(reason) | 保存首个非空错误并close，唤醒双方；非空error使后续pop停止消费 |
| error/has_error | 锁内读取错误；没有独立的取消标志 |

因此错误取消不等于正常close排空。cancel后队列里未消费的对象仍由队列持有，join后销毁；不能承诺“错误后消费者排空全部剩余”。空reason不会置has_error，实际近似close语义，是当前实现限制。队列没有自身timeout或重开方法，完整流水线须在上层处理等待/取消和重建。

## 3. Reader与线程

每worker使用独立AioHipsDataset/reader，或由单I/O线程串行调用同一句柄；不要推定CFITSIO任意共享句柄可并发读取。条目若引用reader会话，该会话覆盖最后消费者。数据集关闭、缓存淘汰、数组复用都等待在途借用结束。

正常收尾为停止producer→close→consumer排空→等待writer完成并关闭文件→join所有worker→销毁队列。错误时保留首错和后续收尾错误，取消唤醒后释放各owner对象。正式发布只有在所有写任务完成后登记清单；开发诊断丢弃策略另按探针合同。

## 4. 生产接线与验证

当前Mosaic采样/集成可由Runtime租约下的worker独立读tile，不因此等同已使用本队列解耦I/O与计算。具体接线从sampler.cpp及节点adapter查起。旧文列出的测试路径和用例名要先取得文件、输入、命令和结果，不能将名称当作通过证据。

验证容量背压、满/空时close/cancel、空reason、move后失败、reader释放、在飞内存、读写失败与join死锁，分别测实际I/O×compute重叠和科学数值等价。队列size受限不证明总RSS受限，模拟sleep重叠也不证明真实磁盘和科学内核性能。原文保存在[来源档案](../../../实验/documentation-refactor/docs/SCHEDULING_SOURCE.txt)，本轮不改队列代码。
''',
}

if __name__ == '__main__':
    archive = ROOT / '实验/documentation-refactor/docs/SCHEDULING_SOURCE.txt'
    if archive.exists():
        raise SystemExit('Archive already exists')
    rev = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    with archive.open('wb') as out:
        out.write(f'Scheduling source documents; commit={rev}\n'.encode())
        for name in DOCUMENTS:
            raw = (ROOT / name).read_bytes()
            blob = subprocess.check_output(['git', 'hash-object', '--stdin'], cwd=ROOT, input=raw).decode().strip()
            out.write(f'\n===== {name} | blob={blob} | bytes={len(raw)} =====\n'.encode())
            out.write(raw)
            out.write(b'\n===== END SOURCE =====\n')
    for name, body in DOCUMENTS.items():
        (ROOT / name).write_text(body, encoding='utf-8', newline='\n')
