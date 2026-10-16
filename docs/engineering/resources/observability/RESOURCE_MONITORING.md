# 资源监控合同

本合同解释资源采样字段、身份、完整性与实现边界。资源预算和评价见[性能模型](../PERFORMANCE_MODEL.md)，正式运行事件见[CLI 协议](../../contracts/CLI_PROTOCOL.md)。采样只观察执行，不改变科学算法、精度或调度决定。

## 1. 两种时序工件

| 文件 | 生产者 | 格式及生命周期 |
|---|---|---|
| `resource_timeseries.csv` | `lib/infrastructure/cli/resource_recorder.h` | 生产 CLI 资源记录，运行收尾写出，列与该生产者同步 |
| `monitor_timeseries.csv` | `lib/infrastructure/observability/monitoring/monitor.py` | Python 伴随器，每秒采样及区间边界采样，含 seed 与行指纹链 |

两者按文件身份消费，不能互替。Python `runner.py::HeavyRunGuard` 要求其管理的 `cpu_heavy/io` 运行先创建监控再执行；这是该辅助执行器的检查行为，不代表三阶段 CLI 已自动接入它，也不构成研发审批。

`ResourceMonitor` 的调用为 `begin → start → set_phase/采样 → stop → seal`。运行层拥有实例和采样线程；区间为 `init/active/io/flush`，切换前记录离开区间的边界样本。周期默认1秒，实际 `interval_s` 使用单调时钟，短区间不能按默认间隔假定工作量。

## 2. Python CSV 字段

UTF-8/LF。header 顺序为下表顺序；最后一列是 `row_fingerprint`。header 后一行是 `seq=0` 的 seed，之后数据样本为 `seq=1..N`。具体序列化与校验由 `monitor.py` 的 `HEADER`、`_fingerprint`、`verify_csv` 实现。

| 字段 | 来源、单位及解释 |
|---|---|
| `t_iso_utc` | UTC 秒精度墙钟，用于展示和跨进程对齐 |
| `seq` | 本文件样本序号，严格递增 |
| `run_id` | 实际运行身份，所有行一致 |
| `run_phase` | `init/active/io/flush` |
| `interval_s` | 距上个样本单调时间差，秒 |
| `cpu_pct` | 本进程 CPU 秒差/墙钟秒差×100，100%为一核 |
| `sys_cpu_pct` | 系统非空闲/总 jiffies，整机百分比 |
| `active_workers` | 当前 observer 实际返回活动节点数，不能直接解释为忙碌计算线程 |
| `granted_workers` | 当前 observer 扫描事件中正租约最大值；无该值时回落到最近正 `workers` |
| `provider` | 最近携带非空 provider 的受支持 trace 事件 |
| `module` | 最近 module/node/worker 归属事件中的模块或节点 ID |
| `rss_bytes` | Linux VmRSS，字节 |
| `private_bytes` | Linux RssAnon，字节；不等于所有平台的 private bytes |
| `commit_bytes` | 当前 Linux 后端的 VmSize，字节；是虚拟地址空间，不能作为 Windows commit 使用 |
| `read_bytes` / `write_bytes` | `/proc/self/io` 的物理存储 I/O 计数，字节；与缓存命中的逻辑读写分开 |
| `queue_wait` | 当前后端读取系统一分钟 loadavg；该字段不是 ACSD 队列等待时长 |
| `lock_wait_ns_est` | 非自愿上下文切换差值×1000 的代理；没有直接锁等待时间测量 |
| `io_wait_rate_est` | 缺页次数差/秒的代理；不能作为块 I/O 等待时间 |
| `faults_rate` | minflt+majflt 差/秒 |
| `row_fingerprint` | 本行及前行链式 SHA-256，64位十六进制 |

缺失/不可得数值应留空，真零才填0。当前 observer 的0还可能是未观测哨兵，消费者必须按来源区分；既有字段名保留兼容，不改变其统计含义。上述代理的量纲和效度疑点见[待推导问题](../../../../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)。

## 3. 指纹链与证据强度

实现使用公开固定 salt `b"acsd-monitor-v1"`。seed 绑定有序 header 和 run_id；每行绑定上一指纹、按约定 JSON 规范化的列字符串和序号。校验核对 header、seed、链、连续 seq、run_id、区间枚举及时间戳。墙钟秒可相等，历时仍由单调时钟测量。

`seal` 执行 flush、fsync、关闭和只读权限设置；`begin` 拒绝对非空 CSV 续写。未重新计算链的改行、插行和截断会被相应检查发现，但链不保证内容来自真实采样：持有公开算法的人仍能重建整条链，只读权限也不是来源认证。真实观测结论需要外部运行身份、命令、二进制/输入与产物哈希、采样来源和独立保存的锚共同支持。

校验成功只说明已检查结构与链一致，不证明指标科学有效、无漏采、无系统时间跳变或三阶段产品正确。旧文中“不可手工合成”的强断言及错误 salt/排序描述已保存在[来源档案](../../../../实验/documentation-refactor/docs/RESOURCE_RECORDS_SOURCE.txt)。本轮不更改文件格式或指纹算法。

## 4. 观测与平台边界

`trace_feed.py::TraceSnapshotObserver` 复制构造时事件列表，`observe()` 重放该快照；它不是自动跟随新事件的实时订阅。当前实现没有按构造 `run_id` 过滤事件，也没有只取活动节点租约；调用方须先限定单运行输入，旧注释中的实时/活动租约语义不能作为已实现能力。

Linux 后端是 `linux_procfs.py`，默认读取 `/proc/self`，测得的是调用该采集器的进程。外挂监控另一产品 PID 时需使用对应目标进程采集器，不能把伴随器自己的CPU记为产品CPU。系统 `/proc/stat` iowait、进程树 delayacct 块等待与缺页速率具有不同对象和单位。

`windows_pdh_etw.py` 当前为显式不可用 stub，不能据同名接口宣称已经支持 Windows 采样。CLI 自己的 Windows 资源记录与这个 Python 后端分别核验。

## 5. 生命周期、错误与验证

采样线程和区间切换由实例锁串行化，stop 后收尾并关闭句柄。采样失败、丢失样本和不可用后端明确报告；测试注入 collector 与真实观测分开留档。metric 日志可通过 `trace_feed.emit_metric_event` 适配到[结构化日志](STRUCTURED_LOGGING.md)，不能凭输出相同 run_id 判断已完成运行层接线。

验证覆盖真实目标身份、短区间分段、序号与采样缺失、结构/链损坏、跨 run 混入、旧快照与实时数据的差别，以及各平台后端。性能评价按实际统计量解释，CPU、内存和线程数据不自动决定产品退出码或发布。
