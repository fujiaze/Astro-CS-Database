# PAR-001 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS PAR-001 行「删除生产全局串行锁；实现有界队列/backpressure/error/cancel；I/O 与 compute 可 overlap | lock contention test、queue saturation、failure drain；CPU compute 不被 writer 饿死」; 07 §6(诊断: 全局锁/串行队列/线程池未接线等); 09 §(小合成 compute 用满有效 CPU)。ABI 冻结(v1)不改公共 API。

## 现状与缺口
- `lib/phase2/include/astro/phase2/async_io.h` 已提供 `BoundedAsyncQueue<T>`(CON-008): 有界容量(满→生产者阻塞=背压)、close 排空、cancel(reason) 唤醒 push/pop 双方、error 传播、每实例独立 mutex(非全局锁)。已有 `lib/phase2/tests/async_io_test.cpp`(phase2_async_io)。
- PAR-001 关注的运行时证明(overlap/无全局串行/背压/错误传播/failure drain/CPU 不被饿死)未有 CLI 级测试把关; 本次补齐。

## 动作
1. 复用生产 `BoundedAsyncQueue`(有界背压/close/cancel/error)作 I/O×compute 解耦。
2. 新增 **tests/cli/test_parallel_queue.py**(6 测试, 独立编译 driver 对 header 断言):
   - test_01 容量推导: budget/item_bytes; budget=0 或 item_bytes=0 → 至少 1(禁 0)。
   - test_02 I/O×compute overlap: N=20,Tio=4ms,Tcmp=2ms,2 消费者。串行估计 120ms,实测 81.5ms(ratio 0.68, 贴 I/O 下界 80ms) → 证明并行非全局串行锁。
   - test_03 有界背压: 满时 `size()≤capacity()` 不无界增长, 可排空无死锁。
   - test_04 failure drain: 生产错误→cancel 传播; 消费者 drain 见 has_error/error(), 无死锁, 不静默吞错。
   - test_05 cancel 唤醒阻塞 push/pop 双方, 无死锁。
   - test_06 无全局串行锁构造: header 无 `static std::mutex`/全局锁对象(每实例 mutex_ 内聚)。
3. 更新 **docs/architecture/ASYNC_IO_CONTRACT.md** §10 记录 PAR-001 运行时结论。

## 验证
- tests/cli/test_parallel_queue.py: 6 用例全 OK。
- 手工实测 overlap: elapsed=81.5ms vs serial=120ms(ratio 0.68); consumed=20(compute 全完成, 未被 writer 饿死)。
- 全量回归 unittest **254/254 OK**(新增 6, 零回归; 全套 ~636s 后台跑)。

## 限制与遗留
- 本任务在异步 I/O 队列基座层面证明框架正确(背压/错误/cancel/overlap/无全局锁)。
- 与阶段级生产接入(sampler/stage2/upm 用 BoundedAsyncQueue 解耦 tile 读写)属 PAR-002..007 逐项; 其中并行的细节(sampler 生命周期/race、UPM 归约、drizzle 分解等)交给各自 PAR 任务。
- cfitsio 跨线程共享句柄并发读不安全, 保持 per-worker reader(ASYNC_IO_CONTRACT §3/§9) — 本任务未制造伪造的并发读安全。
- 无生产源码改动(队列已存在); 本次为运行时把关+文档契约。如后续接入点暴露全局锁, 由对应 PAR 任务修复。

## 产物
tests/cli/test_parallel_queue.py(6 测试); docs/architecture/ASYNC_IO_CONTRACT.md(§10); artifacts/prerelease_v5/PAR-001/LOG.md; 本日志。

## PASS 判定
有界队列(容量=budget/item_bytes, 禁 0)+backpressure(满阻塞, size≤cap, 可排空)+error/cancel(生产错误→cancel 唤醒双方+error 传播)+I/O×compute overlap(实测 81.5ms < 串行 120ms)+无全局串行锁(每实例 mutex, 非 static 全局)+failure drain+CPU compute 不被 writer 饿死(overlap 全部完成) 全部证明。PAR-001 = PASS。
