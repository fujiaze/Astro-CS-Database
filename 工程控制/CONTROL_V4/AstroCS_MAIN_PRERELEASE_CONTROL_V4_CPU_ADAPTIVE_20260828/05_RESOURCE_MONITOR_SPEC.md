# 重计算资源监控规范

## 1. 统一入口

实现 `tools/perf/run_profile.py`（或同等跨平台入口）。所有预计或实际超过5秒的计算命令必须通过它启动；直接运行视为无效证据。

## 2. 采样

默认每500 ms记录：

- wall、process user/sys CPU、CPU core-equivalent、线程数；
- 系统 CPU busy/iowait；
- RSS/PSS/commit/private bytes、available memory、swap；
- minor/major faults、context switches；
- process/system read/write bytes和吞吐；
- 磁盘 busy/await/queue（平台可得时）；
- stage name、stage kind、selected workers/variant；
- exit code、signal、timeout、取消原因。

原始 series 保存于运行节点；审核包只带摘要 JSON 和必要的降采样 CSV。

## 3. Stage 标记

CLI 输出结构化 `stage_begin/stage_end`，每段声明：

- `compute`：应使用CPU；
- `memory`：以本机内存带宽为上限；
- `io`：低CPU允许；
- `mixed`：分别报告计算与等待。

不得把整个 CLI 混成一个平均数掩盖计算阶段低利用率。

## 4. 自动判定

对持续超过5秒的 `compute`：

- `max_threads >= 2`（当可用CPU>=2）；
- 平均使用核数应达到 `0.80 × min(selected_workers, available_cpus)`；
- 1-worker 与 autotuned worker 比较必须有实际吞吐提升；
- CPU低且 iowait、磁盘吞吐、内存带宽均不高时直接 FAIL。

对 `memory`：

- 吞吐达到同机内存基准的合理比例；
- 若带宽未饱和且CPU也低，FAIL；
- 报告 cache miss/布局证据（工具可得时）。

对 `io`：

- 低CPU允许；短于5秒或低于总wall 5%的I/O不作为并行门禁；
- 长I/O必须证明磁盘/网络吞吐或等待，而不是全局锁空转。

## 5. 内存健康

- 记录 warm-up 后 RSS/PSS 曲线、峰值、结束回落、swap变化。
- 重复相同工作单元时做内存趋势回归；持续单调增长或每轮不回落必须 FAIL 并定位。
- OOM、swap thrash、major fault 爆发、队列无界增长立即停止。

## 6. 结果

每次生成：

- `profile_summary.json`；
- `profile_series.csv`（运行节点保留）；
- `utilization_verdict.json`；
- stage级瓶颈分类和下一步建议。

禁止只输出一张总CPU截图或口头声称“已并行”。

