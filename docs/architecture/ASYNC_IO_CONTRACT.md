# Async I/O Contract（CON-008 有界异步 I/O 合同）

> 状态：CON-008 IN_PROGRESS
> 关联代码：`lib/phase2/include/astro/phase2/async_io.h`
> 目标：HiPS/FITS/XISF 读取与计算解耦，使用有界 producer/consumer；
> 队列容量由 `memory_budget_bytes / item_bytes` 推导，禁止无界队列。

## 1. 模型

- **生产者**：读取 HiPS/FITS/XISF tile / 文件块，产出 buffer 所有权对象。
- **消费者**：计算/写入阶段消费 buffer。
- **队列**：`astro::phase2::BoundedAsyncQueue<T>`，固定容量，满时生产者阻塞（背压），
  空时消费者阻塞。
- **容量**：`bounded_queue_capacity(memory_budget_bytes, item_bytes)`；
  `item_bytes` 必须包含该条目在队列中持有期间的全部内存占用（buffer + 控制块）。
  最低容量为 1，禁止 0。

## 2. Buffer 所有权

- 每队列条目持有 `T` 的所有权；`T` 必须是可移动类型（`std::move` 入队）。
- 入队后，生产者不再访问该 buffer；出队后所有权移交给消费者。
- 消费者处理后必须显式释放或复用，不得把已出队 buffer 重新入队。
- 禁止两个线程同时读写同一 buffer；每个 `TilePair` 等条目绑定唯一 reader 会话。

## 3. Reader 线程安全

- 已有生产结论：cfitsio 同一 `AioHipsDataset*` 不保证并发读安全。
- 因此每个 worker / 每个 reader 使用独立数据集句柄；异步 IO 池中
  `io_workers` 个 worker 各自持有自己的 reader 实例。
- 共享句柄只允许串行读取；若要在异步队列中共享，需要由 reader 层加串行锁，
  并计入串行时间预算（当前不采用，优先 per-worker reader）。

## 4. 异常传播

- `BoundedAsyncQueue::cancel(reason)` 唤醒所有阻塞者，后续 `push/pop` 返回失败。
- Worker 捕获读取/写入异常后调用 `cancel`，主线程通过 `error()` 获得第一错误文本。
- 禁止在 worker 线程中向主线程“尽力而忽略”错误；未取消且未关闭的队列不得静默吞错。

## 5. 取消

- 取消语义：
  - 已入队条目可以被丢弃（取决于消费端策略）；
  - 阻塞在 `push` 的生产者立即返回 `false`；
  - 阻塞在 `pop` 的消费者立即返回 `nullopt`；
  - 已持有的 buffer 由各自 owner 释放。
- 取消后不可恢复，必须重建 pipeline。

## 6. Flush / Shutdown

- **正常关闭**：`close()`。生产者停止入队，消费者继续排空队列，直到 `pop() == nullopt`。
- **Flush**：消费者必须在计算阶段结束时保证所有已入队 buffer 被消费；对写队列，
  flush 表示等待所有 writer 完成并关闭文件。
- **Shutdown**：所有 worker 线程 join 后销毁队列；禁止在仍有线程访问时析构。

## 7. 内存预算

| 参数 | 含义 |
|---|---|
| `memory_budget_bytes` | 该 pipeline 可用内存预算 |
| `item_bytes` | 单条目在队列中占用的估算字节 |
| `capacity()` | 队列条目上限 = `max(1, budget / item_bytes)` |

- 队列容量不是全局内存上限；各阶段还需要自己的工作缓冲预算。
- 严禁用 `std::deque::size()` 无限增长；超预算应通过背压或分块降级。

## 8. 测试要求

- 必须覆盖：
  - 队列满背压；
  - 正常 close 排空；
  - cancel 唤醒阻塞者并传播错误；
  - 读取失败/写入失败传导；
  - 多 worker 并发消费确定性。
- 当前已覆盖队列基座；生产型模拟读取/写入失败待接入 stage2/sampler 后补齐。

## 9. 待接入点

- `stage2.cpp`：tile 读取/写入可使用 `BoundedAsyncQueue<TileReadTask>` /
  `BoundedAsyncQueue<TileWriteResult>`；
- `sampler.cpp`：`SamplerReader` 可扩展为每 worker 异步预取队列；
- `aio_hips_reader` / `aio_hips_writer`：保持每个 worker 独立句柄，避免共享
  cfitsio 句柄。
