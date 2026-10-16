# 有界异步 I/O

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
