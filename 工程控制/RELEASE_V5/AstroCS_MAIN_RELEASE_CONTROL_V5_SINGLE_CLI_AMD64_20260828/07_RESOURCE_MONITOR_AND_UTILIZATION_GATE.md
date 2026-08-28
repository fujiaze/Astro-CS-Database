# 内置资源监控与“禁止低利用率”门禁

## 1. 强制启用

所有 `benchmark/test synthetic/phase*/run` 自动启动进程级监控；不能通过普通配置关闭。允许 `--resource-detail summary|timeseries` 控制粒度，但 summary 仍强制存在。

每个 stage 在代码中标注 `compute|memory|io|mixed`，并发出开始/结束事件。没有 stage 标注的 >5 秒区间视为 P1。

## 2. 必采指标

- 进程 user/system CPU 时间、平均/峰值等效核数；
- thread count、runnable threads、context switches；
- RSS/PSS（Linux）、private working set/commit（Windows）、峰值与斜率；
- 系统可用内存、swap/page faults；
- 进程 read/write bytes/ops；磁盘 busy/await/queue（可得时）；
- 选择的 kernel/backend/workers/block；
- stage wall time、输入规模、吞吐；
- 1-worker 对照的合成 microbenchmark speedup；
- memory benchmark 带宽及本 stage 的估算/实测带宽比例。

原始 timeseries 留在执行节点；审核包只放摘要和降采样曲线，禁止再次打包几十 MB 日志。

## 3. 机器门禁

对 `compute` 且 wall >=5s：

- `available_cpus >=2` 时 `selected_workers >=2`、观察到 `max_active_threads >=2`；
- `avg_equivalent_cores >= 0.80 * min(selected_workers, available_cpus)`；
- 合成同规模 N-worker 相对 1-worker 必须有正向加速，且不得因全局锁退化；
- CPU 低、iowait 低、memory bandwidth 也低：直接 FAIL，不得解释为“单线程算法正常”。

对 `memory`：允许 CPU 未满，但必须证明吞吐达到同次 memory benchmark 的预冻结比例；否则 FAIL。比例由 BENCH-003 在看结果前写入测试合同，不能事后修改。

对 `io`：低 CPU 合法；但必须有 bytes/ops/await 证据。短于 5s 或总时长低于 5% 的串行 I/O 不作为发布阻塞。

`mixed` 必须拆出 compute/io 子区间；不得用 mixed 标签掩盖低利用率。

## 4. 快速失败，避免浪费 32R

- 每个重任务先做预检和短 synthetic representative run。
- 正式 run 前 10 秒若满足“低 CPU + 非 I/O + 非内存带宽饱和”，协作取消，返回 10。
- Windows 32R 只有所有 synthetic/小真实数据资源门禁通过后才允许启动。

## 5. 内存泄漏/增长

为每个可重复 stage 做至少 20 次小规模循环：丢弃预热后，用稳健斜率、峰值和 allocator/OS 指标判断。任何无界增长、每轮 retained bytes 持续增加或接近 OOM 均 FAIL。Sanitizer/ASan 通过不能替代运行曲线。

## 6. 自动诊断输出

失败必须给出最可能类别与证据：全局锁/串行队列、线程池未接线、任务粒度过细、nested oversubscription、I/O 等待、memory bound、allocation/churn、page fault、泄漏、错误 affinity。不得只输出“CPU 使用率低”。

