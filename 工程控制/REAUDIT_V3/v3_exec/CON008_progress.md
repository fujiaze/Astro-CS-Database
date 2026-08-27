# CON-008 progress note

- Commit: 10038458a52359d0dfdc422e23f44c004ae714ec
- Pushed origin/main: yes (253e7ee..1003845)
- Added:
  - `astro::phase2::BoundedAsyncQueue<T>` header-only bounded producer/consumer
    with backpressure, close/drain, cancel/error propagation.
  - `bounded_queue_capacity(memory_budget_bytes, item_bytes)` helper for memory-budget
    derived queue capacity.
  - Translation unit `async_io.cpp` and CMake target `phase2_async_io`.
  - 5 unit tests covering roundtrip, close/drain, backpressure, cancel/error.
- Validation:
  - `phase2_async_io` PASS in ON and OFF builds (5/5).
- Remaining for CON-008:
  - Production wiring into stage2/sampler/AIO read/write pipelines.
  - Document ownership/exception/cancel/flush/shutdown in ARCH docs.
  - Test read failure/write failure/queue full in production-like pipeline.

## 2026-08-27 第6轮尝试：AsyncTileReader 背景线程读取受阻

- 尝试在 `BoundedAsyncQueue` 之上实现 `HipsAsyncTileReader`，每 worker 独立
  `AioHipsDataset` handle 读取 signal/support tile。
- 结果：
  - io_workers=1 时测试**挂起**；
  - io_workers=2 时出现 **segfault**；
  - 表明当前 AIO 读取路径在非主线程/多线程并发下不确定安全（与 sampler
    并行真实数据失败现象一致）。
- 本轮未提交该未验证分支，已从工作树移除，保持 main 干净。
- 后续 CON-008 需先解决 AIO reader 线程安全：
  1. 确认 cfitsio/AIO 是否允许后台线程打开/读取（可能需在独立 IO 线程内
     串行化所有 AIO 调用，或使用每 worker 独立进程/句柄并验证）；
  2. 若无法证明线程安全，异步 IO 只能作为“有界预取队列 + 单一 IO 线程”
     形态落地，并明确记录该约束，不能伪造并发读安全。
